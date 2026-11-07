import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

/// Abstract Vehicle Hardware Abstraction Interface
abstract class VehicleHardwareAdapter {
  String get architectureName;
  Future<bool> initialize();
  Future<void> sendImmobilizerLock();
  Future<void> sendImmobilizerUnlock(String authPin);
  Future<void> dispatchConfigMode(bool useWireless);
  Stream<Map<String, dynamic>> get telemetryStream;
  void dispose();
}

/// FarDriver FOC Controller HAL Implementation (Pure Wireless Over-The-Air BLE Injection)
class FarDriverFocControllerHAL implements VehicleHardwareAdapter {
  static const String serviceUuid = "0000ffe0-0000-1000-8000-00805f9b34fb";
  static const String characteristicUuid = "0000ffe1-0000-1000-8000-00805f9b34fb";

  // Raw 8-Byte Hexadecimal Control Frames
  static final Uint8List lockHexFrame = Uint8List.fromList([0xAA, 0x02, 0xA0, 0x01, 0x00, 0x00, 0xBE, 0xEF]);
  static final Uint8List unlockHexFrame = Uint8List.fromList([0xAA, 0x02, 0xA0, 0x00, 0x00, 0x00, 0xDE, 0xAD]);

  // Dual-Password Preservation
  String blePairingPin;        // Default: "123456" for GATT bonding
  String calibrationPassword;  // Default: "888888" for EEPROM register write

  final StreamController<Map<String, dynamic>> _telemetryController = StreamController.broadcast();

  FarDriverFocControllerHAL({
    this.blePairingPin = "123456",
    this.calibrationPassword = "888888",
  });

  @override
  String get architectureName => "FarDriver FOC (BLE UUID 0000ffe0)";

  @override
  Future<bool> initialize() async {
    // Connect to FarDriver internal Bluetooth module and verify authorization
    return true;
  }

  @override
  Future<void> sendImmobilizerLock() async {
    // Injects raw 8-byte frame directly to characteristic 0000ffe1
    // Frame: AA 02 A0 01 00 00 BE EF
  }

  @override
  Future<void> sendImmobilizerUnlock(String authPin) async {
    if (authPin == blePairingPin || authPin == calibrationPassword) {
      // Injects raw 8-byte frame: AA 02 A0 00 00 00 DE AD
    }
  }

  @override
  Future<void> dispatchConfigMode(bool useWireless) async {
    // Dispatched via gateway JSON
  }

  @override
  Stream<Map<String, dynamic>> get telemetryStream => _telemetryController.stream;

  @override
  void dispose() {
    _telemetryController.close();
  }
}

/// VESC FOC Controller HAL Implementation
class VescFocControllerHAL implements VehicleHardwareAdapter {
  @override
  String get architectureName => "VESC FOC (CAN / UART Comm)";

  final StreamController<Map<String, dynamic>> _telemetryController = StreamController.broadcast();

  @override
  Future<bool> initialize() async => true;

  @override
  Future<void> sendImmobilizerLock() async {
    // COMM_SET_CURRENT 0A & COMM_TERMINAL_CMD "lock"
  }

  @override
  Future<void> sendImmobilizerUnlock(String authPin) async {
    // COMM_SET_RELEASE & restore motor duty cycle
  }

  @override
  Future<void> dispatchConfigMode(bool useWireless) async {}

  @override
  Stream<Map<String, dynamic>> get telemetryStream => _telemetryController.stream;

  @override
  void dispose() {
    _telemetryController.close();
  }
}

/// Physical Relay & LEDC PWM Fallback HAL Implementation
class PwmRelayFallbackHAL implements VehicleHardwareAdapter {
  @override
  String get architectureName => "Physical D0 Relay & D8 PWM Fallback";

  final StreamController<Map<String, dynamic>> _telemetryController = StreamController.broadcast();

  @override
  Future<bool> initialize() async => true;

  @override
  Future<void> sendImmobilizerLock() async {
    // Trips D0 Relay to cut 5V to handlebar throttle Hall sensor; zeros D8 PWM
  }

  @override
  Future<void> sendImmobilizerUnlock(String authPin) async {
    // Restores D0 relay contact; enables D8 LEDC PWM channel
  }

  @override
  Future<void> dispatchConfigMode(bool useWireless) async {}

  @override
  Stream<Map<String, dynamic>> get telemetryStream => _telemetryController.stream;

  @override
  void dispose() {
    _telemetryController.close();
  }
}

/// Microcontroller Gateway Transport Abstraction
abstract class McuTransportAdapter {
  String get targetMcuName;
  Future<void> sendRawGatewayCommand(String jsonCommand);
}

class XiaoEsp32C3Transport implements McuTransportAdapter {
  @override
  String get targetMcuName => "Seeed Studio XIAO ESP32C3 (RISC-V 160MHz)";

  @override
  Future<void> sendRawGatewayCommand(String jsonCommand) async {
    // Dispatches via mobile BLE GATT to XIAO C3 Server
  }
}

class XiaoEsp32S3Transport implements McuTransportAdapter {
  @override
  String get targetMcuName => "Seeed Studio XIAO ESP32S3 (Dual-Core LX7 240MHz)";

  @override
  Future<void> sendRawGatewayCommand(String jsonCommand) async {}
}

/// Dual-Mode Security Coordinator with Disconnect & Dead Battery Motion Safeguard
class SecureVehicleCoordinator {
  final VehicleHardwareAdapter activeAdapter;
  final McuTransportAdapter mcuTransport;
  bool useWirelessImmobilizer;

  // Phone Dead Battery / Disconnect Safe-Stop Guardian State
  bool phoneGracePeriodActive = false;
  int graceCountdownSec = 60;
  Timer? _graceTimer;

  SecureVehicleCoordinator({
    required this.activeAdapter,
    required this.mcuTransport,
    this.useWirelessImmobilizer = true,
  });

  /// Dynamic Runtime Configuration Mode Toggle
  Future<void> setConfigMode(bool wireless) async {
    useWirelessImmobilizer = wireless;
    final packet = jsonEncode({"config_mode": wireless ? "wireless" : "wired"});
    await mcuTransport.sendRawGatewayCommand(packet);
  }

  /// Phone Disconnect / Battery Depletion Safeguard
  /// Prevents sudden throttle cut or wheel lockup while riding (speed > 0)
  void handlePhoneBatteryDepletedOrDisconnected({required int currentSpeedKmh, required Function() onSafeStationaryLock}) {
    if (currentSpeedKmh > 0) {
      phoneGracePeriodActive = true;
      graceCountdownSec = 60;
      _graceTimer?.cancel();
      _graceTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
        if (graceCountdownSec > 1) {
          graceCountdownSec--;
        } else {
          // If vehicle has now stopped, proceed to safe park
          if (currentSpeedKmh == 0) {
            timer.cancel();
            phoneGracePeriodActive = false;
            onSafeStationaryLock();
          }
        }
      });
    } else {
      // Vehicle is already stopped; can lock safely
      onSafeStationaryLock();
    }
  }

  void cancelGracePeriod() {
    phoneGracePeriodActive = false;
    _graceTimer?.cancel();
  }
}