# Hardware and Operating Limitations

This document details the physical, radio frequency, and protocol boundaries of the Ai-Thinker BW16 (Realtek RTL8720DN) Wi-Fi repeater.

## Single radio architecture (1T1R)

The Realtek RTL8720DN contains one physical 1-Transmitter 1-Receiver (1T1R) RF transceiver capable of operating in either the 2.4 GHz (802.11b/g/n) or 5 GHz (802.11a/n) band.

### Frequency and channel synchronization
Because there is only one physical synthesizer and antenna switch:
- The Access Point (AP) interface and Station (STA) interface **must** operate on the exact same radio channel and band.
- If the station connects to an upstream 5 GHz router on Channel 36, the repeater AP automatically operates on 5 GHz Channel 36.
- The repeater cannot simultaneously connect to a 5 GHz router while broadcasting a 2.4 GHz AP, or vice versa.
- When the upstream station disconnects or is unconfigured, the AP defaults to 2.4 GHz Channel 1.

### 5 GHz Channel and Regulatory Limits

The RTL8720DN radio supports both 2.4 GHz and 5 GHz, but SoftAP (repeater broadcast) is subject to channel band constraints:

| 5 GHz Band | Channel Range | Station (Connect to Router) | SoftAP (Repeater Broadcast) | Notes |
|:---|:---|:---:|:---:|:---|
| **UNII-1 (Low Band)** | **36, 40, 44, 48** | **Supported** | **Supported** | **Recommended for 5 GHz repeating**. Stable, no radar restrictions. |
| **UNII-2 / DFS (Radar)** | 52 to 144 | Supported (client only) | **Not Supported** | Requires certified DFS Master radar detection. SoftAP cannot broadcast. |
| **UNII-3 (High Band)** | 149 to 165 | Supported (scans & connects) | **Restricted / Unstable** | SoftAP on channels > 48 in AmebaD is restricted by regional domain tables (`ioctl[RTKIOCSIWFREQ]`). |

> [!TIP]
> **Recommended Configurations:**
> 1. **2.4 GHz Repeating (Channels 1–13):** Best wall penetration, full 20 dBm (100 mW) hardware TX power boost, and 100% compatibility with all phones, PCs, and IoT smart devices.
> 2. **5 GHz Repeating (Channels 36–48):** If 5 GHz speed is required, set your home router's 5 GHz channel manually to **36, 40, 44, or 48**. Do not leave it on "Auto" if the router might pick DFS channels (52–144) or UNII-3 (149–165).

### Throughput halving
Single-radio repeating uses time-division multiplexing:
- Packets received from repeated clients on the AP interface must be buffered and retransmitted on the STA interface over the same frequency.
- The maximum effective throughput is approximately half of the raw point-to-point Wi-Fi link rate under equivalent signal conditions.

### Known throughput issue

Measured throughput can be substantially lower than the expected single-radio reduction. In one 2.4 GHz test, a phone achieved approximately 8 Mbps download and 10 Mbps upload through the repeater, compared with 37 Mbps download and 41 Mbps upload directly. These are observations from one setup, not guaranteed performance figures.

The WLAN driver reported buffer allocation failures during traffic; failed transmissions can drop forwarded frames. Increasing the packet buffer pool alone did not improve that test. The complete cause and an effective fix remain under investigation.

## Layer 2 frame handling (802.11 3-address limitation)

Standard IEEE 802.11 client associations use 3-address MAC headers (Source, Destination, BSSID). Unlike 802.11s mesh or 4-address WDS links:
- Standard consumer Wi-Fi routers reject frames containing client MAC addresses that did not associate directly with their BSSID.
- AmebaD handles this in its LwIP bridge layer by using MAC translation / proxy-ARP frame handling (`br_rpt_handle`).
- Under standard network traffic (TCP, UDP, ICMP, DHCP, DNS, HTTP/HTTPS), traffic flows transparently. Some strict enterprise security policies (802.1X with port MAC security) may require explicit authorization.

## Power supply requirements

The RTL8720DN draws:
- ~60-80 mA in idle receiving state.
- Up to 450-500 mA peak current during 802.11n / 802.11a RF transmission bursts.

Poor power regulation, thin USB cables, or weak 3.3V LDOs will cause brownout resets (CPU crash or Wi-Fi reset). Always power the board with a stable 3.3V source rated for at least 500 mA continuous current.

## Concurrent client limits

The repeater firmware limits concurrent SoftAP associations to **8 clients**. This constraint preserves SRAM headroom for LwIP packet buffers (pbufs), TCP reassembly queues, and the Web UI server while avoiding out-of-memory panics.
