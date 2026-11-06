# 0.2.2 (2026-11-01)

- **Asynchronous Non-Blocking Repeater Startup**:
  - SoftAP (`<SSID>-EXT`) and local DHCP server now start immediately on boot in 200 ms without hanging inside `setup()`.
  - Upstream STA connection runs asynchronously in the background. The setup portal and web dashboard are always instantly accessible at `192.168.4.1`.
- **L2 Bridge Deferred Port Registration**:
  - Ports are attached to `bridgeif` before the station starts DHCP; setup mode uses the AP netif directly without bridge ports.
  - Clears the AP port's local address while bridging, so the WLAN driver's IP filter accepts forwarded unicast traffic.
  - Relays client DHCP exchanges to the upstream router and restores local DHCP when upstream connectivity is lost.
- **Proactive Background Wi-Fi Scanner & Cache**:
  - Implemented background scanning on boot; cached network records allow `GET /scan_json` to respond in 1 ms with zero web server blocking.
  - Added polling state (`scanning: true/false`) and manual force refresh (`/scan_json?refresh=1`).
- **Web UI Form & Selection Hardening**:
  - Fixed JavaScript `SyntaxError` in `pickNet()` caused by inline unescaped quotes in `onclick`. Switched to HTML5 `data-ssid` and `data-ch` attributes.
  - Added band filter buttons (`All`, `2.4 GHz Only`, `5 GHz Only`) and channel badges (`Ch X (2.4 GHz / 5 GHz)`).
  - Added automatic channel synchronization when selecting any network.
- **Configurable RGB LED Modes**:
  - Added `ledMode` config (Auto telemetry, Complete Dark/Off, and 7 static colors: Green, Blue, Red, Cyan, Magenta, Yellow, White).
  - Added live `POST /set_led` endpoint and UART serial console command `led <mode>`.
- **5 GHz Channel Documentation**:
  - Clarified UNII-1 (Channels 36–48) full support vs. DFS (52–144) SoftAP restrictions and UNII-3 (149–165) driver limits in `docs/limitations.md`.

# 0.2.1 (2026-11-01)

- **Double-Reset Recovery (DRD)**: Implemented hardware reset detection via AmebaD RTC backup register `BKUP_REG3`. Double-pressing the physical RST button within 3.5 seconds clears NVRAM credentials and reboots into the Setup Portal.
- **Automatic Fallback Supervisor**: Added recovery state machine that automatically resets the AP to `BW16-Repeater` on 2.4 GHz Channel 1 after 3 failed uplink connection attempts (~60 seconds).
- **L2 Bridge DHCP Conflict Resolution**: Added `dhcps_deinit()` when bridge initializes, preventing Ameba internal DHCP server from handing out conflicting IP addresses without default gateways, restoring transparent client Internet access.
- **Bridge Port Stability Guard**: Added port count guard (`get_bridge_portnum() < 2`) to prevent duplicate port registrations on Wi-Fi reconnect cycles.
- **Security Hardening**:
  - HTML entity sanitization (`esc()`) on scanned SSIDs and client MAC addresses to prevent stored XSS.
  - Clamped HTTP header and payload sizes to 2048 bytes with HTTP 413 responses to prevent heap starvation DoS.
  - Fixed 4-argument `WiFi.config()` call for static IP gateway and subnet assignment.
  - Hardened Base64 and flash string buffer bounds with guaranteed null termination.
- **Documentation & License**:
  - Overhauled documentation in `docs/` covering architecture, L2 bridging vs NAT, macOS build, flashing, limitations, and verification test plan.
  - Updated license copyright to `highstat`.

# 0.2.0 (2026-10-31)

- **Extender WiFi Settings UI**: Replaced legacy basic UI with commercial-grade setup wizard matching hardware extenders:
  - Golden/amber header with "WiFi REPEATER" branding.
  - Mode selection screen (Repeater Mode vs. AP Mode) with SVG architecture diagrams.
  - Animated Wi-Fi network scanner modal ("Scanning Access Point, please wait...").
  - Scanned network picker with live RSSI signal bars, 2.4G/5G band tags, and one-click connection.
  - Live Status Dashboard with real-time RSSI, channel, uptime, and connected client management.
- **Hardware RF Power Boost**: Implemented maximum output power index 63 (~20 dBm / 100 mW) via low-level Realtek driver commands.
- **Smart RSSI Steering**: Added anti-sticky client monitoring with remote Deauth/Kick capabilities.
- **L2 Transparent Bridge**: Replaced non-functional NAT skeleton with native Realtek AmebaD IEEE 802.1D bridge (`bridgeif`) and hardware MAC spoofing (`br_rpt_handle`).
- **Dual-Band Channel Sync**: Added automatic detection of upstream router channel (2.4 GHz channels 1-13 & 5 GHz channels 36-48) to synchronize STA and AP on the same radio synthesizer.
- **Captive Portal Fix**: Isolated DNS hijacking to unconfigured / setup mode only (`192.168.4.1`), ensuring internet access flows transparently in bridge mode.
- **RGB LED Telemetry**: Added `LedStatus` driver for onboard RGB LED (Red = Setup, Blue = Connecting, Green = Bridge Active).
- **Automated Build Patch**: Enhanced `scripts/build.sh` to automatically configure `CONFIG_EXAMPLE_WLAN_REPEATER=1` in the AmebaD core.

# 0.1.0 (2025-11-16)

- Initialized BW16 WiFi Repeater project
- Added modules: WiFi, NAT/bridge (skeleton), DHCP (skeleton), BLE (skeleton), Web UI, Logger, Utils
- Build via arduino-cli and CI (GitHub Actions)
- Documentation and scripts
