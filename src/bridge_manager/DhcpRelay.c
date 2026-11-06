#include "DhcpRelay.h"

#include <string.h>
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip_netconf.h"

extern struct netif xnetif[NET_IF_NUM];

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define BOOTP_MIN_LEN 240
#define BOOTP_HOPS 3
#define BOOTP_XID 4
#define BOOTP_GIADDR 24
#define BOOTP_CHADDR 28
#define RELAY_CLIENTS 8
#define RELAY_TIMEOUT_MS 60000U

struct relay_client {
  unsigned char xid[4];
  unsigned char mac[6];
  u32_t last_seen;
  unsigned char used;
};

static struct udp_pcb *relay_pcb;
static struct relay_client clients[RELAY_CLIENTS];

static int valid_bootp(struct pbuf *p, unsigned char header[BOOTP_MIN_LEN]) {
  if (p->tot_len < BOOTP_MIN_LEN || p->tot_len > 1500 ||
      pbuf_copy_partial(p, header, BOOTP_MIN_LEN, 0) != BOOTP_MIN_LEN) return 0;
  return header[1] == 1 && header[2] == 6 &&
      header[236] == 0x63 && header[237] == 0x82 &&
      header[238] == 0x53 && header[239] == 0x63;
}

static unsigned int message_type(struct pbuf *p) {
  unsigned char options[300];
  u16_t count = p->tot_len - BOOTP_MIN_LEN;
  if (count > sizeof(options)) count = sizeof(options);
  if (pbuf_copy_partial(p, options, count, BOOTP_MIN_LEN) != count) return 0;
  for (u16_t i = 0; i < count;) {
    unsigned int code = options[i++];
    if (code == 0) continue;
    if (code == 255 || i >= count) break;
    unsigned int length = options[i++];
    if (i + length > count) break;
    if (code == 53 && length == 1) return options[i];
    i += length;
  }
  return 0;
}

static void remember_client(const unsigned char *header) {
  int slot = -1;
  u32_t oldest = 0;
  u32_t now = sys_now();
  for (int i = 0; i < RELAY_CLIENTS; ++i) {
    if (clients[i].used &&
        memcmp(clients[i].xid, header + BOOTP_XID, 4) == 0 &&
        memcmp(clients[i].mac, header + BOOTP_CHADDR, 6) == 0) {
      slot = i;
      break;
    }
    if (!clients[i].used || (u32_t)(now - clients[i].last_seen) > RELAY_TIMEOUT_MS) {
      slot = i;
      break;
    }
    if (slot < 0 || (u32_t)(now - clients[i].last_seen) > oldest) {
      slot = i;
      oldest = now - clients[i].last_seen;
    }
  }
  if (slot < 0) return;
  memcpy(clients[slot].xid, header + BOOTP_XID, 4);
  memcpy(clients[slot].mac, header + BOOTP_CHADDR, 6);
  clients[slot].last_seen = now;
  clients[slot].used = 1;
}

static int known_client(const unsigned char *header) {
  u32_t now = sys_now();
  for (int i = 0; i < RELAY_CLIENTS; ++i) {
    if (clients[i].used &&
        (u32_t)(now - clients[i].last_seen) <= RELAY_TIMEOUT_MS &&
        memcmp(clients[i].xid, header + BOOTP_XID, 4) == 0 &&
        memcmp(clients[i].mac, header + BOOTP_CHADDR, 6) == 0) return 1;
  }
  return 0;
}

static void receive_packet(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                           const ip_addr_t *source, u16_t source_port) {
  (void)arg;
  if (!p) return;
  unsigned char header[BOOTP_MIN_LEN];
  if (!valid_bootp(p, header)) {
    pbuf_free(p);
    return;
  }

  ip_addr_t bridge_ip;
  ip_addr_t router_ip;
  ip_addr_copy_from_ip4(bridge_ip, *netif_ip4_addr(&xnetif[2]));
  ip_addr_copy_from_ip4(router_ip, *netif_ip4_gw(&xnetif[2]));
  if (ip_addr_isany(&bridge_ip) || ip_addr_isany(&router_ip)) {
    pbuf_free(p);
    return;
  }

  if (header[0] == 1 && source_port == DHCP_CLIENT_PORT &&
      p->if_idx == netif_get_index(&xnetif[1]) && header[BOOTP_HOPS] < 4) {
    struct pbuf *out = pbuf_alloc(PBUF_TRANSPORT, p->tot_len, PBUF_RAM);
    if (out && pbuf_copy(out, p) == ERR_OK) {
      unsigned char hops = header[BOOTP_HOPS] + 1;
      pbuf_take_at(out, &hops, 1, BOOTP_HOPS);
      pbuf_take_at(out, &ip_2_ip4(&bridge_ip)->addr, 4, BOOTP_GIADDR);
      remember_client(header);
      err_t result = udp_sendto_if_src(pcb, out, &router_ip,
                                       DHCP_SERVER_PORT, &xnetif[2], &bridge_ip);
      printf("[RELAY] request type=%d result=%d\n", message_type(p), result);
    }
    if (out) pbuf_free(out);
  } else if (header[0] == 2 && source_port == DHCP_SERVER_PORT &&
             ip_addr_cmp(source, &router_ip) && known_client(header)) {
    struct pbuf *out = pbuf_alloc(PBUF_TRANSPORT, p->tot_len, PBUF_RAM);
    if (out && pbuf_copy(out, p) == ERR_OK) {
      err_t result = udp_sendto_if_src(pcb, out, IP_ADDR_BROADCAST,
                                       DHCP_CLIENT_PORT, &xnetif[1], &bridge_ip);
      printf("[RELAY] response type=%d result=%d\n", message_type(p), result);
    }
    if (out) pbuf_free(out);
  }
  pbuf_free(p);
}

static err_t start_on_tcpip(struct tcpip_api_call_data *unused) {
  (void)unused;
  if (relay_pcb) return ERR_OK;
  relay_pcb = udp_new();
  if (!relay_pcb) return ERR_MEM;
  ip_set_option(relay_pcb, SOF_BROADCAST);
  err_t result = udp_bind(relay_pcb, IP_ADDR_ANY, DHCP_SERVER_PORT);
  if (result != ERR_OK) {
    udp_remove(relay_pcb);
    relay_pcb = NULL;
    return result;
  }
  memset(clients, 0, sizeof(clients));
  udp_recv(relay_pcb, receive_packet, NULL);
  return ERR_OK;
}

int bw16_dhcp_relay_start(void) {
  struct tcpip_api_call_data call;
  return tcpip_api_call(start_on_tcpip, &call);
}

static err_t stop_on_tcpip(struct tcpip_api_call_data *unused) {
  (void)unused;
  if (relay_pcb) {
    udp_remove(relay_pcb);
    relay_pcb = NULL;
  }
  memset(clients, 0, sizeof(clients));
  return ERR_OK;
}

void bw16_dhcp_relay_stop(void) {
  struct tcpip_api_call_data call;
  tcpip_api_call(stop_on_tcpip, &call);
}
