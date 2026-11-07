/**
 * Firmware for Seeed Studio XIAO ESP32C3 (RISC-V Architecture)
 * Ultra-compact 21x17.5mm stealth controller for electric vehicle security
 * 
 * TARGET: Seeed Studio XIAO ESP32C3 (RISC-V 32-bit single-core @ 160MHz)
 * DUAL-ROLE BLE GATEWAY + DUAL-MODE HYBRID IMMOBILIZER (WIRELESS BLE & WIRED RELAY)
 */

#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_task_wdt.h>
#include <driver/twai.h>
#include "xiao_pin_config.h"

// Automotive Mobile App GATT Service UUIDs (ESP32 acts as GATT Server)
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_TELEMETRY_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_COMMAND_UUID      "d29ae63c-b783-49a3-8c46-9b76c8c25f77"
#define CHAR_SECURITY_UUID     "f5a14603-91b7-4a0b-8d02-12a8f89e2101"

// FarDriver FOC Controller GATT UUIDs (ESP32 acts as GATT Client)
#define FARDRIVER_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define FARDRIVER_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

// Raw 8-Byte Hexadecimal Control Payloads for FarDriver BLE Injection
const uint8_t FARDRIVER_LOCK_HEX[8]   = {0xAA, 0x02, 0xA0, 0x01, 0x00, 0x00, 0xBE, 0xEF};
const uint8_t FARDRIVER_UNLOCK_HEX[8] = {0xAA, 0x02, 0xA0, 0x00, 0x00, 0x00, 0xDE, 0xAD};

// Core Vehicle State Machine & 3-State Geofence Proximity Subsystem
enum VehicleState {
    SYSTEM_LOCKED,
    NORMAL_RUNNING,
    DECELERATION_RAMP,
    SAFE_STATIONARY
};

typedef enum { STATE_LOCKED, STATE_UNLOCKED, STATE_HOME_PARKED } BikeState;
static BikeState current_bike_state = STATE_LOCKED;

struct XiaoC3State {
    char masterPin[7]            = "123456";
    char farDriverPairPin[7]     = "123456";
    char farDriverParamPass[7]   = "888888";
    VehicleState currentState    = SYSTEM_LOCKED;
    bool isArmed                 = true;
    bool ignitionRelay           = false;
    bool throttleCutoff          = true;  // Physical D0 Relay State
    bool alarmTriggered          = false;
    uint8_t failedAttempts       = 0;
    bool useWirelessImmobilizer  = true;  // NVS Flag: true = BLE Frame Injection, false = D0 Relay
    bool farDriverBleConnected   = false;
    uint32_t phoneDisconnectTime = 0;
} secState;

// Live FarDriver FOC Telemetry
struct FarDriverTelemetry {
    uint16_t rpm = 0;
    float currentA = 0.0f;
    float voltageV = 72.4f;
    uint8_t mosfetTemp = 36;
    uint8_t motorTemp = 42;
    uint8_t faultCode = 0x00;
} fdLive;

// Non-Volatile Storage (Preferences.h)
Preferences preferences;

// BLE Server & Client Handles
BLEServer *pServer = nullptr;
BLECharacteristic *pTelemetryChar = nullptr;
BLECharacteristic *pSecurityChar = nullptr;
BLEClient *pFarDriverClient = nullptr;
BLERemoteCharacteristic *pFarDriverRemoteChar = nullptr;
bool isPhoneConnected = false;
volatile uint32_t lastCanFrameMs = 0;
volatile bool remoteCommandStationary = false;

// Task Handles
TaskHandle_t TaskCANHandle;
TaskHandle_t TaskBMSHandle;

// Spinlock for Wheel Hall Sensor Interrupt (Pin D2)
portMUX_TYPE hallMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t wheelPulseCounter = 0;
volatile uint32_t lastPulseTimeUs = 0;

void IRAM_ATTR onWheelHallPulse() {
    portENTER_CRITICAL_ISR(&hallMux);
    wheelPulseCounter++;
    lastPulseTimeUs = micros();
    portEXIT_CRITICAL_ISR(&hallMux);
}

