# Complete Wiring & Integration Guide: Seeed Studio XIAO ESP32C3 & S3

## 1. Overview & Board Profiles
The **Seeed Studio XIAO ESP32C3** (RISC-V 160MHz) and **XIAO ESP32S3** (Xtensa Dual-Core 240MHz) offer micro-dimensions (21 x 17.5 mm) with full automotive CAN bus (TWAI) and Bluetooth Low Energy 5.0 capabilities.

---

## 2. Pinout Comparison Table

| Board Silk | Pin Function | XIAO ESP32C3 (GPIO) | XIAO ESP32S3 (GPIO) | Target Peripheral | Logic Level |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **D0** | Throttle Cutoff Relay | **GPIO 2** | **GPIO 1** | Optocoupler Relay IN1 | 3.3V Active-HIGH |
| **D1** | 12V Contactor Trigger | **GPIO 3** | **GPIO 2** | Relay IN2 (Ignition) | 3.3V Active-HIGH |
| **D2** | Motor Hall Pulse (Ext1)| **GPIO 4 (RTC)** | **GPIO 3 (RTC)** | Motor Phase Hall Pulse Wire | 3.3V Input (Kinetic Wake) |
| **D3** | Emergency Override / Key | **GPIO 5 (RTC)** | **GPIO 4 (RTC)** | Handlebar Key / Hidden Switch | 3.3V Pull-Up (Ext0 Wake) |
| **D4** | TWAI CAN Receive | **GPIO 6** | **GPIO 5** | SN65HVD230 Transceiver CRX | 3.3V Input |
| **D5** | TWAI CAN Transmit | **GPIO 7** | **GPIO 6** | SN65HVD230 Transceiver CTX | 3.3V Output |
| **D6** | 110dB Siren Piezo MOSFET | **GPIO 21** | **GPIO 43** | Piezo Gate Driver | 3.3V Output |
| **D7** | Tamper Vibration Sensor | **GPIO 20** | **GPIO 44** | SW-420 Tilt Sensor DOUT | 3.3V Pull-Up |
| **D8** | Handlebar E-Brake Switch | **GPIO 8** | **GPIO 7** | Brake Microswitch (to GND) | 3.3V Pull-Up |
| **D9** | **DROPPED / UNUSED** | **GPIO 9 (BOOT)** | **GPIO 8** | **DO NOT WIRE SWITCH TO D9** | Eliminates Boot Strapping Lock |
| **D10**| Heartbeat Status LED | **GPIO 10** | **GPIO 9** | Indicator LED (330Ω resistor) | 3.3V Output |
| **5V** | DC Main Power Input | 5V Pin | 5V Pin | 12V-to-5V Stepdown VOUT | 5.0V DC (1A+) |
| **3V3**| Regulated 3.3V Output | 3V3 Pin | 3V3 Pin | SN65HVD230 VCC | 3.3V DC (500mA) |
| **GND**| Common Ground | GND Pin | GND Pin | Star Ground Bus | 0V Reference |

---

## 3. Step-by-Step Wiring Sequence

### Step 1: Auxiliary 12V and 5V Regulated Power
1. Connect the 72V traction battery to the 72V-to-12V DC-DC converter. Place a 15A inline ceramic fuse on the 12V output.
2. From the 12V line, connect an ultra-compact 12V-to-5V 3A buck regulator.
3. Wire the 5V output to the **XIAO 5V** pin and regulator ground to the **XIAO GND** pin.
4. Solder a 100µF 16V low-ESR electrolytic capacitor across the 5V and GND pins of the XIAO.

### Step 2: FarDriver FOC CAN Bus (SN65HVD230)
1. Power the SN65HVD230 module by connecting **VCC to XIAO 3V3** and **GND to XIAO GND**.
2. Connect **CRX (Receiver) to XIAO D2** and **CTX (Transmitter) to XIAO D3**.
3. Connect the transceiver **CAN_H** and **CAN_L** screw terminals to FarDriver CAN_H and CAN_L with a 24 AWG twisted-pair shielded harness.
4. Ensure the 120Ω resistor jumper is installed across the CAN bus lines.

### Step 3: JK-BMS 20S Smart Balancer TTL Interface
1. Tap the 4-pin communication socket on the JK-BMS (Pins: GND, RXD, TXD, VCC).
2. Connect **JK-BMS GND to XIAO GND** (Common ground is mandatory for signal integrity).
3. On XIAO C3: Connect JK-BMS TXD to **D4**, and JK-BMS RXD to **D5**.
4. On XIAO S3: Connect JK-BMS TXD to **D7**, and JK-BMS RXD to **D6**.
5. *Note: JK-BMS TTL communication operates at 3.3V logic levels at 115200 baud.*

### Step 4: Anti-Theft Throttle Lockout & Contactor Relays
1. Power a 2-channel 5V optocoupled relay module from the 5V regulator rail.
2. Connect **Relay IN2 to XIAO D1** (12V Main Contactor Ignition switch).
3. Connect **Relay IN1 to XIAO D0** (Throttle Cutoff).
4. Splice the +5V Hall sensor supply wire leading to your handlebar throttle. Run this wire through the **Normally Closed (NC)** contacts of Relay 1.
5. When armed, the XIAO sets D0 HIGH, tripping the relay to disconnect the Hall 5V power, physically paralyzing the throttle.

### Step 5: High-Decibel Siren & Tilt Sensor
1. Connect an IRLZ44N N-channel logic-level MOSFET:
   - Gate to XIAO D6 (C3) / D8 (S3) via 220Ω resistor, with 10kΩ pull-down to GND.
   - Drain to Siren (-).
   - Source to GND.
2. Connect Siren (+) to +12V aux rail. Install a 1N4007 flyback diode across the siren terminals.
3. Wire the SW-420 vibration tilt module to 3.3V, GND, and digital input D7 (C3) / D5 (S3).