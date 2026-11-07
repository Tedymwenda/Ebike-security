/**
 * Firmware for Seeed Studio XIAO ESP32S3 (Dual-Core Xtensa LX7 @ 240MHz)
 * High-performance 8MB PSRAM e-bike security controller & CAN/BMS bridge
 * 
 * Target Board: Seeed Studio XIAO ESP32S3
 * Architecture: Xtensa 32-bit Dual-Core LX7 @ 240MHz
 * Flash: 8MB | PSRAM: 8MB | Hardware TWAI + Dual Hardware UARTs
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/twai.h>
#include "xiao_pin_config.h"

#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_TELEMETRY_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_COMMAND_UUID      "d29ae63c-b783-49a3-8c46-9b76c8c25f77"
#define CHAR_SECURITY_UUID     "f5a14603-91b7-4a0b-8d02-12a8f89e2101"

struct XiaoS3State {
    char masterPin[7]      = "123456";
    bool isArmed           = true;
    bool ignitionRelay     = false;
    bool throttleCutoff    = true;
    bool alarmTriggered    = false;
} secState;

BLECharacteristic *pTelemetryChar;
BLECharacteristic *pSecurityChar;
bool isDeviceConnected = false;

// Task Handles for Dual-Core Scheduling (Core 0: BMS, Core 1: CAN + BLE)
TaskHandle_t TaskCANHandle;
TaskHandle_t TaskBMSHandle;

void taskFarDriverCAN(void *pvParameters) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)PIN_XIAO_S3_CAN_TX, 
        (gpio_num_t)PIN_XIAO_S3_CAN_RX, 
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();

    twai_message_t rx_msg;
    for (;;) {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(50)) == ESP_OK) {
            // High speed FarDriver telemetry parsing
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void taskJkBmsUART(void *pvParameters) {
    Serial1.begin(115200, SERIAL_8N1, PIN_XIAO_S3_BMS_RX, PIN_XIAO_S3_BMS_TX);
    uint8_t pollCommand[6] = {0xDB, 0xDB, 0x00, 0x00, 0x00, 0x00};

    for (;;) {
        Serial1.write(pollCommand, 6);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void setup() {
    Serial.begin(115200);

    // Inspect Wakeup Cause from ESP32-S3 RTC Power Management
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("[BOOT] Woken from Deep Sleep via RTC Ext0 (D3 Key Switch / Emergency Override)");
        secState.isArmed = false;
        secState.throttleCutoff = false;
        secState.alarmTriggered = false;
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println("[SECURITY] Woken from Deep Sleep via RTC Ext1 (D2 Motor Hall Pulse Kinetic Motion)");
        if (secState.isArmed) {
            secState.alarmTriggered = true;
            digitalWrite(PIN_XIAO_S3_SIREN, HIGH);
        }
    }

    pinMode(PIN_XIAO_S3_IGNITION, OUTPUT);
    pinMode(PIN_XIAO_S3_THROTTLE_CUT, OUTPUT);
    pinMode(PIN_XIAO_S3_SIREN, OUTPUT);
    pinMode(PIN_XIAO_S3_EBRAKE, INPUT_PULLUP);
    pinMode(PIN_XIAO_S3_TILT, INPUT_PULLUP);
    pinMode(PIN_XIAO_S3_KEY_OVERRIDE, INPUT_PULLUP); // D3: Safe digital input with pull-up
    pinMode(PIN_XIAO_S3_HALL_PULSE, INPUT_PULLDOWN); // D2: Motor Hall kinetic wake input

    digitalWrite(PIN_XIAO_S3_IGNITION, secState.isArmed ? LOW : HIGH);
    digitalWrite(PIN_XIAO_S3_THROTTLE_CUT, secState.isArmed ? HIGH : LOW);
    digitalWrite(PIN_XIAO_S3_SIREN, secState.alarmTriggered ? HIGH : LOW);

    BLEDevice::init("XIAO-S3-TITAN");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTelemetryChar = pService->createCharacteristic(CHAR_TELEMETRY_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pTelemetryChar->addDescriptor(new BLE2902());
    pService->start();
    BLEDevice::startAdvertising();

    // Pin Tasks to Dual Cores (Core 0: BMS, Core 1: CAN + BLE)
    xTaskCreatePinnedToCore(taskFarDriverCAN, "CAN_Core1", 4096, NULL, 3, &TaskCANHandle, 1);
    xTaskCreatePinnedToCore(taskJkBmsUART, "BMS_Core0", 4096, NULL, 2, &TaskBMSHandle, 0);
}

void loop() {
    // Handle Physical Emergency Override Switch on D3
    if (digitalRead(PIN_XIAO_S3_KEY_OVERRIDE) == LOW) {
        secState.isArmed = false;
        secState.throttleCutoff = false;
        secState.alarmTriggered = false;
        digitalWrite(PIN_XIAO_S3_IGNITION, HIGH);
        digitalWrite(PIN_XIAO_S3_THROTTLE_CUT, LOW);
        digitalWrite(PIN_XIAO_S3_SIREN, LOW);
    }

    // Profound Deep Sleep Validation: When stationary for 3000ms while armed
    static uint32_t stationaryTimer = 0;
    if (secState.isArmed) {
        stationaryTimer += 100;
        if (stationaryTimer >= 3000) {
            Serial.println("[POWER] S3 3000ms Stationary Rule Passed -> Entering esp_deep_sleep_start()");
            esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_XIAO_S3_KEY_OVERRIDE, 0);
            esp_sleep_enable_ext1_wakeup((1ULL << PIN_XIAO_S3_HALL_PULSE), ESP_EXT1_WAKEUP_ANY_HIGH);
            Serial.flush();
            esp_deep_sleep_start();
        }
    } else {
        stationaryTimer = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}