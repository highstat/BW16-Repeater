# 0.2.0 (2026-10-31)

- **L2 Transparent Bridge**: Replaced non-functional NAT skeleton with native Realtek AmebaD IEEE 802.1D bridge (`bridgeif`) and hardware MAC spoofing (`br_rpt_handle`).
- **Dual-Band Channel Sync**: Added automatic detection of upstream router channel (2.4 GHz channels 1-13 & 5 GHz channels 36-48) to synchronize STA and AP on the same radio synthesizer.
- **Captive Portal Fix**: Isolated DNS hijacking to unconfigured / setup mode only (`192.168.4.1`), ensuring internet access flows transparently in bridge mode.
- **RGB LED Telemetry**: Added `LedStatus` driver for onboard RGB LED (Red = Setup, Blue = Connecting, Green = Bridge Active).
- **Automated Build Patch**: Enhanced `scripts/build.sh` to automatically configure `CONFIG_EXAMPLE_WLAN_REPEATER=1` in the AmebaD core.
- **CI & Arduino CLI Compatibility**: Standardized main sketch as `BW16-Repeater.ino` and updated GitHub Actions CI.

# 0.1.0 (2025-11-16)

- Initialized BW16 WiFi Repeater project
- Added modules: WiFi, NAT/bridge (skeleton), DHCP (skeleton), BLE (skeleton), Web UI, Logger, Utils
- Build via arduino-cli and CI (GitHub Actions)
- Documentation and scripts
