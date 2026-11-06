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
  extern int wifi_get_rssi(int *pRSSI);
  extern int wext_del_station(const char *ifname, unsigned char* hwaddr);
  extern int wifi_get_associated_client_list(void * client_list_buffer, unsigned short buffer_length);
  extern int wext_private_command(const char *ifname, char *cmd, int show_msg);
  extern int wifi_is_up(rtw_interface_t interface);
  extern int wifi_is_connected_to_ap(void);
  void dhcps_deinit(void);
  void sys_reset(void);
}

struct ConnectedClient {
  char mac[18];
};

struct ScannedNetwork {
  char ssid[33];
  int16_t rssi;
  uint8_t channel;
  uint8_t enc;
};

class BridgeManager;

class WifiManager {
 public:
  void begin(ConfigStore& cfg, Logger& log, BridgeManager& bridge);
  void loop();
  bool staAssociated() const;
  bool staConnected() const;
  IPAddress bridgeIP() const;
  IPAddress staIP() const;
  IPAddress apIP() const;
  int getActiveChannel() const { return activeChannel; }
  int getRouterRssi();
  bool isSetupMode() const { return setupMode; }
  void restartAP();
  bool auditAP();
  void restartSTA();
  String lastStatusMsg() const { return lastMsg; }
  void applyTxPowerBoost();
  int getAssociatedClients(ConnectedClient clients[], int maxClients);
  void kickClient(const char* macStr);
  void kickAllClients();
  int scanNetworks(ScannedNetwork results[], int maxResults);
  bool triggerScan();
  bool isScanning() const;
  int getCachedScanResults(ScannedNetwork results[], int maxResults);
  int getCachedCount() const;

 private:
  void startSTA(const RepeaterConfig& c);
  void startAP(const RepeaterConfig& c, int channel);
  void ensureReconnect();
  void syncChannelAndStartAP();
  void checkSmartKick();

  Logger* logger = nullptr;
  ConfigStore* cfgRef = nullptr;
  BridgeManager* bridgeRef = nullptr;
  unsigned long lastCheck = 0;
  unsigned long lastStaTryMs = 0;
  unsigned long lastKickCheck = 0;
  bool setupMode = false;
  bool apRunning = false;
  int activeChannel = 1;
  int staFailCount = 0;
  String lastMsg;
};
