# Seeed Studio XIAO_ESP32C3 Hardware Wiring & Pinout Manual

Generated Profile: XIAO_ESP32C3
Drivetrain: HUB_DIRECT
Immobilizer Mode: WIRELESS SOFTWARE OVERRIDE (FarDriver BLE FFE0/FFE1)
FarDriver Max Phase: 330A | Flux Weakening: 25A
JK-BMS 100% SoC Cell Voltage: 4.200V | 0% SoC: 3.000V
FarDriver BLE Pairing PIN: 123456 | Calibration Password: 888888

## Dual-Mode Wireless BLE Override Architecture
- FarDriver Service UUID: 0000ffe0-0000-1000-8000-00805f9b34fb
- FarDriver Characteristic UUID: 0000ffe1-0000-1000-8000-00805f9b34fb
- FarDriver LOCK Hex Frame:   AA 02 A0 01 00 00 BE EF
- FarDriver UNLOCK Hex Frame: AA 02 A0 00 00 00 DE AD
- Mobile App JSON Config Packet: {"config_mode": "wireless"} or {"config_mode": "wired"}
- Motion Safety Safeguard: Under NO circumstances lock wheels or cut power while moving (speed > 0 km/h). When phone battery dies or disconnects, a 60s safe-stop grace period allows safe braking to stationary before immobilization.

## Pinout Table

- **D0 (GPIO 2)**: Throttle Hall 5V Line Cutoff Opto-Relay
  * Peripheral: Optocoupler Relay IN1 (Normally Closed inline with Throttle 5V Hall)
  * Wire Color: #ef4444 (Red/Black Stripe)
  * Logic Level: 3.3V CMOS Output (Active HIGH cuts throttle)
  * Implementation: Safe default: GPIO Low keeps throttle connected. High disconnects 5V Hall VCC.

- **D1 (GPIO 3)**: 12V Main Contactor Ignition Relay Trigger
  * Peripheral: Relay Module IN2 (Switches 12V to FarDriver Ignition Wire)
  * Wire Color: #a855f7 (Purple)
  * Logic Level: 3.3V CMOS Output (Active HIGH arms ignition)
  * Implementation: Requires transistor/MOSFET driver module if relay coil is 12V or 5V.

- **D0 (GPIO 2)**: Throttle Hall 5V Line Cutoff Opto-Relay
  * Peripheral: Optocoupler Relay IN1 (Normally Closed inline with Throttle 5V Hall)
  * Wire Color: #ef4444 (Red/Black Stripe)
  * Logic Level: 3.3V CMOS Output (Active HIGH cuts throttle)
  * Implementation: Safe default: GPIO Low keeps throttle connected. High disconnects 5V Hall VCC.

- **D1 (GPIO 3)**: 12V Main Contactor Ignition Relay Trigger
  * Peripheral: Relay Module IN2 (Switches 12V to FarDriver Ignition Wire)
  * Wire Color: #a855f7 (Purple)
  * Logic Level: 3.3V CMOS Output (Active HIGH arms ignition)
  * Implementation: Requires transistor/MOSFET driver module if relay coil is 12V or 5V.

- **D2 (GPIO 4)**: Motor Hall Pulse Output (RTC Ext1 Kinetic Wake-up)
  * Peripheral: Motor Hall Sensor Signal Wire (Phase A or B Pulse)
  * Wire Color: #06b6d4 (Cyan/White)
  * Logic Level: 3.3V Digital Input (RTC IO capable)
  * Implementation: Configured as esp_sleep_enable_ext1_wakeup() source. Kinetic wheel movement while armed wakes MCU and immediately triggers the 110dB siren.

- **D3 (GPIO 5)**: Emergency Override Switch / Physical Key Switch (RTC Ext0 Wake-up)
  * Peripheral: Handlebar Hidden Key Switch / Emergency Override Pushbutton
  * Wire Color: #0ea5e9 (Blue/Yellow)
  * Logic Level: 3.3V Digital Input (Internal Pull-Up, Active LOW)
  * Implementation: RTC IO capable unconstrained pin. Configured as esp_sleep_enable_ext0_wakeup(PIN, 0). Replaces hazardous D9 to prevent ESP32-C3 boot strapping conflicts.

