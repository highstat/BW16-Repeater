#!/usr/bin/env python3
"""Build the AmebaD lwIP objects that must share CONFIG_BRIDGE ABI."""

import argparse
import json
import subprocess
from pathlib import Path


SOURCES = (
    "common/network/lwip/lwip_v2.0.2/src/api/api_lib.c",
    "common/network/lwip/lwip_v2.0.2/src/api/api_msg.c",
    "common/network/lwip/lwip_v2.0.2/port/realtek/freertos/bridgeif.c",
    "common/network/lwip/lwip_v2.0.2/port/realtek/freertos/bridgeif_fdb.c",
    "common/network/lwip/lwip_v2.0.2/src/netif/ppp/auth.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/autoip.c",
    "common/network/lwip/lwip_v2.0.2/port/realtek/freertos/br_rpt_handle.c",
    "common/network/lwip/lwip_v2.0.2/src/core/def.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/dhcp.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/dhcp6.c",
    "common/network/dhcp/dhcps.c",
    "common/network/lwip/lwip_v2.0.2/src/core/dns.c",
    "common/network/lwip/lwip_v2.0.2/src/api/err.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/etharp.c",
    "common/network/lwip/lwip_v2.0.2/src/netif/ethernet.c",
    "common/network/lwip/lwip_v2.0.2/port/realtek/freertos/ethernetif.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/ethip6.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/icmp.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/icmp6.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/igmp.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/inet6.c",
    "common/network/lwip/lwip_v2.0.2/src/core/inet_chksum.c",
    "common/network/lwip/lwip_v2.0.2/src/core/init.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ip.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/ip4.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/ip4_addr.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv4/ip4_frag.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/ip6.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/ip6_addr.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/ip6_frag.c",
    "common/drivers/wlan/realtek/src/osdep/lwip_intf.c",
    "common/api/lwip_netconf.c",
    "common/network/mDNS/mDNSPlatform.c",
    "common/network/lwip/lwip_v2.0.2/src/core/mem.c",
    "common/network/lwip/lwip_v2.0.2/src/core/memp.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/mld6.c",
    "common/network/lwip/lwip_v2.0.2/src/core/ipv6/nd6.c",
    "common/network/lwip/lwip_v2.0.2/src/api/netbuf.c",
    "common/network/lwip/lwip_v2.0.2/src/api/netdb.c",
    "common/network/lwip/lwip_v2.0.2/src/core/netif.c",
    "common/network/lwip/lwip_v2.0.2/src/api/netifapi.c",
    "common/network/lwip/lwip_v2.0.2/src/core/pbuf.c",
    "common/network/lwip/lwip_v2.0.2/src/core/raw.c",
    "common/network/lwip/lwip_v2.0.2/src/apps/sntp/sntp.c",
    "common/network/lwip/lwip_v2.0.2/src/api/sockets.c",
    "common/network/lwip/lwip_v2.0.2/src/core/stats.c",
    "common/network/lwip/lwip_v2.0.2/src/core/sys.c",
    "common/network/lwip/lwip_v2.0.2/src/core/tcp.c",
    "common/network/lwip/lwip_v2.0.2/src/core/tcp_in.c",
    "common/network/lwip/lwip_v2.0.2/src/core/tcp_out.c",
    "common/network/lwip/lwip_v2.0.2/src/api/tcpip.c",
    "common/network/lwip/lwip_v2.0.2/src/core/timeouts.c",
    "common/network/lwip/lwip_v2.0.2/src/core/udp.c",
    "common/network/lwip/lwip_v2.0.2/src/netif/ppp/utils.c",
    "common/api/wifi/wifi_simple_config.c",
    "common/api/network/src/wlan_network.c",
)


