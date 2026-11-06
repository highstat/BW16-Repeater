/*
╔═╗   ╔═╗ ╔═╗           ╔═╗                   ╔═╗                   ╔═╗     
║ ║   ║ ║ ╚═╝ ╔═══════╗ ║ ╚═════╗   ╔═════╗ ╔═╝ ╚═╗   ╔═══════╗   ╔═╝ ╚═╗   
║ ╚═══╝ ║ ╔═╗ ║ ╔═══╗ ║ ║ ╔═══╗ ║ ╔═╝   ╔═╝ ╚═╗ ╔═╝   ║ ╔═══╗ ║   ╚═╗ ╔═╝   
║ ╔═══╗ ║ ║ ║ ║ ║   ║ ║ ║ ║   ║ ║ ╚═══╗ ╚═╗   ║ ║     ║ ║   ║ ║     ║ ║     
║ ║   ║ ║ ║ ║ ║ ╚═══╝ ║ ║ ║   ║ ║ ╔═══╝ ╔═╝   ║ ╚═══╗ ║ ╚═══╝ ╚═╗   ║ ╚═══╗ 
╚═╝   ╚═╝ ╚═╝ ╚═════╗ ║ ╚═╝   ╚═╝ ╚═════╝     ╚═════╝ ╚═════════╝   ╚═════╝ 
                ╔═══╝ ║                                                     
                ╚═════╝                                                     
                © Copyright 2026 
            `https://github.com/highstat/BW16-Repeater` 
    Name: BridgeManager 
    Description: L2 IEEE 802.1D transparent bridge manager for AmebaD RTL8720DN
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "BridgeManager.h"
#include "DhcpRelay.h"
#include "../wifi_manager/WifiManager.h"

extern "C" {
  #include "lwip/netif.h"
  extern struct netif xnetif[NET_IF_NUM];
  err_t tcpip_input(struct pbuf*, struct netif*);
}

static netif_input_fn bridgeInputs[2] = {nullptr, nullptr};
static ip4_addr_t localApIp, localApMask, localApGateway;
static bool localApAddressSaved = false;

void BridgeManager::begin(ConfigStore& cfg, Logger& log, WifiManager& wifi) {
  cfgRef = &cfg;
  logger = &log;
  wifiRef = &wifi;
  bridgeActive = false;
  if (logger) logger->info(F("[BRIDGE] BridgeManager initialized"));
}

bool BridgeManager::initBridgePorts() {
  // Do not expose the upstream bridge and the local server simultaneously.
  dhcps_deinit();
  uint8_t portCount = get_bridge_portnum();
  bool registeredNow = portCount < 2;
  if (portCount == 0) {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    bridgeif_set_mac_init((char*)mac);
    bridgeif_add_port_sta_netif();
    portCount = get_bridge_portnum();
  }
  if (portCount == 1) {
    bridgeif_add_port_ap_netif();
    portCount = get_bridge_portnum();
  }

  if (portCount != 2) {
    if (logger) logger->error(String(F("[BRIDGE] Port registration failed; count=")) + portCount);
    return false;
  }
  // A bridge port must not retain the setup address. The WLAN driver filters
  // unicast IP against the port address before ethernetif_recv(): with
  // 192.168.4.1 still on AP, packets for the upstream network are discarded.
  if (!ip4_addr_isany(netif_ip4_addr(&xnetif[1]))) {
    ip4_addr_copy(localApIp, *netif_ip4_addr(&xnetif[1]));
    ip4_addr_copy(localApMask, *netif_ip4_netmask(&xnetif[1]));
    ip4_addr_copy(localApGateway, *netif_ip4_gw(&xnetif[1]));
    localApAddressSaved = true;
  }
  ip4_addr_t zero;
  ip4_addr_set_zero(&zero);
  netif_set_addr(&xnetif[1], &zero, &zero, &zero);
  // AP restarts and local DHCP fallback replace the port input callback.
  // Restore the callbacks installed by Realtek without registering ports twice.
  for (uint8_t i = 0; i < 2; ++i) {
    if (registeredNow) bridgeInputs[i] = xnetif[i].input;
    if (bridgeInputs[i]) xnetif[i].input = bridgeInputs[i];
    netif_clear_flags(&xnetif[i], NETIF_FLAG_ETHARP);
  }
  if (registeredNow) {
    ip4_addr_t zero;
    ip4_addr_set_zero(&zero);
    netif_set_addr(&xnetif[2], &zero, &zero, &zero);
  }
  if (registeredNow && logger) logger->info(F("[BRIDGE] STA and AP ports registered"));
  return true;
}

bool BridgeManager::enableBridge() {
  if (bridgeActive) return true;
  if (!wifiRef || !wifiRef->staConnected()) {
    return false;
  }

  if (!initBridgePorts()) return false;

  // Stop internal AP DHCP server so upstream router provides IP and DNS directly to clients
  dhcps_deinit();

  int relayResult = bw16_dhcp_relay_start();
  if (relayResult != 0) {
    if (logger) logger->error(String(F("[RELAY] start failed: ")) + relayResult);
    restoreLocalAP();
    return false;
  }

  bridgeActive = true;
  if (wifiRef) {
    wifiRef->kickAllClients();
  }

  if (logger) {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    char macBuf[20];
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    logger->info(String(F("[BRIDGE] L2 Hardware Bridge ACTIVATED (MAC: ")) + macBuf + F(")"));
  }
  return true;
}

void BridgeManager::restoreLocalAP() {
  bridgeActive = false;
  bw16_dhcp_relay_stop();
  xnetif[1].input = tcpip_input;
  netif_set_flags(&xnetif[1], NETIF_FLAG_ETHARP);
  dhcps_deinit();
  if (localApAddressSaved)
    netif_set_addr(&xnetif[1], &localApIp, &localApMask, &localApGateway);
  dhcps_init(&xnetif[1]);
  if (logger) logger->info(F("[BRIDGE] Bridge standby; local AP DHCP server restored"));
}

void BridgeManager::disableBridge() {
  if (!bridgeActive) return;
  restoreLocalAP();
}

void BridgeManager::loop() {
  if (millis() - lastCheck > 1500) {
    lastCheck = millis();
    if (wifiRef && wifiRef->staConnected()) {
      if (!bridgeActive) {
        enableBridge();
      }
    } else {
      if (bridgeActive) {
        disableBridge();
      }
    }
  }
}
