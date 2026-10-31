#!/usr/bin/env bash
set -euo pipefail
FQBN=${FQBN:-realtek:AmebaD:Ai-Thinker_BW16}

echo "Updating Arduino core indexes..."
arduino-cli core update-index
arduino-cli core install realtek:AmebaD || true

# Enable CONFIG_EXAMPLE_WLAN_REPEATER in installed AmebaD core if not already enabled
for H in \
  "$HOME/Library/Arduino15/packages/realtek/hardware/AmebaD/3.1.9/system/project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h" \
  "$HOME/.arduino15/packages/realtek/hardware/AmebaD/3.1.9/system/project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h"
do
  if [ -f "$H" ]; then
    sed -i.bak 's/#define CONFIG_EXAMPLE_WLAN_REPEATER[[:space:]]*0/#define CONFIG_EXAMPLE_WLAN_REPEATER    1/' "$H" 2>/dev/null || true
    echo "Configured CONFIG_EXAMPLE_WLAN_REPEATER=1 in $H"
  fi
done

echo "Compiling BW16-Repeater for $FQBN..."
arduino-cli compile --fqbn "$FQBN" ./ || {
  echo "Build failed. Verify FQBN. Available AmebaD boards:" >&2
  arduino-cli board listall | sed -n '1p;/realtek:AmebaD/p'
  exit 1
}
echo "Build succeeded!"