def instrument_ethernetif(source_text: str) -> str:
    marker = "void ethernetif_recv(struct netif *netif, int total_len)\n{"
    if source_text.count(marker) != 1:
        raise RuntimeError("Unexpected ethernetif_recv definition")

    helper = r'''volatile unsigned int bw16_rx_counts[2][8];
static unsigned int bw16_arp_count;
static unsigned int bw16_ip_count;

void bw16_net_diag_dump(int reset)
{
    int i, j;
    for (i = 0; i < 2; ++i) {
        printf("[COUNTERS] wlan=%d driver=%d eth=%d down=%d nomem=%d copied=%d broadcast=%d inputerr=%d ipreject=%d\n",
               i, bw16_rx_counts[i][0], bw16_rx_counts[i][1], bw16_rx_counts[i][2],
               bw16_rx_counts[i][3], bw16_rx_counts[i][4], bw16_rx_counts[i][5],
               bw16_rx_counts[i][6], bw16_rx_counts[i][7]);
        if (reset) for (j = 0; j < 8; ++j) bw16_rx_counts[i][j] = 0;
    }
    if (reset) { bw16_arp_count = 0; bw16_ip_count = 0; }
}

static void bw16_trace_ip(const char *direction, struct netif *netif, struct pbuf *p)
{
	unsigned char h[78];
	unsigned int ihl, source_port = 0, destination_port = 0;
	if (bw16_ip_count >= 120 || p->tot_len < 34 ||
	    pbuf_copy_partial(p, h, 34, 0) != 34 ||
	    h[12] != 8 || h[13] != 0 || (h[14] >> 4) != 4) return;
	ihl = (h[14] & 15) * 4;
	if (ihl < 20 || ihl > 60 || p->tot_len < 14 + ihl + 4 ||
	    pbuf_copy_partial(p, h, 14 + ihl + 4, 0) != 14 + ihl + 4) return;
	if (h[23] == 6 || h[23] == 17) {
		source_port = (h[14 + ihl] << 8) | h[15 + ihl];
		destination_port = (h[16 + ihl] << 8) | h[17 + ihl];
	}
	if (source_port == 67 || source_port == 68 ||
	    destination_port == 67 || destination_port == 68) return;
	++bw16_ip_count;
	printf("[IP] %s wlan=%d proto=%d src=%d.%d.%d.%d:%d dst=%d.%d.%d.%d:%d eth=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       direction, netif_get_idx(netif), h[23],
	       h[26],h[27],h[28],h[29],source_port,
	       h[30],h[31],h[32],h[33],destination_port,
	       h[0],h[1],h[2],h[3],h[4],h[5]);
}

static void bw16_trace_arp(const char *direction, struct netif *netif, struct pbuf *p)
{
	unsigned char h[42];
	if (bw16_arp_count >= 100 || pbuf_copy_partial(p, h, sizeof(h), 0) != sizeof(h) ||
	    h[12] != 0x08 || h[13] != 0x06 || h[18] != 6 || h[19] != 4)
		return;
	++bw16_arp_count;
	printf("[ARP] %s wlan=%d op=%d src=%d.%d.%d.%d dst=%d.%d.%d.%d eth=%02x:%02x:%02x:%02x:%02x:%02x esrc=%02x:%02x:%02x:%02x:%02x:%02x sha=%02x:%02x:%02x:%02x:%02x:%02x tha=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       direction, netif_get_idx(netif), (h[20] << 8) | h[21],
	       h[28], h[29], h[30], h[31], h[38], h[39], h[40], h[41],
	       h[0], h[1], h[2], h[3], h[4], h[5],
	       h[6], h[7], h[8], h[9], h[10], h[11],
	       h[22], h[23], h[24], h[25], h[26], h[27],
	       h[32], h[33], h[34], h[35], h[36], h[37]);
}

static int bw16_dhcp_message_type(struct pbuf *p)
{
	unsigned char ethernet_ip[34];
	unsigned char udp_header[8];
	unsigned char options[312];
	unsigned int ip_header_len;
	unsigned int udp_offset;
	unsigned int option_offset;
	unsigned int option_len;
	unsigned int i;
	unsigned int src_port;
	unsigned int dst_port;

	if (p == NULL || p->tot_len < 42 || pbuf_copy_partial(p, ethernet_ip, 34, 0) != 34)
		return 0;
	if (ethernet_ip[12] != 0x08 || ethernet_ip[13] != 0x00 || (ethernet_ip[14] >> 4) != 4)
		return 0;
	ip_header_len = (ethernet_ip[14] & 0x0f) * 4;
	if (ip_header_len < 20 || ip_header_len > 60 || ethernet_ip[23] != 17)
		return 0;
	udp_offset = 14 + ip_header_len;
	if (p->tot_len < udp_offset + 8 || pbuf_copy_partial(p, udp_header, 8, udp_offset) != 8)
		return 0;
	src_port = ((unsigned int)udp_header[0] << 8) | udp_header[1];
	dst_port = ((unsigned int)udp_header[2] << 8) | udp_header[3];
	if (!((src_port == 68 && dst_port == 67) || (src_port == 67 && dst_port == 68)))
		return 0;
	option_offset = udp_offset + 8 + 240;
	if (p->tot_len <= option_offset)
		return 0;
	option_len = p->tot_len - option_offset;
	if (option_len > sizeof(options))
		option_len = sizeof(options);
	if (pbuf_copy_partial(p, options, option_len, option_offset) != option_len)
		return 0;
	for (i = 0; i < option_len;) {
		unsigned int code = options[i];
		if (code == 0) {
			++i;
			continue;
		}
		if (code == 255 || i + 1 >= option_len)
			break;
		if (code == 53 && options[i + 1] >= 1 && i + 2 < option_len)
			return options[i + 2];
		i += options[i + 1] + 2;
	}
	return 0;
}

'''
    checksum_helper = r"""static int bw16_dhcp_checksum(struct pbuf *p)
{
    unsigned char hdr[74];
    unsigned int offset, length;
    ip4_addr_t src, dst;
    struct pbuf *udp;
    int result;
    if (pbuf_copy_partial(p, hdr, 42, 0) != 42) return -1;
    offset = 14 + (hdr[14] & 15) * 4;
    if (offset + 8 > sizeof(hdr) || pbuf_copy_partial(p, hdr, offset + 8, 0) != offset + 8) return -1;
    if (hdr[offset + 6] == 0 && hdr[offset + 7] == 0) return 0;
    length = ((unsigned int)hdr[offset + 4] << 8) | hdr[offset + 5];
    if (length < 8 || offset + length > p->tot_len) return -1;
    memcpy(&src.addr, hdr + 26, 4);
    memcpy(&dst.addr, hdr + 30, 4);
    udp = pbuf_alloc(PBUF_RAW, length, PBUF_RAM);
    if (!udp) return -1;
    pbuf_copy_partial(p, udp->payload, length, offset);
    result = inet_chksum_pseudo(udp, 17, length, &src, &dst) == 0 ? 1 : 2;
    pbuf_free(udp);
    return result;
}

"""
    source_text = '#include "lwip/inet_chksum.h"\nextern volatile unsigned int bw16_rx_counts[2][8];\n' + source_text
    helper += checksum_helper
    source_text = source_text.replace(marker, helper + marker, 1)
    tx_marker = "static err_t low_level_output(struct netif *netif, struct pbuf *p)"
    source_text = source_text.replace(tx_marker, "static void bw16_trace_arp(const char *, struct netif *, struct pbuf *);\nstatic void bw16_trace_ip(const char *, struct netif *, struct pbuf *);\nstatic int bw16_dhcp_message_type(struct pbuf *p);\nstatic int bw16_dhcp_checksum(struct pbuf *p);\n" + tx_marker, 1)
    tx_send = "if (rltk_wlan_send(netif_get_idx(netif), sg_list, sg_len, p->tot_len) == 0)"
    tx_replacement = """bw16_trace_arp("TX", netif, p);
        bw16_trace_ip("TX", netif, p);
        int dhcp_type = bw16_dhcp_message_type(p);
        int checksum = dhcp_type ? bw16_dhcp_checksum(p) : -1;
        int tx_result = rltk_wlan_send(netif_get_idx(netif), sg_list, sg_len, p->tot_len);
        if (dhcp_type) {
            unsigned char hdr[14] = {0};
            pbuf_copy_partial(p, hdr, sizeof(hdr), 0);
            printf("[TX] DHCP type=%d wlan=%d result=%d checksum=%d src=%02x:%02x:%02x:%02x:%02x:%02x len=%d first=%d\\n", dhcp_type, netif_get_idx(netif), tx_result, checksum, hdr[6],hdr[7],hdr[8],hdr[9],hdr[10],hdr[11],p->tot_len,p->len);
        }
        if (tx_result == 0)"""
    if source_text.count(tx_send) != 1:
        raise RuntimeError("Unexpected WLAN transmit dispatch")
    source_text = source_text.replace(tx_send, tx_replacement, 1)


    wlan_guard = """\tif(!rltk_wlan_running(netif_get_idx(netif)))
		return;"""
    if source_text.count(wlan_guard) != 1:
        raise RuntimeError("Unexpected ethernetif WLAN-running guard")
    source_text = source_text.replace(
        wlan_guard,
        """\tint wlan_idx = netif_get_idx(netif);
	if (wlan_idx < 0 || wlan_idx > 1) return;
	++bw16_rx_counts[wlan_idx][1];
	if(!rltk_wlan_running(wlan_idx)) {
		++bw16_rx_counts[wlan_idx][2];
		printf("[RX] dropped: WLAN down netif=%c%c index=%d\\n", netif->name[0], netif->name[1], wlan_idx);
		return;
	}""",
        1,
    )

    allocation = "\tp = pbuf_alloc(PBUF_RAW, total_len, PBUF_POOL);\n\tif (p == NULL) {"
    if allocation not in source_text:
        raise RuntimeError("Unexpected RX allocation")
    source_text = source_text.replace(allocation, allocation + "\n\t\t++bw16_rx_counts[wlan_idx][3];", 1)

    input_tail = """\t// Pass received packet to the interface
	if (ERR_OK != netif->input(p, netif))
		pbuf_free(p);"""
    if source_text.count(input_tail) < 1:
        raise RuntimeError("Unexpected ethernetif input dispatch")
    source_text = source_text.replace(
        input_tail,
        """\t// Pass received packet to the interface
	bw16_trace_arp("RX", netif, p);
	bw16_trace_ip("RX", netif, p);
	++bw16_rx_counts[wlan_idx][4];
	unsigned char destination[6];
	if (pbuf_copy_partial(p, destination, 6, 0) == 6 &&
	    destination[0] == 255 && destination[1] == 255 && destination[2] == 255 &&
	    destination[3] == 255 && destination[4] == 255 && destination[5] == 255)
		++bw16_rx_counts[wlan_idx][5];
	int dhcp_message_type = bw16_dhcp_message_type(p);
	if (dhcp_message_type) printf("[RX-BEGIN] DHCP type=%d wlan=%d len=%d\\n", dhcp_message_type, wlan_idx, p->tot_len);
	err_t input_result = netif->input(p, netif);
	if (dhcp_message_type) {
		printf("[RX] DHCP type=%d netif=%c%c index=%d input=%s result=%d\\n",
			dhcp_message_type, netif->name[0], netif->name[1], wlan_idx,
			netif->input == tcpip_input ? "tcpip_input" : "other", input_result);
	}
	if (ERR_OK != input_result) {
		++bw16_rx_counts[wlan_idx][6];
		pbuf_free(p);
	}""",
        1,
    )
    return source_text


