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
    Name: WifiManager 
    Description: Dual-Band STA+AP control with channel sync and RF boost for RTL8720DN
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "WifiManager.h"
#include "../bridge_manager/BridgeManager.h"
#include "../utils/BuildConfig.h"
#include "../utils/Utils.h"
#include "../utils/LedStatus.h"

extern "C" {
  #include "lwip/netif.h"
  extern struct netif xnetif[NET_IF_NUM];
  err_t tcpip_input(struct pbuf *p, struct netif *inp);
  err_t bridgeif_tcpip_input(struct pbuf *p, struct netif *inp);
  uint8_t* LwIP_GetIP(struct netif *pnetif);
  int wext_get_mode(const char *ifname, int *mode);
  int wext_get_ssid(const char *ifname, unsigned char *ssid);
}

static const char* netifInputName(netif_input_fn input) {
  if (input == tcpip_input) return "tcpip_input";
  if (input == bridgeif_tcpip_input) return "bridgeif_tcpip_input";
  return input ? "other" : "null";
}

static const char* wlanModeName(int mode) {
  if (mode == RTW_MODE_MASTER) return "AP";
  if (mode == RTW_MODE_INFRA) return "STA";
  return "unknown";
}

static struct netif* resolveApNetif(const char* expectedSsid, uint8_t& index,
                                    int modes[2], bool ssidMatches[2]) {
  static const char* const wlanNames[2] = {"wlan0", "wlan1"};
  struct netif* const netifs[2] = {&xnetif[0], &xnetif[1]};
  struct netif* match = nullptr;
  uint8_t matches = 0;

  for (uint8_t i = 0; i < 2; ++i) {
    modes[i] = -1;
    ssidMatches[i] = false;
    if (wext_get_mode(wlanNames[i], &modes[i]) != 0 || modes[i] != RTW_MODE_MASTER) {
      continue;
    }

    unsigned char ssid[33] = {0};
    if (wext_get_ssid(wlanNames[i], ssid) > 0) {
      ssid[32] = 0;
      ssidMatches[i] = strcmp((const char*)ssid, expectedSsid) == 0;
      if (ssidMatches[i]) {
        match = netifs[i];
        index = i;
        ++matches;
      }
    }
  }

  return matches == 1 ? match : nullptr;
}

static IPAddress netifAddress(const ip4_addr_t* address) {
  return IPAddress(ip4_addr1(address), ip4_addr2(address),
                   ip4_addr3(address), ip4_addr4(address));
}

struct ClientListPayload {
  int count;
  unsigned char mac_list[8][6];
};

static ScannedNetwork s_cachedNets[36];
static int s_cachedCount = 0;
static ScannedNetwork s_incomingNets[36];
static int s_incomingCount = 0;
static volatile bool s_scanInProgress = false;
static volatile bool s_scanDone = false;
static unsigned long s_lastScanStarted = 0;

static rtw_result_t internalScanHandler(rtw_scan_handler_result_t* res) {
  if (res->scan_complete == RTW_TRUE) {
    if (s_incomingCount > 0) {
      s_cachedCount = s_incomingCount;
      for (int i = 0; i < s_cachedCount; i++) {
        s_cachedNets[i] = s_incomingNets[i];
      }
    }
    s_scanInProgress = false;
    s_scanDone = true;
    return RTW_SUCCESS;
  }

  rtw_scan_result_t* rec = &res->ap_details;
  if (rec->SSID.len > 0 && rec->SSID.len <= 32 && s_incomingCount < 36) {
    rec->SSID.val[rec->SSID.len] = 0;
    bool found = false;
    for (int i = 0; i < s_incomingCount; i++) {
      if (strcmp(s_incomingNets[i].ssid, (char*)rec->SSID.val) == 0) {
        found = true;
        if (rec->signal_strength > s_incomingNets[i].rssi) {
          s_incomingNets[i].rssi = rec->signal_strength;
          s_incomingNets[i].channel = (uint8_t)rec->channel;
        }
        break;
      }
    }
    if (!found && s_incomingCount < 36) {
      strncpy(s_incomingNets[s_incomingCount].ssid, (char*)rec->SSID.val, 32);
      s_incomingNets[s_incomingCount].ssid[32] = 0;
      s_incomingNets[s_incomingCount].rssi = rec->signal_strength;
      s_incomingNets[s_incomingCount].channel = (uint8_t)rec->channel;
      s_incomingNets[s_incomingCount].enc = (rec->security == RTW_SECURITY_OPEN) ? 0 : 1;
      s_incomingCount++;
    }
  }
  return RTW_SUCCESS;
}

