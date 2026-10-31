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

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "../utils/ConfigStore.h"
#include "../logger/Logger.h"

extern "C" {
  void bridgeif_set_mac_init(char *bmac);
  void bridgeif_add_port_ap_netif(void);
  void bridgeif_add_port_sta_netif(void);
  void dhcps_deinit(void);
  uint8_t LwIP_DHCP(uint8_t idx, uint8_t dhcp_state);
}

class WifiManager;

class BridgeManager {
 public:
  void begin(ConfigStore& cfg, Logger& log, WifiManager& wifi);
  void loop();
  bool enableBridge();
  void disableBridge();
  bool isBridgeActive() const { return bridgeActive; }

 private:
  ConfigStore* cfgRef = nullptr;
  Logger* logger = nullptr;
  WifiManager* wifiRef = nullptr;
  bool bridgeActive = false;
  uint32_t lastCheck = 0;
};
