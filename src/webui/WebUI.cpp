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
    Name: WebUI 
    Description: Commercial Extender Setup Portal & Telemetry Dashboard for BW16
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "WebUI.h"
#include "../utils/Base64.h"

extern "C" {
  void sys_reset(void);
}



void WebUI::begin(ConfigStore& cfg, Logger& log, WifiManager& wifi, BridgeManager& bridge, DhcpServer& dhcp) {
  store = &cfg;
  logger = &log;
  wifiRef = &wifi;
  bridgeRef = &bridge;
  dhcpRef = &dhcp;
  server.begin();
  if (logger) logger->info(F("[WEB] Extender Web UI server listening on port 80"));
}

void WebUI::loop() {
  WiFiClient c = server.available();
  if (c) {
    if (logger) logger->info(F("[WEB] Client connection accepted"));
    handleClient(c);
  }
}

bool WebUI::authOk(const String& h) {
  // Allow unauthenticated access in setup mode for frictionless onboarding
  if (wifiRef && wifiRef->isSetupMode()) return true;
  String u = store->get().webUser;
  String p = store->get().webPass;
  if (u.length() == 0 && p.length() == 0) return true;
  String token = b64(String(u + ":" + p));
  String needle = String("Authorization: Basic ") + token;
  return h.indexOf(needle) >= 0;
}

static String urlDecode(const String& in) {
  String out = "";
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    char ch = in.charAt(i);
    if (ch == '+') {
      out += ' ';
    } else if (ch == '%' && i + 2 < in.length()) {
      char h1 = in.charAt(i + 1);
      char h2 = in.charAt(i + 2);
      int v1 = (h1 >= '0' && h1 <= '9') ? (h1 - '0') : ((h1 >= 'A' && h1 <= 'F') ? (h1 - 'A' + 10) : ((h1 >= 'a' && h1 <= 'f') ? (h1 - 'a' + 10) : -1));
      int v2 = (h2 >= '0' && h2 <= '9') ? (h2 - '0') : ((h2 >= 'A' && h2 <= 'F') ? (h2 - 'A' + 10) : ((h2 >= 'a' && h2 <= 'f') ? (h2 - 'a' + 10) : -1));
      if (v1 >= 0 && v2 >= 0) {
        out += (char)((v1 << 4) | v2);
        i += 2;
      } else {
        out += ch;
      }
    } else {
      out += ch;
    }
  }
  return out;
}

static String getParam(const String& body, const String& key) {
  String k = key + "=";
  int pos = -1;
  if (body.startsWith(k)) {
    pos = 0;
  } else {
    pos = body.indexOf("&" + k);
    if (pos >= 0) pos += 1;
  }
  if (pos < 0) return "";
  int start = pos + k.length();
  int amp = body.indexOf('&', start);
  String val = (amp >= 0) ? body.substring(start, amp) : body.substring(start);
  return urlDecode(val);
}

