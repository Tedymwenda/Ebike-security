import 'dart:async';
import 'dart:convert';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../hal/controller_hal.dart';

class VehicleTelemetry {
  final int speedKmh;
  final double currentA;
  final double voltageV;
  final bool isArmed;
  final bool isAlarmTriggered;
  final int packetsPerSecond;
  final bool useWirelessImmobilizer;
  final String farDriverPairPin;
  final String farDriverParamPass;
  final bool phoneGraceActive;
  final int phoneGraceCountdown;
  final String wifiHomeSsid;
  final String emergencyRecoveryPin;

  const VehicleTelemetry({
    this.speedKmh = 0,
    this.currentA = 0.0,
    this.voltageV = 72.8,
    this.isArmed = true,
    this.isAlarmTriggered = false,
    this.packetsPerSecond = 0,
    this.useWirelessImmobilizer = true,
    this.farDriverPairPin = "123456",
    this.farDriverParamPass = "888888",
    this.phoneGraceActive = false,
    this.phoneGraceCountdown = 0,
    this.wifiHomeSsid = "Home_Driveway_5G",
    this.emergencyRecoveryPin = "123456",
  });

  VehicleTelemetry copyWith({
    int? speedKmh,
    double? currentA,
    double? voltageV,
    bool? isArmed,
    bool? isAlarmTriggered,
    int? packetsPerSecond,
    bool? useWirelessImmobilizer,
    String? farDriverPairPin,
    String? farDriverParamPass,
    bool? phoneGraceActive,
    int? phoneGraceCountdown,
    String? wifiHomeSsid,
    String? emergencyRecoveryPin,
  }) {
    return VehicleTelemetry(
      speedKmh: speedKmh ?? this.speedKmh,
      currentA: currentA ?? this.currentA,
      voltageV: voltageV ?? this.voltageV,
      isArmed: isArmed ?? this.isArmed,
      isAlarmTriggered: isAlarmTriggered ?? this.isAlarmTriggered,
      packetsPerSecond: packetsPerSecond ?? this.packetsPerSecond,
      useWirelessImmobilizer: useWirelessImmobilizer ?? this.useWirelessImmobilizer,
      farDriverPairPin: farDriverPairPin ?? this.farDriverPairPin,
      farDriverParamPass: farDriverParamPass ?? this.farDriverParamPass,
      phoneGraceActive: phoneGraceActive ?? this.phoneGraceActive,
      phoneGraceCountdown: phoneGraceCountdown ?? this.phoneGraceCountdown,
      wifiHomeSsid: wifiHomeSsid ?? this.wifiHomeSsid,
      emergencyRecoveryPin: emergencyRecoveryPin ?? this.emergencyRecoveryPin,
    );
  }
}

class BleController extends StateNotifier<VehicleTelemetry> {
  BleController() : super(const VehicleTelemetry());

  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _cmdCharacteristic;
  int _packetCounter = 0;
  Timer? _fpsTimer;

  static const String serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  static const String charTelemetryUuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  static const String charCmdUuid = "d29ae63c-b783-49a3-8c46-9b76c8c25f77";

  void startBenchmarkCounter() {
    _fpsTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      state = state.copyWith(packetsPerSecond: _packetCounter);
      _packetCounter = 0;
    });
  }

  void parseRawTelemetry(String raw) {
    _packetCounter++;
    // Format: SPD:45,CUR:32.4,VOLT:72.8,ARM:1,ALM:0,WLS:1
    try {
      final parts = raw.split(',');
      int spd = state.speedKmh;
      double cur = state.currentA;
      double volt = state.voltageV;
      bool arm = state.isArmed;
      bool alm = state.isAlarmTriggered;
      bool wls = state.useWirelessImmobilizer;

      for (var p in parts) {
        final kv = p.split(':');
        if (kv.length == 2) {
          if (kv[0] == 'SPD') spd = int.tryParse(kv[1]) ?? spd;
          if (kv[0] == 'CUR') cur = double.tryParse(kv[1]) ?? cur;
          if (kv[0] == 'VOLT') volt = double.tryParse(kv[1]) ?? volt;
          if (kv[0] == 'ARM') arm = kv[1] == '1';
          if (kv[0] == 'ALM') alm = kv[1] == '1';
          if (kv[0] == 'WLS') wls = kv[1] == '1';
        }
      }
      state = state.copyWith(
        speedKmh: spd,
        currentA: cur,
        voltageV: volt,
        isArmed: arm,
        isAlarmTriggered: alm,
        useWirelessImmobilizer: wls,
      );
    } catch (_) {}
  }

  /// Dispatch serialized JSON command configuration packet to ESP32 Gateway
  Future<void> sendConfigModeCommand(bool isWireless) async {
    state = state.copyWith(useWirelessImmobilizer: isWireless);
    if (_cmdCharacteristic != null) {
      final jsonPacket = jsonEncode({"config_mode": isWireless ? "wireless" : "wired"});
      await _cmdCharacteristic!.write(utf8.encode(jsonPacket));
    }
  }

  Future<void> sendDisarmCommand(String pin) async {
    if (_cmdCharacteristic != null) {
      await _cmdCharacteristic!.write(utf8.encode("DISARM:$pin"));
    }
  }

  Future<void> sendArmCommand() async {
    if (_cmdCharacteristic != null) {
      await _cmdCharacteristic!.write(utf8.encode("ARM"));
    }
  }

  void updatePasswords({String? pairingPin, String? paramPass}) {
    state = state.copyWith(
      farDriverPairPin: pairingPin ?? state.farDriverPairPin,
      farDriverParamPass: paramPass ?? state.farDriverParamPass,
    );
  }

  /// TASK 2: Serialize Wi-Fi SSID, Password and Emergency PIN into BLE JSON pipeline
  Future<void> sendWifiAndEmergencyConfig({
    required String ssid,
    required String pass,
    required String emergencyPin,
  }) async {
    state = state.copyWith(
      wifiHomeSsid: ssid,
      emergencyRecoveryPin: emergencyPin,
    );
    if (_cmdCharacteristic != null) {
      final jsonPacket = jsonEncode({
        "wifi_ssid": ssid,
        "wifi_pass": pass,
        "emergency_pin": emergencyPin,
      });
      await _cmdCharacteristic!.write(utf8.encode(jsonPacket));
    }
  }

  @override
  void dispose() {
    _fpsTimer?.cancel();
    super.dispose();
  }
}

final telemetryProvider = StateNotifierProvider<BleController, VehicleTelemetry>((ref) {
  final controller = BleController();
  controller.startBenchmarkCounter();
  return controller;
});