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
#include "../utils/BuildConfig.h"
#include "../utils/Utils.h"
#include "../utils/LedStatus.h"

struct ClientListPayload {
  int count;
  unsigned char mac_list[8][6];
};

static ScannedNetwork* s_scanList = nullptr;
static int s_scanCount = 0;
static int s_scanMax = 0;
static volatile bool s_scanDone = false;

static rtw_result_t internalScanHandler(rtw_scan_handler_result_t* res) {
  if (res->scan_complete == RTW_TRUE) {
    s_scanDone = true;
    return RTW_SUCCESS;
  }
  rtw_scan_result_t* rec = &res->ap_details;
  if (rec->SSID.len > 0 && rec->SSID.len <= 32 && s_scanList && s_scanCount < s_scanMax) {
    rec->SSID.val[rec->SSID.len] = 0;
    bool found = false;
    for (int i = 0; i < s_scanCount; i++) {
      if (strcmp(s_scanList[i].ssid, (char*)rec->SSID.val) == 0) {
        found = true;
        if (rec->signal_strength > s_scanList[i].rssi) {
          s_scanList[i].rssi = rec->signal_strength;
          s_scanList[i].channel = (uint8_t)rec->channel;
        }
        break;
      }
    }
    if (!found && s_scanCount < s_scanMax) {
      strncpy(s_scanList[s_scanCount].ssid, (char*)rec->SSID.val, 32);
      s_scanList[s_scanCount].ssid[32] = 0;
      s_scanList[s_scanCount].rssi = rec->signal_strength;
      s_scanList[s_scanCount].channel = (uint8_t)rec->channel;
      s_scanList[s_scanCount].enc = (rec->security == RTW_SECURITY_OPEN) ? 0 : 1;
      s_scanCount++;
    }
  }
  return RTW_SUCCESS;
}

int WifiManager::scanNetworks(ScannedNetwork results[], int maxResults) {
  s_scanList = results;
  s_scanCount = 0;
  s_scanMax = maxResults;
  s_scanDone = false;
  
  if (wifi_scan_networks(internalScanHandler, NULL) != RTW_SUCCESS) {
    s_scanList = nullptr;
    return 0;
  }
  
  unsigned long start = millis();
  while (!s_scanDone && (millis() - start < 4000)) {
    delay(100);
  }
  
  int count = s_scanCount;
  s_scanList = nullptr;
  return count;
}

void WifiManager::begin(ConfigStore& cfg, Logger& log) {
  logger = &log;
  cfgRef = &cfg;
  const auto& c = cfg.get();

  activeChannel = c.apChannel > 0 ? c.apChannel : 1;

  if (!c.staSsid[0]) {
    setupMode = true;
    LedStatus::set(LedMode::SETUP_PORTAL);
    // Pure AP Mode on wlan0: default netif, robust HTTP server & DHCP
    startAP(c, activeChannel);
    lastMsg = String(F("Setup Mode active (SSID: ")) + c.apSsid + F(")");
    if (logger) logger->info(lastMsg);
  } else {
    setupMode = false;
    LedStatus::set(LedMode::CONNECTING);
    // Concurrent STA + AP mode for active repeater
    WiFi.enableConcurrent();
    
    // Start AP on the configured channel
    startAP(c, activeChannel);
    
    // Allow AP beacons & netif to settle cleanly before initiating STA scan
    delay(2500);
    
    // Start STA connection to upstream router on the exact same channel
    startSTA(c);
    lastMsg = String(F("Connecting to upstream: ")) + c.staSsid;
    if (logger) logger->info(lastMsg);
  }
}

void WifiManager::applyTxPowerBoost() {
  if (setupMode) return; // Do not boost RF in setup portal mode
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
  WiFi.begin((char*)c.staSsid, c.staPass);
  lastStaTryMs = millis();
}

