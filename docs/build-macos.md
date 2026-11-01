# Building on macOS

This guide explains how to compile the BW16 Wi-Fi Repeater firmware from source on macOS using `arduino-cli`.

## Prerequisites

1. **Homebrew**:
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. **Arduino CLI**:
   ```bash
   brew install arduino-cli
   ```

3. **Python 3 and required tools**:
   ```bash
   brew install python3
   pip3 install pyserial cryptography
   ```

4. **USB-to-UART driver**:
   Depending on your board or programmer, install the driver for your USB-to-UART chip (CH340, CP2102, or FTDI).

## AmebaD core installation

Initialize the Arduino CLI configuration and add the Realtek AmebaD board index:

```bash
arduino-cli config init
arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/Ameba-AIoT/ameba-arduino-d/master/Arduino_package/package_realtek_amebad_index.json
arduino-cli core update-index
arduino-cli core install realtek:AmebaD
```

Verify that the board is recognized:

```bash
arduino-cli board listall | grep -i AmebaD
```

You should see `realtek:AmebaD:Ai-Thinker_BW16`.

## Enabling repeater mode in AmebaD SDK

The AmebaD LwIP stack includes native IEEE 802.1D bridging, but it must be enabled in the SDK headers:

1. Locate `platform_opts.h` in your local AmebaD package:
   ```bash
   find ~/Library/Arduino15/packages/realtek -name "platform_opts.h" 2>/dev/null
   # Typically at:
   # ~/Library/Arduino15/packages/realtek/hardware/AmebaD/<version>/system/project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h
   ```

2. Ensure that `#define CONFIG_EXAMPLE_WLAN_REPEATER 1` is set:
   ```c
   #define CONFIG_EXAMPLE_WLAN_REPEATER 1
   ```

Note: The included build script `scripts/build.sh` automatically patches this file if needed.

## Compiling the firmware

Run the automated build script:

```bash
bash scripts/build.sh
```

Or compile manually using `arduino-cli`:

```bash
arduino-cli compile --fqbn realtek:AmebaD:Ai-Thinker_BW16 ./
```

After compilation, the combined firmware binary is placed in:
- `build/BW16-Repeater.ino.bin`
- `bin/bw16_repeater_firmware.bin`

## Troubleshooting build issues

### Missing python dependencies
If compilation fails during image generation with `ModuleNotFoundError: No module named 'serial'` or `cryptography`:
```bash
pip3 install pyserial cryptography
```

### AmebaD index 404 error
If `arduino-cli core update-index` reports a 404 error, ensure you are using the `master` branch URL:
`https://raw.githubusercontent.com/Ameba-AIoT/ameba-arduino-d/master/Arduino_package/package_realtek_amebad_index.json`
Do not use the deprecated `dev` branch URL.
