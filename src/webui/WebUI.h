/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            `https://github.com/highstat/BW16-Repeater` 
    Name: WebUI 
    Description: Commercial-grade Extender WiFi Settings portal & Dashboard
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#pragma once
#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "../utils/ConfigStore.h"
#include "../logger/Logger.h"
#include "../wifi_manager/WifiManager.h"
#include "../bridge_manager/BridgeManager.h"
#include "../dhcp_server/DhcpServer.h"
#include "../utils/Base64.h"
#include "../utils/Utils.h"

class WebUI {
 public:
  void begin(ConfigStore& cfg, Logger& log, WifiManager& wifi, BridgeManager& bridge, DhcpServer& dhcp);
  void loop();
 private:
  void handleClient(WiFiClient& c);
  void serveMainPage(WiFiClient& c);
  bool authOk(const String& h);
  WiFiServer server{80};
  ConfigStore* store = nullptr;
  Logger* logger = nullptr;
  WifiManager* wifiRef = nullptr;
  BridgeManager* bridgeRef = nullptr;
  DhcpServer* dhcpRef = nullptr;
};
