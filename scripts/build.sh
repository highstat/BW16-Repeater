#!/usr/bin/env bash
set -euo pipefail
FQBN=${FQBN:-realtek:AmebaD:Ai-Thinker_BW16}

echo "Updating Arduino core indexes..."
arduino-cli core update-index
arduino-cli core install realtek:AmebaD@3.1.9

ARDUINO_DATA=""
for CANDIDATE in "$HOME/Library/Arduino15" "$HOME/.arduino15"; do
  if [ -d "$CANDIDATE/packages/realtek/hardware/AmebaD/3.1.9" ]; then
    ARDUINO_DATA="$CANDIDATE"
    break
  fi
done
if [ -z "$ARDUINO_DATA" ]; then
  echo "AmebaD core 3.1.9 was installed, but its Arduino data directory was not found." >&2
  exit 1
fi
CORE_ROOT="$ARDUINO_DATA/packages/realtek/hardware/AmebaD/3.1.9"

# Enable CONFIG_EXAMPLE_WLAN_REPEATER in installed AmebaD core if not already enabled
PLATFORM_OPTS="$CORE_ROOT/system/project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h"
if [ ! -f "$PLATFORM_OPTS" ]; then
  echo "Missing AmebaD platform options: $PLATFORM_OPTS" >&2
  exit 1
fi
sed -i.bak 's/#define CONFIG_EXAMPLE_WLAN_REPEATER[[:space:]]*0/#define CONFIG_EXAMPLE_WLAN_REPEATER    1/' "$PLATFORM_OPTS"
if ! grep -Eq '^#define CONFIG_EXAMPLE_WLAN_REPEATER[[:space:]]+1' "$PLATFORM_OPTS"; then
  echo "Unable to enable CONFIG_EXAMPLE_WLAN_REPEATER in $PLATFORM_OPTS" >&2
  exit 1
fi
echo "Configured CONFIG_EXAMPLE_WLAN_REPEATER=1 in $PLATFORM_OPTS"

echo "Compiling BW16-Repeater for $FQBN..."
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}bw16-build.XXXXXX")
BUILD_DIR="$WORK_DIR/arduino"
BRIDGE_LIB="$WORK_DIR/lib_bridge_core.a"
COMPILE_LOG="$WORK_DIR/compile.log"
ABI_DIR="$WORK_DIR/lwip-vendor-abi"
mkdir -p "$ABI_DIR"
python3 - "$CORE_ROOT" "$ABI_DIR" <<'PY'
import pathlib, sys
core, output = map(pathlib.Path, sys.argv[1:])
source = (core / "system/component/common/api/network/include/lwipopts.h").read_text()
before = "#define LWIP_NUM_NETIF_CLIENT_DATA            1"
if source.count(before) != 1:
    raise SystemExit("Unexpected vendor lwipopts: cannot preserve netif ABI")
source = source.replace(before, "#define LWIP_NUM_NETIF_CLIENT_DATA            0", 1)
(output / "lwipopts.h").write_text(source)
PY
BUILD_FLAGS="-I \"$ABI_DIR\""
if [ -n "${BW16_NET_DIAGNOSTICS:-}" ]; then
  BUILD_FLAGS="$BUILD_FLAGS -DBW16_NET_DIAGNOSTICS=1"
fi
mkdir -p "$BUILD_DIR"
trap 'rm -rf "$WORK_DIR"' EXIT
arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD_DIR" \
  --build-property "compiler.c.extra_flags=$BUILD_FLAGS" \
  --only-compilation-database ./
python3 scripts/build_bridge_core.py \
  --compile-commands "$BUILD_DIR/compile_commands.json" \
  --core-root "$CORE_ROOT" \
  --output "$BRIDGE_LIB" \
  ${BW16_NET_DIAGNOSTICS:+--diagnostics}

BRIDGE_LDFLAGS="-Wl,--whole-archive $BRIDGE_LIB -Wl,--no-whole-archive"
arduino-cli compile --fqbn "$FQBN" --build-path "$BUILD_DIR" \
  --build-property "compiler.c.extra_flags=$BUILD_FLAGS" \
  --build-property "compiler.libraries.ldflags=$BRIDGE_LDFLAGS" \
  ./ 2>&1 | tee "$COMPILE_LOG" || {
  echo "Build failed. Verify FQBN. Available AmebaD boards:" >&2
  arduino-cli board listall | sed -n '1p;/realtek:AmebaD/p'
  exit 1
}
echo "Locating firmware binary..."
mkdir -p build bin
KM_BIN="$BUILD_DIR/km0_km4_image2.bin"
if grep -Eq 'Operation not permitted|Permission denied|cannot create|No such file' "$COMPILE_LOG"; then
  echo "Firmware packaging failed; existing binaries were not replaced." >&2
  exit 1
fi
if [ ! -s "$KM_BIN" ]; then
  echo "Firmware output missing: $KM_BIN" >&2
  exit 1
fi
cp "$KM_BIN" build/BW16-Repeater.bin
cp "$KM_BIN" bin/bw16_repeater_firmware.bin
if [ -n "${BW16_NET_DIAGNOSTICS:-}" ]; then
  cp "$BUILD_DIR/application.map" build/application.map
  cp "$BUILD_DIR/application.axf" build/application.axf
fi

NM=""
for CANDIDATE in "$ARDUINO_DATA"/packages/realtek/tools/ameba_d_asdk_toolchain/*/bin/arm-none-eabi-nm; do
  if [ -x "$CANDIDATE" ]; then
    NM="$CANDIDATE"
    break
  fi
done
if [ -z "$NM" ]; then
  echo "AmebaD arm-none-eabi-nm was not found." >&2
  exit 1
fi
if ! "$NM" -S "$BUILD_DIR/application.axf" | grep -Eq ' 0000024c B xnetif$'; then
  echo "Unable to verify three netifs with the vendor 196-byte ABI." >&2
  exit 1
fi

echo "Build succeeded!"