// Forward Declarations
void injectFarDriverHex(const uint8_t *payload, size_t len);
void setImmobilizerState(bool locked);
void connectToFarDriverBle();

// ================= FreeRTOS Task: FarDriver TWAI CAN Bus (250kbps) =================
void taskFarDriverCAN(void *pvParameters) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)PIN_XIAO_C3_CAN_TX, 
        (gpio_num_t)PIN_XIAO_C3_CAN_RX, 
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();

    twai_message_t rx_msg;
    for (;;) {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(50)) == ESP_OK) {
            if (rx_msg.identifier == 0x0CF11E05 && rx_msg.data_length_code == 8) {
                fdLive.rpm = (rx_msg.data[0] << 8) | rx_msg.data[1];
                fdLive.currentA = ((rx_msg.data[2] << 8) | rx_msg.data[3]) * 0.1f;
                fdLive.voltageV = ((rx_msg.data[4] << 8) | rx_msg.data[5]) * 0.1f;
                fdLive.mosfetTemp = rx_msg.data[6] - 40;
                fdLive.motorTemp = rx_msg.data[7] - 40;
                lastCanFrameMs = millis();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ================= FreeRTOS Task: JK-BMS TTL UART (115200 Baud) =================
void taskJkBmsUART(void *pvParameters) {
    Serial1.begin(115200, SERIAL_8N1, PIN_XIAO_C3_BMS_RX, PIN_XIAO_C3_BMS_TX);
    uint8_t pollCommand[6] = {0xDB, 0xDB, 0x00, 0x00, 0x00, 0x00};

    for (;;) {
        Serial1.write(pollCommand, 6);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ================= FarDriver Over-the-Air BLE Hex Injection =================
void injectFarDriverHex(const uint8_t *payload, size_t len) {
    if (pFarDriverRemoteChar != nullptr && pFarDriverRemoteChar->canWrite()) {
        pFarDriverRemoteChar->writeValue((uint8_t*)payload, len, false);
        Serial.print("[BLE CLIENT] Injected FarDriver Hex Frame: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf("%02X ", payload[i]);
        }
        Serial.println();
    } else {
        Serial.println("[BLE CLIENT] FarDriver remote characteristic unavailable for write!");
    }
}

// ================= DUAL-MODE HYBRID IMMOBILIZER LOGIC =================
void setImmobilizerState(bool locked) {
    if (secState.useWirelessImmobilizer) {
        // MODE A: PURE WIRELESS SOFTWARE WORKAROUND
        // Bypass physical relay D0 and throttle PWM pins; inject 8-byte hex frames to FarDriver BLE
        if (locked) {
            injectFarDriverHex(FARDRIVER_LOCK_HEX, sizeof(FARDRIVER_LOCK_HEX));
            secState.currentState = SYSTEM_LOCKED;
        } else {
            injectFarDriverHex(FARDRIVER_UNLOCK_HEX, sizeof(FARDRIVER_UNLOCK_HEX));
            secState.currentState = NORMAL_RUNNING;
        }
        // Ensure physical relays remain relaxed/transparent in wireless mode
        digitalWrite(PIN_XIAO_C3_THROTTLE_CUT, LOW);
    } else {
        // MODE B: ORIGINAL PHYSICAL HARNESS & RELAY CUTOUT (D0 & D8)
        if (locked) {
            digitalWrite(PIN_XIAO_C3_THROTTLE_CUT, HIGH); // Trip relay to interrupt 5V Hall supply
            digitalWrite(PIN_XIAO_C3_IGNITION, LOW);      // Open 12V Contactor
            ledcWrite(0, 0);                              // Zero D8 throttle PWM override
            secState.currentState = SYSTEM_LOCKED;
        } else {
            digitalWrite(PIN_XIAO_C3_THROTTLE_CUT, LOW);  // Closed contact passes 5V
            digitalWrite(PIN_XIAO_C3_IGNITION, HIGH);     // 12V Contactor ON
            ledcWrite(0, 255);                            // Restore full throttle pass-through
            secState.currentState = NORMAL_RUNNING;
        }
    }
}

// ================= BLE Server Callbacks (Phone App) =================
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        isPhoneConnected = true;
        secState.phoneDisconnectTime = 0;
        Serial.println("[BLE SERVER] Phone connected.");
    }
    void onDisconnect(BLEServer* pServer) {
        isPhoneConnected = false;
        secState.phoneDisconnectTime = millis();
        Serial.println("[BLE SERVER] Phone disconnected! Motion grace active if riding.");
        pServer->startAdvertising();
    }
};

class CommandCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
        String cmd = pChar->getValue().c_str();
        Serial.printf("[BLE CMD RX] %u bytes\n", static_cast<unsigned>(cmd.length()));

        // Dynamic Runtime Configuration Switching Packet: {"config_mode": "wireless"} or {"config_mode": "wired"}
        if (cmd.indexOf("\"config_mode\": \"wireless\"") >= 0 || cmd.indexOf("\"config_mode\":\"wireless\"") >= 0) {
            secState.useWirelessImmobilizer = true;
            preferences.putBool("wl_immob", true);
            Serial.println("[CONFIG] Switched to WIRELESS BLE FarDriver Override (Relay Bypassed)");
            pSecurityChar->setValue("CONFIG:WIRELESS_BLE_ACTIVE");
            pSecurityChar->notify();
            return;
        } else if (cmd.indexOf("\"config_mode\": \"wired\"") >= 0 || cmd.indexOf("\"config_mode\":\"wired\"") >= 0) {
            secState.useWirelessImmobilizer = false;
            preferences.putBool("wl_immob", false);
            Serial.println("[CONFIG] Switched to WIRED RELAY (D0 & D8 Physical Fallback Active)");
            pSecurityChar->setValue("CONFIG:WIRED_RELAY_ACTIVE");
            pSecurityChar->notify();
            return;
        }

        // Authorization commands
        if (cmd.startsWith("DISARM:")) {
            if (!remoteCommandStationary) {
                pSecurityChar->setValue("ACTION_REJECTED:STATIONARY_STATE_UNVERIFIED");
                pSecurityChar->notify();
                return;
            }
            String pin = cmd.substring(7);
            if (pin.equals(secState.masterPin)) {
                secState.isArmed = false;
                secState.alarmTriggered = false;
                secState.failedAttempts = 0;
                setImmobilizerState(false);
                pSecurityChar->setValue("AUTH_SUCCESS:DISARMED");
            } else {
                secState.failedAttempts++;
                if (secState.failedAttempts >= 3) {
                    secState.alarmTriggered = true;
                    digitalWrite(PIN_XIAO_C3_SIREN, HIGH);
                }
                pSecurityChar->setValue("AUTH_FAILED:INVALID_PIN");
            }
            pSecurityChar->notify();
        } else if (cmd == "ARM") {
            if (!remoteCommandStationary) {
                pSecurityChar->setValue("ACTION_REJECTED:STATIONARY_STATE_UNVERIFIED");
                pSecurityChar->notify();
                return;
            }
            secState.isArmed = true;
            setImmobilizerState(true);
            pSecurityChar->setValue("STATUS:ARMED");
            pSecurityChar->notify();
        }
    }
};

