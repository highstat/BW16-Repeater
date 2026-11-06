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
- **Non-blocking Startup**: Starts the AP interface and local DHCP server immediately on boot. Upstream STA connections run asynchronously in the background so the setup portal and AP are always immediately responsive.
- **Background Scan Cache**: Proactively scans all 2.4 GHz and 5 GHz channels on startup and caches AP records. The `/scan_json` endpoint returns in 1 ms without blocking the web server or dropping client connections.
- **Channel synchronization**: Automatically locks the AP interface to the exact channel of the selected upstream network. Warns against unsupported DFS / high 5 GHz channels (> 48).

### 2. Bridge Manager (`src/bridge_manager/`)
Controls the L2 packet bridge using AmebaD LwIP `bridgeif`:
- **Ordered Port Registration**: Moves the AP to the upstream channel first, then attaches `wlan0` (STA) and `wlan1` (AP) to `bridgeif` before starting the STA DHCP client. This preserves the AP netif input callback while satisfying AmebaD's requirement that both bridge ports exist before DHCP can request an address for `bridgeif`.
- **Bridge-aware Core Override**: The build compiles the related LwIP/network objects together with `CONFIG_EXAMPLE_WLAN_REPEATER=1`. It preserves the vendor 196-byte `netif` layout for the closed WLAN library, provides three `xnetif` entries, and stores bridge port ownership outside `netif.client_data`.
- **Dynamic DHCP Handoff**: Stops the local SoftAP DHCP server before registering bridge ports. When the bridge activates, a DHCP relay forwards client requests to the upstream router and returns its replies. The router supplies the client address, gateway and DNS configuration.
- **Auto-Recovery**: If the upstream Wi-Fi connection is lost, automatically disables the bridge and restores the local AP DHCP server so clients retain connectivity to the device.

### 3. Web UI & Captive Portal (`src/webui/`)
- Lightweight HTTP server listening on TCP port 80.
- DNS captive portal responding to all queries with `192.168.4.1` during unconfigured setup mode.
- Single-page responsive interface with:
  - Band filters (`All`, `2.4 GHz Only`, `5 GHz Only`) with channel badges (`Ch X (2.4 GHz / 5 GHz)`).
  - Robust HTML5 data-attribute network selection (`data-ssid`, `data-ch`) immune to Unicode or quotation syntax breakage.
  - Live Onboard RGB LED selector (`POST /set_led`).
  - Real-time status telemetry and client kick controls.

### 4. Configuration Storage (`src/utils/ConfigStore.cpp`)
- Persists SSID, WPA2 passphrase, extended AP name, radio channel, and LED mode to SPI flash via Ameba `FlashMemory`.
- Verifies checksums and bounds-checks string fields with guaranteed null-terminators.

### 5. LED Status Controller (`src/utils/LedStatus.h`)
Drives the onboard RGB LED with state-based telemetry and user color options:
- **Auto Mode (Default)**:
  - Red blink: Setup portal mode (`BW16-Repeater` active).
  - Blue blink: Connecting to upstream Wi-Fi network.
  - Green solid: L2 bridge active, passing traffic.
- **User Static Modes**: Off (Complete Dark / Sleep), Green, Blue, Red, Cyan, Magenta, Yellow, White.
- Controllable via Web UI dropdown and UART serial CLI (`led <auto|off|green|blue|red|cyan|magenta|yellow|white>`).
