# Layer 2 Transparent Bridge vs Layer 3 NAT

This document explains the network architecture of the BW16 Wi-Fi Repeater and why Layer 2 (L2) transparent bridging is implemented instead of Layer 3 (L3) Network Address Translation (NAT).

## Architecture comparison

| Parameter | Layer 2 Bridge (`CONFIG_BRIDGE`) | Layer 3 NAT (Router Mode) |
| :--- | :--- | :--- |
| Subnet topology | Single shared subnet with upstream router | Separate isolated private subnet |
| DHCP server | Upstream router issues all leases | Repeater runs local DHCP server |
| Client IP addresses | Assigned from upstream pool (e.g. `192.168.1.x`) | Assigned from private pool (e.g. `192.168.4.x`) |
| Local device discovery | Works natively (mDNS, Bonjour, SSDP, DLNA) | Broken across subnets without mDNS relay |
| Gaming and P2P | Open / Moderate NAT (no double NAT) | Strict NAT (double NAT penalty) |
| Port forwarding | Configured once on main router | Requires cascaded port forwarding |
| CPU overhead | Minimal (hardware MAC frame forwarding) | Higher (software connection tracking and rewriting) |
| Memory footprint | Low (static MAC forwarding table) | High (stateful conntrack session tables) |

## Why L2 bridging is used

### 1. Zero double NAT
In NAT mode, repeating creates a secondary network behind the main router. This breaks gaming consoles, VoIP services, and peer-to-peer protocols that require incoming connections. L2 bridging keeps all repeated devices on the primary local area network.

### 2. Seamless local resource access
With a single broadcast domain, devices connected to the repeater can discover and communicate with resources connected to the main router without complex routing:
- Network printers (AirPrint, CUPS).
- Media streaming (Chromecast, Apple AirPlay, DLNA).
- Home automation servers (Home Assistant, ESPHome, MQTT brokers, Philips Hue).
- Local file shares (SMB, NFS).

### 3. Reduced CPU and memory usage
The Realtek RTL8720DN module features an ARM Cortex-M4 CPU running at 200 MHz and 512 KB of internal SRAM. Managing stateful NAT translation tables for multiple high-bandwidth devices consumes RAM and CPU cycles. L2 bridging forwards Ethernet frames via AmebaD LwIP `bridgeif` with minimal CPU overhead.

## AmebaD L2 bridge implementation details

The AmebaD SDK provides an IEEE 802.1D bridge implementation (`bridgeif`) in its LwIP stack:

1. **Port registration**:
   `wlan0` (station interface connected to the home router) and `wlan1` (access point interface serving repeated clients) are registered as switch ports in `bridgeif`.
2. **Bridge port limit**:
   `bridgeif_add_port()` is guarded against duplicate additions (`get_bridge_portnum() < 2`) to ensure stability during Wi-Fi reconnect cycles.
3. **DHCP server deinitialization**:
   When switching from setup mode to bridge mode, the internal SoftAP DHCP server is stopped via `dhcps_deinit()`. This prevents dual-DHCP conflicts where the repeater assigns a temporary `192.168.4.x` address without a default gateway, which would cause clients to report "No Internet Access".
4. **Transparent frame forwarding**:
   Ethernet frames received on `wlan1` destined for the Internet or upstream LAN are forwarded out through `wlan0` using AmebaD MAC forwarding (`br_rpt_handle`).