def instrument_lwip_intf(source_text: str) -> str:
    source_text = "extern volatile unsigned int bw16_rx_counts[2][8];\n" + source_text
    marker = "void netif_rx(int idx, unsigned int len)\n{"
    if source_text.count(marker) != 1:
        raise RuntimeError("Unexpected driver RX callback")
    source_text = source_text.replace(marker, marker + "\n\tif (idx >= 0 && idx < 2) ++bw16_rx_counts[idx][0];", 1)
    start = source_text.index("int netif_is_valid_IP(")
    end = source_text.index("\n}", start) + 2
    body = source_text[start:end]
    if body.count("return 0;") != 1:
        raise RuntimeError("Unexpected IP admission filter")
    body = body.replace("return 0;", "if (idx >= 0 && idx < 2) ++bw16_rx_counts[idx][7];\n\treturn 0;", 1)
    return source_text[:start] + body + source_text[end:]


def preserve_bridge_vendor_abi(source_text: str) -> str:
    """Keep Realtek forwarding but store port ownership outside struct netif."""
    marker = "u8_t bridgeif_netif_client_id = 0xff;"
    replacement = """/* The closed WLAN library requires the original netif layout. */
static bridgeif_port_t *bw16_bridge_ports[BRIDGEIF_MAX_PORTS];
static bridgeif_port_t *bw16_bridge_port(struct netif *netif)
{
  unsigned int i;
  for (i = 0; i < BRIDGEIF_MAX_PORTS; ++i) {
    bridgeif_port_t *port = bw16_bridge_ports[i];
    if (port && port->port_netif == netif) return port;
  }
  return NULL;
}
typedef char bw16_vendor_netif_size[(sizeof(struct netif) == 196) ? 1 : -1];
"""
    changes = (
        ("#if LWIP_NUM_NETIF_CLIENT_DATA && CONFIG_BRIDGE", "#if CONFIG_BRIDGE"),
        (marker, replacement),
        ("  if (bridgeif_netif_client_id == 0xFF) {\n"
         "    bridgeif_netif_client_id = netif_alloc_client_data_id();\n  }", ""),
        ("(bridgeif_port_t *)netif_get_client_data(netif, bridgeif_netif_client_id)",
         "bw16_bridge_port(netif)"),
        ("netif_set_client_data(portif, bridgeif_netif_client_id, port);",
         "bw16_bridge_ports[port->port_num] = port;"),
    )
    for before, after in changes:
        if source_text.count(before) != 1:
            raise RuntimeError("Unexpected Realtek bridge port ownership code")
        source_text = source_text.replace(before, after, 1)
    return source_text


