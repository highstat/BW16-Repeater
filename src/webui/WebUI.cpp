/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈ `https://github.com/highstat` 
    Name: WebUI 
    Description: Commercial-grade Extender WiFi Settings portal & Dashboard
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "WebUI.h"

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
  if (c) handleClient(c);
}

bool WebUI::authOk(const String& h) {
  // Allow unauthenticated access in setup mode for frictionless captive portal onboarding
  if (wifiRef && wifiRef->isSetupMode()) return true;
  String u = store->get().webUser;
  String p = store->get().webPass;
  if (u.length() == 0 && p.length() == 0) return true;
  String token = b64(String(u + ":" + p));
  String needle = String("Authorization: Basic ") + token;
  return h.indexOf(needle) >= 0;
}

void WebUI::handleClient(WiFiClient& c) {
  String headers = "";
  headers.reserve(768);
  unsigned long t = millis();
  while (c.connected() && millis() - t < 1200) {
    while (c.available()) {
      char ch = c.read();
      headers += ch;
      if (headers.endsWith("\r\n\r\n")) goto hdr_done;
    }
  }
hdr_done:
  String reqLine = headers.substring(0, headers.indexOf("\r\n"));

  // Captive Portal probes
  if (reqLine.startsWith("GET /generate_204") ||
      reqLine.startsWith("GET /hotspot-detect.html") ||
      reqLine.startsWith("GET /success") ||
      reqLine.startsWith("GET /ncsi.txt") ||
      reqLine.startsWith("GET /canonical.html")) {
    String html = F("<html><head><meta http-equiv=\"refresh\" content=\"0;url=/\"></head><body>Redirecting to Extender Setup...</body></html>");
    c.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + String(html.length()) + "\r\n\r\n" + html);
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
  String body = "";
  body.reserve(contentLen > 0 ? contentLen : 256);
  unsigned long t2 = millis();
  while (c.connected() && (int)body.length() < contentLen && millis() - t2 < 1500) {
    while (c.available()) {
      body += char(c.read());
      if ((int)body.length() >= contentLen) break;
    }
  }

  // API: Live Status JSON
  if (reqLine.startsWith("GET /status_json")) {
    int rssi = wifiRef->getRouterRssi();
    int ch = wifiRef->getActiveChannel();
    ConnectedClient clients[8];
    int clientCount = wifiRef->getAssociatedClients(clients, 8);

    String json = "{";
    json += "\"connected\":" + String(wifiRef->staConnected() ? "true" : "false") + ",";
    json += "\"bridge\":" + String(bridgeRef->isBridgeActive() ? "true" : "false") + ",";
    json += "\"sta_ssid\":\"" + String(store->get().staSsid) + "\",";
    json += "\"sta_ip\":\"" + ipToStr(wifiRef->staIP()) + "\",";
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

    c.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " + String(json.length()) + "\r\n\r\n" + json);
    c.stop();
    return;
  }

  // API: Wi-Fi Scan JSON
  if (reqLine.startsWith("GET /scan_json")) {
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      int r = WiFi.RSSI(i);
      int enc = WiFi.encryptionType(i);
      if (s.length() == 0) continue;
      json += "{";
      json += "\"ssid\":\"" + s + "\",";
      json += "\"rssi\":" + String(r) + ",";
      json += "\"enc\":" + String(enc);
      json += "}";
      if (i < n - 1) json += ",";
    }
    json += "]}";

    c.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: " + String(json.length()) + "\r\n\r\n" + json);
    c.stop();
    return;
  }

  // API: Kick Client
  if (reqLine.startsWith("POST /kick")) {
    int mPos = body.indexOf("mac=");
    if (mPos >= 0) {
      int amp = body.indexOf('&', mPos);
      String mac = body.substring(mPos + 4, amp >= 0 ? amp : body.length());
      mac.trim();
      wifiRef->kickClient(mac.c_str());
    }
    c.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK");
    c.stop();
    return;
  }

  // API: Save Wi-Fi Configuration
  if (reqLine.startsWith("POST /save_wifi")) {
    int sPos = body.indexOf("ssid=");
    int pPos = body.indexOf("pass=");
    int apSPos = body.indexOf("ap_ssid=");
    int apPPos = body.indexOf("ap_pass=");

    if (sPos >= 0) {
      int amp = body.indexOf('&', sPos);
      String sv = body.substring(sPos + 5, amp >= 0 ? amp : body.length());
      sv.replace("+", " ");
      sv.trim();
      strncpy(store->mutableRef().staSsid, sv.c_str(), 63);
    }
    if (pPos >= 0) {
      int amp = body.indexOf('&', pPos);
      String pv = body.substring(pPos + 5, amp >= 0 ? amp : body.length());
      pv.trim();
      strncpy(store->mutableRef().staPass, pv.c_str(), 63);
    }
    if (apSPos >= 0) {
      int amp = body.indexOf('&', apSPos);
      String asv = body.substring(apSPos + 8, amp >= 0 ? amp : body.length());
      asv.replace("+", " ");
      asv.trim();
      if (asv.length() > 0) strncpy(store->mutableRef().apSsid, asv.c_str(), 63);
    }
    if (apPPos >= 0) {
      int amp = body.indexOf('&', apPPos);
      String apv = body.substring(apPPos + 8, amp >= 0 ? amp : body.length());
      apv.trim();
      if (apv.length() >= 8) strncpy(store->mutableRef().apPass, apv.c_str(), 63);
    }

    store->mutableRef().mode = RepeaterMode::BRIDGE;
    store->save();

    String resp = F("{\"status\":\"saved\",\"message\":\"Connecting to Wi-Fi... Please reconnect to extended network.\"}");
    c.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + String(resp.length()) + "\r\n\r\n" + resp);
    c.stop();

    delay(800);
    NVIC_SystemReset();
    return;
  }

  // SERVE MAIN HTML (Extender WiFi Settings UI matching photo)
  serveMainPage(c);
  c.stop();
}

