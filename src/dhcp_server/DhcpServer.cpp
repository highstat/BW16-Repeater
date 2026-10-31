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
  dnsEnabled = true;
  dns.begin(53);
  if (logger) logger->info(F("[DNS] Captive Portal DNS server started on port 53"));
}

void DhcpServer::loop() {
  if (dnsEnabled) {
    handleDns();
  }
}

void DhcpServer::handleDns() {
  int p = dns.parsePacket();
  if (p <= 0) return;
  uint8_t buf[512];
  int len = p > 512 ? 512 : p;
  int r = dns.read(buf, len);
  if (r <= 12) return;

  int pos = 12;
  while (pos < r && buf[pos] != 0) { pos++; }
  if (pos >= r) return;
  pos++;
  if (pos + 4 > r) return;

  uint32_t rawIp = store ? store->get().apIP : ((192 << 24) | (168 << 16) | (4 << 8) | 1);
  IPAddress ip((rawIp >> 24) & 255, (rawIp >> 16) & 255, (rawIp >> 8) & 255, rawIp & 255);

  uint8_t ans[512];
  int a = 0;
  ans[a++] = buf[0]; ans[a++] = buf[1]; // ID
  ans[a++] = 0x81; ans[a++] = 0x80;     // Flags: Response, Opcode 0, Auth 0, Trunc 0, RD 1, RA 1
  ans[a++] = buf[4]; ans[a++] = buf[5]; // QDCOUNT
  ans[a++] = 0x00; ans[a++] = 0x01;     // ANCOUNT = 1
  ans[a++] = 0x00; ans[a++] = 0x00;     // NSCOUNT = 0
  ans[a++] = 0x00; ans[a++] = 0x00;     // ARCOUNT = 0

  for (int i = 12; i < pos + 4 && i < r; i++) { ans[a++] = buf[i]; }

  ans[a++] = 0xC0; ans[a++] = 0x0C;     // Pointer to Question name
  ans[a++] = 0x00; ans[a++] = 0x01;     // Type A
  ans[a++] = 0x00; ans[a++] = 0x01;     // Class IN
  ans[a++] = 0x00; ans[a++] = 0x00;     // TTL
  ans[a++] = 0x00; ans[a++] = 0x3C;     // 60 seconds
  ans[a++] = 0x00; ans[a++] = 0x04;     // Data length: 4 bytes
  ans[a++] = ip[0]; ans[a++] = ip[1]; ans[a++] = ip[2]; ans[a++] = ip[3];

  dns.beginPacket(dns.remoteIP(), dns.remotePort());
  dns.write(ans, a);
  dns.endPacket();
}
