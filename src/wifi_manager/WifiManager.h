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

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "../utils/ConfigStore.h"
#include "../logger/Logger.h"

extern "C" {
  #include "wifi_constants.h"
  #include "wifi_structures.h"
  #include "wifi_conf.h"
  extern int wifi_get_channel(int *channel);
}

class WifiManager {
 public:
  void begin(ConfigStore& cfg, Logger& log);
  void loop();
  bool staConnected() const;
  IPAddress staIP() const;
  IPAddress apIP() const;
  int getActiveChannel() const { return activeChannel; }
  bool isSetupMode() const { return setupMode; }
  void restartAP();
  void restartSTA();
  String lastStatusMsg() const { return lastMsg; }

 private:
  void startSTA(const RepeaterConfig& c);
  void startAP(const RepeaterConfig& c, int channel);
  void ensureReconnect();
  void syncChannelAndStartAP();

  Logger* logger = nullptr;
  ConfigStore* cfgRef = nullptr;
  unsigned long lastCheck = 0;
  unsigned long lastStaTryMs = 0;
  bool setupMode = false;
  bool apRunning = false;
  int activeChannel = 1;
  String lastMsg;
};
