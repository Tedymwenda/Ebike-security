import React, { useState } from 'react';
import {
  Pressable,
  SafeAreaView,
  ScrollView,
  StatusBar,
  StyleSheet,
  Text,
  TextInput,
  View,
} from 'react-native';

enum SecurityTarget {
  FarDriver = 'FARDRIVER',
  SmartBms = 'SMART BMS',
  Ignition = 'IGNITION',
}

type TargetRoute = {
  routeId: string;
  hardwareAddress: string | null;
};

const targetRoutes: Record<SecurityTarget, TargetRoute> = {
  [SecurityTarget.FarDriver]: { routeId: 'far_driver', hardwareAddress: null },
  [SecurityTarget.SmartBms]: { routeId: 'smart_bms', hardwareAddress: null },
  [SecurityTarget.Ignition]: { routeId: 'ignition', hardwareAddress: null },
};

const targets = Object.values(SecurityTarget) as SecurityTarget[];

export default function App() {
  const [activeTarget, setActiveTarget] = useState<SecurityTarget>(SecurityTarget.FarDriver);
  const [credential, setCredential] = useState('');
  const [result, setResult] = useState('Select a component and enter its credential.');

  const selectTarget = (target: SecurityTarget) => {
    setActiveTarget(target);
    setCredential('');
    setResult('Select a component and enter its credential.');
  };

  const submitCredential = () => {
    if (credential.trim().length === 0) {
      setResult(`Enter a password or PIN for ${activeTarget}.`);
      return;
    }

    const route = targetRoutes[activeTarget];
    setCredential('');
    setResult(
      route.hardwareAddress === null
        ? `ROUTE BLOCKED: No hardware address is configured for ${activeTarget}. No BLE data was sent.`
        : `ROUTE BLOCKED: BLE credential transport is not configured for ${route.routeId}.`,
    );
  };

  return (
    <SafeAreaView style={styles.safeArea}>
      <StatusBar barStyle="light-content" />
      <ScrollView contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
        <View style={styles.header}>
          <View style={styles.headerTitle}>
            <Text style={styles.title}>TITAN SECURITY</Text>
            <Text style={styles.subtitle}>COMPONENT ACCESS</Text>
          </View>
          <View style={styles.badge}>
            <Text style={styles.badgeText}>BLE ROUTE LOCKED</Text>
          </View>
        </View>

        <View style={[styles.panel, styles.routePanel]}>
          <Text style={styles.sectionLabel}>ROUTE STATUS</Text>
          <Text style={styles.routeValue}>UNCONFIGURED</Text>
        </View>

        <View style={styles.panel}>
          <Text style={styles.sectionLabel}>ACTIVE COMPONENT</Text>
          <View style={styles.tabs}>
            {targets.map((target, index) => {
              const selected = target === activeTarget;
              return (
                <Pressable
                  key={target}
                  accessibilityRole="button"
                  accessibilityState={{ selected }}
                  onPress={() => selectTarget(target)}
                  style={[
                    styles.tab,
                    index < targets.length - 1 && styles.tabSpacing,
                    selected ? styles.tabSelected : styles.tabUnselected,
                  ]}
                >
                  <Text style={[styles.tabText, selected ? styles.tabTextSelected : styles.tabTextUnselected]}>
                    {target}
                  </Text>
                </Pressable>
              );
            })}
          </View>

          <Text style={styles.activeTarget}>ACTIVE TARGET: {activeTarget}</Text>
          <Text style={[styles.sectionLabel, styles.credentialLabel]}>PASSWORD / PIN</Text>
          <TextInput
            accessibilityLabel="Password or PIN"
            autoCapitalize="none"
            autoCorrect={false}
            onChangeText={setCredential}
            onSubmitEditing={submitCredential}
            placeholder="Enter password or PIN"
            placeholderTextColor={colors.placeholder}
            returnKeyType="done"
            secureTextEntry
            selectionColor={colors.accent}
            style={styles.input}
            value={credential}
          />
          <Pressable
            accessibilityRole="button"
            onPress={submitCredential}
            style={({ pressed }) => [styles.submitButton, pressed && styles.submitPressed]}
          >
            <Text style={styles.submitText}>VALIDATE ROUTE</Text>
          </Pressable>
          <Text accessibilityLiveRegion="polite" style={styles.result}>
            {result}
          </Text>
        </View>

        <Text style={styles.notice}>
          Credentials stay local. No authenticated hardware addresses are configured; no BLE packet is sent.
        </Text>
      </ScrollView>
    </SafeAreaView>
  );
}

