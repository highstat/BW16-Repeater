/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈ `https://github.com/highstat` 
    Name: BridgeManager 
    Description: L2 IEEE 802.1D transparent bridge manager for AmebaD RTL8720DN
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "BridgeManager.h"
#include "../wifi_manager/WifiManager.h"

void BridgeManager::begin(ConfigStore& cfg, Logger& log, WifiManager& wifi) {
  cfgRef = &cfg;
  logger = &log;
  wifiRef = &wifi;
  bridgeActive = false;
  if (logger) logger->info(F("[BRIDGE] BridgeManager initialized"));
}

bool BridgeManager::enableBridge() {
  if (bridgeActive) return true;
  if (!wifiRef || !wifiRef->staConnected()) {
    if (logger) logger->warn(F("[BRIDGE] Cannot enable: STA not connected"));
    return false;
  }

  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);

  bridgeif_set_mac_init((char*)mac);
  bridgeif_add_port_sta_netif();
  bridgeif_add_port_ap_netif();

  bridgeActive = true;
  if (logger) {
    char macBuf[20];
    snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    logger->info(String(F("[BRIDGE] L2 Bridge ACTIVE (MAC: ")) + macBuf + F(")"));
  }
  return true;
}

void BridgeManager::disableBridge() {
  bridgeActive = false;
  if (logger) logger->info(F("[BRIDGE] Bridge disabled"));
}

void BridgeManager::loop() {
  if (millis() - lastCheck > 3000) {
    lastCheck = millis();
    if (wifiRef && wifiRef->staConnected()) {
      if (!bridgeActive) {
        enableBridge();
      }
    } else {
      if (bridgeActive) {
        bridgeActive = false;
        if (logger) logger->warn(F("[BRIDGE] STA disconnected, bridge suspended"));
      }
    }
  }
}
