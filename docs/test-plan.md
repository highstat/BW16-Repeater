# Verification and Test Plan

This document outlines the test cases used to verify the functionality, stability, and security of the BW16 Wi-Fi Repeater firmware.

## Test matrix

| Test ID | Test Category | Description | Expected Outcome |
| :--- | :--- | :--- | :--- |
| TC-01 | Boot & Setup Portal | Power on unconfigured board | AP `BW16-Repeater` appears on 2.4 GHz Ch 1; Web UI reachable at `192.168.4.1` |
| TC-02 | Captive Portal DNS | Connect client and request any external URL | DNS queries resolve to `192.168.4.1`; browser prompts or redirects to setup portal |
| TC-03 | Network Scanning | Click scan button in Web UI | List of surrounding 2.4 GHz and 5 GHz networks displayed with RSSI and channel |
| TC-04 | 2.4 GHz Upstream Connection | Configure 2.4 GHz WPA2 network | Board connects to router; channel matches router; AP changes to configured extended SSID |
| TC-05 | 5 GHz Upstream Connection | Configure 5 GHz WPA2 network | Board connects to 5 GHz router; channel switches to 5 GHz channel; AP operates on 5 GHz |
| TC-06 | L2 Bridge DHCP Verification | Connect client to extended AP | Client receives IP, subnet mask, default gateway, and DNS servers directly from upstream router |
| TC-07 | Internet Access | Ping and load websites from client | Ping to gateway and 8.8.8.8 succeeds; HTTPS websites load without errors |
| TC-08 | Local Subnet Discovery | Ping/discover devices on main LAN | Devices on main router and repeated clients can ping each other; mDNS services visible |
| TC-09 | Double-Reset Recovery (DRD) | Press physical RST twice within 3.5 seconds | Board resets flash configuration and reboots into Setup Portal (`BW16-Repeater`) |
| TC-10 | Automatic Fallback | Power off upstream router | After 3 failed reconnect attempts (~60s), AP resets to `BW16-Repeater` on Ch 1 |
| TC-11 | Web UI Client Kick | Kick connected client from dashboard | Client is deauthenticated; reconnects cleanly or is disconnected |
| TC-12 | Serial CLI Console | Connect via UART at 115200 baud | `help`, `status`, `scan`, `reboot`, and `reset` commands execute properly |
| TC-13 | HTTP Security & Payload Limit | Send oversized HTTP request (> 2048 bytes) | Server rejects with HTTP 413 Payload Too Large; board does not crash |
| TC-14 | XSS Sanitization | Broadcast SSID containing HTML tags | SSID names in Web UI scan table are HTML-escaped; no script execution |

## Execution instructions

### 1. Verification of TC-01 (Setup Portal)
1. Flash firmware and open `python3 monitor.py`.
2. Scan for Wi-Fi networks on a phone or laptop.
3. Verify that `BW16-Repeater` is visible with an open security profile.
4. Connect to `BW16-Repeater` and verify the assigned IP address is `192.168.4.100`.
5. Navigate to `http://192.168.4.1` and verify the setup wizard loads.

### 2. Verification of TC-06 & TC-07 (L2 Bridge & Internet Access)
1. Select your home Wi-Fi in the setup wizard, enter the password, and click Connect & Amplify.
2. Wait for the board to reboot and display the green LED status.
3. Connect your laptop to the extended Wi-Fi network.
4. Check your IP configuration:
   - macOS: `ipconfig getifaddr en0`
   - Linux: `ip addr show`
   - Windows: `ipconfig`
5. Verify that your IP address belongs to your home router's subnet (e.g. `192.168.1.x`), not `192.168.4.x`.
6. Test connectivity:
   ```bash
   ping -c 4 8.8.8.8
   curl -I https://www.google.com
   ```

### 3. Verification of TC-09 (Double-Reset Recovery)
1. With the repeater running in bridge mode, press the physical RST button once.
2. Within 3.5 seconds, press the RST button a second time.
3. Observe serial monitor log:
   ```text
   [DRD] Double reset detected! Resetting to Setup Portal...
   ```
4. Verify that `BW16-Repeater` reappears and settings are reset to factory defaults.