bool WifiManager::triggerScan() {
  if (s_scanInProgress) {
    if (millis() - s_lastScanStarted < 10000) {
      return true; // Scan already active
    }
    s_scanInProgress = false;
  }

  s_incomingCount = 0;
  s_scanInProgress = true;
  s_scanDone = false;
  s_lastScanStarted = millis();

  if (wifi_scan_networks(internalScanHandler, NULL) != RTW_SUCCESS) {
    s_scanInProgress = false;
    return false;
  }
  return true;
}

bool WifiManager::isScanning() const {
  if (s_scanInProgress && (millis() - s_lastScanStarted > 10000)) {
    s_scanInProgress = false;
  }
  return s_scanInProgress;
}

int WifiManager::getCachedCount() const {
  return s_cachedCount;
}

int WifiManager::getCachedScanResults(ScannedNetwork results[], int maxResults) {
  int count = s_cachedCount < maxResults ? s_cachedCount : maxResults;
  for (int i = 0; i < count; i++) {
    results[i] = s_cachedNets[i];
  }
  return count;
}

int WifiManager::scanNetworks(ScannedNetwork results[], int maxResults) {
  if (!s_scanInProgress && s_cachedCount == 0) {
    triggerScan();
  }

  unsigned long start = millis();
  while (s_scanInProgress && (millis() - start < 8500)) {
    delay(100);
  }

  return getCachedScanResults(results, maxResults);
}

void WifiManager::begin(ConfigStore& cfg, Logger& log, BridgeManager& brg) {
  logger = &log;
  cfgRef = &cfg;
  bridgeRef = &brg;
  const auto& c = cfg.get();

  activeChannel = c.apChannel > 0 ? c.apChannel : 1;

  if (c.mode == RepeaterMode::AP) {
    setupMode = false;
    LedStatus::set(LedMode::BRIDGE_ACTIVE);
    startAP(c, activeChannel);
    lastMsg = String(F("Standalone AP active (SSID: ")) + c.apSsid + F(")");
    if (logger) logger->info(lastMsg);
  } else if (!c.staSsid[0]) {
    setupMode = true;
    LedStatus::set(LedMode::SETUP_PORTAL);
    startAP(c, activeChannel);
    triggerScan();
    lastMsg = String(F("Setup Mode active (SSID: ")) + c.apSsid + F(")");
    if (logger) logger->info(lastMsg);
  } else {
    setupMode = false;
    LedStatus::set(LedMode::CONNECTING);
    // Concurrent STA + AP mode for repeater connecting to upstream router
    WiFi.enableConcurrent();

    // Start AP with local DHCP server active initially (guarantees fast IP acquisition)
    startAP(c, activeChannel);
    triggerScan();

    lastStaTryMs = 0; // Trigger background connection in first loop() iteration
    lastMsg = String(F("Repeater AP active (SSID: ")) + c.apSsid + F("), connecting to: ") + c.staSsid;
    if (logger) logger->info(lastMsg);
  }
}

void WifiManager::applyTxPowerBoost() {
  if (setupMode || (cfgRef && cfgRef->get().mode == RepeaterMode::AP)) return; // Do not boost RF in setup portal mode
  if (cfgRef && cfgRef->get().txPowerBoost) {
    if (wifi_is_up(RTW_STA_INTERFACE)) {
      char cmd[32];
      snprintf(cmd, sizeof(cmd), "txpower patha=%d", 63);
      wext_private_command("wlan0", cmd, 0);
      if (logger) logger->info(F("[RF] Hardware TX power boost applied (63 / ~20 dBm)"));
    }
  }
}

void WifiManager::startSTA(const RepeaterConfig& c) {
  if (!c.staSsid[0]) return;
  if (c.staStatic && c.staIP && c.staMask && c.staGateway) {
    IPAddress ip((c.staIP >> 24) & 255, (c.staIP >> 16) & 255, (c.staIP >> 8) & 255, c.staIP & 255);
    IPAddress gw((c.staGateway >> 24) & 255, (c.staGateway >> 16) & 255, (c.staGateway >> 8) & 255, c.staGateway & 255);
    IPAddress mask((c.staMask >> 24) & 255, (c.staMask >> 16) & 255, (c.staMask >> 8) & 255, c.staMask & 255);
    WiFi.config(ip, gw, gw, mask);
  }
  if (c.staPass[0] && strlen(c.staPass) > 0) {
    WiFi.begin((char*)c.staSsid, c.staPass);
  } else {
    WiFi.begin((char*)c.staSsid);
  }
  auditAP();
  lastStaTryMs = millis();
}

