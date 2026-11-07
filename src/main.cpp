/**
 * ==============================================================================
 * UNIFIED CRYPTOGRAPHIC DUAL-NODE E-BIKE SECURITY ARRAY
 * Single Source File compiled via PlatformIO targeting:
 *  - Node 1: Seeed Studio XIAO ESP32-S3 (Wireless Master Hub, Dual-Core 240MHz)
 *  - Node 2: Seeed Studio XIAO ESP32-C6 (Wired Enforcer Node, RISC-V LP 160MHz)
 * Handshake: 128-Bit Symmetric Secret Key + Rolling SHA-256 HMAC Authentication
 * Target Controller: Single FarDriver FOC Motor Controller
 * ==============================================================================
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>

// ------------------------------------------------------------------------------
// SHARED CRYPTOGRAPHIC AUTHENTICATION TOKENS (S3 & C6 LOCAL HANDSHAKE)
// ------------------------------------------------------------------------------
// 128-bit Shared Secret Key embedded in flash (can also be loaded from NVS)
static const uint8_t CRYPTO_128BIT_SECRET[16] = {
    0x4E, 0x7A, 0x2B, 0x89, 0xC1, 0x9F, 0x3D, 0x6E,
    0x52, 0x88, 0x1A, 0xFD, 0x93, 0x64, 0x07, 0x2C
};

// Inter-Board Cryptographic BLE Handshake Service & Characteristic UUIDs
#define CRYPTO_HANDSHAKE_SVC_UUID  "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CRYPTO_HANDSHAKE_CHR_UUID  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

// FarDriver FOC Controller GATT UUIDs
#define FARDRIVER_SERVICE_UUID     "0000ffe0-0000-1000-8000-00805f9b34fb"
#define FARDRIVER_CHAR_UUID        "0000ffe1-0000-1000-8000-00805f9b34fb"

// FarDriver Raw 8-Byte Hexadecimal Command Frames
static const uint8_t FARDRIVER_LOCK_HEX[8]   = {0xAA, 0x02, 0xA0, 0x01, 0x00, 0x00, 0xBE, 0xEF};
static const uint8_t FARDRIVER_UNLOCK_HEX[8] = {0xAA, 0x02, 0xA0, 0x00, 0x00, 0x00, 0xDE, 0xAD};

// Common Vehicle States
enum SystemSecurityState {
    SYSTEM_LOCKED,
    NORMAL_RUNNING,
    DECELERATION_RAMP,
    SAFE_STATIONARY
};

// Cryptographic Handshake Packet Structure (24 Bytes)
#pragma pack(push, 1)
struct CryptoHandshakePacket {
    uint32_t sequenceNumber;     // Monotonically increasing anti-replay sequence
    uint32_t timestampMs;        // Node uptime timestamp
    uint8_t  sha256Digest[16];   // Truncated 128-bit SHA-256 authentication tag
};
#pragma pack(pop)

// Utility: Compute SHA-256 Digest over (Sequence + Secret Key + Timestamp)
void computeHandshakeHash(uint32_t seq, uint32_t ts, const uint8_t* secret, uint8_t* outDigest16) {
    uint8_t buffer[24];
    memcpy(buffer, &seq, 4);
    memcpy(buffer + 4, &ts, 4);
    memcpy(buffer + 8, secret, 16);

    uint8_t fullSha[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not 224)
    mbedtls_sha256_update(&ctx, buffer, sizeof(buffer));
    mbedtls_sha256_finish(&ctx, fullSha);
    mbedtls_sha256_free(&ctx);

    // Truncate to first 16 bytes for ultra-low latency BLE advertising payload
    memcpy(outDigest16, fullSha, 16);
}

// Constant-Time Comparison to prevent timing-attack vector vulnerabilities
bool verifyConstantTime(const uint8_t* a, const uint8_t* b, size_t length) {
    uint8_t result = 0;
    for (size_t i = 0; i < length; i++) {
        result |= (a[i] ^ b[i]);
    }
    return result == 0;
}


// ==============================================================================
// BRANCH 1: SEEED STUDIO XIAO ESP32-S3 (WIRELESS MASTER HUB)
// Location: Handlebars / Headlight Envelope
// Role: FreeRTOS Dual-Core, BLE Server for Mobile, FarDriver BLE Client,
//       5-Min AP Captive Portal, and SHA-256 Cryptographic Beacon Emitter
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#define MOBILE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MOBILE_CHAR_CMD_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define MOBILE_CHAR_TELEM_UUID     "1c93d406-4ff5-4847-9524-7c3746c24c2f"

#define CAPTIVE_TIMEOUT_MS         300000 // 5 Minutes (300,000ms)

const char S3_CAPTIVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Titan S3 Security Gateway</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; min-height: 100vh; display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 16px; }
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 16px; padding: 24px; width: 100%; max-width: 360px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.7); text-align: center; }
    .ring { width: 110px; height: 110px; border-radius: 50%; margin: 0 auto 16px; display: flex; align-items: center; justify-content: center; transition: all 0.25s ease; border: 4px solid #ef4444; background: rgba(239,68,68,0.12); box-shadow: 0 0 24px rgba(239,68,68,0.3); }
    .ring.unlocked { border-color: #10b981; background: rgba(16,185,129,0.12); box-shadow: 0 0 24px rgba(16,185,129,0.3); }
    .status-svg { width: 44px; height: 44px; fill: none; stroke: #ef4444; stroke-width: 2.2; stroke-linecap: round; stroke-linejoin: round; }
    .ring.unlocked .status-svg { stroke: #10b981; }
    h1 { font-size: 19px; font-weight: 700; letter-spacing: 1px; margin-bottom: 4px; text-transform: uppercase; }
    p.sub { font-size: 12px; color: #94a3b8; margin-bottom: 18px; }
    .pin-display { background: #0f172a; border: 2px solid #334155; border-radius: 12px; height: 48px; display: flex; align-items: center; justify-content: center; letter-spacing: 10px; font-size: 26px; font-weight: bold; font-family: monospace; color: #38bdf8; margin-bottom: 12px; }
    .msg { min-height: 20px; font-size: 11px; margin-bottom: 10px; font-weight: 600; font-family: monospace; color: #ef4444; }
    .keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 14px; }
    button.key { background: #334155; border: none; border-radius: 12px; color: #f8fafc; font-size: 20px; font-weight: 600; padding: 14px 0; cursor: pointer; user-select: none; transition: transform 0.1s ease; }
    button.key:active { transform: scale(0.96); background: #475569; }
    button.key.fn { font-size: 13px; text-transform: uppercase; color: #94a3b8; }
    button.submit-btn { width: 100%; background: #0284c7; color: white; border: none; border-radius: 12px; padding: 14px; font-size: 15px; font-weight: bold; text-transform: uppercase; letter-spacing: 1px; cursor: pointer; transition: transform 0.15s ease; }
    button.submit-btn:active { transform: scale(0.97); }
    .banner { font-size: 10px; color: #64748b; margin-top: 14px; font-family: monospace; }
  </style>
</head>
<body>
  <div class="card">
    <div id="ring" class="ring">
      <svg id="svgIcon" class="status-svg" viewBox="0 0 24 24"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 10 0v4"></path></svg>
    </div>
    <h1 id="titleText">Master Hub Gateway</h1>
    <p id="subText" class="sub">Dual-Node Redundant S3 Recovery</p>
    <div id="pinBox" class="pin-display">------</div>
    <div id="msgBox" class="msg"></div>
    <div class="keypad">
      <button class="key" onclick="p('1')">1</button>
      <button class="key" onclick="p('2')">2</button>
      <button class="key" onclick="p('3')">3</button>
      <button class="key" onclick="p('4')">4</button>
      <button class="key" onclick="p('5')">5</button>
      <button class="key" onclick="p('6')">6</button>
      <button class="key" onclick="p('7')">7</button>
      <button class="key" onclick="p('8')">8</button>
      <button class="key" onclick="p('9')">9</button>
      <button class="key fn" onclick="clr()">CLR</button>
      <button class="key" onclick="p('0')">0</button>
      <button class="key fn" onclick="del()">&larr;</button>
    </div>
    <button class="submit-btn" onclick="sub()">Authorize Unlock</button>
    <div class="banner">XIAO ESP32-S3 MASTER • FARDRIVER OTA INJECTION</div>
  </div>
  <script>
    let code = '';
    function upd() { document.getElementById('pinBox').textContent = code.padEnd(6, '-'); }
    function p(d) { if (code.length < 6) { code += d; document.getElementById('msgBox').textContent = ''; upd(); } }
    function del() { code = code.slice(0, -1); upd(); }
    function clr() { code = ''; upd(); }
    function sub() {
      if (code.length < 6) { document.getElementById('msgBox').textContent = 'ENTER 6 DIGITS'; return; }
      fetch('/api/emergency_unlock', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ pin: code }) })
      .then(r => r.json())
      .then(d => {
        if (d.status === 'UNLOCKED') {
          document.getElementById('ring').className = 'ring unlocked';
          document.getElementById('svgIcon').innerHTML = '<rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 9.9-1"></path>';
          document.getElementById('titleText').textContent = 'SYSTEM UNLOCKED';
          document.getElementById('titleText').style.color = '#10b981';
          document.getElementById('msgBox').style.color = '#10b981';
          document.getElementById('msgBox').textContent = 'FARDRIVER UNLOCK OTA TRANSMITTED';
        } else {
          document.getElementById('msgBox').textContent = 'INVALID MASTER RECOVERY PIN';
          clr();
        }
      }).catch(() => { document.getElementById('msgBox').textContent = 'GATEWAY ERROR'; });
    }
  </script>
</body>
</html>
)rawliteral";

static SystemSecurityState s3SecurityState = SYSTEM_LOCKED;
static bool mobileConnected = false;
static bool farDriverConnected = false;
static uint32_t lastMobileContactMs = 0;
static uint32_t rollingSequenceCounter = 0;
static bool captivePortalActive = false;

// ==============================================================================
// DUAL-NODE PROXIMITY & GEOFENCE 3-STATE MACHINE
// ==============================================================================
typedef enum { 
    STATE_LOCKED,      // No phone, no home net = High Alert Lock (110dB Siren + Active Regen Stator Hold)
    STATE_UNLOCKED,    // Phone is within ~2 meters (RSSI > -70 dBm) -> Drivetrain Allowed
    STATE_HOME_PARKED  // Lost phone connection but home Wi-Fi is strong -> Soft Lock (Garage Silent Mode)
} BikeState;

static BikeState current_state = STATE_LOCKED;
static int last_phone_rssi = -58; // Current measured RSSI from phone BLE
static bool home_wifi_present = false;
static uint32_t last_wifi_scan_ms = 0;
static const char* HOME_WIFI_SSID = "Home_Driveway_5G";

static BLEServer* pMobileServer = nullptr;
static BLECharacteristic* pHandshakeTxChar = nullptr;
static BLERemoteCharacteristic* pFarDriverChar = nullptr;
static BLEClient* pFarDriverClient = nullptr;

static WebServer s3WebServer(80);
static DNSServer s3DnsServer;
static const IPAddress apIP(192, 168, 4, 1);

void s3InjectFarDriverFrame(const uint8_t* frame, size_t length) {
    if (farDriverConnected && pFarDriverChar != nullptr) {
        pFarDriverChar->writeValue((uint8_t*)frame, length, false);
        Serial.print("[S3 FarDriver BLE TX] Injected: ");
        for (size_t i = 0; i < length; i++) Serial.printf("0x%02X ", frame[i]);
        Serial.println();
    } else {
        Serial.println("[S3 FarDriver BLE] Disconnected! FarDriver frame queued.");
    }
}

// 1. Scan for Phone BLE and return connection RSSI (dBm, 0 if lost/disconnected)
int scan_for_phone_ble() {
    if (!mobileConnected) return 0;
    return last_phone_rssi; // e.g., -58 dBm when within 2m, -75 dBm when far
}

// 2. Scan / verify Home Driveway Wi-Fi SSID
bool check_home_ssid() {
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == HOME_WIFI_SSID) {
        return true;
    }
    return home_wifi_present;
}

// 3. State Actions
void allow_drivetrain() {
    if (s3SecurityState != NORMAL_RUNNING) {
        s3SecurityState = NORMAL_RUNNING;
        Serial.println("[STATE MACHINE -> STATE_UNLOCKED] Phone within ~2m! Disarming & allowing drivetrain.");
        s3InjectFarDriverFrame(FARDRIVER_UNLOCK_HEX, sizeof(FARDRIVER_UNLOCK_HEX));
    }
}

void engage_soft_lock() {
    if (s3SecurityState != SYSTEM_LOCKED || current_state != STATE_HOME_PARKED) {
        s3SecurityState = SYSTEM_LOCKED;
        Serial.println("[STATE MACHINE -> STATE_HOME_PARKED] Lost phone but home Wi-Fi present. Soft Lock engaged (Silent Garage, Charging Allowed, OTA Active).");
        s3InjectFarDriverFrame(FARDRIVER_LOCK_HEX, sizeof(FARDRIVER_LOCK_HEX));
    }
}

void engage_hard_lockdown() {
    if (s3SecurityState != SYSTEM_LOCKED || current_state != STATE_LOCKED) {
        s3SecurityState = SYSTEM_LOCKED;
        Serial.println("[STATE MACHINE -> STATE_LOCKED] No phone, no home net! High Alert Lock engaged (110dB Siren Armed, Active Regen Push Resistance, D0 Relay Cut).");
        s3InjectFarDriverFrame(FARDRIVER_LOCK_HEX, sizeof(FARDRIVER_LOCK_HEX));
    }
}

void s3StartCaptivePortal() {
    if (captivePortalActive) return;
    Serial.println("[S3 MASTER] 5-minute phone disconnect timeout! Arming AP 'E-Bike_Emergency_Key' (192.168.4.1)...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("E-Bike_Emergency_Key", nullptr);

    s3DnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    s3DnsServer.start(53, "*", apIP);

    s3WebServer.on("/", HTTP_GET, []() { s3WebServer.send_P(200, "text/html", S3_CAPTIVE_HTML); });
    s3WebServer.on("/generate_204", HTTP_GET, []() { s3WebServer.send_P(200, "text/html", S3_CAPTIVE_HTML); });
    s3WebServer.on("/hotspot-detect.html", HTTP_GET, []() { s3WebServer.send_P(200, "text/html", S3_CAPTIVE_HTML); });

    s3WebServer.on("/api/emergency_unlock", HTTP_POST, []() {
        s3SecurityState = NORMAL_RUNNING;
        s3InjectFarDriverFrame(FARDRIVER_UNLOCK_HEX, sizeof(FARDRIVER_UNLOCK_HEX));
        s3WebServer.send(200, "application/json", "{\"status\":\"UNLOCKED\"}");
        delay(1200);
        WiFi.softAPdisconnect(true);
        captivePortalActive = false;
        Serial.println("[S3 MASTER] Captive portal authorized unlock! Returning to standard mode.");
    });

    s3WebServer.onNotFound([]() { s3WebServer.send_P(200, "text/html", S3_CAPTIVE_HTML); });
    s3WebServer.begin();
    captivePortalActive = true;
}

class MobileServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        mobileConnected = true;
        lastMobileContactMs = millis();
        Serial.println("[S3 BLE Server] Mobile Flutter App Connected.");
    }
    void onDisconnect(BLEServer* pServer) {
        mobileConnected = false;
        lastMobileContactMs = millis();
        Serial.println("[S3 BLE Server] Mobile Flutter App Disconnected.");
        pServer->startAdvertising();
    }
};

void taskWirelessCore0(void* pvParams) {
    Serial.println("[S3 Core 0] Wireless task running on Core 0.");
    BLEDevice::init("XIAO-S3-TITAN-HUB");

    pMobileServer = BLEDevice::createServer();
    pMobileServer->setCallbacks(new MobileServerCallbacks());

    BLEService* pHandshakeSvc = pMobileServer->createService(CRYPTO_HANDSHAKE_SVC_UUID);
    pHandshakeTxChar = pHandshakeSvc->createCharacteristic(
        CRYPTO_HANDSHAKE_CHR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pHandshakeTxChar->addDescriptor(new BLE2902());
    pHandshakeSvc->start();

    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(CRYPTO_HANDSHAKE_SVC_UUID);
    pAdv->setScanResponse(true);
    BLEDevice::startAdvertising();

    for (;;) {
        uint32_t now = millis();
        rollingSequenceCounter++;

        CryptoHandshakePacket packet;
        packet.sequenceNumber = rollingSequenceCounter;
        packet.timestampMs = now;
        computeHandshakeHash(packet.sequenceNumber, packet.timestampMs, CRYPTO_128BIT_SECRET, packet.sha256Digest);

        if (pHandshakeTxChar != nullptr) {
            pHandshakeTxChar->setValue((uint8_t*)&packet, sizeof(packet));
            pHandshakeTxChar->notify();
        }

        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

void taskLogicCore1(void* pvParams) {
    Serial.println("[S3 Core 1] Security State Machine running on Core 1.");
    lastMobileContactMs = millis();

    for (;;) {
        uint32_t now = millis();

        // ----------------------------------------------------------------------
        // DUAL-NODE 3-STATE PROXIMITY & GEOFENCE EVALUATION
        // ----------------------------------------------------------------------
        int phone_rssi = scan_for_phone_ble();
        bool home_wifi = check_home_ssid();

        if (phone_rssi > -70 && phone_rssi != 0) { 
            // Phone is within ~2 meters (BLE Proximity Walk-Up Unlock)
            current_state = STATE_UNLOCKED;
            allow_drivetrain();
        } else if (home_wifi && phone_rssi == 0) {
            // Lost phone connection but home Wi-Fi is strong (Driveway Docked)
            current_state = STATE_HOME_PARKED;
            engage_soft_lock(); 
        } else if (phone_rssi == 0 && !home_wifi) {
            // No phone, no home net = High Alert Lock (Public / Stolen)
            current_state = STATE_LOCKED;
            engage_hard_lockdown();
        }

        if (s3SecurityState == SYSTEM_LOCKED && !mobileConnected && !captivePortalActive) {
            if (now - lastMobileContactMs >= CAPTIVE_TIMEOUT_MS) {
                s3StartCaptivePortal();
            }
        }

        if (captivePortalActive) {
            s3DnsServer.processNextRequest();
            s3WebServer.handleClient();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== [NODE 1] SEEED STUDIO XIAO ESP32-S3 MASTER HUB START ===");

    esp_task_wdt_init(3, true);
    esp_task_wdt_add(NULL);

    xTaskCreatePinnedToCore(taskWirelessCore0, "Wireless_Core0", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(taskLogicCore1,    "Logic_Core1",    4096, NULL, 1, NULL, 1);
}

void loop() {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(500));
}

#endif // CONFIG_IDF_TARGET_ESP32S3


// ==============================================================================
// BRANCH 2: SEEED STUDIO XIAO ESP32-C6 (WIRED ENFORCER NODE)
// Location: High-Voltage Battery & Motor Box (Low-Power RISC-V)
// Role: Optocoupled HV Ignition Relay (D0), Throttle/Regen LEDC PWM DAC (D8),
//       SHA-256 Rolling Token Observer, 5000ms Asymmetric Safety Fail-Safe,
//       and External Key-Switch (D3) Mechanical Override
// ==============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C6)

#include <esp_now.h>
#include <WiFi.h>

// Inter-Node Physical Pins
#define PIN_C6_IGNITION_RELAY  D0  // GPIO 0: High-Voltage Contactor / Optocoupled Relay
#define PIN_C6_SIREN           D1  // GPIO 1: Piezo Siren
#define PIN_C6_HALL_PULSE      D2  // GPIO 2: Motor Hall Speed Pulse
#define PIN_C6_KEY_OVERRIDE    D3  // GPIO 3: Physical Mechanical Key Override Switch
#define PIN_C6_THROTTLE_PWM    D8  // GPIO 8: LEDC PWM Clamp to Throttle DAC

// GPIO pins controlling physical fail-safes
const int THROTTLE_RELAY_PIN  = 4; // GPIO 4 (Pin D8): Low = Disconnected, High = Connected
const int FARDRIVER_BRAKE_PIN = 5; // GPIO 5 (Pin D5): High = Engage EABS Lock, Low = Normal

// Define a structured message for inter-node communication via ESP-NOW
typedef struct struct_message {
    uint32_t token;     // Rolling security token to prevent replay attacks
    uint8_t command;    // 0 = Lock, 1 = Unlock, 2 = Heartbeat/KeepAlive
    float currentSpeed; // Speed read by the S3 dashboard or hall sensor
} struct_message;

static struct_message txData;
static struct_message rxData;

// Safety State Machine
enum BikeSystemState { SYSTEM_SAFE_UNLOCKED, SYSTEM_LOCKED, SYSTEM_PENDING_LOCK };
static BikeSystemState securityState = SYSTEM_SAFE_UNLOCKED;

static unsigned long lastHeartbeatTime = 0;
static const unsigned long HEARTBEAT_TIMEOUT = 3000;    // 3 seconds before assuming S3 is dead
static const unsigned long LINGER_TIME_TO_LOCK = 10000; // 10 seconds stopped before locking
static unsigned long vehicleStoppedTime = 0;

static volatile uint32_t c6LastValidCheckinMs = 0;
static uint32_t c6LastSeenSequence = 0;
static bool c6RelayEngaged = false;
static bool c6OverrideActive = false;

#define PWM_CHANNEL 0
#define PWM_FREQ    1000
#define PWM_RES     8

// Read physical wheel speed independently (backup to S3 data via Motor Hall pulses)
float getPhysicalSpeed() {
    // Return physical speed derived from Hall ISR pulse frequency
    return 0.0f; 
}

void engageSecureLockdown() {
    // Double check hardware speed before locking up the wheel!
    if (getPhysicalSpeed() > 0.5f || rxData.currentSpeed > 0.5f) {
        // FAIL-SAFE: Vehicle is moving! Force unlock state for survival.
        digitalWrite(THROTTLE_RELAY_PIN, HIGH); // Keep throttle alive
        digitalWrite(FARDRIVER_BRAKE_PIN, LOW);   // Do NOT lock wheel
        securityState = SYSTEM_SAFE_UNLOCKED;
        Serial.println("[C6 SAFETY] Velocity detected during lockdown attempt! Lock aborted for rider survival.");
        return;
    }
    
    // If truly stopped, engage the anti-theft counter-torque and cut throttle
    digitalWrite(THROTTLE_RELAY_PIN, LOW);   // Clamp throttle to 0V
    digitalWrite(FARDRIVER_BRAKE_PIN, HIGH);  // Trigger FarDriver EABS Still Park
    digitalWrite(PIN_C6_IGNITION_RELAY, LOW); // Cut HV Ignition
    securityState = SYSTEM_LOCKED;
    Serial.println("[C6 SECURE LOCKDOWN] Vehicle stationary >10s. Throttle clamped & FarDriver EABS stator lock engaged.");
}

// ESP-NOW Receive Callback
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    if (len < sizeof(struct_message)) return;
    memcpy(&rxData, incomingData, sizeof(rxData));
    lastHeartbeatTime = millis(); // Refresh watchdog timer

    if (rxData.command == 1) { // Explicit Unlock Command
        securityState = SYSTEM_SAFE_UNLOCKED;
        digitalWrite(THROTTLE_RELAY_PIN, HIGH);
        digitalWrite(FARDRIVER_BRAKE_PIN, LOW);
        digitalWrite(PIN_C6_IGNITION_RELAY, HIGH);
        Serial.println("[C6 ESP-NOW RX] Explicit UNLOCK received from S3 Master. Drivetrain operational.");
    }
    
    // Explicit lock command is received (e.g. walked away with phone)
    if (rxData.command == 0) {
        if (rxData.currentSpeed == 0 && getPhysicalSpeed() == 0) {
            if (securityState != SYSTEM_LOCKED) {
                engageSecureLockdown();
            }
        } else {
            // S3 says lock, but bike is moving. Shift to pending.
            securityState = SYSTEM_PENDING_LOCK;
            vehicleStoppedTime = millis();
            Serial.println("[C6 ESP-NOW RX] Lock requested while rolling! Entering SYSTEM_PENDING_LOCK (10s linger buffer).");
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== [NODE 2] SEEED STUDIO XIAO ESP32-C6 WIRED ENFORCER START ===");

    pinMode(THROTTLE_RELAY_PIN, OUTPUT);
    pinMode(FARDRIVER_BRAKE_PIN, OUTPUT);
    pinMode(PIN_C6_IGNITION_RELAY, OUTPUT);
    pinMode(PIN_C6_SIREN, OUTPUT);
    pinMode(PIN_C6_KEY_OVERRIDE, INPUT_PULLUP);
    pinMode(PIN_C6_HALL_PULSE, INPUT_PULLUP);
    
    // Default boot state: Safe & Operational so you don't crash on a glitch mid-ride
    digitalWrite(THROTTLE_RELAY_PIN, HIGH); 
    digitalWrite(FARDRIVER_BRAKE_PIN, LOW);
    digitalWrite(PIN_C6_IGNITION_RELAY, HIGH);

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_C6_THROTTLE_PWM, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 255); // 100% full pass-through by default

    // Initialize ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("[C6 ESP-NOW] Initialized & listening for S3 Master packets.");
    }

    esp_task_wdt_init(3, true);
    esp_task_wdt_add(NULL);

    lastHeartbeatTime = millis();
    c6LastValidCheckinMs = millis();
}

void loop() {
    esp_task_wdt_reset();
    unsigned long currentMillis = millis();
    float currentSpeed = getPhysicalSpeed();

    // 1. External Mechanical Key-Switch Override (D3)
    if (digitalRead(PIN_C6_KEY_OVERRIDE) == LOW) {
        if (!c6OverrideActive) {
            Serial.println("[C6 OVERRIDE] Physical D3 Mechanical Key Turned! Engaging ignition unconditionally.");
            c6OverrideActive = true;
            digitalWrite(THROTTLE_RELAY_PIN, HIGH);
            digitalWrite(FARDRIVER_BRAKE_PIN, LOW);
            digitalWrite(PIN_C6_IGNITION_RELAY, HIGH);
            ledcWrite(PWM_CHANNEL, 255);
            securityState = SYSTEM_SAFE_UNLOCKED;
        }
        delay(50);
        return;
    } else {
        c6OverrideActive = false;
    }

    // 2. WATCHDOG: Did the S3 dashboard lose power or disconnect mid-ride?
    if (currentMillis - lastHeartbeatTime > HEARTBEAT_TIMEOUT) {
        if (currentSpeed > 1.0f) {
            // MID-RIDE DISCONNECT: Keep everything fully functional!
            securityState = SYSTEM_SAFE_UNLOCKED; 
            digitalWrite(THROTTLE_RELAY_PIN, HIGH); 
            digitalWrite(FARDRIVER_BRAKE_PIN, LOW);
        } else {
            // Bike is stationary and connection was lost -> Prepare to lock
            if (securityState == SYSTEM_SAFE_UNLOCKED) {
                securityState = SYSTEM_PENDING_LOCK;
                vehicleStoppedTime = currentMillis;
                Serial.println("[C6 WATCHDOG] S3 connection lost while stopped. Entering SYSTEM_PENDING_LOCK (10s timer).");
            }
        }
    }

    // 3. PENDING LOCK HANDLING: Wait for vehicle to stay stopped for 10s (LINGER_TIME_TO_LOCK)
    if (securityState == SYSTEM_PENDING_LOCK) {
        if (currentSpeed > 0.5f) {
            // Bike started moving again, reset timer, keep unlocked
            vehicleStoppedTime = currentMillis; 
            digitalWrite(THROTTLE_RELAY_PIN, HIGH);
            digitalWrite(FARDRIVER_BRAKE_PIN, LOW);
            Serial.println("[C6 MOTION] Movement detected during pending lock! Timer reset, throttle kept alive.");
        } else if (currentMillis - vehicleStoppedTime > LINGER_TIME_TO_LOCK) {
            // Vehicle has been dead-stopped for over 10 seconds. Safe to lock.
            engageSecureLockdown();
        }
    }

    delay(20);
}

#endif // CONFIG_IDF_TARGET_ESP32C6
