<p align="center">
  <img alt="BW16 Architecture" width="800" src="img/architecture.png">
</p>

<p align="center">
  <a href="https://www.amebaiot.com/en/amebad-bw16/"><img src="https://img.shields.io/badge/Platform-BW16%20RTL8720DN-blue.svg" alt="Platform"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License"></a>
  <img src="https://img.shields.io/badge/Wi--Fi-Dual--Band%202.4G%20%2F%205G-orange.svg" alt="WiFi">
  <img src="https://img.shields.io/badge/Architecture-L2%20Transparent%20Bridge-brightgreen.svg" alt="Bridge">
</p>

# BW16 Dual-Band Wi-Fi Repeater (RTL8720DN)

High-performance, hardware-accelerated Wi-Fi repeater and range extender for the **Ai-Thinker BW16** module (Realtek RTL8720DN dual-core ARM Cortex-M4 @ 200MHz + Cortex-M0 @ 20MHz).

Unlike naive software relays or missing L3 NAT implementations, this project runs on **Realtek's native L2 IEEE 802.1D MAC Bridge (`bridgeif`)** with hardware MAC spoofing (`br_rpt_handle`), delivering wire-speed throughput without double-NAT latency.

---

## ⚡ Key Features

- **True L2 Transparent Bridge (`bridgeif`)**: Connected devices receive IP addresses directly from your home router's DHCP server. No double NAT, full UPnP compatibility, zero port-forwarding issues for security cameras and IoT.
- **Dual-Band 2.4 GHz & 5 GHz Auto-Sync**: Automatically detects the upstream router channel (including 5 GHz channels 36, 40, 44, 48) and locks the repeater AP to the identical radio synthesizer channel.
- **Captive Portal Setup Mode**: If no network is configured or connection is lost, boots an open/protected SoftAP at `http://192.168.4.1/` with a DNS hijacking portal and automatic Wi-Fi network scanner.
- **Hardware RGB LED Telemetry**:
  - 🔴 **Blinking Red**: Setup / Captive Portal mode active (`192.168.4.1`).
  - 🔵 **Blinking Blue**: Connecting / Searching for upstream router.
  - 🟢 **Solid Green**: L2 Bridge ACTIVE & forwarding frames.
- **Embedded Web UI & BLE Configuration**: Fast status monitoring and credential configuration via Web UI (`http://<IP>/`) and BLE service `0xFFF0`.

---

## 📁 Repository Structure

```text
├── BW16-Repeater.ino           # Main Arduino CLI entrypoint
├── src/
│   ├── bridge_manager/         # L2 IEEE 802.1D Bridge & Realtek br_rpt_handle glue
│   ├── wifi_manager/           # Dual-band STA+AP sync & auto-reconnect
│   ├── dhcp_server/            # Captive Portal DNS resolver (192.168.4.1)
│   ├── webui/                  # Responsive Web UI configuration & network scanner
│   ├── ble_config/             # Bluetooth LE (0xFFF0) config service
│   ├── logger/                 # Serial logging subsystem (115200 baud)
│   ├── utils/                  # ConfigStore (FlashMemory), Watchdog, LedStatus
│   └── main.ino                # Application lifecycle and CLI monitor
├── scripts/
│   └── build.sh                # 1-click build script with auto-patching
├── docs/                       # Detailed guides (build, flash, architecture)
└── img/                        # Architecture diagrams
```

---

## 🛠️ Build & Compilation

### Requirements
- [arduino-cli](https://arduino.github.io/arduino-cli/latest/) installed on macOS/Linux/Windows.
- Realtek AmebaD core (`realtek:AmebaD:Ai-Thinker_BW16`).

### 1-Click Build
```bash
bash scripts/build.sh
```

### Manual Compilation
```bash
# Compile with Arduino CLI
arduino-cli compile --fqbn realtek:AmebaD:Ai-Thinker_BW16 ./
```

---

## 🚀 Flashing & Upload

1. Connect the BW16 module via USB-UART adapter (3.3V, TX, RX, GND).
2. Put the BW16 into download mode:
   - Hold the **Burn (Download)** button.
   - Press and release the **RST (Reset)** button.
   - Release the **Burn** button.
3. Flash the firmware:
   ```bash
   arduino-cli upload -p /dev/tty.usbserial-* --fqbn realtek:AmebaD:Ai-Thinker_BW16 ./
   ```

---

## 🌐 First-Time Setup (Captive Portal)

1. Power on the BW16. On first boot, the RGB LED will blink red.
2. Connect your phone or laptop to the Wi-Fi hotspot:
   - **SSID**: `BW16-Repeater`
   - **Password**: `bw16pass` (or open if unconfigured)
3. Open `http://192.168.4.1/` (or wait for the Captive Portal prompt).
   - **Default Web Login**: user `admin`, password `admin`
4. Select your home 2.4 GHz or 5 GHz Wi-Fi network from the scan list, enter your password, and save.
5. The BW16 will reboot, sync channels, turn the LED solid **Green**, and bridge all traffic seamlessly!

---

## 📜 License

MIT License © 2025-2026 [highstat](https://github.com/highstat)
