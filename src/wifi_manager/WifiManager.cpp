/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈ `https://github.com/highstat` 
    Name: WifiManager 
    Description: Dual-Band STA+AP control with channel sync for RTL8720DN
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "WifiManager.h"
#include "../utils/BuildConfig.h"
#include "../utils/Utils.h"
#include "../utils/LedStatus.h"

void WifiManager::begin(ConfigStore& cfg, Logger& log) {
  logger = &log;
  cfgRef = &cfg;
  const auto& c = cfg.get();

  if (!c.staSsid[0]) {
    // First boot or unconfigured -> Setup Mode (SoftAP only)
    setupMode = true;
    LedStatus::set(LedMode::SETUP_PORTAL);
    activeChannel = c.apChannel > 0 ? c.apChannel : 1;
    startAP(c, activeChannel);
    lastMsg = String(F("Setup Mode active (SSID: ")) + c.apSsid + F(")");
    if (logger) logger->info(lastMsg);
  } else {
    // Standard Repeater Mode -> Connect to upstream router
    setupMode = false;
    LedStatus::set(LedMode::CONNECTING);
    startSTA(c);
    lastMsg = String(F("Connecting to upstream: ")) + c.staSsid;
    if (logger) logger->info(lastMsg);
  }
}

void WifiManager::startSTA(const RepeaterConfig& c) {
  if (!c.staSsid[0]) return;
  WiFi.disconnect();
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
  if (logger) {
    logger->info(String(F("[AP] AP started (SSID: ")) + c.apSsid +
                 F(", Channel: ") + String(channel) +
                 (channel > 14 ? F(" [5GHz]") : F(" [2.4GHz]")) +
                 F(", Status: ") + String(st) + F(")"));
  }
  delay(100);
}

void WifiManager::syncChannelAndStartAP() {
  if (!cfgRef) return;
  int ch = 0;
  if (wifi_get_channel(&ch) >= 0 && ch > 0) {
    activeChannel = ch;
  } else {
    activeChannel = cfgRef->get().apChannel > 0 ? cfgRef->get().apChannel : 6;
  }

  if (logger) {
    logger->info(String(F("[WIFI] Upstream STA synced on channel: ")) + String(activeChannel) +
                 (activeChannel > 14 ? F(" (5GHz)") : F(" (2.4GHz)")));
  }

  startAP(cfgRef->get(), activeChannel);
}

void WifiManager::loop() {
  if (millis() - lastCheck > 2500) {
    lastCheck = millis();
    ensureReconnect();
  }
}

void WifiManager::ensureReconnect() {
  if (!cfgRef) return;
  const auto& c = cfgRef->get();

  if (setupMode) {
    // In setup mode, nothing to reconnect
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!apRunning) {
      syncChannelAndStartAP();
    }
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