void WifiManager::startAP(const RepeaterConfig& c, int channel) {
  IPAddress ip((c.apIP >> 24) & 255, (c.apIP >> 16) & 255, (c.apIP >> 8) & 255, c.apIP & 255);
  IPAddress gw((c.apGateway >> 24) & 255, (c.apGateway >> 16) & 255, (c.apGateway >> 8) & 255, c.apGateway & 255);
  IPAddress mask((c.apMask >> 24) & 255, (c.apMask >> 16) & 255, (c.apMask >> 8) & 255, c.apMask & 255);
  WiFi.config(ip, ip, ip, mask);

  if (channel <= 0) channel = 1;
  activeChannel = channel;
  String chStr = String(channel);

  int st = 0;
  if (c.apPass[0] && strlen(c.apPass) >= 8) {
    st = WiFi.apbegin((char*)c.apSsid, (char*)c.apPass, (char*)chStr.c_str(), 0);
  } else {
    st = WiFi.apbegin((char*)c.apSsid, (char*)chStr.c_str(), (uint8_t)0);
  }

  apRunning = (st == WL_CONNECTED || st == 0 || st == 1);
  if (!setupMode) {
    applyTxPowerBoost();
  }

  if (logger) {
    logger->info(String(F("[AP] AP active (SSID: ")) + c.apSsid +
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

  if (setupMode) return;

  if (!apRunning) {
    startAP(c, activeChannel);
  }

  if (wifi_is_connected_to_ap() == 0) {
    staFailCount = 0;
    syncChannelAndStartAP();
    LedStatus::set(LedMode::BRIDGE_ACTIVE);
    lastMsg = String(F("Connected to ")) + c.staSsid + F(" (L2 Bridge Active)");
  } else {
    LedStatus::set(LedMode::CONNECTING);
    // Non-aggressive, gentle retry every 20s so AP beacons remain stable
    if (millis() - lastStaTryMs > 20000) {
      lastStaTryMs = millis();
      staFailCount++;
      if (logger) logger->warn(String(F("Reconnecting STA to ")) + c.staSsid + F(" (attempt ") + String(staFailCount) + F("/3)"));
      
      if (staFailCount >= 3) {
        if (logger) logger->error(F("Cannot reach upstream router after 3 attempts. Reverting to Setup Portal (BW16-Repeater)."));
        cfgRef->mutableRef().staSsid[0] = 0;
        cfgRef->mutableRef().staPass[0] = 0;
        strncpy(cfgRef->mutableRef().apSsid, "BW16-Repeater", 63);
        cfgRef->mutableRef().apPass[0] = 0;
        cfgRef->mutableRef().apChannel = 1;
        cfgRef->save();

        setupMode = true;
        staFailCount = 0;
        LedStatus::set(LedMode::SETUP_PORTAL);
        delay(200);
        sys_reset();
        return;
      }
      
      WiFi.begin((char*)c.staSsid, c.staPass);
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

void WifiManager::checkSmartKick() {
  if (!cfgRef || !cfgRef->get().rssiKickEnabled || !apRunning) return;
}

bool WifiManager::staAssociated() const {
  if (setupMode) return false;
  return (wifi_is_connected_to_ap() == 0);
}

bool WifiManager::staConnected() const {
  if (setupMode) return false;
  return staAssociated();
}

IPAddress WifiManager::staIP() const {
  return WiFi.localIP(0);
}

IPAddress WifiManager::bridgeIP() const {
  return WiFi.localIP(2);
}

IPAddress WifiManager::apIP() const {
  if (!cfgRef) return IPAddress(192, 168, 4, 1);
  uint32_t v = cfgRef->get().apIP;
  return IPAddress((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
}

void WifiManager::restartAP() {
  if (cfgRef) startAP(cfgRef->get(), activeChannel);
}

void WifiManager::restartSTA() {
  if (cfgRef) {
    setupMode = false;
    staFailCount = 0;
    startSTA(cfgRef->get());
  }
}
