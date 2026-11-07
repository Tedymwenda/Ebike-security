/**
 * ============================================================================
 * ONBOARD WI-FI AUTO-SYNC & EMERGENCY CAPTIVE PORTAL WEB INTERFACE
 * Target: Seeed Studio XIAO ESP32-C6 / XIAO ESP32-S3 / XIAO ESP32-C3
 * Coexists with: BLE Dual-Role Gateway + Dual-Mode Physical Relay Fallback
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

#define PIN_RELAY_IGNITION    D0  // GPIO 2: Physical Relay (Ignition Cutout)
#define PIN_ALARM_SIREN       D1  // GPIO 3: Piezo / Siren
#define PIN_HALL_SPEED        D2  // GPIO 4: Motor Hall Pulse (RTC Ext1 Wakeup)
#define PIN_KEY_SWITCH        D3  // GPIO 5: Safe Override Key Switch (RTC Ext0 Wakeup)
#define PIN_THROTTLE_PWM      D8  // GPIO 8: LEDC Throttle DAC/PWM Override

#define CAPTIVE_PORTAL_TIMEOUT_MS 300000 // 5 Minutes (300 seconds)

const uint8_t FARDRIVER_LOCK_HEX[8]   = {0xAA, 0x02, 0xA0, 0x01, 0x00, 0x00, 0xBE, 0xEF};
const uint8_t FARDRIVER_UNLOCK_HEX[8] = {0xAA, 0x02, 0xA0, 0x00, 0x00, 0x00, 0xDE, 0xAD};

#define FARDRIVER_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define FARDRIVER_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

enum VehicleState {
    SYSTEM_LOCKED,
    NORMAL_RUNNING,
    DECELERATION_RAMP,
    SAFE_STATIONARY
};

struct HubConfig {
    char wifiHomeSsid[33]         = "Home_Driveway_5G";
    char wifiHomePass[65]         = "SuperSecureWiFi2026";
    char homeAssistantUrl[128]    = "http://homeassistant.local:8123/api/webhook/ebike_docked";
    char masterEmergencyPin[7]    = "123456";
    bool useWirelessImmobilizer   = true;
} config;

VehicleState currentState = SYSTEM_LOCKED;
bool isPhoneBleConnected = false;
bool isFarDriverConnected = false;
bool isHomeWifiConnected = false;
bool isCaptiveApActive = false;
uint32_t lastPhoneSeenMs = 0;
uint32_t lastWifiScanMs = 0;

Preferences preferences;
WebServer webServer(80);
DNSServer dnsServer;
const IPAddress apIP(192, 168, 4, 1);
const IPAddress apNetmask(255, 255, 255, 0);

BLERemoteCharacteristic* pFarDriverChar = nullptr;

const char EMERGENCY_CAPTIVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>E-Bike Emergency Recovery Gateway</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #0f172a; color: #f8fafc; min-height: 100vh; display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 16px; }
    .card { background: #1e293b; border: 1px solid #334155; border-radius: 16px; padding: 24px; width: 100%; max-width: 360px; box-shadow: 0 25px 50px -12px rgba(0,0,0,0.7); text-align: center; }
    .status-ring { width: 110px; height: 110px; border-radius: 50%; margin: 0 auto 16px; display: flex; align-items: center; justify-content: center; transition: all 0.3s ease; }
    .status-ring.locked { border: 4px solid #ef4444; background: rgba(239,68,68,0.12); box-shadow: 0 0 24px rgba(239,68,68,0.3); }
    .status-ring.unlocked { border: 4px solid #10b981; background: rgba(16,185,129,0.12); box-shadow: 0 0 24px rgba(16,185,129,0.3); }
    .status-svg { width: 44px; height: 44px; fill: none; stroke-width: 2.2; stroke-linecap: round; stroke-linejoin: round; }
    .locked .status-svg { stroke: #ef4444; }
    .unlocked .status-svg { stroke: #10b981; }
    h1 { font-size: 19px; font-weight: 700; letter-spacing: 1px; margin-bottom: 4px; text-transform: uppercase; }
    p.sub { font-size: 12px; color: #94a3b8; margin-bottom: 18px; }
    .pin-display { background: #0f172a; border: 2px solid #334155; border-radius: 12px; height: 48px; display: flex; align-items: center; justify-content: center; letter-spacing: 10px; font-size: 26px; font-weight: bold; font-family: monospace; color: #38bdf8; margin-bottom: 12px; }
    .pin-display.error { border-color: #ef4444; animation: shake 0.3s ease-in-out; }
    @keyframes shake { 0%, 100% { transform: translateX(0); } 25% { transform: translateX(-6px); } 75% { transform: translateX(6px); } }
    .msg-box { min-height: 20px; font-size: 11px; margin-bottom: 10px; font-weight: 600; font-family: monospace; }
    .keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 14px; }
    button.key { background: #334155; border: none; border-radius: 12px; color: #f8fafc; font-size: 20px; font-weight: 600; padding: 14px 0; cursor: pointer; user-select: none; transition: 0.1s; }
    button.key:active { background: #475569; transform: scale(0.97); }
    button.key.fn { font-size: 13px; text-transform: uppercase; color: #94a3b8; font-weight: bold; }
    button.submit-btn { width: 100%; background: #0284c7; color: white; border: none; border-radius: 12px; padding: 14px; font-size: 15px; font-weight: bold; text-transform: uppercase; letter-spacing: 1px; cursor: pointer; transition: 0.15s; }
    button.submit-btn:hover { background: #0369a1; }
    button.submit-btn:active { transform: scale(0.98); }
    .banner { font-size: 10px; color: #64748b; margin-top: 14px; font-family: monospace; }
  </style>
</head>
<body>
  <div class="card">
    <div id="ring" class="status-ring locked">
      <svg id="svgIcon" class="status-svg" viewBox="0 0 24 24">
        <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
        <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
      </svg>
    </div>
    <h1 id="titleText">Security Gateway</h1>
    <p id="subText" class="sub">Dead-Phone Emergency Captive Portal</p>
    <div id="pinBox" class="pin-display">------</div>
    <div id="msg" class="msg-box" style="color: #ef4444;"></div>
    <div class="keypad">
      <button class="key" onclick="press('1')">1</button>
      <button class="key" onclick="press('2')">2</button>
      <button class="key" onclick="press('3')">3</button>
      <button class="key" onclick="press('4')">4</button>
      <button class="key" onclick="press('5')">5</button>
      <button class="key" onclick="press('6')">6</button>
      <button class="key" onclick="press('7')">7</button>
      <button class="key" onclick="press('8')">8</button>
      <button class="key" onclick="press('9')">9</button>
      <button class="key fn" onclick="clearPin()">CLR</button>
      <button class="key" onclick="press('0')">0</button>
      <button class="key fn" onclick="backspace()">&larr;</button>
    </div>
    <button class="submit-btn" onclick="submitPin()">Authorize Vehicle Unlock</button>
    <div class="banner">GATEWAY IP: 192.168.4.1 • FARDRIVER BLE INJECTION</div>
  </div>
  <script>
    let entered = '';
    function updateDisplay() { document.getElementById('pinBox').textContent = entered.padEnd(6, '-'); }
    function press(n) { if (entered.length < 6) { entered += n; document.getElementById('msg').textContent = ''; updateDisplay(); } }
    function backspace() { entered = entered.slice(0, -1); updateDisplay(); }
    function clearPin() { entered = ''; updateDisplay(); }
    function submitPin() {
      if (entered.length < 6) { document.getElementById('msg').textContent = 'ENTER FULL 6-DIGIT RECOVERY PIN'; return; }
      fetch('/api/emergency_unlock', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ pin: entered }) })
      .then(r => r.json())
      .then(d => {
        if (d.status === 'UNLOCKED') {
          document.getElementById('ring').className = 'status-ring unlocked';
          document.getElementById('svgIcon').innerHTML = '<rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 9.9-1"></path>';
          document.getElementById('titleText').textContent = 'VEHICLE UNLOCKED';
          document.getElementById('titleText').style.color = '#10b981';
          document.getElementById('subText').textContent = 'FarDriver FOC OTA Hex Frame Injected';
          document.getElementById('msg').style.color = '#10b981';
          document.getElementById('msg').textContent = 'READY TO RIDE • SYSTEM DISARMED';
        } else {
          const pb = document.getElementById('pinBox'); pb.classList.add('error');
          setTimeout(() => pb.classList.remove('error'), 350);
          document.getElementById('msg').style.color = '#ef4444';
          document.getElementById('msg').textContent = 'INVALID MASTER RECOVERY PIN';
          clearPin();
        }
      }).catch(() => { document.getElementById('msg').textContent = 'GATEWAY CONNECTION ERROR'; });
    }
  </script>
</body>
</html>
)rawliteral";

void injectFarDriverHex(const uint8_t* frame, size_t len) {
    if (config.useWirelessImmobilizer && isFarDriverConnected && pFarDriverChar != nullptr) {
        pFarDriverChar->writeValue((uint8_t*)frame, len, false);
    } else {
        if (frame == FARDRIVER_LOCK_HEX) {
            digitalWrite(PIN_RELAY_IGNITION, LOW);
            ledcWrite(0, 0);
        } else {
            digitalWrite(PIN_RELAY_IGNITION, HIGH);
            ledcWrite(0, 255);
        }
    }
}

void triggerHomeDrivewaySync() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiHomeSsid, config.wifiHomePass);
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) { delay(300); attempts++; }
    if (WiFi.status() == WL_CONNECTED) {
        isHomeWifiConnected = true;
        HTTPClient http;
        http.begin(config.homeAssistantUrl);
        http.addHeader("Content-Type", "application/json");
        http.POST("{"vehicle":"Titan_EBike","status":"docked_driveway","battery_volts":72.8}");
        http.end();
        ArduinoOTA.setHostname("ebike-titan-xiao");
        ArduinoOTA.begin();
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        isHomeWifiConnected = false;
    }
}

void startEmergencyCaptivePortal() {
    if (isCaptiveApActive) return;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, apNetmask);
    WiFi.softAP("E-Bike_Emergency_Key", nullptr);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    webServer.on("/", HTTP_GET, []() { webServer.send_P(200, "text/html", EMERGENCY_CAPTIVE_HTML); });
    webServer.on("/generate_204", HTTP_GET, []() { webServer.send_P(200, "text/html", EMERGENCY_CAPTIVE_HTML); });
    webServer.on("/hotspot-detect.html", HTTP_GET, []() { webServer.send_P(200, "text/html", EMERGENCY_CAPTIVE_HTML); });
    webServer.on("/api/emergency_unlock", HTTP_POST, []() {
        if (!webServer.hasArg("plain")) { webServer.send(400, "application/json", "{"error":"missing body"}"); return; }
        StaticJsonDocument<128> doc;
        deserializeJson(doc, webServer.arg("plain"));
        const char* enteredPin = doc["pin"] | "";
        if (strcmp(enteredPin, config.masterEmergencyPin) == 0) {
            currentState = NORMAL_RUNNING;
            injectFarDriverHex(FARDRIVER_UNLOCK_HEX, sizeof(FARDRIVER_UNLOCK_HEX));
            webServer.send(200, "application/json", "{"status":"UNLOCKED"}");
            delay(1000);
            WiFi.softAPdisconnect(true);
            isCaptiveApActive = false;
        } else {
            webServer.send(403, "application/json", "{"status":"DENIED"}");
        }
    });
    webServer.onNotFound([]() { webServer.send_P(200, "text/html", EMERGENCY_CAPTIVE_HTML); });
    webServer.begin();
    isCaptiveApActive = true;
}

void setup() {
    Serial.begin(115200);
    preferences.begin("ebike_wifi", false);
    String ssid = preferences.getString("ssid", "Home_Driveway_5G");
    String pass = preferences.getString("pass", "SuperSecureWiFi2026");
    String pin = preferences.getString("m_pin", "123456");
    strncpy(config.wifiHomeSsid, ssid.c_str(), sizeof(config.wifiHomeSsid));
    strncpy(config.wifiHomePass, pass.c_str(), sizeof(config.wifiHomePass));
    strncpy(config.masterEmergencyPin, pin.c_str(), sizeof(config.masterEmergencyPin));
    pinMode(PIN_RELAY_IGNITION, OUTPUT);
    pinMode(PIN_ALARM_SIREN, OUTPUT);
    digitalWrite(PIN_RELAY_IGNITION, HIGH);
    digitalWrite(PIN_ALARM_SIREN, LOW);
    esp_task_wdt_init(2, true);
    esp_task_wdt_add(NULL);
    lastPhoneSeenMs = millis();
}

void loop() {
    esp_task_wdt_reset();
    uint32_t now = millis();
    if (isCaptiveApActive) { dnsServer.processNextRequest(); webServer.handleClient(); }
    if (isHomeWifiConnected) { ArduinoOTA.handle(); }
    if (currentState == SYSTEM_LOCKED && !isPhoneBleConnected && !isCaptiveApActive) {
        if (now - lastPhoneSeenMs >= CAPTIVE_PORTAL_TIMEOUT_MS) { startEmergencyCaptivePortal(); }
    }
    if ((currentState == SAFE_STATIONARY || currentState == SYSTEM_LOCKED) && !isHomeWifiConnected && !isCaptiveApActive) {
        if (now - lastWifiScanMs >= 30000) { lastWifiScanMs = now; triggerHomeDrivewaySync(); }
    }
    delay(10);
}