- **D4 (GPIO 6)**: TWAI CAN Bus Receive (CAN RX)
  * Peripheral: SN65HVD230 / VP230 Transceiver Pin CRX (R)
  * Wire Color: #10b981 (Green)
  * Logic Level: 3.3V Digital Input (TWAI Controller)
  * Implementation: TWAI CAN RX at 250kbps from FarDriver FOC / VESC bus.

- **D5 (GPIO 7)**: TWAI CAN Bus Transmit (CAN TX)
  * Peripheral: SN65HVD230 / VP230 Transceiver Pin CTX (D)
  * Wire Color: #34d399 (Emerald)
  * Logic Level: 3.3V Digital Output (TWAI Controller)
  * Implementation: TWAI CAN TX at 250kbps to FarDriver FOC controller.

- **D6 (GPIO 21)**: 110dB Siren Piezo MOSFET Driver
  * Peripheral: IRLZ44N / 2N7000 N-MOSFET Gate with 10kΩ pull-down
  * Wire Color: #ec4899 (Pink)
  * Logic Level: 3.3V CMOS Output (Active HIGH triggers siren)
  * Implementation: Switches low-side of 12V 110dB piezo horn. Includes 1N4007 flyback diode.

- **D7 (GPIO 20)**: SW-420 Tilt & Vibration Tamper Sensor
  * Peripheral: SW-420 High-Sensitivity Shock Sensor Digital Output
  * Wire Color: #f43f5e (Rose)
  * Logic Level: 3.3V Digital Input Pull-Up (Active LOW on shock)
  * Implementation: Triggers instant anti-tamper alarm if bike is disturbed while immobilizer is armed.

- **D8 (GPIO 8)**: Handlebar E-Brake Cutoff Switch
  * Peripheral: Brake Lever Microswitch (Normally Open to GND)
  * Wire Color: #eab308 (Yellow)
  * Logic Level: 3.3V Digital Input Pull-Up (Active LOW when pulled)
  * Implementation: Triggers instantaneous throttle cutoff and optional regenerative braking ramp.

- **D9 (GPIO 9)**: DROPPED / UNUSED (Eliminates ESP32-C3 Boot Strapping Pin Conflict)
  * Peripheral: NO PERIPHERAL CONNECTED (Do not tie low during power-on)
  * Wire Color: #64748b (Slate / Unpopulated)
  * Logic Level: DO NOT CONNECT MECHANICAL SWITCH TO GND
  * Implementation: CRITICAL: GPIO 9 is a boot strapping pin on ESP32-C3. Pulling it LOW during power-on forces ROM bootloader mode. Mechanical switch execution on D9 is dropped completely.

- **D10 (GPIO 10)**: Status LED / Heartbeat Pulse
  * Peripheral: Onboard or External 3.3V Blue Status LED (330Ω resistor)
  * Wire Color: #38bdf8 (Sky Blue)
  * Logic Level: 3.3V CMOS Output
  * Implementation: Blinks at 1Hz during normal standby; turns off during profound deep sleep.

- **5V (GPIO 0)**: Primary 5V DC Power Input
  * Peripheral: 12V-to-5V 3A Micro-Buck Regulator VOUT+
  * Wire Color: #f97316 (Orange 18 AWG)
  * Logic Level: 5.0V Regulated DC (4.75V - 5.25V)
  * Implementation: Power input to internal LDO. Add 100uF low-ESR capacitor between 5V and GND.

- **3V3 (GPIO 0)**: Regulated 3.3V Power Output
  * Peripheral: SN65HVD230 VCC and Tilt Sensor VCC
  * Wire Color: #fb923c (Light Orange)
  * Logic Level: 3.3V DC (500mA max from onboard regulator)
  * Implementation: Powers the CAN transceiver and low-power sensor logic.

- **GND (GPIO 0)**: Common Signal & Power Return Ground
  * Peripheral: Stepdown GND, CAN Transceiver GND, JK-BMS GND, Relay GND
  * Wire Color: #0f172a (Black 18 AWG)
  * Logic Level: 0V Reference
  * Implementation: Crucial: Use a single-point star ground to prevent motor phase return loops.

## Step-by-Step Installation Instructions

