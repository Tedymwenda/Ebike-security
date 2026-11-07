#pragma once
#include <Arduino.h>

// ================= Seeed Studio XIAO ESP32C3 Pin Definitions =================
// Board Silk: D0..D10
#define PIN_XIAO_C3_THROTTLE_CUT   2   // D0 (GPIO2) -> Throttle Hall Cutoff Optocoupler (Active HIGH)
#define PIN_XIAO_C3_IGNITION       3   // D1 (GPIO3) -> 12V Contactor Ignition Relay Trigger (Active HIGH)
#define PIN_XIAO_C3_HALL_PULSE     4   // D2 (GPIO4) -> Motor Hall Pulse Output (RTC Ext1 Kinetic Wake-up)
#define PIN_XIAO_C3_KEY_OVERRIDE   5   // D3 (GPIO5) -> Physical Key Switch / Emergency Override (RTC Ext0 Wake-up)
#define PIN_XIAO_C3_CAN_RX         6   // D4 (GPIO6) -> SN65HVD230 CRX (TWAI CAN RX)
#define PIN_XIAO_C3_CAN_TX         7   // D5 (GPIO7) -> SN65HVD230 CTX (TWAI CAN TX)
#define PIN_XIAO_C3_SIREN          21  // D6 (GPIO21)-> 110dB Siren Piezo MOSFET Driver
#define PIN_XIAO_C3_TILT           20  // D7 (GPIO20)-> SW-420 Tilt Vibration Tamper Sensor
#define PIN_XIAO_C3_EBRAKE         8   // D8 (GPIO8) -> E-Brake Microswitch Input (Active LOW)
// D9 (GPIO9) DROPPED COMPLETELY: Avoids Bootloader Strapping Pin Pull-Down Conflict on ESP32-C3!
#define PIN_XIAO_C3_LED            10  // D10(GPIO10)-> Status LED

// ================= Seeed Studio XIAO ESP32S3 Pin Definitions =================
// Board Silk: D0..D10
#define PIN_XIAO_S3_THROTTLE_CUT   1   // D0 (GPIO1) -> Throttle Cutoff Optocoupler
#define PIN_XIAO_S3_IGNITION       2   // D1 (GPIO2) -> 12V Contactor Ignition Relay
#define PIN_XIAO_S3_HALL_PULSE     3   // D2 (GPIO3) -> Motor Hall Pulse (RTC Ext1 Wake-up)
#define PIN_XIAO_S3_KEY_OVERRIDE   4   // D3 (GPIO4) -> Physical Key / Emergency Override (RTC Ext0 Wake-up)
#define PIN_XIAO_S3_EBRAKE         5   // D4 (GPIO5) -> Handlebar E-Brake Switch
#define PIN_XIAO_S3_TILT           6   // D5 (GPIO6) -> Vibration Tamper Sensor
#define PIN_XIAO_S3_CAN_TX         43  // D6 (GPIO43)-> CAN TX / JK-BMS TX
#define PIN_XIAO_S3_CAN_RX         44  // D7 (GPIO44)-> CAN RX / JK-BMS RX
#define PIN_XIAO_S3_SIREN          7   // D8 (GPIO7) -> 110dB Siren MOSFET Gate
// D9 DROPPED