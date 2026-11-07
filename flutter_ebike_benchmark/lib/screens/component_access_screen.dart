import 'package:flutter/material.dart';

enum SecurityTarget { farDriver, smartBms, ignition }

extension SecurityTargetLabel on SecurityTarget {
  String get label => switch (this) {
        SecurityTarget.farDriver => 'FARDRIVER',
        SecurityTarget.smartBms => 'SMART BMS',
        SecurityTarget.ignition => 'IGNITION',
      };
}

class TargetRoute {
  const TargetRoute({required this.routeId, required this.hardwareAddress});

  final String routeId;
  final String? hardwareAddress;
}

const Map<SecurityTarget, TargetRoute> targetRoutes = {
  SecurityTarget.farDriver: TargetRoute(routeId: 'far_driver', hardwareAddress: null),
  SecurityTarget.smartBms: TargetRoute(routeId: 'smart_bms', hardwareAddress: null),
  SecurityTarget.ignition: TargetRoute(routeId: 'ignition', hardwareAddress: null),
};

class ComponentAccessScreen extends StatefulWidget {
  const ComponentAccessScreen({super.key});

  @override
  State<ComponentAccessScreen> createState() => _ComponentAccessScreenState();
}

class _ComponentAccessScreenState extends State<ComponentAccessScreen> {
  final TextEditingController _credentialController = TextEditingController();
  SecurityTarget _activeTarget = SecurityTarget.farDriver;
  String _result = 'Select a component and enter its credential.';

  @override
  void dispose() {
    _credentialController.dispose();
    super.dispose();
  }

  void _selectTarget(SecurityTarget target) {
    setState(() {
      _activeTarget = target;
      _credentialController.clear();
      _result = 'Select a component and enter its credential.';
    });
  }

  void _submitCredential() {
    if (_credentialController.text.trim().isEmpty) {
      setState(() => _result = 'Enter a password or PIN for ${_activeTarget.label}.');
      return;
    }

    final route = targetRoutes[_activeTarget]!;
    _credentialController.clear();
    setState(() {
      _result = route.hardwareAddress == null
          ? 'ROUTE BLOCKED: No hardware address is configured for ${_activeTarget.label}. No BLE data was sent.'
          : 'ROUTE BLOCKED: BLE credential transport is not configured for ${route.routeId}.';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF030712),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  const Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          'TITAN SECURITY',
                          style: TextStyle(
                            color: Colors.white,
                            fontFamily: 'monospace',
                            fontSize: 16,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        SizedBox(height: 4),
                        Text(
                          'COMPONENT ACCESS',
                          style: TextStyle(
                            color: Color(0xFF94A3B8),
                            fontFamily: 'monospace',
                            fontSize: 10,
                          ),
                        ),
                      ],
                    ),
                  ),
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                    decoration: BoxDecoration(
                      color: const Color(0xFF0F172A),
                      border: Border.all(color: const Color(0xFF334155)),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: const Text(
                      'BLE ROUTE LOCKED',
                      style: TextStyle(
                        color: Color(0xFFFBBF24),
                        fontFamily: 'monospace',
                        fontSize: 9,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 24),
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  border: Border.all(color: const Color(0xFF1E293B)),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: const Row(
                  children: [
                    Expanded(
                      child: Text(
                        'ROUTE STATUS',
                        style: TextStyle(
                          color: Color(0xFF94A3B8),
                          fontFamily: 'monospace',
                          fontSize: 10,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                    Text(
                      'UNCONFIGURED',
                      style: TextStyle(
                        color: Color(0xFFFBBF24),
                        fontFamily: 'monospace',
                        fontSize: 10,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 16),
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: const Color(0xFF0F172A),
                  border: Border.all(color: const Color(0xFF1E293B)),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const Text(
                      'ACTIVE COMPONENT',
                      style: TextStyle(
                        color: Color(0xFF94A3B8),
                        fontFamily: 'monospace',
                        fontSize: 10,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 10),
                    Row(
                      children: SecurityTarget.values
                          .map((target) => Expanded(
                                child: _targetButton(
                                  target,
                                  addRightGap: target != SecurityTarget.ignition,
                                ),
                              ))
                          .toList(),
                    ),
                    const SizedBox(height: 16),
                    Text(
                      'ACTIVE TARGET: ${_activeTarget.label}',
                      style: const TextStyle(
                        color: Color(0xFF06B6D4),
                        fontFamily: 'monospace',
                        fontSize: 11,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 16),
                    const Text(
                      'PASSWORD / PIN',
                      style: TextStyle(
                        color: Color(0xFF94A3B8),
                        fontFamily: 'monospace',
                        fontSize: 10,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 8),
                    SizedBox(
                      height: 48,
                      child: TextField(
                        controller: _credentialController,
                        obscureText: true,
                        autocorrect: false,
                        enableSuggestions: false,
                        style: const TextStyle(
                          color: Colors.white,
                          fontFamily: 'monospace',
                          fontSize: 14,
                        ),
                        decoration: InputDecoration(
                          hintText: 'Enter password or PIN',
                          hintStyle: const TextStyle(color: Color(0xFF64748B), fontSize: 12),
                          filled: true,
                          fillColor: const Color(0xFF030712),
                          contentPadding: const EdgeInsets.symmetric(horizontal: 12),
                          enabledBorder: OutlineInputBorder(
                            borderRadius: BorderRadius.circular(8),
                            borderSide: const BorderSide(color: Color(0xFF334155)),
                          ),
                          focusedBorder: OutlineInputBorder(
                            borderRadius: BorderRadius.circular(8),
                            borderSide: const BorderSide(color: Color(0xFF06B6D4)),
                          ),
                        ),
                      ),
                    ),
                    const SizedBox(height: 12),
                    SizedBox(
                      height: 48,
                      child: ElevatedButton(
                        onPressed: _submitCredential,
                        style: ElevatedButton.styleFrom(
                          backgroundColor: const Color(0xFF06B6D4),
                          foregroundColor: const Color(0xFF030712),
                          elevation: 0,
                          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                        ),
                        child: const Text(
                          'VALIDATE ROUTE',
                          style: TextStyle(
                            fontFamily: 'monospace',
                            fontSize: 11,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ),
                    ),
                    const SizedBox(height: 12),
                    Text(
                      _result,
                      style: const TextStyle(
                        color: Color(0xFFFBBF24),
                        fontFamily: 'monospace',
                        fontSize: 10,
                        height: 1.4,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 16),
              const Text(
                'Credentials stay local. No authenticated hardware addresses are configured; no BLE packet is sent.',
                style: TextStyle(
                  color: Color(0xFF64748B),
                  fontFamily: 'monospace',
                  fontSize: 10,
                  height: 1.5,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _targetButton(SecurityTarget target, {required bool addRightGap}) {
    final selected = target == _activeTarget;
    return Padding(
      padding: EdgeInsets.only(right: addRightGap ? 6 : 0),
      child: SizedBox(
        height: 44,
        child: Material(
          color: selected ? const Color(0xFF06B6D4) : const Color(0xFF030712),
          borderRadius: BorderRadius.circular(8),
          child: InkWell(
            borderRadius: BorderRadius.circular(8),
            onTap: () => _selectTarget(target),
            child: Container(
              alignment: Alignment.center,
              decoration: BoxDecoration(
                border: Border.all(
                  color: selected ? const Color(0xFF06B6D4) : const Color(0xFF334155),
                ),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Text(
                target.label,
                textAlign: TextAlign.center,
                style: TextStyle(
                  color: selected ? const Color(0xFF030712) : const Color(0xFFCBD5E1),
                  fontFamily: 'monospace',
                  fontSize: 9,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}