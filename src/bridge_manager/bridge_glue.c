#include "netif/bridgeif.h"
#include "lwip/netif.h"
#include "autoconf.h"

extern struct netif xnetif[];
extern err_t bridgeif_mac_init(struct netif *netif, char *bmac);
extern err_t bridgeif_add_port(struct netif *bridgeif, struct netif *portif);
extern u8_t bridgeif_get_portnum(struct netif *bridgeif);

void bridgeif_set_mac_init(char *bmac)
{
  bridgeif_mac_init(&xnetif[NET_IF_NUM - 1], bmac);
}

void bridgeif_add_port_ap_netif(void)
{
  bridgeif_add_port(&xnetif[NET_IF_NUM - 1], &xnetif[1]);
}

void bridgeif_add_port_sta_netif(void)
{
  bridgeif_add_port(&xnetif[NET_IF_NUM - 1], &xnetif[0]);
}

u8_t get_bridge_portnum(void)
{
  return bridgeif_get_portnum(&xnetif[NET_IF_NUM - 1]);
}