void WebUI::handleClient(WiFiClient& c) {
  c.setRecvTimeout(2000);
  String headers = "";
  headers.reserve(1024);
  unsigned long t = millis();
  while (c.connected() && millis() - t < 1500) {
    while (c.available()) {
      char ch = (char)c.read();
      headers += ch;
      if (headers.endsWith("\r\n\r\n")) goto hdr_done;
      if (headers.length() > 2048) { c.stop(); return; }
    }
    delay(2);
  }
hdr_done:
  int firstLineEnd = headers.indexOf("\r\n");
  String reqLine = (firstLineEnd >= 0) ? headers.substring(0, firstLineEnd) : headers;
  if (logger) logger->info(String(F("[WEB] HTTP: ")) + reqLine);

  // Captive Portal probes
  if (reqLine.startsWith("GET /generate_204")) {
    c.print(F("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"));
    c.flush();
    delay(20);
    c.stop();
    return;
  }

  if (reqLine.startsWith("GET /hotspot-detect.html") ||
      reqLine.startsWith("GET /canonical.html") ||
      reqLine.startsWith("GET /success") ||
      reqLine.startsWith("GET /ncsi.txt") ||
      reqLine.startsWith("GET /connecttest.txt")) {
    String html = F("<html><head><meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\"></head><body>Redirecting to Extender Setup...</body></html>");
    c.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + String(html.length()) + "\r\nConnection: close\r\n\r\n" + html);
    c.flush();
    delay(20);
    c.stop();
    return;
  }

  // Basic Auth check
  if (!authOk(headers)) {
    String body401 = F("<html><body><h3>401 Unauthorized</h3><p>Access requires login.</p></body></html>");
    c.print("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=BW16-Extender\r\nContent-Type: text/html\r\nContent-Length: " + String(body401.length()) + "\r\n\r\n" + body401);
    c.stop();
    return;
  }

  // Read body if POST
  int clPos = headers.indexOf("Content-Length:");
  int contentLen = 0;
  if (clPos >= 0) {
    int endLine = headers.indexOf("\r\n", clPos);
    if (endLine > clPos) {
      String v = headers.substring(clPos + 15, endLine);
      v.trim();
      contentLen = v.toInt();
    }
  }
  if (contentLen < 0 || contentLen > 2048) {
    c.print(F("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n"));
    c.stop();
    return;
  }
  String body = "";
  body.reserve(contentLen > 0 ? contentLen : 256);
  unsigned long t2 = millis();
  while (c.connected() && (int)body.length() < contentLen && millis() - t2 < 1500) {
    while (c.available()) {
      body += char(c.read());
      if ((int)body.length() >= contentLen) break;
    }
    delay(2);
  }

  // API: Live Status JSON
  if (reqLine.startsWith("GET /status_json")) {
    int rssi = wifiRef->getRouterRssi();
    int ch = wifiRef->getActiveChannel();
    ConnectedClient clients[8];
    int clientCount = wifiRef->getAssociatedClients(clients, 8);

    bool isBridge = bridgeRef ? bridgeRef->isBridgeActive() : false;
    bool isApMode = (store->get().mode == RepeaterMode::AP);
    bool isConnected = wifiRef->staAssociated() || isApMode;
    IPAddress curIp = isBridge ? wifiRef->bridgeIP() : wifiRef->apIP();
    if (curIp[0] == 0) curIp = wifiRef->apIP();

    String json = "{";
    json += "\"mode\":\"" + String(isApMode ? "ap" : "repeater") + "\",";
    json += "\"connected\":" + String(isConnected ? "true" : "false") + ",";
    json += "\"bridge\":" + String(isBridge ? "true" : "false") + ",";
    json += "\"sta_ssid\":\"" + String(store->get().staSsid) + "\",";
    json += "\"sta_ip\":\"" + ipToStr(curIp) + "\",";
    json += "\"ap_ssid\":\"" + String(store->get().apSsid) + "\",";
    json += "\"ap_ip\":\"" + ipToStr(wifiRef->apIP()) + "\",";
    json += "\"channel\":" + String(ch) + ",";
    json += "\"band\":\"" + String(ch > 14 ? "5 GHz" : "2.4 GHz") + "\",";
    json += "\"rssi\":" + String(rssi) + ",";
    json += "\"tx_boost\":true,";
    json += "\"uptime_s\":" + String(millis() / 1000) + ",";
    json += "\"clients\":[";
    for (int i = 0; i < clientCount; i++) {
      json += "\"" + String(clients[i].mac) + "\"";
      if (i < clientCount - 1) json += ",";
    }
    json += "]}";

    c.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + String(json.length()) + "\r\n\r\n" + json);
    c.stop();
    return;
  }

  // API: Wi-Fi Scan JSON
  if (reqLine.startsWith("GET /scan_json")) {
    ScannedNetwork nets[32];
    int n = wifiRef ? wifiRef->scanNetworks(nets, 32) : 0;
    String json = "{\"networks\":[";
    for (int idx = 0; idx < n; idx++) {
      json += "{";
      json += "\"ssid\":\"" + String(nets[idx].ssid) + "\",";
      json += "\"rssi\":" + String(nets[idx].rssi) + ",";
      json += "\"enc\":" + String(nets[idx].enc) + ",";
      json += "\"channel\":" + String(nets[idx].channel) + ",";
      json += "\"band\":\"" + String(nets[idx].channel > 14 ? "5GHz" : "2.4GHz") + "\"";
      json += "}";
      if (idx < n - 1) json += ",";
    }
    json += "]}";

    c.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + String(json.length()) + "\r\n\r\n" + json);
    c.stop();
    return;
  }

  // API: Factory Reset / Setup Mode
  if (reqLine.startsWith("POST /reset")) {
    store->mutableRef().mode = RepeaterMode::BRIDGE;
    store->mutableRef().staSsid[0] = 0;
    store->mutableRef().staPass[0] = 0;
    strncpy(store->mutableRef().apSsid, "BW16-Repeater", 63);
    store->mutableRef().apPass[0] = 0;
    store->mutableRef().apChannel = 1;
    store->save();
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"reset\"}"));
    c.flush();
    delay(50);
    c.stop();
    delay(400);
    sys_reset();
    return;
  }

  // API: Kick Client
  if (reqLine.startsWith("POST /kick")) {
    String mac = getParam(body, "mac");
    if (mac.length() >= 17) {
      wifiRef->kickClient(mac.c_str());
    }
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK"));
    c.stop();
    return;
  }

  // API: Save Wi-Fi Configuration (Repeater Mode)
  if (reqLine.startsWith("POST /save_wifi")) {
    String s = getParam(body, "ssid");
    String p = getParam(body, "pass");
    String apS = getParam(body, "ap_ssid");
    String apP = getParam(body, "ap_pass");
    String chStr = getParam(body, "channel");
    int ch = chStr.toInt();
    if (ch <= 0 || ch > 165) ch = 1;

    store->mutableRef().mode = RepeaterMode::BRIDGE;
    store->mutableRef().apChannel = (uint8_t)ch;

    strncpy(store->mutableRef().staSsid, s.c_str(), 63);
    store->mutableRef().staSsid[63] = 0;
    strncpy(store->mutableRef().staPass, p.c_str(), 63);
    store->mutableRef().staPass[63] = 0;

    if (apS.length() > 0) {
      strncpy(store->mutableRef().apSsid, apS.c_str(), 63);
      store->mutableRef().apSsid[63] = 0;
    } else if (s.length() > 0) {
      String defAp = s + "-EXT";
      strncpy(store->mutableRef().apSsid, defAp.c_str(), 63);
      store->mutableRef().apSsid[63] = 0;
    }

    if (apP.length() >= 8) {
      strncpy(store->mutableRef().apPass, apP.c_str(), 63);
      store->mutableRef().apPass[63] = 0;
    } else if (apP.length() == 0 && p.length() >= 8) {
      strncpy(store->mutableRef().apPass, p.c_str(), 63);
      store->mutableRef().apPass[63] = 0;
    } else {
      store->mutableRef().apPass[0] = 0;
    }

    store->save();
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}"));
    c.flush();
    delay(50);
    c.stop();
    delay(400);
    sys_reset();
    return;
  }

  // API: Save Standalone AP Configuration (AP Mode)
  if (reqLine.startsWith("POST /save_ap")) {
    String apS = getParam(body, "ap_ssid");
    String apP = getParam(body, "ap_pass");
    String chStr = getParam(body, "channel");
    int ch = chStr.toInt();
    if (ch <= 0 || ch > 165) ch = 1;

    store->mutableRef().mode = RepeaterMode::AP;
    store->mutableRef().staSsid[0] = 0;
    store->mutableRef().staPass[0] = 0;
    store->mutableRef().apChannel = (uint8_t)ch;

    if (apS.length() > 0) {
      strncpy(store->mutableRef().apSsid, apS.c_str(), 63);
      store->mutableRef().apSsid[63] = 0;
    } else {
      strncpy(store->mutableRef().apSsid, "BW16-Hotspot", 63);
      store->mutableRef().apSsid[63] = 0;
    }

    if (apP.length() >= 8) {
      strncpy(store->mutableRef().apPass, apP.c_str(), 63);
      store->mutableRef().apPass[63] = 0;
    } else {
      store->mutableRef().apPass[0] = 0;
    }

    store->save();
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}"));
    c.flush();
    delay(50);
    c.stop();
    delay(400);
    sys_reset();
    return;
  }

  // Serve Main Page (HTML/CSS/JS)
  c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
            "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'>"
            "<title>BW16 Extender & AP Settings</title>"
            "<style>"
            "* {box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;}"
            "body {background:#f4f6f9;color:#333;line-height:1.5;padding-bottom:40px;}"
            ".header {background:linear-gradient(135deg, #deb052 0%, #be8d36 100%);color:#2b220e;padding:16px 24px;display:flex;align-items:center;border-bottom:3px solid #9f7324;box-shadow:0 2px 8px rgba(0,0,0,0.08);}"
            ".brand-box {background:#fff;border-radius:50%;width:42px;height:42px;display:flex;align-items:center;justify-content:center;margin-right:14px;box-shadow:0 2px 5px rgba(0,0,0,0.15);flex-shrink:0;}"
            ".brand-box svg {width:24px;height:24px;fill:#be8d36;}"
            ".title-box h1 {font-size:18px;font-weight:800;letter-spacing:0.5px;color:#2b220e;text-transform:uppercase;}"
            ".title-box p {font-size:11px;color:#4a3b1a;font-weight:600;}"
            ".main-content {max-width:760px;margin:20px auto;padding:0 14px;}"
            ".tab-bar {display:flex;background:#e2e8f0;border-radius:8px;padding:4px;margin-bottom:16px;gap:4px;}"
            ".tab-btn {flex:1;padding:10px 14px;background:none;border:none;border-radius:6px;font-size:13px;font-weight:700;color:#4a5568;cursor:pointer;transition:all 0.15s;text-align:center;}"
            ".tab-btn.tab-active {background:#fff;color:#2b220e;box-shadow:0 2px 4px rgba(0,0,0,0.08);}"
            ".card {background:#fff;border:1px solid #dcdfe4;border-radius:8px;padding:20px;box-shadow:0 2px 6px rgba(0,0,0,0.04);margin-bottom:18px;}"
            ".card h2 {font-size:16px;color:#2d3748;margin-bottom:6px;font-weight:700;}"
            ".card p.sub {font-size:12px;color:#718096;margin-bottom:14px;}"
            ".btn-gold {background:linear-gradient(to bottom, #deb052, #be8d36);color:#fff;border:1px solid #9f7324;border-radius:6px;padding:9px 24px;font-size:13px;font-weight:700;cursor:pointer;box-shadow:0 2px 4px rgba(0,0,0,0.1);transition:all 0.15s;}"
            ".btn-gold:hover {background:linear-gradient(to bottom, #e5b85a, #c6943c);transform:translateY(-1px);}"
            ".btn-gold:active {transform:translateY(1px);}"
            ".input-group {margin-bottom:12px;}"
            ".input-group label {display:block;font-size:12px;font-weight:700;color:#4a5568;margin-bottom:4px;}"
            ".input-group input, .input-group select {width:100%;max-width:100%;padding:9px 12px;border:1px solid #cbd5e1;border-radius:6px;font-size:13px;outline:none;background:#fff;}"
            ".input-group input:focus, .input-group select:focus {border-color:#be8d36;box-shadow:0 0 0 2px rgba(190,141,54,0.15);}"
            ".net-item {display:flex;align-items:center;justify-content:space-between;padding:10px 12px;border-bottom:1px solid #edf2f7;cursor:pointer;transition:background 0.15s;}"
            ".net-item:hover {background:#f7fafc;}"
            ".badge-band {font-size:10px;padding:2px 6px;border-radius:3px;font-weight:700;margin-left:8px;}"
            ".badge-5g {background:#ebf8ff;color:#2b6cb0;border:1px solid #bee3f8;}"
            ".badge-24g {background:#feebc8;color:#c05621;border:1px solid #fbd38d;}"
            ".signal-bar {display:flex;align-items:flex-end;gap:2px;height:12px;margin-left:8px;}"
            ".bar-seg {width:3px;background:#cbd5e0;border-radius:1px;}"
            ".bar-on {background:#38a169;}"
            ".status-panel {background:#fff;border:1px solid #dcdfe4;border-radius:8px;padding:22px;box-shadow:0 2px 6px rgba(0,0,0,0.04);}"
            ".status-header {display:flex;align-items:center;justify-content:space-between;padding-bottom:14px;border-bottom:1px solid #e2e8f0;margin-bottom:16px;flex-wrap:wrap;gap:8px;}"
            ".tag-green {background:#c6f6d5;color:#22543d;padding:4px 10px;border-radius:4px;font-size:11px;font-weight:700;}"
            ".stat-grid {display:grid;grid-template-columns:repeat(auto-fit, minmax(180px, 1fr));gap:12px;margin-bottom:18px;}"
            ".stat-box {background:#f8fafc;border:1px solid #e2e8f0;border-radius:6px;padding:12px;}"
            ".stat-box h4 {font-size:11px;color:#718096;text-transform:uppercase;margin-bottom:3px;font-weight:700;}"
            ".stat-box p {font-size:15px;font-weight:700;color:#2d3748;}"
            ".modal-overlay {position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.45);display:flex;align-items:center;justify-content:center;z-index:9999;}"
            ".modal-box {background:#fff;border:1px solid #e2e8f0;border-radius:12px;box-shadow:0 20px 40px rgba(0,0,0,0.15);padding:24px 30px;text-align:center;max-width:400px;width:90%;}"
            ".dots-loader {display:flex;justify-content:center;gap:6px;margin:12px 0;}"
            ".dot {width:7px;height:7px;background:#be8d36;border-radius:50%;animation:pulse 1.2s infinite ease-in-out;}"
            ".dot:nth-child(2) {animation-delay:0.2s;}"
            ".dot:nth-child(3) {animation-delay:0.4s;}"
            ".dot:nth-child(4) {animation-delay:0.6s;}"
            "@keyframes pulse {0%,100%{transform:scale(0.8);opacity:0.4;} 50%{transform:scale(1.3);opacity:1;}}"
            "</style></head><body>"));

  // Send Header
  c.print(F("<div class='header'>"
            "<div class='brand-box'>"
            "<svg viewBox='0 0 24 24'><path d='M12 3c-4.97 0-9.28 2.37-12 6.05l1.53 1.53c2.32-3.17 6.03-5.22 10.47-5.22s8.15 2.05 10.47 5.22l1.53-1.53c-2.72-3.68-7.03-6.05-12-6.05zm0 4.5c-3.67 0-6.85 1.76-8.86 4.47l1.54 1.54c1.65-2.22 4.26-3.65 7.32-3.65s5.67 1.43 7.32 3.65l1.54-1.54c-2.01-2.71-5.19-4.47-8.86-4.47zm0 4.5c-2.38 0-4.44 1.14-5.73 2.9l1.55 1.55c.96-1.31 2.49-2.15 4.18-2.15s3.22.84 4.18 2.15l1.55-1.55c-1.29-1.76-3.35-2.9-5.73-2.9zm0 4.5c-1.2 0-2.22.58-2.86 1.46l2.86 2.87 2.86-2.87c-.64-.88-1.66-1.46-2.86-1.46z'/></svg>"
            "</div>"
            "<div class='title-box'>"
            "<h1>BW16 Extender & AP Settings</h1>"
            "<p>Realtek RTL8720DN Dual-Band 2.4G / 5G</p>"
            "</div>"
            "</div>"
            "<div class='main-content'>"));

  // Mode Tabs Bar
  c.print(F("<div id='mode-tabs' class='tab-bar'>"
            "<button id='tab-btn-rep' class='tab-btn tab-active' onclick='switchTab(\"rep\")'>Repeater Mode</button>"
            "<button id='tab-btn-ap' class='tab-btn' onclick='switchTab(\"ap\")'>Access Point Mode</button>"
            "</div>"));

  // TAB 1: Repeater Mode View
  c.print(F("<div id='view-rep' class='card'>"
            "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;'>"
            "<div>"
            "<h2>Select Wi-Fi Network to Amplify</h2>"
            "<p class='sub'>Connects to your home router and creates an extended network with L2 bridging.</p>"
            "</div>"
            "<button class='btn-gold' style='padding:6px 14px;font-size:12px;' onclick='scanNetworks()'>Refresh Scan</button>"
            "</div>"
            "<div id='scan-status' style='display:none;' class='dots-loader'><div class='dot'></div><div class='dot'></div><div class='dot'></div><div class='dot'></div></div>"
            "<div id='network-list' style='max-height:220px;overflow-y:auto;border:1px solid #edf2f7;border-radius:6px;margin-bottom:16px;'></div>"
            "<div id='rep-form'>"
            "<div class='input-group'>"
            "<label>Home Wi-Fi Network (SSID):</label>"
            "<input type='text' id='inp-sta-ssid' placeholder='Click a network above or type manually'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Home Wi-Fi Password:</label>"
            "<input type='password' id='inp-sta-pass' placeholder='Enter home Wi-Fi password'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Repeater Network Name (SSID):</label>"
            "<input type='text' id='inp-ap-ssid' placeholder='Leave empty for <SSID>-EXT'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Repeater Password:</label>"
            "<input type='password' id='inp-ap-pass' placeholder='Leave empty to use same password'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Radio Channel:</label>"
            "<select id='sel-channel'>"
            "<option value='1'>Auto / Same as router (Channel 1)</option>"
            "<option value='6'>2.4 GHz - Channel 6</option>"
            "<option value='11'>2.4 GHz - Channel 11</option>"
            "<option value='36'>5 GHz - Channel 36</option>"
            "<option value='40'>5 GHz - Channel 40</option>"
            "<option value='44'>5 GHz - Channel 44</option>"
            "<option value='48'>5 GHz - Channel 48</option>"
            "</select>"
            "</div>"
            "<button class='btn-gold' style='width:100%;padding:12px;margin-top:6px;' onclick='submitRepeater()'>Connect & Amplify</button>"
            "</div>"
            "</div>"));

  // TAB 2: Access Point Mode View
  c.print(F("<div id='view-ap' class='card' style='display:none;'>"
            "<h2>Standalone Access Point Settings</h2>"
            "<p class='sub'>BW16 will run as an independent local Wi-Fi hotspot with its own DHCP server (no home router needed).</p>"
            "<div class='input-group'>"
            "<label>Hotspot Network Name (SSID):</label>"
            "<input type='text' id='inp-hotspot-ssid' value='BW16-Hotspot' placeholder='e.g. BW16-Hotspot'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Hotspot Password:</label>"
            "<input type='password' id='inp-hotspot-pass' placeholder='Leave empty for Open network, or min 8 chars'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Radio Band & Channel:</label>"
            "<select id='sel-hotspot-channel'>"
            "<option value='1'>2.4 GHz - Channel 1 (Recommended)</option>"
            "<option value='6'>2.4 GHz - Channel 6</option>"
            "<option value='11'>2.4 GHz - Channel 11</option>"
            "<option value='36'>5 GHz - Channel 36 (High Speed 5G)</option>"
            "<option value='40'>5 GHz - Channel 40</option>"
            "<option value='44'>5 GHz - Channel 44</option>"
            "<option value='48'>5 GHz - Channel 48</option>"
            "</select>"
            "</div>"
            "<button class='btn-gold' style='width:100%;padding:12px;margin-top:6px;' onclick='submitApMode()'>Start Access Point</button>"
            "</div>"));

  // View: Status Dashboard
  c.print(F("<div id='view-dashboard' class='status-panel' style='display:none;'>"
            "<div class='status-header'>"
            "<div>"
            "<h2 id='dash-title' style='font-size:18px;color:#2d3748;'>Device Status</h2>"
            "<p id='dash-sub' style='font-size:12px;color:#718096;'>Active</p>"
            "</div>"
            "<span id='dash-badge' class='tag-green'>ACTIVE</span>"
            "</div>"
            "<div class='stat-grid'>"
            "<div class='stat-box'><h4 id='stat-lbl-1'>Network</h4><p id='stat-val-1'>-</p></div>"
            "<div class='stat-box'><h4 id='stat-lbl-2'>Signal (RSSI)</h4><p id='stat-val-2'>-</p></div>"
            "<div class='stat-box'><h4>Radio Channel</h4><p id='stat-ch'>-</p></div>"
            "<div class='stat-box'><h4>TX Output Power</h4><p>20 dBm (Max Boost)</p></div>"
            "<div class='stat-box'><h4>Extender SSID</h4><p id='stat-ap-ssid'>-</p></div>"
            "<div class='stat-box'><h4>Uptime</h4><p id='stat-uptime'>-</p></div>"
            "</div>"
            "<h3 style='font-size:14px;color:#4a5568;margin-bottom:8px;'>Connected Clients</h3>"
            "<div id='client-table' style='margin-bottom:18px;'></div>"
            "<div style='display:flex;gap:10px;flex-wrap:wrap;'>"
            "<button class='btn-gold' onclick='reconfigure()'>Reconfigure</button>"
            "<button class='btn-gold' style='background:#e53e3e;border-color:#c53030;' onclick='showResetModal()'>Reset to Setup Portal</button>"
            "</div>"
            "</div>"));

  // Inline Modal: Reset Confirmation (No window.confirm!)
  c.print(F("<div id='modal-reset' class='modal-overlay' style='display:none;'>"
            "<div class='modal-box'>"
            "<h3 style='margin-bottom:8px;font-size:16px;color:#2d3748;'>Reset to Factory Setup?</h3>"
            "<p style='font-size:13px;color:#718096;margin-bottom:18px;'>This will erase stored Wi-Fi settings and restore the default open network (BW16-Repeater).</p>"
            "<div style='display:flex;justify-content:center;gap:12px;'>"
            "<button class='btn-gold' style='background:#e53e3e;border-color:#c53030;' onclick='confirmReset()'>Reset Now</button>"
            "<button class='btn-gold' style='background:#718096;border-color:#4a5568;' onclick='hideResetModal()'>Cancel</button>"
            "</div>"
            "</div>"
            "</div>"
            "</div>"));

  // Client-Side Logic (No prompt / confirm / alert)
  c.print(R"rawliteral(<script>
function esc(s) {
  return String(s||"").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&#39;");
}

let activeTab = 'rep';
function switchTab(t) {
  activeTab = t;
  document.getElementById('tab-btn-rep').className = 'tab-btn' + (t === 'rep' ? ' tab-active' : '');
  document.getElementById('tab-btn-ap').className = 'tab-btn' + (t === 'ap' ? ' tab-active' : '');
  document.getElementById('view-rep').style.display = (t === 'rep' ? 'block' : 'none');
  document.getElementById('view-ap').style.display = (t === 'ap' ? 'block' : 'none');
}

function checkStatus() {
  fetch('/status_json').then(r=>r.json()).then(d=>{
    if (d.connected) {
      document.getElementById('mode-tabs').style.display = 'none';
      document.getElementById('view-rep').style.display = 'none';
      document.getElementById('view-ap').style.display = 'none';
      document.getElementById('view-dashboard').style.display = 'block';

      if (d.mode === 'ap') {
        document.getElementById('dash-title').innerText = 'Access Point Active';
        document.getElementById('dash-sub').innerText = 'Standalone local Wi-Fi hotspot with DHCP server';
        document.getElementById('dash-badge').innerText = 'AP MODE ACTIVE';
        document.getElementById('stat-lbl-1').innerText = 'Hotspot IP';
        document.getElementById('stat-val-1').innerText = d.ap_ip;
        document.getElementById('stat-lbl-2').innerText = 'Security';
        document.getElementById('stat-val-2').innerText = 'WPA2 / Protected';
      } else {
        document.getElementById('dash-title').innerText = 'Repeater Active & Bridging';
        document.getElementById('dash-sub').innerText = 'Hardware L2 Bridge (IEEE 802.1D)';
        document.getElementById('dash-badge').innerText = 'L2 BRIDGE ACTIVE';
        document.getElementById('stat-lbl-1').innerText = 'Home Wi-Fi';
        document.getElementById('stat-val-1').innerText = d.sta_ssid;
        document.getElementById('stat-lbl-2').innerText = 'Signal (RSSI)';
        document.getElementById('stat-val-2').innerText = d.rssi + ' dBm';
      }

      document.getElementById('stat-ch').innerText = 'Ch ' + d.channel + ' (' + d.band + ')';
      document.getElementById('stat-ap-ssid').innerText = d.ap_ssid;
      document.getElementById('stat-uptime').innerText = Math.floor(d.uptime_s / 60) + ' min';

      let clHtml = '<ul style="list-style:none;">';
      if (!d.clients || d.clients.length === 0) {
        clHtml += '<li style="color:#a0aec0;font-size:12px;padding:6px 0;">No clients currently connected.</li>';
      } else {
        d.clients.forEach(c=>{
          clHtml += '<li style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #edf2f7;font-size:13px;"><span>' + esc(c) + '</span><button onclick="kickClient(\'' + esc(c) + '\')" style="color:#e53e3e;background:none;border:none;cursor:pointer;font-weight:700;">Kick</button></li>';
        });
      }
      clHtml += '</ul>';
      document.getElementById('client-table').innerHTML = clHtml;
    }
  }).catch(e=>{});
}
checkStatus();
setInterval(checkStatus, 6000);

function scanNetworks() {
  document.getElementById('scan-status').style.display = 'flex';
  fetch('/scan_json').then(r=>r.json()).then(d=>{
    document.getElementById('scan-status').style.display = 'none';
    renderNetworks(d.networks || []);
  }).catch(e=>{
    document.getElementById('scan-status').style.display = 'none';
  });
}

function renderNetworks(nets) {
  let html = '';
  if (nets.length === 0) {
    html = '<div style="padding:14px;text-align:center;color:#718096;font-size:12px;">No networks detected or network hidden. Enter details manually below.</div>';
  } else {
    nets.forEach(n=>{
      let bars = n.rssi >= -60 ? 4 : (n.rssi >= -72 ? 3 : (n.rssi >= -82 ? 2 : 1));
      let bandLabel = n.channel > 14 ? '5 GHz' : '2.4 GHz';
      let badgeClass = n.channel > 14 ? 'badge-5g' : 'badge-24g';
      html += '<div class="net-item" onclick="pickNet(' + JSON.stringify(n.ssid) + ',' + (n.channel||1) + ')">';
      html += '<div style="display:flex;align-items:center;"><strong>' + esc(n.ssid) + '</strong><span class="badge-band ' + badgeClass + '">' + bandLabel + ' (Ch ' + n.channel + ')</span></div>';
      html += '<div style="display:flex;align-items:center;"><span style="font-size:11px;color:#718096;margin-right:6px;">' + n.rssi + ' dBm</span>';
      html += '<div class="signal-bar">';
      for (let j=1; j<=4; j++) { html += '<div class="bar-seg ' + (j<=bars?'bar-on':'') + '" style="height:' + (j*3+2) + 'px;"></div>'; }
      html += '</div></div></div>';
    });
  }
  document.getElementById('network-list').innerHTML = html;
}

// Initial scan on load
scanNetworks();

function pickNet(s, ch) {
  document.getElementById('inp-sta-ssid').value = s;
  document.getElementById('inp-ap-ssid').value = s + '-EXT';
  let sel = document.getElementById('sel-channel');
  if (sel) sel.value = String(ch || 1);
  document.getElementById('inp-sta-pass').focus();
}

function submitRepeater() {
  let s = document.getElementById('inp-sta-ssid').value.trim();
  let p = document.getElementById('inp-sta-pass').value;
  let apS = document.getElementById('inp-ap-ssid').value.trim();
  let apP = document.getElementById('inp-ap-pass').value;
  let ch = document.getElementById('sel-channel').value;

  if (!s) {
    document.getElementById('inp-sta-ssid').focus();
    return;
  }

  let targetAp = apS || (s + '-EXT');
  document.getElementById('rep-form').innerHTML = '<div style="text-align:center;padding:24px;"><h3 style="color:#2b220e;">Settings Applied</h3><p style="color:#4a5568;margin-top:10px;font-size:13px;">Repeater is connecting to home router and restarting...<br><br>Connect your devices to: <strong style="color:#be8d36;font-size:15px;">' + esc(targetAp) + '</strong></p></div>';

  let body = 'ssid=' + encodeURIComponent(s) + '&pass=' + encodeURIComponent(p) + '&ap_ssid=' + encodeURIComponent(apS) + '&ap_pass=' + encodeURIComponent(apP) + '&channel=' + encodeURIComponent(ch);
  fetch('/save_wifi', {method:'POST', body:body}).catch(e=>{});
}

function submitApMode() {
  let apS = document.getElementById('inp-hotspot-ssid').value.trim() || 'BW16-Hotspot';
  let apP = document.getElementById('inp-hotspot-pass').value;
  let ch = document.getElementById('sel-hotspot-channel').value;

  document.getElementById('view-ap').innerHTML = '<div style="text-align:center;padding:24px;"><h3 style="color:#2b220e;">Access Point Started</h3><p style="color:#4a5568;margin-top:10px;font-size:13px;">Board is rebooting into Standalone Access Point mode...<br><br>Connect your devices to: <strong style="color:#be8d36;font-size:15px;">' + esc(apS) + '</strong></p></div>';

  let body = 'ap_ssid=' + encodeURIComponent(apS) + '&ap_pass=' + encodeURIComponent(apP) + '&channel=' + encodeURIComponent(ch);
  fetch('/save_ap', {method:'POST', body:body}).catch(e=>{});
}

function showResetModal() {
  document.getElementById('modal-reset').style.display = 'flex';
}

function hideResetModal() {
  document.getElementById('modal-reset').style.display = 'none';
}

function confirmReset() {
  hideResetModal();
  document.getElementById('view-dashboard').innerHTML = '<div style="text-align:center;padding:30px;"><h3 style="color:#2b220e;">Resetting to Setup Portal</h3><p style="color:#718096;margin-top:10px;">Please reconnect to Wi-Fi: <strong>BW16-Repeater</strong></p></div>';
  fetch('/reset', {method:'POST'}).catch(e=>{});
  setTimeout(()=>{ window.location.reload(); }, 3500);
}

function kickClient(mac) {
  fetch('/kick', {method:'POST', body:'mac=' + encodeURIComponent(mac)}).then(()=>checkStatus());
}

function reconfigure() {
  document.getElementById('view-dashboard').style.display = 'none';
  document.getElementById('mode-tabs').style.display = 'flex';
  switchTab(activeTab);
  scanNetworks();
}
</script></body></html>)rawliteral");
  c.flush();
  delay(80);
  c.stop();
}
