# System Architecture

This document provides a technical overview of the BW16 Wi-Fi Repeater firmware architecture, hardware subsystems, network stack, and state machines.

## Architecture diagram

```text
+------------------------------------------------------------------------+
|              BW16 (RTL8720DN) L2 TRANSPARENT WI-FI REPEATER            |
+------------------------------------------------------------------------+

  [ CLIENTS ]               [ BW16 REPEATER (AmebaD) ]            [ HOME ROUTER ]
+--------------+          +---------------------------+          +-------------------+
| Phone / PC   |          |  Realtek RTL8720DN Module |          | Home Wi-Fi Router |
| 192.168.1.45 |<--Wi-Fi--| wlan1 (AP)   wlan0 (STA)  |--Wi-Fi-->| 192.168.1.1       |
| (Via DHCP)   |  (Ch X)  |     |             |       |  (Ch X)  | (DHCP Server)     |
+--------------+          |     +---bridgeif--+       |          +-------------------+
                          |       (L2 Switch)         |
                          |   AmebaD LwIP Stack       |
                          |   RGB LED + Web UI        |
                          +---------------------------+
```

## Hardware overview

- **System on Chip**: Realtek AmebaD (RTL8720DN).
- **Core 0 (KM4)**: ARM Cortex-M4 @ 200 MHz, handles application logic, Wi-Fi networking, LwIP stack, HTTP server, and storage.
- **Core 1 (KM0)**: ARM Cortex-M0 @ 20 MHz, handles low-level RF baseband, MAC scheduling, and power management.
- **SRAM**: 512 KB total on-chip RAM shared between cores.
- **Flash**: 2 MB SPI NOR flash with wear-leveling NVRAM emulation for configuration storage.
- **Radio**: Dual-band 2.4 GHz (802.11b/g/n) and 5 GHz (802.11a/n) 1T1R.

## Subsystem modules

### 1. Wi-Fi Manager (`src/wifi_manager/`)
Manages radio state transitions, channel synchronization, scanning, and station reconnects:
- **Concurrent STA+AP mode**: Configures `wlan0` as 802.11 station and `wlan1` as 802.11 access point.
- **Channel synchronization**: Automatically queries upstream BSSID channel and configures the AP interface on the identical frequency synthesizer channel.
- **Auto-fallback supervisor**: Tracks consecutive association failures. If 3 consecutive connection attempts fail (~60s), clears stored credentials and resets AP to `BW16-Repeater` on 2.4 GHz Channel 1.

### 2. Bridge Manager (`src/bridge_manager/`)
Controls the L2 packet bridge using AmebaD LwIP `bridgeif`:
- Attaches `wlan0` and `wlan1` netifs to the bridge switch.
- Guards against duplicate port registrations on link flaps.
- Deinitializes the SoftAP DHCP server (`dhcps_deinit()`) once the bridge forms to prevent gateway conflicts.

### 3. Web UI & Captive Portal (`src/webui/`)
- Lightweight HTTP server listening on TCP port 80.
- DNS captive portal responding to all queries with `192.168.4.1` during unconfigured setup mode.
- Single-page interface with responsive layout, scan network picker, status telemetry, and client deauth controls.
- Input sanitization (HTML entity escaping) and request size limiting (2048 byte max header and body).

### 4. Double Reset Detector (DRD) (`src/main.ino`)
- Uses RTC backup register `BKUP_REG3` (`BKUP_Read(3)` / `BKUP_Write(3)`), which persists across soft and physical resets.
- On cold boot, writes magic token `0xD00B1E5E` to register 3.
- If the chip resets again within 3.5 seconds, the magic value is read, triggering a factory reset back to Setup Portal mode.
- After 3.5 seconds of clean operation, register 3 is cleared.

### 5. Configuration Storage (`src/utils/ConfigStore.cpp`)
- Persists SSID, WPA2 passphrase, extended AP name, channel, and mode to SPI flash via Ameba `FlashMemory`.
- Verifies checksums and bounds-checks string fields with guaranteed null-terminators.

### 6. LED Status Controller (`src/utils/LedStatus.h`)
Drives onboard RGB LED with state-based blink patterns:
- Red blink: Setup portal mode (`BW16-Repeater` active).
- Blue blink: Connecting to upstream Wi-Fi network.
- Green solid: L2 bridge active, passing traffic.