### Step 1: Power Supply Stepdown & Filtering
Connect your e-bike battery (72V nominal, 60V-84V) to the 72V-to-12V DC-DC converter with a 15A inline ceramic fuse.
From the 12V auxiliary line, connect a high-efficiency 12V-to-5V 3A buck regulator.
Connect the 5V output to the XIAO 5V pin, and regulator ground to the XIAO GND pin.
Solder a 100µF 16V electrolytic capacitor across XIAO 5V and GND to suppress inductive transients caused by horn/relay switching.

### Step 2: Motor Hall Pulse (D2) & Emergency Override / Key Switch (D3) RTC Wake-Up
Connect Motor Hall Pulse Output (Phase A or B sensor) to XIAO D2 (GPIO4 on C3 / GPIO3 on S3).
D2 is configured as an RTC external wake-up source (esp_sleep_enable_ext1_wakeup). Any kinetic wheel movement while parked immediately wakes the MCU; if the immobilizer is armed, it instantly trips the 110dB siren.
Connect the Handlebar Physical Key Switch / Emergency Override button to XIAO D3 (GPIO5 on C3 / GPIO4 on S3) referenced to GND with internal pull-up.
D3 is configured as ext0 RTC wake-up (esp_sleep_enable_ext0_wakeup, active LOW). Turning the key or pressing the override wakes the MCU from deep sleep.
CRITICAL: D9 (GPIO9) mechanical switch execution has been dropped completely to prevent ESP32-C3 bootloader strapping conflicts!

### Step 3: Profound Deep Sleep (esp_deep_sleep_start) & 3000ms Validation Rule
When stationary validation rules hit 3000ms (speed = 0 km/h, motor phase current = 0A, throttle = 0%, and security armed), the ESP32 firmware invokes esp_deep_sleep_start().
In this state, power consumption collapses to ~15µA quiescent current.
The system remains in deep sleep until awakened by:
1) Kinetic wheel roll on D2 (Hall sensor ext1 interrupt -> triggers siren if armed)
2) Physical key turn or emergency override button on D3 (ext0 active LOW interrupt)

### Step 4: SN65HVD230 CAN Bus Transceiver (D4 / D5)
Connect the SN65HVD230 transceiver module 3.3V pin to XIAO 3V3, and GND to XIAO GND.
Connect CRX (Receiver) to XIAO D4 (GPIO6 on C3 / GPIO5 on S3).
Connect CTX (Transmitter) to XIAO D5 (GPIO7 on C3 / GPIO6 on S3).
Connect the transceiver CAN_H and CAN_L terminals to FarDriver controller CAN_H and CAN_L pins using twisted-pair shielded wire (24 AWG).
Ensure the 120Ω termination jumper is closed on the SN65HVD230 board.

### Step 5: Anti-Theft Throttle Cutoff & Ignition Relays (D0 / D1)
Use an optocoupled dual-relay board powered by 5V.
Ignition Relay: Connect XIAO D1 (GPIO3 C3 / GPIO2 S3) to Relay IN2. Wire the relay Normally Open (NO) contacts in series with the FarDriver orange electric lock key wire (+12V ignition).
Throttle Cutoff Relay: Connect XIAO D0 (GPIO2 C3 / GPIO1 S3) to Relay IN1. Cut the +5V red supply wire going to the handlebar Hall throttle. Wire this wire through the Normally Closed (NC) contacts of the relay.
When armed or immobilizer locked, XIAO sets D0 HIGH, energizing the relay and physically breaking the throttle 5V power, preventing any motor movement even if the throttle is twisted.

### Step 6: 110dB Piezo Siren & SW-420 Tamper Shock Sensor (D6 / D7)
SW-420 Tilt/Vibration Sensor: Connect VCC to 3.3V, GND to GND, and DOUT to XIAO D7 (GPIO20 C3 / GPIO44 S3).
110dB Siren Driver: Connect an N-channel logic-level MOSFET (IRLZ44N) with Gate connected to XIAO D6 (GPIO21 C3 / GPIO43 S3) through a 220Ω resistor. Connect a 10kΩ pull-down resistor from Gate to GND.
Connect Siren (+) to +12V aux, Siren (-) to MOSFET Drain, and MOSFET Source to GND. Add a 1N4007 flyback diode across the siren terminals.

