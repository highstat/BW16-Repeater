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
