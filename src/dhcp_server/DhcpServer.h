/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈ `https://github.com/highstat` 
    Name: DhcpServer 
    Description: Captive Portal DNS controller for setup mode 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "../utils/ConfigStore.h"
#include "../logger/Logger.h"

class DhcpServer {
 public:
  void begin(ConfigStore& cfg, Logger& log);
  void loop();
  void setDnsCaptiveEnabled(bool en) { dnsEnabled = en; }
  bool isDnsCaptiveEnabled() const { return dnsEnabled; }

 private:
  void handleDns();
  WiFiUDP dns;
  ConfigStore* store = nullptr;
  Logger* logger = nullptr;
  bool dnsEnabled = true;
};
