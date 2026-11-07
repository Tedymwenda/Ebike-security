#!/usr/bin/env bash
set -Eeuo pipefail

FLUTTER_HOME="$HOME/development/flutter"
WORKSPACE_DIR="$HOME/development/ebike-mobile-workspace"
ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
SCRIPT_NAME="$(basename -- "${BASH_SOURCE[0]}")"

log() { printf '\n[mobile-toolchain] %s\n' "$*"; }
fail() { printf '\n[mobile-toolchain] ERROR: %s\n' "$*" >&2; exit 1; }

add_flutter_path() {
  local line='export PATH="$HOME/development/flutter/bin:$PATH"'
  local rc
  for rc in "$HOME/.bashrc" "$HOME/.profile" "$HOME/.zshrc"; do
    [[ -e "$rc" ]] || touch "$rc"
    grep -Fqx "$line" "$rc" || printf '\n%s\n' "$line" >> "$rc"
  done
  export PATH="$FLUTTER_HOME/bin:$PATH"
}

install_flutter() {
  local architecture archive_info base_url archive_path checksum temp_dir
  case "$(uname -m)" in
    x86_64|amd64) architecture="x64" ;;
    aarch64|arm64)
      log "The official stable Flutter release index has no Linux ARM64 host SDK. Android ARM64 is a build target, not a Flutter host SDK; use an x86_64 Linux build host."
      return 0
      ;;
    *) fail "No official Flutter Linux SDK is available for $(uname -m)." ;;
  esac

  if [[ -x "$FLUTTER_HOME/bin/flutter" ]]; then
    log "Keeping existing Flutter SDK at $FLUTTER_HOME."
    add_flutter_path
    return
  fi
  [[ ! -e "$FLUTTER_HOME" ]] || fail "$FLUTTER_HOME exists but is not a Flutter SDK; move it before retrying."

  log "Resolving the latest stable Flutter Linux $architecture SDK."
  archive_info="$(python3 <<'PY'
import json
import re
import urllib.request

url = "https://storage.googleapis.com/flutter_infra_release/releases/releases_linux.json"
with urllib.request.urlopen(url, timeout=60) as response:
    index = json.load(response)

pattern = re.compile(r"linux/flutter_linux_[^/]+-stable\.tar\.xz$")

releases = [release for release in index["releases"]
            if release.get("channel") == "stable" and pattern.search(release.get("archive", ""))]
if not releases:
    raise SystemExit("No stable Linux x64 archive appears in the official Flutter index.")
release = max(releases, key=lambda item: item.get("release_date", ""))
print(index["base_url"], release["archive"], release["sha256"])
PY
  )"
  read -r base_url archive_path checksum <<< "$archive_info"

  temp_dir="$(mktemp -d)"
  trap 'rm -rf "${temp_dir:-}"' EXIT
  log "Downloading and verifying the official Flutter archive."
  curl --fail --location --retry 3 "$base_url/$archive_path" --output "$temp_dir/flutter.tar.xz"
  printf '%s  %s\n' "$checksum" "$temp_dir/flutter.tar.xz" | sha256sum --check --status \
    || fail "Flutter archive checksum verification failed."
  mkdir -p "$HOME/development" "$temp_dir/sdk"
  tar -xJf "$temp_dir/flutter.tar.xz" --strip-components=1 -C "$temp_dir/sdk"
  mv "$temp_dir/sdk" "$FLUTTER_HOME"
  add_flutter_path
  log "Flutter installed at $FLUTTER_HOME."
}