// ================= BLE Client Connection to FarDriver =================
class FarDriverClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        secState.farDriverBleConnected = true;
        Serial.println("[BLE CLIENT] Connected to FarDriver internal Bluetooth module.");
    }
    void onDisconnect(BLEClient* pclient) {
        secState.farDriverBleConnected = false;
        pFarDriverRemoteChar = nullptr;
        Serial.println("[BLE CLIENT] FarDriver disconnected. Will retry...");
    }
};

void connectToFarDriverBle() {
    pFarDriverClient = BLEDevice::createClient();
    pFarDriverClient->setClientCallbacks(new FarDriverClientCallback());

    // Connect using FarDriver default BLE service
    BLERemoteService* pRemoteService = pFarDriverClient->getService(BLEUUID(FARDRIVER_SERVICE_UUID));
    if (pRemoteService != nullptr) {
        pFarDriverRemoteChar = pRemoteService->getCharacteristic(BLEUUID(FARDRIVER_CHAR_UUID));
        if (pFarDriverRemoteChar != nullptr) {
            Serial.println("[BLE CLIENT] FarDriver FFE1 characteristic discovered successfully.");
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize 1500ms Hardware Safety Watchdog
    esp_task_wdt_init(1500, true);
    esp_task_wdt_add(NULL);

    // Initialize Non-Volatile Storage (Preferences.h)
    preferences.begin("ebike_sec", false);
    secState.useWirelessImmobilizer = preferences.getBool("wl_immob", true);
    Serial.printf("[BOOT] NVS useWirelessImmobilizer: %s\n", secState.useWirelessImmobilizer ? "TRUE" : "FALSE");

    // Inspect Wakeup Cause from ESP32 RTC Power Management
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("[BOOT] Woken via RTC Ext0 (D3 Key Switch / Emergency Override)");
        secState.isArmed = false;
        secState.throttleCutoff = false;
        secState.alarmTriggered = false;
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println("[SECURITY] Woken via RTC Ext1 (D2 Motor Hall Kinetic Tamper)");
        if (secState.isArmed) {
            secState.alarmTriggered = true;
            digitalWrite(PIN_XIAO_C3_SIREN, HIGH);
        }
    }

    // Configure Hardware Pins (Pin D9 mechanical switch dropped to eliminate strapping pin conflict)
    pinMode(PIN_XIAO_C3_IGNITION, OUTPUT);
    pinMode(PIN_XIAO_C3_THROTTLE_CUT, OUTPUT);
    pinMode(PIN_XIAO_C3_SIREN, OUTPUT);
    pinMode(PIN_XIAO_C3_EBRAKE, INPUT_PULLUP);
    pinMode(PIN_XIAO_C3_TILT, INPUT_PULLUP);
    pinMode(PIN_XIAO_C3_KEY_OVERRIDE, INPUT_PULLUP); // D3: Safe digital input with pull-up
    pinMode(PIN_XIAO_C3_HALL_PULSE, INPUT_PULLDOWN); // D2: Motor Hall kinetic wake input
    pinMode(PIN_XIAO_C3_LED, OUTPUT);

    // Attach Wheel Hall Pulse Interrupt on D2 with spinlock protection
    attachInterrupt(digitalPinToInterrupt(PIN_XIAO_C3_HALL_PULSE), onWheelHallPulse, RISING);

    // Initialize D8 LEDC PWM Throttle Channel (10kHz, 8-bit resolution)
    ledcSetup(0, 10000, 8);
    ledcAttachPin(PIN_XIAO_C3_PWM_THROTTLE, 0);
    ledcWrite(0, 255);

    // Initial safe physical state
    digitalWrite(PIN_XIAO_C3_IGNITION, secState.isArmed ? LOW : HIGH);
    digitalWrite(PIN_XIAO_C3_THROTTLE_CUT, secState.isArmed ? HIGH : LOW);
    digitalWrite(PIN_XIAO_C3_SIREN, secState.alarmTriggered ? HIGH : LOW);

    // Initialize Dual-Role BLE Subsystem
    BLEDevice::init("XIAO-EBIKE-SEC");

    // GATT Server for mobile phone app
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTelemetryChar = pService->createCharacteristic(CHAR_TELEMETRY_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pTelemetryChar->addDescriptor(new BLE2902());

    BLECharacteristic *pCmdChar = pService->createCharacteristic(CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
    pCmdChar->setCallbacks(new CommandCallback());

    pSecurityChar = pService->createCharacteristic(CHAR_SECURITY_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    pSecurityChar->addDescriptor(new BLE2902());

    pService->start();
    BLEDevice::startAdvertising();

    // Start FreeRTOS Tasks on RISC-V Core
    xTaskCreate(taskFarDriverCAN, "CAN_Task", 3072, NULL, 3, &TaskCANHandle);
    xTaskCreate(taskJkBmsUART, "BMS_Task", 3072, NULL, 2, &TaskBMSHandle);
}

void loop() {
    // Pet 1500ms Hardware Safety Watchdog
    esp_task_wdt_reset();

    // Calculate current speed from Wheel Hall interrupt
    portENTER_CRITICAL(&hallMux);
    uint32_t pulses = wheelPulseCounter;
    wheelPulseCounter = 0;
    portEXIT_CRITICAL(&hallMux);

    int currentSpeedKmh = (int)(fdLive.rpm * 0.015f);
    const uint32_t nowMs = millis();
    const bool canTelemetryFresh = lastCanFrameMs != 0 && nowMs - lastCanFrameMs <= 1000;
    const bool zeroCurrent = fdLive.currentA > -0.5f && fdLive.currentA < 0.5f;
    static uint32_t stationarySinceMs = 0;
    if (canTelemetryFresh && fdLive.rpm == 0 && zeroCurrent) {
        if (stationarySinceMs == 0) stationarySinceMs = nowMs;
        remoteCommandStationary = nowMs - stationarySinceMs >= 3000;
    } else {
        stationarySinceMs = 0;
        remoteCommandStationary = false;
    }

    // Physical Tamper Shock Sensor Trigger
    if (secState.isArmed && digitalRead(PIN_XIAO_C3_TILT) == LOW) {
        secState.alarmTriggered = true;
        digitalWrite(PIN_XIAO_C3_SIREN, HIGH);
    }

    // Physical Key / Emergency Override Switch on D3
    if (digitalRead(PIN_XIAO_C3_KEY_OVERRIDE) == LOW) {
        secState.isArmed = false;
        secState.throttleCutoff = false;
        secState.alarmTriggered = false;
        setImmobilizerState(false);
    }

    // Electronic Brake Cutoff (Immediate Motor Hall Inhibit)
    if (digitalRead(PIN_XIAO_C3_EBRAKE) == LOW) {
        digitalWrite(PIN_XIAO_C3_THROTTLE_CUT, HIGH);
    } else if (!secState.isArmed && !secState.useWirelessImmobilizer) {
        digitalWrite(PIN_XIAO_C3_THROTTLE_CUT, LOW);
    }

    // PHONE BATTERY DEAD / DISCONNECT SAFETY SAFEGUARD:
    // If phone disconnects while bike is in motion (currentSpeedKmh > 0), DO NOT lock immediately!
    // Enter DECELERATION_RAMP or maintain power until SAFE_STATIONARY (speed == 0)
    if (!isPhoneConnected && secState.phoneDisconnectTime > 0) {
        if (currentSpeedKmh > 0) {
            secState.currentState = DECELERATION_RAMP;
            // Maintain safe drive, withhold immobilization until rider stops
        } else {
            // Bike has safely come to a halt
            secState.currentState = SAFE_STATIONARY;
        }
    }

    // Broadcast High-Speed Telemetry Frame via BLE GATT
    if (isPhoneConnected) {
        char packet[96];
        snprintf(packet, sizeof(packet), "SPD:%d,CUR:%.1f,VOLT:%.1f,ARM:%d,ALM:%d,WLS:%d",
                 currentSpeedKmh, fdLive.currentA, fdLive.voltageV,
                 secState.isArmed ? 1 : 0, secState.alarmTriggered ? 1 : 0,
                 secState.useWirelessImmobilizer ? 1 : 0);
        pTelemetryChar->setValue(packet);
        pTelemetryChar->notify();
    }

    // Profound Deep Sleep Validation: When stationary for 3000ms while armed
    static uint32_t stationaryTimer = 0;
    if (secState.isArmed && canTelemetryFresh && fdLive.rpm == 0 && zeroCurrent) {
        stationaryTimer += 100;
        if (stationaryTimer >= 3000) {
            Serial.println("[POWER] 3000ms Stationary Validation Passed -> Entering esp_deep_sleep_start() (~15uA)");
            esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_XIAO_C3_KEY_OVERRIDE, 0);
            esp_sleep_enable_ext1_wakeup((1ULL << PIN_XIAO_C3_HALL_PULSE), ESP_EXT1_WAKEUP_ANY_HIGH);
            Serial.flush();
            esp_deep_sleep_start();
        }
    } else {
        stationaryTimer = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}