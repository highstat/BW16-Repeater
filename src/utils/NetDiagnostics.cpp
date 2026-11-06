#include "NetDiagnostics.h"

#ifdef BW16_NET_DIAGNOSTICS
#include <WiFi.h>
#include <sys_api.h>
extern "C" {
#include "wifi_conf.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
extern struct netif xnetif[NET_IF_NUM];
void LwIP_Init(void);
void bw16_net_diag_dump(int reset);
void bridgeif_set_mac_init(char* mac);
void bridgeif_add_port_sta_netif(void);
void bridgeif_add_port_ap_netif(void);
void dhcps_deinit(void);
err_t tcpip_input(struct pbuf* p, struct netif* interface);
err_t tcpip_callback_with_block(void (*callback)(void*), void* context, u8_t block);
}

static WiFiServer diagnosticServer(80);
static char topology;
static bool serverStarted;

static void setStaEthernetFlag(void* arg) {
  if (arg) netif_set_flags(&xnetif[0], NETIF_FLAG_ETHERNET);
  else netif_clear_flags(&xnetif[0], NETIF_FLAG_ETHERNET);
  printf("[DIAG] STA Ethernet flag=%d flags=%02x\n",
      (xnetif[0].flags & NETIF_FLAG_ETHERNET) != 0, xnetif[0].flags);
}

static void disableUnusedBridge(void*) {
  ip4_addr_t zero;
  ip4_addr_set_zero(&zero);
  netif_set_down(&xnetif[2]);
  netif_set_link_down(&xnetif[2]);
  netif_set_addr(&xnetif[2], &zero, &zero, &zero);
  printf("[DIAG] unused bridge disabled\n");
}

static void startDiagnosticDhcp(void* arg) {
  struct netif* interface = static_cast<struct netif*>(arg);
  ip4_addr_t zero;
  ip4_addr_set_zero(&zero);
  netif_set_addr(interface, &zero, &zero, &zero);
  netif_set_default(interface);
  netif_set_up(interface);
  netif_set_link_up(interface);
  if (topology != 'b') {
    interface->input = tcpip_input;
    netif_set_flags(interface, NETIF_FLAG_ETHARP);
  }
  printf("[DIAG] DHCP netif=%c%c result=%d\n", interface->name[0],
         interface->name[1], dhcp_start(interface));
}

bool beginNetDiagnostics(const RepeaterConfig& config) {
  Serial.println("[DIAG] Select within 5s: s=STA c=STA+AP b=bridge p=production");
  unsigned long deadline = millis() + 5000;
  while ((long)(millis() - deadline) < 0) {
    if (Serial.available()) {
      char choice = (char)Serial.read();
      if (choice == 'p') return false;
      if (choice == 's' || choice == 'c' || choice == 'b') {
        topology = choice;
        break;
      }
    }
    delay(10);
  }
  if (!topology) return false;
  printf("[DIAG] topology=%c; no scans, reconnect, RF boost or flash writes\n", topology);
  LwIP_Init();
  if (topology != 'b')
    tcpip_callback_with_block(disableUnusedBridge, nullptr, 1);
  if (wifi_on(topology == 's' ? RTW_MODE_STA : RTW_MODE_STA_AP) < 0) {
    Serial.println("[DIAG] wifi_on failed; send reboot");
    return true;
  }
  if (topology == 'b') {
    bridgeif_set_mac_init((char*)xnetif[0].hwaddr);
    bridgeif_add_port_sta_netif();
    bridgeif_add_port_ap_netif();
  }
  if (topology != 's') {
    int result = wifi_start_ap((char*)config.apSsid,
        config.apPass[0] ? RTW_SECURITY_WPA2_AES_PSK : RTW_SECURITY_OPEN,
        (char*)config.apPass, strlen(config.apSsid), strlen(config.apPass), 6);
    printf("[DIAG] AP channel=6 result=%d\n", result);
  }
  dhcps_deinit();
  int result = wifi_connect((char*)config.staSsid,
      config.staPass[0] ? RTW_SECURITY_WPA2_AES_PSK : RTW_SECURITY_OPEN,
      (char*)config.staPass, strlen(config.staSsid), strlen(config.staPass), -1, NULL);
  printf("[DIAG] STA association result=%d\n", result);
  if (result == RTW_SUCCESS) {
    tcpip_callback_with_block(startDiagnosticDhcp, &xnetif[topology == 'b' ? 2 : 0], 1);
  }
  return true;
}

void loopNetDiagnostics() {
  static unsigned long lastReport;
  struct netif* interface = &xnetif[topology == 'b' ? 2 : 0];
  if (!serverStarted && !ip4_addr_isany_val(*netif_ip4_addr(interface))) {
    diagnosticServer.begin();
    serverStarted = true;
    Serial.println("[DIAG] HTTP server ready");
  }
  if (serverStarted) {
    WiFiClient client = diagnosticServer.available();
    if (client) {
      while (client.available()) client.read();
      client.print("HTTP/1.1 200 OK\r\nContent-Length: 8\r\nConnection: close\r\n\r\nBW16 OK\n");
      client.stop();
    }
  }
  if (millis() - lastReport >= 10000) {
    lastReport = millis();
    for (int i = 0; i < NET_IF_NUM; ++i) {
      struct netif* n = &xnetif[i];
      printf("[DIAG] netif=%d %c%c flags=%02x up=%d link=%d input=%s IP=%s\n",
          i, n->name[0], n->name[1], n->flags, netif_is_up(n), netif_is_link_up(n),
          n->input == tcpip_input ? "tcpip" : "bridge/other", ip4addr_ntoa(netif_ip4_addr(n)));
      printf("[DIAG] netif=%d mask=%s\n", i, ip4addr_ntoa(netif_ip4_netmask(n)));
      printf("[DIAG] netif=%d gateway=%s\n", i, ip4addr_ntoa(netif_ip4_gw(n)));
    }
    bw16_net_diag_dump(0);
  }
  static String command;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (command == "reboot") sys_reset();
      if (command == "stats") bw16_net_diag_dump(0);
      if (command == "clear") bw16_net_diag_dump(1);
      if (topology == 's' && command == "ethernet_off")
        tcpip_callback_with_block(setStaEthernetFlag, nullptr, 1);
      if (topology == 's' && command == "ethernet_on")
        tcpip_callback_with_block(setStaEthernetFlag, (void*)1, 1);
      command = "";
    } else if (command.length() < 16) command += ch;
  }
  delay(1);
}
#else
bool beginNetDiagnostics(const RepeaterConfig&) { return false; }
void loopNetDiagnostics() {}
#endif