const colors = {
  background: '#030712',
  panel: '#0F172A',
  border: '#1E293B',
  outline: '#334155',
  accent: '#06B6D4',
  muted: '#94A3B8',
  placeholder: '#64748B',
  text: '#FFFFFF',
  tabText: '#CBD5E1',
  warning: '#FBBF24',
};

const mono = 'monospace';

const styles = StyleSheet.create({
  safeArea: { flex: 1, backgroundColor: colors.background },
  content: { padding: 16, flexGrow: 1 },
  header: { minHeight: 40, flexDirection: 'row', alignItems: 'center' },
  headerTitle: { flex: 1 },
  title: { color: colors.text, fontFamily: mono, fontSize: 16, fontWeight: '700' },
  subtitle: { marginTop: 4, color: colors.muted, fontFamily: mono, fontSize: 10 },
  badge: {
    paddingHorizontal: 10,
    paddingVertical: 8,
    borderWidth: 1,
    borderColor: colors.outline,
    borderRadius: 8,
    backgroundColor: colors.panel,
  },
  badgeText: { color: colors.warning, fontFamily: mono, fontSize: 9, fontWeight: '700' },
  panel: {
    padding: 16,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: 12,
    backgroundColor: colors.panel,
  },
  routePanel: {
    marginTop: 24,
    marginBottom: 16,
    minHeight: 56,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  sectionLabel: { color: colors.muted, fontFamily: mono, fontSize: 10, fontWeight: '700' },
  routeValue: { color: colors.warning, fontFamily: mono, fontSize: 10, fontWeight: '700' },
  tabs: { flexDirection: 'row', marginTop: 10 },
  tab: {
    flex: 1,
    height: 44,
    alignItems: 'center',
    justifyContent: 'center',
    borderWidth: 1,
    borderRadius: 8,
  },
  tabSpacing: { marginRight: 6 },
  tabSelected: { borderColor: colors.accent, backgroundColor: colors.accent },
  tabUnselected: { borderColor: colors.outline, backgroundColor: colors.background },
  tabText: { textAlign: 'center', fontFamily: mono, fontSize: 9, fontWeight: '700' },
  tabTextSelected: { color: colors.background },
  tabTextUnselected: { color: colors.tabText },
  activeTarget: {
    marginTop: 16,
    color: colors.accent,
    fontFamily: mono,
    fontSize: 11,
    fontWeight: '700',
  },
  credentialLabel: { marginTop: 16 },
  input: {
    height: 48,
    marginTop: 8,
    paddingHorizontal: 12,
    borderWidth: 1,
    borderColor: colors.outline,
    borderRadius: 8,
    backgroundColor: colors.background,
    color: colors.text,
    fontFamily: mono,
    fontSize: 14,
  },
  submitButton: {
    height: 48,
    marginTop: 12,
    alignItems: 'center',
    justifyContent: 'center',
    borderRadius: 8,
    backgroundColor: colors.accent,
  },
  submitPressed: { opacity: 0.8 },
  submitText: { color: colors.background, fontFamily: mono, fontSize: 11, fontWeight: '700' },
  result: { marginTop: 12, color: colors.warning, fontFamily: mono, fontSize: 10, lineHeight: 14 },
  notice: { marginTop: 16, color: colors.placeholder, fontFamily: mono, fontSize: 10, lineHeight: 15 },
});