void WebUI::serveMainPage(WiFiClient& c) {
  c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: close\r\n\r\n"));

  // Send in streaming chunks to keep RAM usage minimal
  c.print(F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
            "<title>Extender WiFi Settings</title><style>"
            "* {box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;}"
            "body {background:#f3f5f8;color:#333;min-height:100vh;display:flex;flex-direction:column;}"
            ".header {background:#0f172a;padding:16px 28px;display:flex;align-items:center;border-bottom:1px solid #1e293b;box-shadow:0 1px 3px rgba(0,0,0,0.1);}"
            ".brand-box {display:flex;align-items:center;margin-right:20px;color:#38bdf8;}"
            ".brand-box svg {width:32px;height:32px;fill:currentColor;}"
            ".brand-sub {font-size:10px;font-weight:700;letter-spacing:1px;text-transform:uppercase;margin-top:2px;}"
            ".title-box h1 {font-size:18px;color:#f8fafc;font-weight:600;letter-spacing:-0.2px;}"
            ".title-box p {font-size:12px;color:#94a3b8;margin-top:2px;}"
            ".main-content {max-width:960px;width:95%;margin:28px auto;flex:1;}"
            ".mode-card {background:#ffffff;border:1px solid #e2e8f0;border-radius:12px;padding:24px;margin-bottom:20px;box-shadow:0 1px 3px rgba(0,0,0,0.03);}"
            ".mode-card h2 {font-size:17px;color:#333;font-weight:600;margin-bottom:18px;text-align:center;}"
            ".diagram-row {display:flex;align-items:center;justify-content:center;gap:18px;flex-wrap:wrap;padding:14px 0;}"
            ".icon-node {display:flex;flex-direction:column;align-items:center;}"
            ".icon-node svg {width:46px;height:46px;fill:#2b2b2b;}"
            ".waves {display:flex;align-items:center;color:#444;font-size:24px;font-weight:700;letter-spacing:-2px;}"
            ".cable-line {width:70px;height:3px;background:#333;margin:0 10px;position:relative;top:-6px;}"
            ".btn-gold {background:#0f172a;color:#ffffff;border:1px solid #0f172a;border-radius:8px;padding:10px 32px;font-size:13px;font-weight:600;cursor:pointer;box-shadow:0 1px 2px rgba(0,0,0,0.05);transition:all 0.15s;}"
            ".btn-gold:hover {background:#1e293b;border-color:#1e293b;transform:translateY(-1px);}"
            ".btn-gold:active {transform:translateY(1px);}"
            ".modal-overlay {position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.45);display:flex;align-items:center;justify-content:center;z-index:9999;}"
            ".modal-box {background:#ffffff;border:1px solid #e2e8f0;border-radius:14px;box-shadow:0 20px 40px rgba(0,0,0,0.12);padding:32px 40px;text-align:center;max-width:440px;width:90%;}"
            ".dots-loader {display:flex;justify-content:center;gap:8px;margin-bottom:14px;}"
            ".dot {width:7px;height:7px;background:#2b6cb0;border-radius:50%;animation:pulse 1.2s infinite ease-in-out;}"
            ".dot:nth-child(2) {animation-delay:0.2s;}"
            ".dot:nth-child(3) {animation-delay:0.4s;}"
            ".dot:nth-child(4) {animation-delay:0.6s;}"
            ".dot:nth-child(5) {animation-delay:0.8s;}"
            ".dot:nth-child(6) {animation-delay:1.0s;}"
            "@keyframes pulse {0%,100%{transform:scale(0.8);opacity:0.4;} 50%{transform:scale(1.4);opacity:1;}}"
            ".scan-text {font-size:15px;color:#2d3748;font-weight:600;}"
            ".net-item {display:flex;align-items:center;justify-content:space-between;padding:12px 14px;border-bottom:1px solid #edf2f7;cursor:pointer;transition:background 0.15s;}"
            ".net-item:hover {background:#f7fafc;}"
            ".badge-band {font-size:11px;padding:2px 6px;border-radius:3px;font-weight:600;margin-left:8px;}"
            ".badge-5g {background:#ebf8ff;color:#2b6cb0;border:1px solid #bee3f8;}"
            ".badge-24g {background:#feebc8;color:#c05621;border:1px solid #fbd38d;}"
            ".signal-bar {display:flex;align-items:flex-end;gap:2px;height:14px;margin-left:10px;}"
            ".bar-seg {width:3px;background:#cbd5e0;border-radius:1px;}"
            ".bar-on {background:#38a169;}"
            ".conn-form {background:#f7fafc;border:1px solid #e2e8f0;border-radius:6px;padding:18px;margin-top:14px;}"
            ".input-group {margin-bottom:12px;}"
            ".input-group label {display:block;font-size:13px;font-weight:600;color:#4a5568;margin-bottom:4px;}"
            ".input-group input {width:100%;max-width:380px;padding:9px 12px;border:1px solid #cbd5e1;border-radius:6px;font-size:13px;outline:none;transition:border-color 0.15s;}"
            ".status-panel {background:#fff;border:1px solid #dcdfe4;border-radius:6px;padding:24px;box-shadow:0 1px 4px rgba(0,0,0,0.05);}"
            ".status-header {display:flex;align-items:center;justify-content:space-between;padding-bottom:16px;border-bottom:1px solid #e2e8f0;margin-bottom:18px;}"
            ".tag-green {background:#c6f6d5;color:#22543d;padding:4px 10px;border-radius:4px;font-size:12px;font-weight:700;}"
            ".stat-grid {display:grid;grid-template-columns:repeat(auto-fit, minmax(200px, 1fr));gap:16px;margin-bottom:20px;}"
            ".stat-box {background:#f7fafc;border:1px solid #e2e8f0;border-radius:6px;padding:14px;}"
            ".stat-box h4 {font-size:12px;color:#718096;text-transform:uppercase;margin-bottom:4px;}"
            ".stat-box p {font-size:16px;font-weight:700;color:#2d3748;}"
            "</style></head><body>"));

  // Send Header
  c.print(F("<div class='header'>"
            "<div class='brand-box'>"
            "<svg viewBox='0 0 24 24'><path d='M12 3c-4.97 0-9.28 2.37-12 6.05l1.53 1.53c2.32-3.17 6.03-5.22 10.47-5.22s8.15 2.05 10.47 5.22l1.53-1.53c-2.72-3.68-7.03-6.05-12-6.05zm0 4.5c-3.67 0-6.85 1.76-8.86 4.47l1.54 1.54c1.65-2.22 4.26-3.65 7.32-3.65s5.67 1.43 7.32 3.65l1.54-1.54c-2.01-2.71-5.19-4.47-8.86-4.47zm0 4.5c-2.38 0-4.44 1.14-5.73 2.9l1.55 1.55c.96-1.31 2.49-2.15 4.18-2.15s3.22.84 4.18 2.15l1.55-1.55c-1.29-1.76-3.35-2.9-5.73-2.9zm0 4.5c-1.2 0-2.22.58-2.86 1.46l2.86 2.87 2.86-2.87c-.64-.88-1.66-1.46-2.86-1.46z'/></svg>"
            "</div>"
            "<div class='title-box'>"
            "<h1>Extender Settings</h1>"
            "<p>High-performance transparent L2 bridge</p>"
            "</div>"
            "</div>"
            "<div class='main-content'>"));

  // Mode Selection / Welcome Wizard Container
  c.print(F("<div id='view-modes'>"
            "<div class='mode-card'>"
            "<h2>Repeater Mode Settings</h2>"
            "<div class='diagram-row'>"
            "<div class='icon-node'>"
            "<svg viewBox='0 0 24 24'><path d='M2 9v11h20v-11h-20zm18 9h-16v-7h16v7zm-14-14h2v3h-2zm10 0h2v3h-2z'/></svg>"
            "</div>"
            "<div class='waves'>)))</div>"
            "<div class='icon-node'>"
            "<svg viewBox='0 0 24 24'><path d='M6 2v6h2v-6h-2zm10 0v6h2v-6h-2zm-9 8c-1.1 0-2 .9-2 2v9c0 1.1.9 2 2 2h10c1.1 0 2-.9 2-2v-9c0-1.1-.9-2-2-2h-10zm5 11c-.55 0-1-.45-1-1s.45-1 1-1 1 .45 1 1-.45 1-1 1z'/></svg>"
            "</div>"
            "<div class='waves'>)))</div>"
            "<div class='icon-node'>"
            "<svg viewBox='0 0 24 24'><path d='M4 6h16v10h-16v-10zm8 12h-4v2h8v-2h-4zm9-16h-18c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h7v2h-3v2h12v-2h-3v-2h7c1.1 0 2-.9 2-2v-14c0-1.1-.9-2-2-2z'/></svg>"
            "</div>"
            "<div style='margin-left:30px;'>"
            "<button class='btn-gold' onclick='startRepeaterWizard()'>NEXT</button>"
            "</div>"
            "</div>"
            "</div>"

            "<div class='mode-card'>"
            "<h2>AP Mode Settings</h2>"
            "<div class='diagram-row'>"
            "<div class='icon-node'>"
            "<svg viewBox='0 0 24 24'><path d='M2 9v11h20v-11h-20zm18 9h-16v-7h16v7zm-14-14h2v3h-2zm10 0h2v3h-2z'/></svg>"
            "</div>"
            "<div class='cable-line'></div>"
            "<div class='icon-node'>"
            "<svg viewBox='0 0 24 24'><path d='M6 2v6h2v-6h-2zm10 0v6h2v-6h-2zm-9 8c-1.1 0-2 .9-2 2v9c0 1.1.9 2 2 2h10c1.1 0 2-.9 2-2v-9c0-1.1-.9-2-2-2h-10zm5 11c-.55 0-1-.45-1-1s.45-1 1-1 1 .45 1 1-.45 1-1 1z'/></svg>"
            "</div>"
            "<div class='waves'>)))</div>"
            "<div class='icon-node'>"
            "<svg viewBox='0 0 24 24'><path d='M4 6h16v10h-16v-10zm8 12h-4v2h8v-2h-4zm9-16h-18c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h7v2h-3v2h12v-2h-3v-2h7c1.1 0 2-.9 2-2v-14c0-1.1-.9-2-2-2z'/></svg>"
            "</div>"
            "<div style='margin-left:30px;'>"
            "<button class='btn-gold' onclick='startApWizard()'>NEXT</button>"
            "</div>"
            "</div>"
            "</div>"
            "</div>"));

  // View: Network Selection / Scan (Photo 2)
  c.print(F("<div id='view-scan' style='display:none;' class='mode-card'>"
            "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:16px;'>"
            "<h2 style='text-align:left;margin:0;'>Select the WiFi signal to be amplified or manually add a network</h2>"
            "<button class='btn-gold' style='padding:6px 18px;font-size:12px;' onclick='showManual()'>Manual Setup</button>"
            "</div>"
            "<div id='network-list'></div>"
            "<div id='connect-box' class='conn-form' style='display:none;'>"
            "<h3 id='selected-ssid-title' style='margin-bottom:12px;font-size:15px;color:#2b6cb0;'></h3>"
            "<div class='input-group'>"
            "<label>Home Wi-Fi Password:</label>"
            "<input type='password' id='inp-pass' placeholder='Enter Wi-Fi password'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Extender Network Name (SSID):</label>"
            "<input type='text' id='inp-ap-ssid' placeholder='BW16-Repeater'>"
            "</div>"
            "<div class='input-group'>"
            "<label>Extender Password (leave empty for same password):</label>"
            "<input type='password' id='inp-ap-pass' placeholder='At least 8 characters'>"
            "</div>"
            "<button class='btn-gold' onclick='submitConnect()'>Connect & Amplify</button>"
            "</div>"
            "</div>"));

  // View: Status Dashboard
  c.print(F("<div id='view-dashboard' style='display:none;' class='status-panel'>"
            "<div class='status-header'>"
            "<div>"
            "<h2 style='font-size:20px;color:#2d3748;'>Repeater Active & Bridging</h2>"
            "<p style='font-size:13px;color:#718096;margin-top:2px;'>Hardware L2 Bridge (IEEE 802.1D) with Realtek br_rpt_handle</p>"
            "</div>"
            "<span class='tag-green'>L2 BRIDGE ACTIVE</span>"
            "</div>"
            "<div class='stat-grid'>"
            "<div class='stat-box'><h4>Home Wi-Fi</h4><p id='stat-sta-ssid'>-</p></div>"
            "<div class='stat-box'><h4>Signal (RSSI)</h4><p id='stat-rssi'>-</p></div>"
            "<div class='stat-box'><h4>Radio Channel</h4><p id='stat-ch'>-</p></div>"
            "<div class='stat-box'><h4>TX Output Power</h4><p>20 dBm (Max Boost)</p></div>"
            "<div class='stat-box'><h4>Repeater SSID</h4><p id='stat-ap-ssid'>-</p></div>"
            "<div class='stat-box'><h4>Uptime</h4><p id='stat-uptime'>-</p></div>"
            "</div>"
            "<h3 style='font-size:15px;color:#4a5568;margin-bottom:10px;'>Connected Clients</h3>"
            "<div id='client-table' style='margin-bottom:20px;'></div>"
            "<button class='btn-gold' onclick='reconfigure()'>Reconfigure Network</button>"
            "</div>"));

  // Modal Loader: "Scanning Access Point, please wait ..." (Photo 2)
  c.print(F("<div id='modal-scan' class='modal-overlay' style='display:none;'>"
            "<div class='modal-box'>"
            "<div class='dots-loader'>"
            "<div class='dot'></div><div class='dot'></div><div class='dot'></div>"
            "<div class='dot'></div><div class='dot'></div><div class='dot'></div>"
            "</div>"
            "<p class='scan-text'>Scanning Access Point, please wait ...</p>"
            "</div>"
            "</div>"));

  // Client-Side Logic
  c.print(F("<script>"
            "let selectedSsid = '';"
            "function checkStatus() {"
            "  fetch('/status_json').then(r=>r.json()).then(d=>{"
            "    if (d.connected && d.bridge) {"
            "      document.getElementById('view-modes').style.display = 'none';"
            "      document.getElementById('view-scan').style.display = 'none';"
            "      document.getElementById('view-dashboard').style.display = 'block';"
            "      document.getElementById('stat-sta-ssid').innerText = d.sta_ssid;"
            "      document.getElementById('stat-rssi').innerText = d.rssi + ' dBm';"
            "      document.getElementById('stat-ch').innerText = 'Ch ' + d.channel + ' (' + d.band + ')';"
            "      document.getElementById('stat-ap-ssid').innerText = d.ap_ssid;"
            "      document.getElementById('stat-uptime').innerText = Math.floor(d.uptime_s / 60) + ' min';"
            "      let clHtml = '<ul style=\"list-style:none;\">';"
            "      if (d.clients.length === 0) { clHtml += '<li style=\"color:#a0aec0;font-size:13px;\">No clients currently connected to repeater.</li>'; }"
            "      d.clients.forEach(c=>{"
            "        clHtml += '<li style=\"display:flex;justify-content:space-between;padding:8px;border-bottom:1px solid #edf2f7;\"><span>' + c + '</span><button onclick=\"kick(\\'' + c + '\\')\" style=\"color:#e53e3e;background:none;border:none;cursor:pointer;\">Kick</button></li>';"
            "      });"
            "      clHtml += '</ul>';"
            "      document.getElementById('client-table').innerHTML = clHtml;"
            "    }"
            "  }).catch(e=>{});"
            "}"
            "checkStatus(); setInterval(checkStatus, 6000);"

            "function startRepeaterWizard() {"
            "  document.getElementById('view-modes').style.display = 'none';"
            "  document.getElementById('view-scan').style.display = 'block';"
            "  scanNetworks();"
            "}"

            "function scanNetworks() {"
            "  document.getElementById('modal-scan').style.display = 'flex';"
            "  fetch('/scan_json').then(r=>r.json()).then(d=>{"
            "    document.getElementById('modal-scan').style.display = 'none';"
            "    renderNetworks(d.networks);"
            "  }).catch(e=>{"
            "    document.getElementById('modal-scan').style.display = 'none';"
            "  });"
            "}"

            "function renderNetworks(nets) {"
            "  let html = '';"
            "  nets.forEach(n=>{"
            "    let bars = n.rssi >= -60 ? 4 : (n.rssi >= -72 ? 3 : (n.rssi >= -82 ? 2 : 1));"
            "    html += '<div class=\"net-item\" onclick=\"pickNet(\\'' + encodeURIComponent(n.ssid) + '\\')\">';"
            "    html += '<div style=\"display:flex;align-items:center;\"><strong>' + n.ssid + '</strong></div>';"
            "    html += '<div style=\"display:flex;align-items:center;\"><span style=\"font-size:12px;color:#718096;\">' + n.rssi + ' dBm</span>';"
            "    html += '<div class=\"signal-bar\">';"
            "    for (let i=1; i<=4; i++) { html += '<div class=\"bar-seg ' + (i<=bars?'bar-on':'') + '\" style=\"height:' + (i*3+2) + 'px;\"></div>'; }"
            "    html += '</div></div></div>';"
            "  });"
            "  document.getElementById('network-list').innerHTML = html;"
            "}"

            "function pickNet(s) {"
            "  selectedSsid = decodeURIComponent(s);"
            "  document.getElementById('selected-ssid-title').innerText = 'Selected Network: ' + selectedSsid;"
            "  document.getElementById('inp-ap-ssid').value = selectedSsid + '-EXT';"
            "  document.getElementById('connect-box').style.display = 'block';"
            "  document.getElementById('inp-pass').focus();"
            "}"

            "function showManual() {"
            "  let manualSsid = prompt('Enter Wi-Fi SSID manually:');"
            "  if (manualSsid) pickNet(manualSsid);"
            "}"

            "function submitConnect() {"
            "  let pass = document.getElementById('inp-pass').value;"
            "  let apS = document.getElementById('inp-ap-ssid').value;"
            "  let apP = document.getElementById('inp-ap-pass').value;"
            "  let body = 'ssid=' + encodeURIComponent(selectedSsid) + '&pass=' + encodeURIComponent(pass) + '&ap_ssid=' + encodeURIComponent(apS) + '&ap_pass=' + encodeURIComponent(apP);"
            "  fetch('/save_wifi', {method:'POST', body:body}).then(r=>r.json()).then(d=>{"
            "    alert('Settings saved! BW16 is now connecting to ' + selectedSsid + ' and enabling the L2 Bridge. Please reconnect your device to: ' + apS);"
            "    window.location.reload();"
            "  });"
            "}"

            "function kick(mac) {"
            "  fetch('/kick', {method:'POST', body:'mac=' + mac}).then(()=>checkStatus());"
            "}"

            "function reconfigure() {"
            "  document.getElementById('view-dashboard').style.display = 'none';"
            "  document.getElementById('view-modes').style.display = 'block';"
            "}"

            "function startApWizard() {"
            "  let apName = prompt('Enter Access Point SSID:', 'BW16-Hotspot');"
            "  if (apName) {"
            "    let apPass = prompt('Enter Access Point Password (min 8 chars):', '12345678');"
            "    let body = 'ssid=&pass=&ap_ssid=' + encodeURIComponent(apName) + '&ap_pass=' + encodeURIComponent(apPass);"
            "    fetch('/save_wifi', {method:'POST', body:body}).then(()=>window.location.reload());"
            "  }"
            "}"
            "</script></body></html>"));
}