void WifiManager::startAP(const RepeaterConfig& c, int channel) {
  IPAddress ip;
  IPAddress mask;
  if (setupMode || c.mode == RepeaterMode::AP) {
    ip = IPAddress((c.apIP >> 24) & 255, (c.apIP >> 16) & 255, (c.apIP >> 8) & 255, c.apIP & 255);
    mask = IPAddress((c.apMask >> 24) & 255, (c.apMask >> 16) & 255, (c.apMask >> 8) & 255, c.apMask & 255);
  } else {
    ip = IPAddress(192, 168, 4, 1);
    mask = IPAddress(255, 255, 255, 0);
  }

  if (channel <= 0) channel = 1;
  activeChannel = channel;
  String chStr = String(channel);

  // WiFi.apbegin() initializes LwIP and starts its own DHCP server. Capture
  // the resulting netif callbacks before it is reconfigured below.

  int st = 0;
  if (c.apPass[0] && strlen(c.apPass) >= 8) {
    st = WiFi.apbegin((char*)c.apSsid, (char*)c.apPass, (char*)chStr.c_str(), 0);
  } else {
    st = WiFi.apbegin((char*)c.apSsid, (char*)chStr.c_str(), (uint8_t)0);
  }

  int wlanModes[2] = {-1, -1};
  bool ssidMatches[2] = {false, false};
  uint8_t apIndex = 0xff;
  struct netif* apNetif = resolveApNetif(c.apSsid, apIndex, wlanModes, ssidMatches);
  if (!apNetif) {
    if (logger) {
      logger->error(String(F("[NETIF] Cannot uniquely resolve AP netif; wlan0 mode=")) +
                    wlanModeName(wlanModes[0]) + F(" ssidMatch=") + (ssidMatches[0] ? F("yes") : F("no")) +
                    F(", wlan1 mode=") + wlanModeName(wlanModes[1]) + F(" ssidMatch=") +
                    (ssidMatches[1] ? F("yes") : F("no")));
    }
    apRunning = false;
    return;
  }

  const char* apWlanName = apIndex == 0 ? "wlan0" : "wlan1";
  const bool bridgeActive = bridgeRef && bridgeRef->isBridgeActive();
  if (logger) {
    logger->info(String(F("[NETIF] AP owner=")) + apWlanName + F(" netif=r") + String(apIndex) +
                 F("; wlan0=") + wlanModeName(wlanModes[0]) + F("/ssidMatch=") + (ssidMatches[0] ? F("yes") : F("no")) +
                 F(" wlan1=") + wlanModeName(wlanModes[1]) + F("/ssidMatch=") + (ssidMatches[1] ? F("yes") : F("no")) +
                 F(" input-after-apbegin=") + netifInputName(apNetif->input));
  }

  // The build removes Realtek's unconditional pre-bridge RX drop. Until the
  // AP is registered as a bridge port, feed frames to the regular LwIP stack.
  if (!bridgeActive) {
    apNetif->input = tcpip_input;
  }
  netif_set_up(apNetif);
  netif_set_link_up(apNetif);

  ip4_addr_t apAddr;
  ip4_addr_t apMask;
  ip4_addr_t apGateway;
  IP4_ADDR(&apAddr, ip[0], ip[1], ip[2], ip[3]);
  IP4_ADDR(&apMask, mask[0], mask[1], mask[2], mask[3]);
  IP4_ADDR(&apGateway, ip[0], ip[1], ip[2], ip[3]);
  if (bridgeActive) {
    ip4_addr_t zero;
    ip4_addr_set_zero(&zero);
    netif_set_addr(apNetif, &zero, &zero, &zero);
  } else {
    netif_set_addr(apNetif, &apAddr, &apMask, &apGateway);
  }
  // Replace the vendor DHCP instance created inside WiFi.apbegin() only after
  // the active AP netif has its final 192.168.4.1/24 address.
  dhcps_deinit();
  if (!bridgeActive) {
    dhcps_init(apNetif);
  }
  if (logger) {
    IPAddress actualIp = netifAddress(netif_ip4_addr(apNetif));
    IPAddress actualMask = netifAddress(netif_ip4_netmask(apNetif));
    IPAddress actualGateway = netifAddress(netif_ip4_gw(apNetif));
    logger->info(String(F("[NETIF] AP netif=r")) + String(apIndex) + F(" input=") + netifInputName(apNetif->input) +
                 F(" ip=") + ipToStr(actualIp) + F(" mask=") + ipToStr(actualMask) +
                 F(" gw=") + ipToStr(actualGateway) + F(" flags=0x") + String(apNetif->flags, HEX) +
                 F(" up=") + (netif_is_up(apNetif) ? F("yes") : F("no")) +
                 F(" link=") + (netif_is_link_up(apNetif) ? F("yes") : F("no")) +
                 F(" dhcp=") + (!bridgeActive ? F("local") : F("off")));
  }

  auditAP();

  // NOTE: Keep internal DHCP server running initially so clients connecting
  // get 192.168.4.x immediately without hanging. dhcps_deinit() will only be
  // triggered once STA has successfully obtained upstream IP in BridgeManager.

  apRunning = (st == WL_CONNECTED || st == 0 || st == 1);
  if (!setupMode) {
    applyTxPowerBoost();
  }

  if (logger) {
    logger->info(String(F("[AP] AP active (SSID: ")) + c.apSsid +
                 (c.apPass[0] ? F(" [WPA2]") : F(" [OPEN]")) +
                 F(", Channel: ") + String(channel) +
                 (channel > 14 ? F(" [5GHz]") : F(" [2.4GHz]")) +
                 F(", Status: ") + String(st) + F(")"));
  }
}

