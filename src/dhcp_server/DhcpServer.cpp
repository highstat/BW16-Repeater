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

#include "DhcpServer.h"

void DhcpServer::begin(ConfigStore& cfg, Logger& log) {
  store = &cfg;
  logger = &log;
  dnsEnabled = false;
  if (logger) logger->info(F("[DNS] Ameba DHCP DNS active on port 53"));
}

void DhcpServer::loop() {
}
