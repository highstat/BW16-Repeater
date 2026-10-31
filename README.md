# BW16 Wi-Fi Repeater

<p align="center">
  <img alt="BW16 Architecture" width="800" src="img/architecture.png">
</p>

A transparent layer 2 Wi-Fi repeater for the Ai-Thinker BW16 board (Realtek RTL8720DN). It connects to an existing 2.4 GHz or 5 GHz network and rebroadcasts it without double NAT or subnet isolation. Connected devices receive IP addresses directly from your main router.

## Hardware

- Board: Ai-Thinker BW16 (RTL8720DN)
- CPU: Realtek AmebaD (ARM Cortex-M4 @ 200 MHz, Cortex-M0 @ 20 MHz)
- Radio: Dual-band 2.4 GHz (802.11b/g/n) and 5 GHz (802.11a/n) 1T1R

## Quick start

1. Flash the firmware or use the prebuilt binary in `bin/bw16_repeater_firmware.bin`.
2. Power on the board.
3. Connect your computer or phone to the open Wi-Fi network named `BW16-Repeater`.
4. Open `http://192.168.4.1` in your browser.
5. Select your home network from the scan list, enter the Wi-Fi password, and click Connect & Amplify.
6. The repeater applies settings, restarts, connects to your router, and starts the extended network. Connect your devices to the new extended network name shown on the screen.

## How to return to setup

If you entered the wrong password, move the repeater out of router range, or need to reconfigure it, use any of these methods:

### Double-tap reset button
Press the physical RST button on the board twice within three seconds. The repeater erases stored Wi-Fi credentials and restarts into setup mode with the `BW16-Repeater` network.

### Automatic fallback
If the repeater cannot reach the configured router after three attempts (around 60 seconds), it automatically clears invalid credentials and returns to setup mode.

### Web interface
On `http://192.168.4.1`, open the dashboard and click Reset to Setup Portal.

### Serial console
Connect via serial monitor at 115200 baud and send the command `reset`.

## Status LED

The onboard RGB LED indicates the current state:

- Blinking red: setup mode active (`BW16-Repeater` at `192.168.4.1`).
- Blinking blue: connecting to upstream Wi-Fi.
- Solid green: repeater bridge active, client traffic forwarding.

## How it works

The firmware enables concurrent station and access point mode on the RTL8720DN radio. Once associated with the upstream router, the AmebaD kernel activates the IEEE 802.1D bridge (`bridgeif`) across `wlan0` (station) and `wlan1` (access point), disables the internal DHCP server, and passes Ethernet frames transparently.

Because the bridge operates at layer 2:
- Clients get their IP, default gateway, and DNS servers straight from your main router.
- Local network devices, printers, Home Assistant, and IP cameras remain visible across both networks.
- UPnP, mDNS, and broadcast discovery work without configuration.

Note on channel selection: The RTL8720DN uses a single radio transceiver. The repeater AP runs on the exact same radio channel and band as the upstream router you connect to.

## Building from source

### Prerequisites

Install Arduino CLI and the AmebaD core:

```bash
arduino-cli config init
arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/Ameba-AIoT/ameba-arduino-d/master/Arduino_package/package_realtek_amebad_index.json
arduino-cli core update-index
arduino-cli core install realtek:AmebaD
```

Enable repeater support in `platform_opts.h`:
Set `#define CONFIG_EXAMPLE_WLAN_REPEATER 1` in:
`~/.arduino15/packages/realtek/hardware/AmebaD/3.1.9/system/project/realtek_amebaD_va0_example/inc/inc_hp/platform_opts.h`

### Build

Run the build script:

```bash
bash scripts/build.sh
```

Or compile directly:

```bash
arduino-cli compile --fqbn realtek:AmebaD:Ai-Thinker_BW16 ./
```

Compiled binaries are placed in `build/` and `bin/`.

## Flashing

To flash the board over USB:

1. Connect the BW16 to your computer with a USB-UART adapter.
2. Put the board into download mode:
   - Hold the BURN button.
   - Press and release the RST button.
   - Release the BURN button.
3. Run the flasher:

```bash
python3 do_flash.py
```

The script detects the serial port, uploads the image at 1.5 Mbaud, and resets the board.

## License

MIT