void WifiManager::syncChannelAndStartAP() {
  if (!cfgRef) return;
  int ch = 0;
  if (wifi_get_channel(&ch) >= 0 && ch > 0 && ch != activeChannel) {
    activeChannel = ch;
    if (logger) {
      logger->info(String(F("[WIFI] Upstream STA synced on channel: ")) + String(activeChannel) +
                   (activeChannel > 14 ? F(" (5GHz)") : F(" (2.4GHz)")));
    }
    startAP(cfgRef->get(), activeChannel);
  }
}

void WifiManager::loop() {
  if (millis() - lastCheck > 2500) {
    lastCheck = millis();
    ensureReconnect();
  }
  if (millis() - lastKickCheck > 5000) {
    lastKickCheck = millis();
    checkSmartKick();
  }
}

void WifiManager::ensureReconnect() {
  if (!cfgRef) return;
  const auto& c = cfgRef->get();

  if (setupMode || (cfgRef && cfgRef->get().mode == RepeaterMode::AP)) return;

  if (!apRunning) {
    startAP(c, activeChannel);
  }

  if (staConnected()) {
    staFailCount = 0;
    syncChannelAndStartAP();
    bool isBridge = bridgeRef ? bridgeRef->isBridgeActive() : false;
    if (isBridge) {
      LedStatus::set(LedMode::BRIDGE_ACTIVE);
      lastMsg = String(F("Connected to ")) + c.staSsid + F(" (L2 Bridge Active)");
    } else {
      LedStatus::set(LedMode::CONNECTING);
      lastMsg = String(F("Connected to ")) + c.staSsid + F(" (Activating Bridge...)");
    }
  } else {
    LedStatus::set(LedMode::CONNECTING);
    if (isScanning()) return;

    int targetChannel = 0;
    for (int i = 0; i < s_cachedCount; i++) {
      if (strcmp(s_cachedNets[i].ssid, c.staSsid) == 0) {
        targetChannel = s_cachedNets[i].channel;
        break;
      }
    }
    if (targetChannel == 0) {
      if (millis() - s_lastScanStarted > 15000) triggerScan();
      lastMsg = String(F("Upstream SSID not found: ")) + c.staSsid;
      return;
    }
    if (targetChannel > 48) {
      lastMsg = String(F("Unsupported upstream channel: ")) + targetChannel;
      if (logger && millis() - lastStaTryMs > 30000) logger->warn(lastMsg);
      lastStaTryMs = millis();
      return;
    }
    if (targetChannel != activeChannel) {
      if (logger) logger->info(String(F("[WIFI] Moving repeater AP to upstream channel ")) + targetChannel);
      startAP(c, targetChannel);
    }

    // Keep retrying in background without erasing stored credentials.
    unsigned long retryDelay = staFailCount == 0 ? 3000 : 30000;
    if (millis() - lastStaTryMs > retryDelay) {
      lastStaTryMs = millis();
      staFailCount++;
      lastMsg = staFailCount > 1
        ? String(F("Upstream connection failed. Check the Wi-Fi password for ")) + c.staSsid + F(".")
        : String(F("Connecting to upstream Wi-Fi ")) + c.staSsid + F("...");
      if (logger) logger->warn(String(F("Connecting STA to ")) + c.staSsid + F(" (attempt ") + String(staFailCount) + F(")"));
      // Stop the local server before making either port visible to the bridge.
      if (bridgeRef && !bridgeRef->initBridgePorts()) {
        bridgeRef->restoreLocalAP();
        lastMsg = F("Bridge port initialization failed");
        if (logger) logger->error(lastMsg);
        return;
      }
      startSTA(c);
      if (bridgeRef && !staConnected()) bridgeRef->restoreLocalAP();
    }
  }
}

