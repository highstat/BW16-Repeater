/*
 * Bridge glue is provided by the bridge-aware lwip_netconf.c override built by
 * scripts/build_bridge_core.py. The closed WLAN library directly accesses these
 * offsets: checking only the number of netifs does not protect its binary ABI.
 */
#include <stddef.h>
#include "lwip/netif.h"

_Static_assert(sizeof(struct netif) == 196,
               "Vendor netif ABI mismatch: build with scripts/build.sh");
_Static_assert(offsetof(struct netif, state) == 144,
               "Vendor WLAN netif state offset changed");
_Static_assert(offsetof(struct netif, hwaddr) == 175,
               "Vendor WLAN netif MAC offset changed");
