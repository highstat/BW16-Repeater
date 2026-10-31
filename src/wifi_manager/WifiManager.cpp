/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈  
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

void WifiManager::begin(ConfigStore& cfg, Logger& log) {
  logger = &log;
  cfgRef = &cfg;
  const auto& c = cfg.get();

  // Always enable concurrent STA + AP mode (RTW_MODE_STA_AP) on AmebaD RTL8720DN
  WiFi.enableConcurrent();

  applyTxPowerBoost();

  activeChannel = c.apChannel > 0 ? c.apChannel : 1;

  if (!c.staSsid[0]) {
    setupMode = true;
    LedStatus::set(LedMode::SETUP_PORTAL);
    startAP(c, activeChannel);
    lastMsg = String(F("Setup Mode active (SSID: ")) + c.apSsid + F(")");
    if (logger) logger->info(lastMsg);
  } else {
    setupMode = false;
    LedStatus::set(LedMode::CONNECTING);
    // Start AP immediately so the extended network is always accessible
    startAP(c, activeChannel);
    // In parallel, connect STA to upstream router
    startSTA(c);
    lastMsg = String(F("Connecting to upstream: ")) + c.staSsid;
    if (logger) logger->info(lastMsg);
  }
}

void WifiManager::applyTxPowerBoost() {
  if (cfgRef && cfgRef->get().txPowerBoost) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "txpower patha=%d", 63);
    wext_private_command("wlan0", cmd, 0);
    wext_private_command("wlan1", cmd, 0);
    if (logger) logger->info(F("[RF] Maximum hardware TX power applied (63 / ~20 dBm)"));
  }
}

void WifiManager::startSTA(const RepeaterConfig& c) {
  if (!c.staSsid[0]) return;
  // Note: Do NOT call WiFi.disconnect() here, as it can disable concurrent AP mode on wlan1
  if (c.staStatic && c.staIP && c.staMask && c.staGateway) {
    IPAddress ip((c.staIP >> 24) & 255, (c.staIP >> 16) & 255, (c.staIP >> 8) & 255, c.staIP & 255);
    IPAddress gw((c.staGateway >> 24) & 255, (c.staGateway >> 16) & 255, (c.staGateway >> 8) & 255, c.staGateway & 255);
    IPAddress mask((c.staMask >> 24) & 255, (c.staMask >> 16) & 255, (c.staMask >> 8) & 255, c.staMask & 255);
    WiFi.config(ip, gw, mask);
  }
  WiFi.begin((char*)c.staSsid, c.staPass);
  lastStaTryMs = millis();
}

void WifiManager::startAP(const RepeaterConfig& c, int channel) {
  IPAddress ip((c.apIP >> 24) & 255, (c.apIP >> 16) & 255, (c.apIP >> 8) & 255, c.apIP & 255);
  IPAddress gw((c.apGateway >> 24) & 255, (c.apGateway >> 16) & 255, (c.apGateway >> 8) & 255, c.apGateway & 255);
  IPAddress mask((c.apMask >> 24) & 255, (c.apMask >> 16) & 255, (c.apMask >> 8) & 255, c.apMask & 255);
  WiFi.config(ip, gw, mask);

  if (channel <= 0) channel = 1;
  activeChannel = channel;
  String chStr = String(channel);

  int st = 0;
  if (c.apPass[0]) {
    st = WiFi.apbegin((char*)c.apSsid, (char*)c.apPass, (char*)chStr.c_str(), 0);
  } else {
    st = WiFi.apbegin((char*)c.apSsid, (char*)chStr.c_str(), (uint8_t)0);
  }

  apRunning = (st == 0 || st == 1);
  applyTxPowerBoost();

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

  // Make sure AP is running at all times
  if (!apRunning) {
    startAP(c, activeChannel);
  }

  if (WiFi.status() == WL_CONNECTED) {
    syncChannelAndStartAP();
    LedStatus::set(LedMode::BRIDGE_ACTIVE);
    lastMsg = String(F("Connected to ")) + c.staSsid + F(" (IP: ") + ipToStr(WiFi.localIP()) + F(")");
  } else {
    LedStatus::set(LedMode::CONNECTING);
    if (millis() - lastStaTryMs > 8000) {
      lastStaTryMs = millis();
      if (logger) logger->warn(String(F("Reconnecting STA to ")) + c.staSsid);
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

bool WifiManager::staConnected() const {
  if (setupMode) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  IPAddress lip = WiFi.localIP();
  return (lip[0] != 0 || lip[1] != 0 || lip[2] != 0 || lip[3] != 0);
}

IPAddress WifiManager::staIP() const {
  return WiFi.localIP();
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
    startSTA(cfgRef->get());
  }
}
