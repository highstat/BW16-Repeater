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
    Name: Logger 
    Description: NVM-backed event logging 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "Logger.h"
#include <string.h>

static const uint32_t LOG_MAGIC = 0xB81610AA;
static const size_t LOG_MAX = 64;
static LogRecord buf[LOG_MAX];
static size_t head = 0;

void Logger::begin() {
  head = 0;
}

void Logger::write(uint8_t lvl, const String& m) {
  LogRecord r; r.ts = millis(); r.level = lvl; strncpy(r.msg, m.c_str(), sizeof(r.msg)-1); r.msg[sizeof(r.msg)-1]=0;
  buf[head++ % LOG_MAX] = r;
  Serial.println(m);
}

void Logger::info(const String& m) { write(1, m); }
void Logger::warn(const String& m) { write(2, m); }
void Logger::error(const String& m) { write(3, m); }

size_t Logger::read(LogRecord* out, size_t max) {
  size_t n = min(max, LOG_MAX);
  for (size_t i=0;i<n;i++) out[i]=buf[i];
  return n;
}