install_android_sdk_x64() {
  local sdkmanager="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"
  local xml_file temp_dir tools_url rc
  if [[ "$(uname -m)" != "x86_64" && "$(uname -m)" != "amd64" ]]; then
    log "Android SDK command-line tools are not published for Linux ARM64. APK/AAB builds need an x86_64 Linux host or remote build service."
    return
  fi

  if [[ ! -x "$sdkmanager" ]]; then
    log "Installing Google's Android SDK command-line tools."
    xml_file="$(mktemp)"
    temp_dir="$(mktemp -d)"
    curl --fail --location --retry 3 \
      https://dl.google.com/android/repository/repository2-1.xml --output "$xml_file"
    tools_url="$(python3 - "$xml_file" <<'PY'
import re
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
urls = []
for package in root.findall(".//remotePackage"):
    if package.get("path") == "cmdline-tools;latest":
        urls.extend((node.text or "").strip() for node in package.findall(".//url"))
files = [value for value in urls if re.fullmatch(r"commandlinetools-linux-[0-9]+_latest\.zip", value)]
if not files:
    raise SystemExit("Android SDK index has no Linux command-line tools archive.")
print("https://dl.google.com/android/repository/" + files[0])
PY
    )"
    curl --fail --location --retry 3 "$tools_url" --output "$temp_dir/tools.zip"
    mkdir -p "$ANDROID_HOME/cmdline-tools/latest"
    unzip -q "$temp_dir/tools.zip" -d "$temp_dir/unpacked"
    cp -R "$temp_dir/unpacked/cmdline-tools/." "$ANDROID_HOME/cmdline-tools/latest/"
    rm -f "$xml_file"
    rm -rf "$temp_dir"
  fi

  export ANDROID_HOME ANDROID_SDK_ROOT="$ANDROID_HOME"
  export PATH="$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"
  for rc in "$HOME/.bashrc" "$HOME/.profile" "$HOME/.zshrc"; do
    [[ -e "$rc" ]] || touch "$rc"
    grep -Fq 'export ANDROID_HOME="$HOME/Android/Sdk"' "$rc" || cat >> "$rc" <<'EOF'
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"
EOF
  done

  set +o pipefail
  yes | "$sdkmanager" --sdk_root="$ANDROID_HOME" --licenses >/dev/null
  set -o pipefail
  "$sdkmanager" --sdk_root="$ANDROID_HOME" \
    "platform-tools" "platforms;android-36" "build-tools;36.0.0"
  log "Android SDK platforms and build tools installed."
}

install_ubuntu() {
  local -a root_command=()
  if [[ "$(id -u)" -ne 0 ]]; then
    command -v sudo >/dev/null 2>&1 || fail "Ubuntu installation needs root or sudo."
    sudo -n true 2>/dev/null || fail "Non-interactive sudo is unavailable; run from an Ubuntu root shell."
    root_command=(sudo -n)
  fi

  log "Updating Ubuntu repositories and installing core dependencies."
  "${root_command[@]}" apt-get update -y
  "${root_command[@]}" apt-get install -y ca-certificates curl git wget python3 xz-utils unzip zip tar gnupg openjdk-17-jdk

  log "Installing Node.js 22 (includes npm) and the global Expo CLI package."
  curl --fail --location --retry 3 https://deb.nodesource.com/setup_22.x | "${root_command[@]}" bash -
  "${root_command[@]}" apt-get install -y nodejs
  command -v npm >/dev/null 2>&1 || fail "Node.js installation did not provide npm."
  npm install --global expo

  install_flutter
  install_android_sdk_x64

  log "Creating the Expo TypeScript template workspace at $WORKSPACE_DIR."
  mkdir -p "$(dirname -- "$WORKSPACE_DIR")"
  if [[ ! -f "$WORKSPACE_DIR/package.json" ]]; then
    npx --yes create-expo-app@latest "$WORKSPACE_DIR" --template blank-typescript
  else
    log "Keeping existing workspace files."
  fi

  cd "$WORKSPACE_DIR"
  log "Installing the requested BLE and location packages."
  npm install react-native-ble-plx expo-location expo-modules-core

  log "Installation complete."
  printf 'Flutter SDK: %s\nExpo workspace: %s\n' "$FLUTTER_HOME" "$WORKSPACE_DIR"
  log "react-native-ble-plx requires an Expo development build; it does not run in Expo Go."
}

if [[ "${1:-}" == "--inside-ubuntu" ]]; then
  install_ubuntu
  exit 0
fi

if [[ -n "${PREFIX:-}" && "$PREFIX" == *com.termux* ]] && command -v pkg >/dev/null 2>&1; then
  log "Termux detected; updating packages and creating an Ubuntu proot for Flutter."
  pkg update -y
  pkg upgrade -y
  pkg install -y nodejs-lts git curl wget python proot-distro
  if ! pkg install -y openjdk-17; then
    log "Termux openjdk-17 is unavailable; using openjdk-21."
    pkg install -y openjdk-21
  fi
  if ! proot-distro login ubuntu -- true >/dev/null 2>&1; then
    proot-distro install ubuntu
  fi
  script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
  proot-distro login ubuntu \
    --bind "$script_directory:/mnt/mobile-toolchain-installer" ubuntu \
    -- bash "/mnt/mobile-toolchain-installer/$SCRIPT_NAME" --inside-ubuntu
  exit 0
fi

if command -v apt-get >/dev/null 2>&1 && [[ -r /etc/os-release ]] \
  && grep -qiE 'ubuntu|debian' /etc/os-release; then
  install_ubuntu
  exit 0
fi

cat >&2 <<'EOF'
Unsupported shell: expected Ubuntu/Debian or Termux. Android/Bionic VSCodroid
cannot run Ubuntu apt packages or Flutter's glibc SDK directly. Install Termux
from F-Droid, run `termux-setup-storage`, put this script in Termux home, and run
it there; it will create an Ubuntu proot for the Flutter SDK.
EOF
exit 2