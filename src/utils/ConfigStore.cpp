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
    Name: ConfigStore 
    Description: Persistent configuration in flash for BW16 repeater 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "ConfigStore.h"
#include <FlashMemory.h>
#include <string.h>

static const uint32_t MAGIC = 0xB8162026;

void ConfigStore::begin() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.mode = RepeaterMode::BRIDGE;
  cfg.prefer5g = true;
  strcpy(cfg.apSsid, "BW16-Repeater");
  cfg.apPass[0] = 0;
  cfg.apChannel = 1;
  cfg.apAutoChannel = true;
  strcpy(cfg.webUser, "admin");
  strcpy(cfg.webPass, "admin");
  cfg.apIP = (192<<24)|(168<<16)|(4<<8)|1;
  cfg.apMask = (255<<24)|(255<<16)|(255<<8)|0;
  cfg.apGateway = cfg.apIP;
  cfg.staStatic = false;
  cfg.staIP = 0;
  cfg.staMask = 0;
  cfg.staGateway = 0;
  cfg.txPowerBoost = true;
  cfg.rssiKickEnabled = true;
  cfg.rssiKickThreshold = -76;
}

void ConfigStore::load() {
  begin();
  FlashMemory.read();
  uint32_t magic = 0;
  uint32_t size = 0;
  if (FlashMemory.buf) {
    memcpy(&magic, FlashMemory.buf, 4);
    memcpy(&size, FlashMemory.buf + 4, 4);

    if (magic == MAGIC && size == sizeof(RepeaterConfig)) {
      memcpy(&cfg, FlashMemory.buf + 8, sizeof(cfg));
      cfg.staSsid[sizeof(cfg.staSsid) - 1] = 0;
      cfg.staPass[sizeof(cfg.staPass) - 1] = 0;
      cfg.apSsid[sizeof(cfg.apSsid) - 1] = 0;
      cfg.apPass[sizeof(cfg.apPass) - 1] = 0;
      cfg.webUser[sizeof(cfg.webUser) - 1] = 0;
      cfg.webPass[sizeof(cfg.webPass) - 1] = 0;
    }
  }
}

void ConfigStore::save() {
  FlashMemory.read();
  if (FlashMemory.buf) {
    uint32_t magic = MAGIC;
    uint32_t size = sizeof(RepeaterConfig);
    memcpy(FlashMemory.buf, &magic, 4);
    memcpy(FlashMemory.buf + 4, &size, 4);
    memcpy(FlashMemory.buf + 8, &cfg, sizeof(cfg));
    FlashMemory.update();
  }
}