int WifiManager::getRouterRssi() {
  if (!staConnected()) return 0;
  int rssi = 0;
  if (wifi_get_rssi(&rssi) >= 0) {
    return rssi;
  }
  return 0;
}

int WifiManager::getAssociatedClients(ConnectedClient clients[], int maxClients) {
  ClientListPayload payload;
  payload.count = 8;
  int ret = wifi_get_associated_client_list(&payload, sizeof(payload));
  if (ret < 0 || payload.count <= 0) return 0;

  int limit = payload.count < maxClients ? payload.count : maxClients;
  for (int i = 0; i < limit; i++) {
    snprintf(clients[i].mac, sizeof(clients[i].mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload.mac_list[i][0], payload.mac_list[i][1], payload.mac_list[i][2],
             payload.mac_list[i][3], payload.mac_list[i][4], payload.mac_list[i][5]);
  }
  return limit;
}

void WifiManager::kickClient(const char* macStr) {
  if (!macStr || strlen(macStr) < 17) return;
  unsigned int m[6];
  if (sscanf(macStr, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
    unsigned char hwaddr[6];
    for (int i = 0; i < 6; i++) hwaddr[i] = (unsigned char)m[i];
    wext_del_station("wlan1", hwaddr);
    if (logger) logger->info(String(F("[STEER] Kicked client: ")) + macStr);
  }
}

void WifiManager::kickAllClients() {
  ConnectedClient clients[8];
  int count = getAssociatedClients(clients, 8);
  for (int i = 0; i < count; i++) {
    kickClient(clients[i].mac);
  }
}

void WifiManager::checkSmartKick() {
  if (!cfgRef || !cfgRef->get().rssiKickEnabled || !apRunning) return;
}

bool WifiManager::staAssociated() const {
  if (setupMode) return false;
  return (wifi_is_connected_to_ap() == 0);
}

bool WifiManager::staConnected() const {
  if (setupMode) return false;
  return staAssociated() && bridgeIP()[0] != 0;
}

IPAddress WifiManager::staIP() const {
  IPAddress br = bridgeIP();
  if (br[0] != 0) return br;
  return WiFi.localIP(0);
}

IPAddress WifiManager::bridgeIP() const {
  uint8_t* ip = LwIP_GetIP(&xnetif[2]);
  if (ip && ip[0] != 0) {
    return IPAddress(ip[0], ip[1], ip[2], ip[3]);
  }
  return IPAddress(0, 0, 0, 0);
}

IPAddress WifiManager::apIP() const {
  if (!cfgRef) return IPAddress(192, 168, 4, 1);
  uint32_t v = cfgRef->get().apIP;
  return IPAddress((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
}

bool WifiManager::auditAP() {
  if (!cfgRef) return false;
  int modes[2] = {-1, -1};
  bool matches[2] = {false, false};
  uint8_t index = 0xff;
  const auto& c = cfgRef->get();
  if (!resolveApNetif(c.apSsid, index, modes, matches)) return false;
  rtw_wifi_setting_t actual = {};
  int result = wifi_get_setting(index == 0 ? "wlan0" : "wlan1", &actual);
  const bool secured = c.apPass[0] != 0;
  const bool securityMatches = actual.security_type ==
      (secured ? RTW_SECURITY_WPA2_AES_PSK : RTW_SECURITY_OPEN);
  const bool credentialMatches = !secured ||
      (strnlen((const char*)actual.password, sizeof(actual.password)) == strlen(c.apPass) &&
       memcmp(actual.password, c.apPass, strlen(c.apPass)) == 0);
  if (logger) {
    logger->info(String(F("[AP-AUDIT] driver-read=")) + (result == 0 ? F("ok") : F("failed")) +
      F(" security-match=") + (securityMatches ? F("yes") : F("no")) +
      F(" credential-match=") + (credentialMatches ? F("yes") : F("no")) +
      F(" channel=") + String(actual.channel));
  }
  volatile unsigned char* secret = actual.password;
  for (size_t i = 0; i < sizeof(actual.password); ++i) secret[i] = 0;
  return result == 0 && securityMatches && credentialMatches;
}

void WifiManager::restartAP() {
  // Rebuild the radio and bridge ports in a known order after AP changes.
  sys_reset();
  while (true) delay(10);
}

void WifiManager::restartSTA() {
  if (cfgRef) {
    setupMode = false;
    staFailCount = 0;
    WiFi.disconnect();
    lastStaTryMs = 0;
    triggerScan();
  }
}