def instrument_dhcps(source_text: str) -> str:
    request_marker = "\t\t\toption_message_type = *(option_start + 2); // 2 => code(1)+lenth(1)"
    if source_text.count(request_marker) != 1:
        raise RuntimeError("Unexpected dhcps DHCP message-type parser")
    source_text = source_text.replace(
        request_marker,
        request_marker
        + "\n\t\t\tprintf(\"[DHCPS] request type=%d netif=%c%c\\n\", option_message_type, dhcps_netif->name[0], dhcps_netif->name[1]);",
        1,
    )

    offer_marker = "\t\t\tdhcps_send_offer(udp_packet_buffer);"
    ack_marker = "\t\t\tdhcps_send_ack(udp_packet_buffer);"
    if source_text.count(offer_marker) != 1 or source_text.count(ack_marker) != 1:
        raise RuntimeError("Unexpected dhcps response handlers")
    source_text = source_text.replace(
        offer_marker,
        "\t\t\tprintf(\"[DHCPS] response=OFFER netif=%c%c\\n\", dhcps_netif->name[0], dhcps_netif->name[1]);\n"
        + offer_marker,
        1,
    )
    source_text = source_text.replace(
        ack_marker,
        "\t\t\tprintf(\"[DHCPS] response=ACK netif=%c%c\\n\", dhcps_netif->name[0], dhcps_netif->name[1]);\n"
        + ack_marker,
        1,
    )
    return source_text


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile-commands", required=True, type=Path)
    parser.add_argument("--core-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--diagnostics", action="store_true")
    args = parser.parse_args()

    commands = json.loads(args.compile_commands.read_text())
    template = next(
        entry["arguments"]
        for entry in commands
        if entry["file"].endswith("/bridge_glue.c")
    )
    if template[-2] != "-o":
        raise RuntimeError("Unexpected Arduino compilation command")

    compile_args = template[:-3]
    objects = []
    object_dir = args.output.parent / "bridge-core-objects"
    object_dir.mkdir(parents=True, exist_ok=True)
    component_root = args.core_root / "system/component"

    for relative in SOURCES:
        source = component_root / relative
        if not source.is_file():
            raise FileNotFoundError(source)
        output = object_dir / (source.stem + ".o")
        compile_source = source
        if relative.endswith("/bridgeif.c"):
            compile_source = object_dir / "bridgeif_vendor_abi.c"
            compile_source.write_text(preserve_bridge_vendor_abi(source.read_text()))
        elif relative.endswith("/ethernetif.c"):
            # Realtek's CONFIG_BRIDGE guard drops every RX frame until both
            # bridge ports exist. Setup/AP mode intentionally uses a normal
            # tcpip_input path before a repeater bridge is formed, so retain
            # the normal netif callback and remove only this early drop.
            guard = (
                "#if CONFIG_BRIDGE\n"
                "\tif (get_bridge_portnum() != (NET_IF_NUM - 1)) {\n"
                "\t\t// return if bridge not ready\n"
                "\t\treturn;\n"
                "\t}\n"
                "#endif\n"
            )
            source_text = source.read_text()
            if source_text.count(guard) != 1:
                raise RuntimeError("Unexpected ethernetif bridge guard")
            compile_source = object_dir / "ethernetif_setup_rx.c"
            source_text = source_text.replace(guard, "", 1)
            if args.diagnostics:
                source_text = instrument_ethernetif(source_text)
            compile_source.write_text(source_text)
        elif args.diagnostics and relative.endswith("/lwip_intf.c"):
            compile_source = object_dir / "lwip_intf_diag.c"
            compile_source.write_text(instrument_lwip_intf(source.read_text()))
        elif args.diagnostics and relative == "common/network/dhcp/dhcps.c":
            compile_source = object_dir / "dhcps_setup_diag.c"
            compile_source.write_text(instrument_dhcps(source.read_text()))
        source_args = compile_args
        if compile_source != source:
            source_args = compile_args + [f"-I{source.parent}"]
        subprocess.run(
            source_args + [str(compile_source), "-o", str(output)], check=True
        )
        objects.append(str(output))

    ar = Path(compile_args[0]).with_name("arm-none-eabi-ar")
    subprocess.run([str(ar), "rcs", str(args.output), *objects], check=True)
    print(f"Bridge ABI core: {len(objects)} objects -> {args.output}")


if __name__ == "__main__":
    main()
