import 'dart:math';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../services/ble_service.dart';

class PerformanceDashboardScreen extends ConsumerWidget {
  const PerformanceDashboardScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final telemetry = ref.watch(telemetryProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('TITAN FOC • FLUTTER ENGINE', style: TextStyle(fontFamily: 'monospace', fontSize: 14)),
        backgroundColor: const Color(0xFF030712),
        elevation: 0,
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 16),
            child: Chip(
              label: Text('${telemetry.packetsPerSecond} PKT/S', style: const TextStyle(fontSize: 11, fontFamily: 'monospace')),
              backgroundColor: const Color(0xFF0F172A),
            ),
          )
        ],
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            children: [
              // Phone Disconnect / Dead Battery Safety Grace Banner
              if (telemetry.phoneGraceActive)
                Container(
                  margin: const EdgeInsets.only(bottom: 16),
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.amber.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: Colors.amberAccent),
                  ),
                  child: Row(
                    children: [
                      const Icon(Icons.warning_amber_rounded, color: Colors.amberAccent),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          'MOTION GRACE ACTIVE (${telemetry.phoneGraceCountdown}s): Phone disconnected while riding. Safe-stop required before immobilization.',
                          style: const TextStyle(color: Colors.amberAccent, fontSize: 11),
                        ),
                      )
                    ],
                  ),
                ),

              // Custom Gauge Canvas (Speedometer)
              SizedBox(
                width: 240,
                height: 240,
                child: CustomPaint(
                  painter: SpeedometerPainter(speed: telemetry.speedKmh.toDouble(), maxSpeed: 120),
                  child: Center(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(
                          '${telemetry.speedKmh}',
                          style: const TextStyle(fontSize: 54, fontWeight: FontWeight.bold, fontFamily: 'monospace'),
                        ),
                        const Text('KM/H', style: TextStyle(color: Color(0xFF06B6D4), fontSize: 12, letterSpacing: 2)),
                        const SizedBox(height: 6),
                        Text('${(telemetry.voltageV * telemetry.currentA).round()} WATTS',
                          style: const TextStyle(color: Colors.white54, fontSize: 11, fontFamily: 'monospace')),
                      ],
                    ),
                  ),
                ),
              ),

              const SizedBox(height: 16),

              // Security status card
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: const Color(0xFF1E293B)),
                ),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('IMMOBILIZER: ${telemetry.isArmed ? "ARMED (LOCKED)" : "DISARMED"}',
                          style: TextStyle(color: telemetry.isArmed ? Colors.redAccent : Colors.greenAccent, fontWeight: FontWeight.bold, fontSize: 12)),
                        Text('PACK: ${telemetry.voltageV}V  •  ${telemetry.currentA}A PHASE',
                          style: const TextStyle(color: Colors.white60, fontSize: 11, fontFamily: 'monospace')),
                      ],
                    ),
                    ElevatedButton(
                      onPressed: () {
                        if (telemetry.isArmed) {
                          ref.read(telemetryProvider.notifier).sendDisarmCommand("123456");
                        } else {
                          ref.read(telemetryProvider.notifier).sendArmCommand();
                        }
                      },
                      style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF06B6D4)),
                      child: Text(telemetry.isArmed ? 'DISARM' : 'ARM', style: const TextStyle(color: Colors.black, fontWeight: FontWeight.bold)),
                    )
                  ],
                ),
              ),

              const SizedBox(height: 16),

              // ADAPTIVE MATERIAL SWITCH SETTINGS TILE: Wireless Software Override vs Wired Relay Cutout
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: const Color(0xFF1E293B)),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                telemetry.useWirelessImmobilizer
                                    ? "Wireless Software Immobilization via BLE"
                                    : "Wired Relay Hard-Cutout",
                                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 13, color: Colors.white),
                              ),
                              const SizedBox(height: 4),
                              Text(
                                telemetry.useWirelessImmobilizer
                                    ? "Injects FarDriver hex {0xAA, 0x02, 0xA0...} over BLE FFE1. Bypasses D0 relay."
                                    : "Engages physical D0 relay & D8 throttle PWM channel.",
                                style: const TextStyle(fontSize: 11, color: Colors.white54),
                              ),
                            ],
                          ),
                        ),
                        Switch.adaptive(
                          value: telemetry.useWirelessImmobilizer,
                          activeColor: const Color(0xFF06B6D4),
                          onChanged: (val) {
                            ref.read(telemetryProvider.notifier).sendConfigModeCommand(val);
                          },
                        ),
                      ],
                    ),
                    const Divider(height: 24, color: Color(0xFF1E293B)),
                    // Serialized JSON Packet Feedback
                    Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                      decoration: BoxDecoration(
                        color: Colors.black54,
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          const Text("Dispatched Gateway JSON:", style: TextStyle(fontSize: 10, color: Colors.white38)),
                          Text(
                            telemetry.useWirelessImmobilizer ? '{"config_mode": "wireless"}' : '{"config_mode": "wired"}',
                            style: const TextStyle(fontSize: 11, fontFamily: 'monospace', color: Color(0xFF06B6D4), fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),

              const SizedBox(height: 16),

              // DUAL-PASSWORD VALIDATION TILES (Pairing PIN: 123456 & Calibration Password: 888888)
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: const Color(0xFF1E293B)),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text("FARDRIVER DUAL-PASSWORD AUTHORIZATION",
                      style: TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: Colors.white70, letterSpacing: 1.2)),
                    const SizedBox(height: 12),
                    Row(
                      children: [
                        Expanded(
                          child: Container(
                            padding: const EdgeInsets.all(10),
                            decoration: BoxDecoration(
                              color: const Color(0xFF030712),
                              borderRadius: BorderRadius.circular(8),
                              border: Border.all(color: const Color(0xFF1E293B)),
                            ),
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                const Text("BLE PAIRING PIN", style: TextStyle(fontSize: 9, color: Colors.white38)),
                                const SizedBox(height: 2),
                                Text(telemetry.farDriverPairPin,
                                  style: const TextStyle(fontSize: 14, fontWeight: FontWeight.bold, fontFamily: 'monospace', color: Color(0xFF06B6D4))),
                              ],
                            ),
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: Container(
                            padding: const EdgeInsets.all(10),
                            decoration: BoxDecoration(
                              color: const Color(0xFF030712),
                              borderRadius: BorderRadius.circular(8),
                              border: Border.all(color: const Color(0xFF1E293B)),
                            ),
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                const Text("CALIB PASSWORD", style: TextStyle(fontSize: 9, color: Colors.white38)),
                                const SizedBox(height: 2),
                                Text(telemetry.farDriverParamPass,
                                  style: const TextStyle(fontSize: 14, fontWeight: FontWeight.bold, fontFamily: 'monospace', color: Colors.amberAccent)),
                              ],
                            ),
                          ),
                        ),
                      ],
                    )
                  ],
                ),
              ),

              const SizedBox(height: 16),

              // TASK 2: ONBOARD WI-FI & EMERGENCY CAPTIVE PORTAL PROVISIONING TILE
              // DESIGN TOKENS FOR CAPTIVE PORTAL PARITY:
              // - Primary Background: Color(0xFF0F172A) / #0f172a
              // - Card Fill: Color(0xFF1E293B) / #1e293b
              // - Card Border: Color(0xFF334155) / #334155 (Radius: 16.0)
              // - Primary Accent: Color(0xFF38BDF8) / #38bdf8 (Cyan)
              // - Success / Disarmed: Color(0xFF10B981) / #10b981 (Emerald)
              // - Danger / Armed: Color(0xFFEF4444) / #ef4444 (Crimson)
              // - Typography: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  borderRadius: BorderRadius.circular(16),
                  border: Border.all(color: const Color(0xFF1E293B)),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      "DRIVEWAY WI-FI & EMERGENCY KEY RECOVERY",
                      style: TextStyle(
                        fontSize: 11,
                        fontWeight: FontWeight.bold,
                        color: Colors.white70,
                        letterSpacing: 1.2,
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextField(
                      decoration: const InputDecoration(
                        labelText: "Home Wi-Fi SSID",
                        labelStyle: TextStyle(fontSize: 11, color: Colors.white54),
                        prefixIcon: Icon(Icons.wifi, color: Color(0xFF06B6D4), size: 18),
                        border: OutlineInputBorder(),
                        contentPadding: EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                      ),
                      controller: TextEditingController(text: telemetry.wifiHomeSsid),
                    ),
                    const SizedBox(height: 8),
                    TextField(
                      obscureText: true,
                      decoration: const InputDecoration(
                        labelText: "Home Wi-Fi Password",
                        labelStyle: TextStyle(fontSize: 11, color: Colors.white54),
                        prefixIcon: Icon(Icons.password, color: Color(0xFF06B6D4), size: 18),
                        border: OutlineInputBorder(),
                        contentPadding: EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                      ),
                    ),
                    const SizedBox(height: 8),
                    TextField(
                      maxLength: 6,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(
                        labelText: "6-Digit Master Emergency Recovery PIN",
                        labelStyle: TextStyle(fontSize: 11, color: Colors.amberAccent),
                        prefixIcon: Icon(Icons.key, color: Colors.amberAccent, size: 18),
                        border: OutlineInputBorder(),
                        contentPadding: EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                      ),
                      controller: TextEditingController(text: telemetry.emergencyRecoveryPin),
                    ),
                    const SizedBox(height: 6),
                    // Serialized JSON Packet Feedback
                    Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                      decoration: BoxDecoration(
                        color: Colors.black54,
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text("DISPATCHED BLE PROVISIONING PACKET:", style: TextStyle(fontSize: 9, color: Colors.white38)),
                          const SizedBox(height: 2),
                          Text(
                            '{"wifi_ssid": "${telemetry.wifiHomeSsid}", "wifi_pass": "••••••••", "emergency_pin": "${telemetry.emergencyRecoveryPin}"}',
                            style: const TextStyle(fontSize: 10, fontFamily: 'monospace', color: Color(0xFF06B6D4), fontWeight: FontWeight.bold),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              )
            ],
          ),
        ),
      ),
    );
  }
}

class SpeedometerPainter extends CustomPainter {
  final double speed;
  final double maxSpeed;

  SpeedometerPainter({required this.speed, required this.maxSpeed});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2 - 16;
    final trackPaint = Paint()
      ..color = const Color(0xFF1E293B)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 12
      ..strokeCap = StrokeCap.round;

    final activePaint = Paint()
      ..color = const Color(0xFF06B6D4)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 12
      ..strokeCap = StrokeCap.round;

    const startAngle = 135 * (pi / 180);
    const sweepAngle = 270 * (pi / 180);

    canvas.drawArc(Rect.fromCircle(center: center, radius: radius), startAngle, sweepAngle, false, trackPaint);
    final currentSweep = (speed / maxSpeed).clamp(0.0, 1.0) * sweepAngle;
    canvas.drawArc(Rect.fromCircle(center: center, radius: radius), startAngle, currentSweep, false, activePaint);
  }

  @override
  bool shouldRepaint(covariant SpeedometerPainter oldDelegate) => oldDelegate.speed != speed;
}