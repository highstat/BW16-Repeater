/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈ `https://github.com/highstat` 
    Name: BW16 WiFi Repeater 
    Description: High-performance L2 Transparent WiFi repeater for BW16 (RTL8720DN) 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

extern "C" {
  extern int wifi_change_channel_plan(uint8_t channel_plan);
  void wifi_set_country_code(void) {
    wifi_change_channel_plan(0x25);
  }
  uint32_t BKUP_Read(uint32_t DwordIdx);
  void BKUP_Write(uint32_t DwordIdx, uint32_t WriteVal);
}

#define DRD_MAGIC 0xD00B1E5E

#include <sys_api.h>
#include "utils/ConfigStore.h"
#include "logger/Logger.h"
#include "wifi_manager/WifiManager.h"
#include "bridge_manager/BridgeManager.h"
#include "dhcp_server/DhcpServer.h"
#include "ble_config/BleConfig.h"
#include "webui/WebUI.h"
#include "utils/Watchdog.h"
#include "utils/LedStatus.h"
#include "utils/Utils.h"

ConfigStore configStore;
Logger logger;
WifiManager wifi;
BridgeManager bridge;
DhcpServer dhcp;
BleConfig ble;
WebUI webui;
Watchdog wdt;

void setup() {
  Serial.begin(115200);
  LedStatus::begin();
  logger.begin();
  configStore.begin();
  configStore.load();
  wdt.begin();

  // Double Reset Detection (DRD)
  uint32_t rstVal = BKUP_Read(3); // BKUP_REG3
  if (rstVal == DRD_MAGIC) {
    BKUP_Write(3, 0);
    logger.warn(F("[DRD] Physical double-reset detected! Clearing config to Setup Portal..."));
    configStore.mutableRef().staSsid[0] = 0;
    configStore.mutableRef().staPass[0] = 0;
    strncpy(configStore.mutableRef().apSsid, "BW16-Repeater", 63);
    configStore.mutableRef().apPass[0] = 0;
    configStore.mutableRef().apChannel = 1;
    configStore.save();
    for (int i = 0; i < 6; i++) {
      LedStatus::set(LedMode::SETUP_PORTAL);
      delay(80);
      LedStatus::set(LedMode::ALL_OFF);
      delay(80);
    }
  } else {
    BKUP_Write(3, DRD_MAGIC);
  }

  logger.info(F("[INIT] Starting BW16 Repeater..."));
  ble.begin(configStore, logger);
  wifi.begin(configStore, logger);
  dhcp.begin(configStore, logger);
  bridge.begin(configStore, logger, wifi);
  webui.begin(configStore, logger, wifi, bridge, dhcp);
  logger.info(F("[INIT] All subsystems initialized successfully!"));
}

void loop() {
  LedStatus::tick();
  ble.loop();
  webui.loop();
  wifi.loop();
  dhcp.loop();
  bridge.loop();
  wdt.feed();

  // If bridge is active, disable DNS captive portal so internet resolves directly
  dhcp.setDnsCaptiveEnabled(!bridge.isBridgeActive());

  static bool drdCleared = false;
  if (!drdCleared && millis() > 3500) {
    drdCleared = true;
    BKUP_Write(3, 0);
  }

  static unsigned long lastHb = 0;
  if (millis() - lastHb > 10000) {
    lastHb = millis();
    IPAddress curIp = bridge.isBridgeActive() ? wifi.bridgeIP() : wifi.apIP();
    if (curIp[0] == 0) curIp = wifi.apIP();
    logger.info(String(F("[HEARTBEAT] IP: ")) + ipToStr(curIp) + F(" | Bridge: ") + (bridge.isBridgeActive() ? F("ACTIVE") : F("STANDBY")));
  }

  static String ln;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (ln.length() > 0) {
        ln.trim();
        if (ln.startsWith("set_sta ")) {
          int sp = ln.indexOf(' ', 8);
          String ssid = ln.substring(8, sp > 0 ? sp : ln.length());
          String pass = "";
          if (sp > 0) pass = ln.substring(sp + 1);
          strncpy(configStore.mutableRef().staSsid, ssid.c_str(), 63);
          configStore.mutableRef().staSsid[63] = 0;
          strncpy(configStore.mutableRef().staPass, pass.c_str(), 63);
          configStore.mutableRef().staPass[63] = 0;
          configStore.save();
          wifi.restartSTA();
          Serial.println("OK: STA config updated");
        } else if (ln.startsWith("set_ap ")) {
          int p1 = ln.indexOf(' ', 7);
          int p2 = p1 > 0 ? ln.indexOf(' ', p1 + 1) : -1;
          int p3 = p2 > 0 ? ln.indexOf(' ', p2 + 1) : -1;
          String ssid = p1 > 0 ? ln.substring(7, p1) : ln.substring(7);
          String pass = p2 > 0 ? ln.substring(p1 + 1, p2) : "";
          int ch = p3 > 0 ? ln.substring(p2 + 1, p3).toInt() : (p2 > 0 ? ln.substring(p2 + 1).toInt() : configStore.get().apChannel);
          strncpy(configStore.mutableRef().apSsid, ssid.c_str(), 63);
          configStore.mutableRef().apSsid[63] = 0;
          strncpy(configStore.mutableRef().apPass, pass.c_str(), 63);
          configStore.mutableRef().apPass[63] = 0;
          configStore.mutableRef().apChannel = (uint8_t)ch;
          configStore.save();
          wifi.restartAP();
          Serial.println("OK: AP config updated");
        } else if (ln == "status" || ln == "show") {
          Serial.println(String("STA SSID: ") + configStore.get().staSsid + " (Connected: " + (wifi.staConnected() ? "YES" : "NO") + ")");
          Serial.println(String("STA IP: ") + ipToStr(wifi.staIP()));
          Serial.println(String("AP SSID: ") + configStore.get().apSsid + " Channel: " + String(wifi.getActiveChannel()));
          Serial.println(String("L2 Bridge: ") + (bridge.isBridgeActive() ? "ACTIVE" : "INACTIVE"));
        } else if (ln == "scan") {
          Serial.println("Scanning Wi-Fi networks...");
          ScannedNetwork nets[32];
          int n = wifi.scanNetworks(nets, 32);
          Serial.println(String("Found ") + n + " networks:");
          for (int i = 0; i < n; i++) {
            Serial.println(String("  [") + (nets[i].channel > 14 ? "5G" : "2.4G") + " Ch " + nets[i].channel + "] " + nets[i].ssid + " (" + nets[i].rssi + " dBm)");
          }
        } else if (ln == "reset" || ln == "factory") {
          Serial.println("Factory reset: resetting to Setup Portal...");
          configStore.mutableRef().staSsid[0] = 0;
          configStore.mutableRef().staPass[0] = 0;
          strncpy(configStore.mutableRef().apSsid, "BW16-Repeater", 63);
          configStore.mutableRef().apPass[0] = 0;
          configStore.mutableRef().apChannel = 1;
          configStore.save();
          sys_reset();
        } else if (ln == "reboot") {
          Serial.println("Rebooting...");
          sys_reset();
        }
        ln = "";
      }
    } else {
      if (ln.length() < 128) {
        ln += ch;
      }
    }
  }
}
