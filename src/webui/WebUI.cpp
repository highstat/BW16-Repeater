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
#include "../utils/LedStatus.h"

extern "C" {
  void sys_reset(void);
  void BKUP_Write(uint32_t DwordIdx, uint32_t WriteVal);
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

static String jsonEscape(const char* s) {
  String out = "";
  if (!s) return out;
  while (*s) {
    if (*s == '"') out += "\\\"";
    else if (*s == '\\') out += "\\\\";
    else if (*s == '\b') out += "\\b";
    else if (*s == '\f') out += "\\f";
    else if (*s == '\n') out += "\\n";
    else if (*s == '\r') out += "\\r";
    else if (*s == '\t') out += "\\t";
    else if ((unsigned char)*s < 32) {} // skip control characters
    else out += *s;
    s++;
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

  // Read body if POST (case-insensitive header & robust read)
  int clPos = headers.indexOf("Content-Length:");
  if (clPos < 0) clPos = headers.indexOf("content-length:");
  if (clPos < 0) clPos = headers.indexOf("Content-length:");
  int contentLen = 0;
  if (clPos >= 0) {
    int endLine = headers.indexOf("\r\n", clPos);
    if (endLine > clPos) {
      String v = headers.substring(clPos + 15, endLine);
      v.trim();
      contentLen = v.toInt();
    }
  }
  if (contentLen > 2048) {
    c.print(F("HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n"));
    c.stop();
    return;
  }
  String body = "";
  if (reqLine.startsWith("POST")) {
    body.reserve(contentLen > 0 ? contentLen : 256);
    unsigned long t2 = millis();
    while (c.connected() && (contentLen > 0 ? (int)body.length() < contentLen : millis() - t2 < 400) && millis() - t2 < 1500) {
      while (c.available()) {
        body += char(c.read());
        if (contentLen > 0 && (int)body.length() >= contentLen) break;
      }
      delay(2);
    }
  }

  if (reqLine.startsWith("POST") && contentLen > 0 && (int)body.length() != contentLen) {
    c.print(F("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nIncomplete request body"));
    c.stop();
    return;
  }

  // API: Live Status JSON
  if (reqLine.startsWith("GET /status_json")) {
    int rssi = wifiRef->getRouterRssi();
    int ch = wifiRef->getActiveChannel();
    ConnectedClient clients[8];
    int clientCount = wifiRef->getAssociatedClients(clients, 8);

    bool isBridge = bridgeRef ? bridgeRef->isBridgeActive() : false;
    bool isApMode = (store->get().mode == RepeaterMode::AP);
    bool isConnected = wifiRef->staConnected() || isApMode;
    bool isAssociated = wifiRef->staAssociated();
    IPAddress curIp = isBridge ? wifiRef->bridgeIP() : wifiRef->apIP();
    if (curIp[0] == 0) curIp = wifiRef->apIP();

    String json = "{";
    json += "\"mode\":\"" + String(isApMode ? "ap" : "repeater") + "\",";
    json += "\"connected\":" + String(isConnected ? "true" : "false") + ",";
    json += "\"associated\":" + String(isAssociated ? "true" : "false") + ",";
    json += "\"bridge\":" + String(isBridge ? "true" : "false") + ",";
    String statusMessage = wifiRef->lastStatusMsg();
    json += "\"status_message\":\"" + jsonEscape(statusMessage.c_str()) + "\",";
    json += "\"sta_ssid\":\"" + jsonEscape(store->get().staSsid) + "\",";
    json += "\"sta_ip\":\"" + ipToStr(curIp) + "\",";
    json += "\"ap_ssid\":\"" + jsonEscape(store->get().apSsid) + "\",";
    json += "\"ap_ip\":\"" + ipToStr(wifiRef->apIP()) + "\",";
    json += "\"channel\":" + String(ch) + ",";
    json += "\"band\":\"" + String(ch > 14 ? "5 GHz" : "2.4 GHz") + "\",";
    json += "\"rssi\":" + String(rssi) + ",";
    json += "\"tx_boost\":true,";
    json += "\"led_mode\":" + String(store->get().ledMode) + ",";
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
    bool forceRefresh = (reqLine.indexOf("refresh=1") > 0);
    if (wifiRef) {
      if (forceRefresh || (!wifiRef->isScanning() && wifiRef->getCachedCount() == 0)) {
        wifiRef->triggerScan();
      }
    }
    ScannedNetwork nets[36];
    int n = wifiRef ? wifiRef->getCachedScanResults(nets, 36) : 0;
    bool scanning = wifiRef ? wifiRef->isScanning() : false;

    String json = "{\"scanning\":" + String(scanning ? "true" : "false") + ",\"networks\":[";
    for (int idx = 0; idx < n; idx++) {
      json += "{";
      json += "\"ssid\":\"" + jsonEscape(nets[idx].ssid) + "\",";
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

  // API: Live LED Color / Mode change
  if (reqLine.startsWith("POST /set_led")) {
    String mStr = getParam(body, "mode");
    uint8_t m = (uint8_t)mStr.toInt();
    if (m > 8) m = 0;
    store->mutableRef().ledMode = m;
    store->save();
    LedStatus::setConfig(m);
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}"));
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
    BKUP_Write(3, 0);
    delay(300);
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
    String secMode = getParam(body, "ap_sec_mode");
    String apP = getParam(body, "ap_pass");
    String chStr = getParam(body, "channel");
    String ledStr = getParam(body, "led_mode");
    int ch = chStr.toInt();
    if (ch <= 0 || ch > 165) ch = 1;
    uint8_t ledM = (uint8_t)ledStr.toInt();
    if (ledM > 8) ledM = 0;

    // Reject invalid bytes before changing or persisting any Wi-Fi settings.
    if (s.length() == 0 || s.length() > 32 || apS.length() > 32 ||
        (apS.length() == 0 && s.length() + 4 > 32) ||
        (p.length() != 0 && (p.length() < 8 || p.length() > 63)) ||
        (secMode == "custom" && (apP.length() < 8 || apP.length() > 63))) {
      c.print(F("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nInvalid Wi-Fi settings"));
      c.stop();
      return;
    }
    store->mutableRef().staStatic = false;
    store->mutableRef().mode = RepeaterMode::BRIDGE;
    store->mutableRef().apChannel = (uint8_t)ch;
    store->mutableRef().ledMode = ledM;
    LedStatus::setConfig(ledM);

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

    if (secMode == "open") {
      store->mutableRef().apPass[0] = 0;
    } else if (secMode == "custom") {
      if (apP.length() >= 8) {
        strncpy(store->mutableRef().apPass, apP.c_str(), 63);
        store->mutableRef().apPass[63] = 0;
      } else {
        store->mutableRef().apPass[0] = 0;
      }
    } else { // "same" or legacy fallback
      if (secMode == "same") {
        if (p.length() >= 8) {
          strncpy(store->mutableRef().apPass, p.c_str(), 63);
          store->mutableRef().apPass[63] = 0;
        } else {
          store->mutableRef().apPass[0] = 0;
        }
      } else {
        if (apP.length() >= 8) {
          strncpy(store->mutableRef().apPass, apP.c_str(), 63);
          store->mutableRef().apPass[63] = 0;
        } else {
          store->mutableRef().apPass[0] = 0;
        }
      }
    }

    if (!store->save()) {
      c.print(F("HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\n\r\nConfiguration storage verification failed"));
      c.stop();
      return;
    }
    c.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"ok\"}"));
    c.flush();
    delay(50);
    c.stop();
    BKUP_Write(3, 0);
    delay(300);
    sys_reset();
    return;
  }

  // API: Save Standalone AP Configuration (AP Mode)
  if (reqLine.startsWith("POST /save_ap")) {
    String apS = getParam(body, "ap_ssid");
    String apP = getParam(body, "ap_pass");
    String chStr = getParam(body, "channel");
    String ledStr = getParam(body, "led_mode");
    int ch = chStr.toInt();
    if (ch <= 0 || ch > 165) ch = 1;
    uint8_t ledM = (uint8_t)ledStr.toInt();
    if (ledM > 8) ledM = 0;

    store->mutableRef().mode = RepeaterMode::AP;
    store->mutableRef().staSsid[0] = 0;
    store->mutableRef().staPass[0] = 0;
    store->mutableRef().apChannel = (uint8_t)ch;
    store->mutableRef().ledMode = ledM;
    LedStatus::setConfig(ledM);

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
    BKUP_Write(3, 0);
    delay(300);
    sys_reset();
    return;
  }

  // Serve Main Page (HTML/CSS/JS)
  c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: public, max-age=300\r\nConnection: close\r\n\r\n"));
  c.print(R"BW16(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#be8d36"><title>BW16 Repeater</title>
<style>
:root{--bg:#f4f6f9;--surface:#fff;--text:#2b220e;--muted:#626978;--line:#dcdfe4;--accent:#be8d36;--accent-dark:#9f7324;--accent-soft:#f8ecd2;--danger:#b42318;--ok:#237a4b;--radius:14px}
*{box-sizing:border-box}html{background:var(--bg)}body{margin:0;color:var(--text);background:var(--bg);font:16px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;-webkit-font-smoothing:antialiased}
button,input{font:inherit}button{touch-action:manipulation}.shell{width:100%;max-width:652px;margin:0 auto;padding:32px 16px calc(48px + env(safe-area-inset-bottom))}
header{display:flex;align-items:center;gap:12px;margin-bottom:30px}.brand-icon{width:42px;height:42px;border-radius:50%;display:grid;place-items:center;background:var(--surface);color:var(--accent);box-shadow:0 3px 12px rgba(43,34,14,.13)}.brand-icon svg{width:24px;height:24px}
h1{font-size:20px;line-height:1.2;letter-spacing:-.02em;margin:0}h2{font-size:28px;line-height:1.16;letter-spacing:-.03em;margin:0 0 10px}p{margin:0}.lead{color:var(--muted);max-width:46ch;margin-bottom:28px}
.panel{background:var(--surface);border:1px solid var(--line);border-radius:var(--radius);padding:22px}.row{display:flex;align-items:center;justify-content:space-between;gap:12px}
.label{display:block;font-size:14px;font-weight:650;margin:0 0 7px}.field{margin-top:18px}input{width:100%;height:48px;border:1px solid #c9cdd5;border-radius:10px;padding:0 13px;color:var(--text);background:#fff;outline:none}
input:focus-visible,button:focus-visible{outline:3px solid #e7ca91;outline-offset:2px;border-color:var(--accent)}.hint{color:var(--muted);font-size:13px;margin-top:7px}
.icon{width:18px;height:18px;flex:0 0 auto;stroke:currentColor;stroke-width:2;fill:none;stroke-linecap:round;stroke-linejoin:round}.scan-head{margin-bottom:10px}.scan-head strong{font-size:14px}.link{min-height:44px;border:0;background:transparent;color:#7d5818;font-weight:700;padding:8px 2px;cursor:pointer;display:inline-flex;align-items:center;gap:7px}
.networks{border:1px solid var(--line);border-radius:10px;overflow:hidden;min-height:54px}.network{width:100%;min-height:56px;border:0;border-bottom:1px solid var(--line);background:#fff;padding:10px 12px;display:flex;align-items:center;justify-content:space-between;gap:12px;text-align:left;cursor:pointer}
.network:last-child{border-bottom:0}.network:hover{background:#fffaf0}.network[aria-pressed="true"]{background:var(--accent-soft);color:#6d4b12}.network:disabled{color:#9297a1;background:#fafafa;cursor:not-allowed}.network-main{min-width:0;display:flex;align-items:center;gap:10px}.network-main .icon{color:var(--accent)}.ssid{font-weight:650;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.meta{color:var(--muted);font-size:12px;white-space:nowrap}.empty{padding:16px;color:var(--muted);font-size:14px}
details{margin-top:20px;border-top:1px solid var(--line);padding-top:16px}summary{cursor:pointer;color:var(--muted);font-size:14px;font-weight:650;min-height:44px;display:flex;align-items:center}
.primary{width:100%;min-height:50px;margin-top:22px;border:1px solid var(--accent-dark);border-radius:10px;background:linear-gradient(#deb052,var(--accent));color:#fff;font-weight:750;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px;box-shadow:0 3px 8px rgba(159,115,36,.18)}.primary:hover{background:linear-gradient(#e5b85a,#b9842f)}.primary:disabled{opacity:.6;cursor:wait}
.message{display:none;margin-top:14px;padding:11px 12px;border-radius:9px;font-size:14px}.message.error{display:block;color:var(--danger);background:#fff0ee}.message.ok{display:block;color:var(--ok);background:#eaf8f1}
.status{display:none}.status-grid{display:grid;grid-template-columns:1fr 1fr;gap:18px;margin:22px 0}.value{font-weight:700;margin-top:3px;word-break:break-word}.badge{display:inline-flex;align-items:center;gap:6px;min-height:32px;padding:0 10px;border-radius:999px;background:#eaf8f1;color:var(--ok);font-size:12px;font-weight:750}.badge.offline{background:#fff0ee;color:var(--danger)}
.secondary{min-height:46px;border:1px solid var(--line);border-radius:10px;background:#fff;color:var(--text);padding:0 14px;font-weight:650;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:7px}.danger{color:var(--danger)}
.foot{margin-top:18px;color:var(--muted);font-size:12px;text-align:center}@media(max-width:480px){.shell{padding:20px 10px calc(36px + env(safe-area-inset-bottom))}.panel{padding:17px 14px}h2{font-size:24px}.status-grid{grid-template-columns:1fr}.actions{flex-direction:column}.actions .secondary{width:100%}.network{padding:10px}.meta{white-space:normal;text-align:right;max-width:42%}}
@media(pointer:coarse){.link,.network,.secondary{min-height:48px}}
@media(prefers-reduced-motion:no-preference){button{transition:background-color .15s ease,opacity .15s ease}}
::selection{background:#f0d9a9;color:#2b220e}
</style></head><body><main class="shell"><header><span class="brand-icon" aria-hidden="true"><svg viewBox="0 0 24 24" class="icon"><path d="M5 12.6a10.7 10.7 0 0 1 14 0M8.5 16a5.7 5.7 0 0 1 7 0M2 9.2a15.8 15.8 0 0 1 20 0"/><circle cx="12" cy="19" r="1" fill="currentColor" stroke="none"/></svg></span><h1>BW16 Repeater</h1></header>
<section id="setup"><h2>Connect to Wi-Fi</h2><p class="lead">Choose your home network and name the extended network.</p>
<div class="panel"><div class="row scan-head"><strong>Available networks</strong><button class="link" id="refresh" type="button"><svg class="icon" viewBox="0 0 24 24"><path d="M20 11a8 8 0 1 0-2.3 5.7M20 4v7h-7"/></svg><span>Refresh</span></button></div>
<div class="networks" id="networks" aria-live="polite"><div class="empty">Scanning for networks…</div></div>
<form id="form"><div class="field"><label class="label" for="ssid">Home network</label><input id="ssid" name="ssid" autocomplete="off" maxlength="63" required placeholder="Wi-Fi name (SSID)"></div>
<div class="field"><label class="label" for="pass">Home Wi-Fi password</label><input id="pass" name="pass" type="password" autocomplete="current-password" maxlength="63" placeholder="Leave empty for an open network"></div>
<details><summary>Advanced settings</summary><div class="field"><label class="label" for="ap-ssid">Extended network name</label><input id="ap-ssid" maxlength="63" placeholder="Adds -EXT by default"><p class="hint">The extended network uses the same password.</p></div></details>
<div id="message" class="message" role="status"></div><button class="primary" id="submit" type="submit"><svg class="icon" viewBox="0 0 24 24"><path d="M5 12h14M14 7l5 5-5 5"/></svg><span>Connect repeater</span></button></form></div></section>
<section id="status" class="status"><div class="row"><div><h2 id="status-title">Repeater online</h2><p id="status-copy" class="lead" style="margin:6px 0 0">The bridge is active.</p></div><span class="badge" id="status-badge"><svg class="icon" viewBox="0 0 24 24"><path d="m6 12 4 4 8-8"/></svg><span>Online</span></span></div>
<div class="panel" style="margin-top:22px"><div class="status-grid"><div><span class="hint">Home network</span><div class="value" id="st-home">—</div></div><div><span class="hint">Extended network</span><div class="value" id="st-ap">—</div></div><div><span class="hint">Bridge IP address</span><div class="value" id="st-ip">—</div></div><div><span class="hint">Radio channel</span><div class="value" id="st-channel">—</div></div></div>
<div class="row actions"><button class="secondary" id="configure" type="button"><svg class="icon" viewBox="0 0 24 24"><path d="M12 15.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Z"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.9l.1.1-2.8 2.8-.1-.1a1.7 1.7 0 0 0-1.9-.3 1.7 1.7 0 0 0-1 1.6v.2h-4V21a1.7 1.7 0 0 0-1-1.6 1.7 1.7 0 0 0-1.9.3l-.1.1L4.2 17l.1-.1a1.7 1.7 0 0 0 .3-1.9A1.7 1.7 0 0 0 3 14H2.8v-4H3a1.7 1.7 0 0 0 1.6-1 1.7 1.7 0 0 0-.3-1.9L4.2 7 7 4.2l.1.1A1.7 1.7 0 0 0 9 4.6a1.7 1.7 0 0 0 1-1.6v-.2h4V3a1.7 1.7 0 0 0 1 1.6 1.7 1.7 0 0 0 1.9-.3l.1-.1L19.8 7l-.1.1a1.7 1.7 0 0 0-.3 1.9 1.7 1.7 0 0 0 1.6 1h.2v4H21a1.7 1.7 0 0 0-1.6 1Z"/></svg><span>Change settings</span></button><button class="secondary danger" id="reset" type="button"><svg class="icon" viewBox="0 0 24 24"><path d="M4 7h16M9 7V4h6v3M7 7l1 13h8l1-13M10 11v5M14 11v5"/></svg><span>Factory reset</span></button></div><div id="reset-msg" class="message" role="status"></div></div></section>
<p class="foot">5 GHz is supported on channels 36–48 only.</p></main>
<script>
var selectedChannel=1,resetArmed=false,scanTimer=0;
function el(id){return document.getElementById(id)}
function message(text,type){var m=el('message');m.textContent=text;m.className='message '+type}
function render(nets,scanning){var box=el('networks');box.textContent='';nets=(nets||[]).sort(function(a,b){return b.rssi-a.rssi}).slice(0,14);if(!nets.length){var e=document.createElement('div');e.className='empty';e.textContent=scanning?'Scanning for networks…':'No networks found';box.appendChild(e);return}nets.forEach(function(n){var blocked=n.channel>48,b=document.createElement('button'),main=document.createElement('span'),name=document.createElement('span'),meta=document.createElement('span');b.type='button';b.className='network';b.disabled=blocked;b.setAttribute('aria-pressed','false');main.className='network-main';main.innerHTML='<svg class="icon" viewBox="0 0 24 24"><path d="M5 12.6a10.7 10.7 0 0 1 14 0M8.5 16a5.7 5.7 0 0 1 7 0M2 9.2a15.8 15.8 0 0 1 20 0"/><circle cx="12" cy="19" r="1" fill="currentColor" stroke="none"/></svg>';name.className='ssid';name.textContent=n.ssid||'Hidden network';main.appendChild(name);meta.className='meta';meta.textContent=blocked?'Channel '+n.channel+' unsupported':'Channel '+n.channel+' · '+(n.rssi||0)+' dBm';b.appendChild(main);b.appendChild(meta);b.onclick=function(){document.querySelectorAll('.network').forEach(function(x){x.setAttribute('aria-pressed','false')});b.setAttribute('aria-pressed','true');el('ssid').value=n.ssid;el('ap-ssid').value=n.ssid+'-EXT';selectedChannel=n.channel;el('pass').focus()};box.appendChild(b)})}
function scan(force){el('refresh').disabled=true;fetch('/scan_json'+(force?'?refresh=1':'')).then(function(r){return r.json()}).then(function(d){render(d.networks,d.scanning);el('refresh').disabled=false;if(d.scanning){clearTimeout(scanTimer);scanTimer=setTimeout(function(){scan(false)},1200)}}).catch(function(){el('refresh').disabled=false;render([],false)})}
function showStatus(d){var online=d.connected&&d.bridge;el('setup').style.display='none';el('status').style.display='block';el('status-title').textContent=online?'Repeater online':'Upstream offline';el('status-copy').textContent=online?'The bridge is active and internet traffic can pass through.':(d.status_message||'Check the home Wi-Fi password and channel.');var badge=el('status-badge'),ico=badge.querySelector('svg');badge.className='badge'+(online?'':' offline');badge.querySelector('span').textContent=online?'Online':'No internet';ico.innerHTML=online?'<path d="m6 12 4 4 8-8"/>':'<circle cx="12" cy="12" r="9"/><path d="M12 7v6M12 17h.01"/>';el('st-home').textContent=d.sta_ssid||'—';el('st-ap').textContent=d.ap_ssid||'—';el('st-ip').textContent=online?d.sta_ip:'Waiting for upstream DHCP';el('st-channel').textContent='Channel '+d.channel+' · '+d.band}
function check(){fetch('/status_json').then(function(r){return r.json()}).then(function(d){if(d.connected||d.sta_ssid)showStatus(d);else scan(false)}).catch(function(){scan(false)})}
el('refresh').onclick=function(){scan(true)};el('configure').onclick=function(){el('status').style.display='none';el('setup').style.display='block';scan(false)};
el('form').onsubmit=function(ev){ev.preventDefault();var s=el('ssid').value.trim(),p=el('pass').value,a=el('ap-ssid').value.trim()||s+'-EXT';if(!s){message('Choose or enter your home network.','error');el('ssid').focus();return}if(p&&p.length<8){message('The Wi-Fi password must contain at least 8 characters.','error');el('pass').focus();return}var btn=el('submit');btn.disabled=true;btn.querySelector('span').textContent='Saving…';message('Saving settings…','');var body='ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)+'&ap_ssid='+encodeURIComponent(a)+'&ap_sec_mode=same&ap_pass=&channel='+selectedChannel+'&led_mode=0';fetch('/save_wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body}).then(function(r){if(!r.ok)throw new Error('Save failed');message('Settings saved. After restart, connect to '+a+'.','ok')}).catch(function(){message('Could not confirm saving. Reconnect and check settings.','error');btn.disabled=false;btn.querySelector('span').textContent='Save and connect'})};
el('reset').onclick=function(){var label=el('reset').querySelector('span');if(!resetArmed){resetArmed=true;label.textContent='Press again to reset';var m=el('reset-msg');m.textContent='This will erase all saved Wi-Fi settings.';m.className='message error';setTimeout(function(){resetArmed=false;label.textContent='Factory reset';m.className='message'},4000);return}fetch('/reset',{method:'POST'});el('reset').disabled=true;label.textContent='Resetting…'};
check();
</script></body></html>)BW16");
  c.flush();
  delay(10);
  c.stop();
  return;

#if 0
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
            ".filter-btn {padding:5px 12px;font-size:11px;font-weight:700;background:#edf2f7;border:1px solid #cbd5e1;border-radius:4px;color:#4a5568;cursor:pointer;transition:all 0.15s;}"
            ".filter-btn.filter-active {background:#be8d36;color:#fff;border-color:#9f7324;}"
            ".badge-band {font-size:11px;padding:2px 8px;border-radius:4px;font-weight:700;margin-left:8px;display:inline-block;}"
            ".badge-5g {background:#ebf8ff;color:#2b6cb0;border:1px solid #bee3f8;}"
            ".badge-24g {background:#feebc8;color:#c05621;border:1px solid #fbd38d;}"
            ".badge-sec {font-size:10px;padding:2px 6px;border-radius:3px;font-weight:600;background:#edf2f7;color:#4a5568;border:1px solid #cbd5e1;margin-left:6px;}"
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
            "<div style='display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;flex-wrap:wrap;gap:8px;'>"
            "<div>"
            "<h2>Select Wi-Fi Network to Amplify</h2>"
            "<p class='sub'>Connects to your home router and creates an extended network with L2 bridging.</p>"
            "</div>"
            "<button class='btn-gold' style='padding:6px 14px;font-size:12px;' onclick='scanNetworks(true)'>Refresh Scan</button>"
            "</div>"
            "<div style='display:flex;gap:6px;margin-bottom:8px;'>"
            "<button type='button' class='filter-btn filter-active' id='flt-all' onclick='setBandFilter(\"all\")'>All Networks</button>"
            "<button type='button' class='filter-btn' id='flt-24' onclick='setBandFilter(\"2.4GHz\")'>2.4 GHz Only</button>"
            "<button type='button' class='filter-btn' id='flt-5' onclick='setBandFilter(\"5GHz\")'>5 GHz Only</button>"
            "</div>"
            "<div id='scan-status' style='display:none;' class='dots-loader'><div class='dot'></div><div class='dot'></div><div class='dot'></div><div class='dot'></div></div>"
            "<div id='network-list' style='max-height:230px;overflow-y:auto;border:1px solid #edf2f7;border-radius:6px;margin-bottom:16px;'></div>"
            "<div id='rep-form'>"
            "<div id='form-err' style='color:#c53030;font-size:12px;margin-bottom:12px;display:none;background:#fff5f5;padding:8px 12px;border-radius:6px;border:1px solid #feb2b2;'></div>"
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
            "<label style='margin-bottom:6px;'>Repeater Security & Password:</label>"
            "<div style='display:flex;flex-direction:column;gap:8px;background:#f8fafc;padding:12px;border:1px solid #e2e8f0;border-radius:6px;margin-bottom:8px;'>"
            "<label style='display:flex;align-items:center;font-weight:600;font-size:13px;cursor:pointer;'>"
            "<input type='radio' name='rep_sec' id='sec-same' value='same' checked onchange='onSecModeChange()' style='width:auto;margin-right:8px;'>"
            "<span>Use same password as Home Wi-Fi (Default)</span>"
            "</label>"
            "<label style='display:flex;align-items:center;font-weight:600;font-size:13px;cursor:pointer;'>"
            "<input type='radio' name='rep_sec' id='sec-open' value='open' onchange='onSecModeChange()' style='width:auto;margin-right:8px;'>"
            "<span>Open network (No password)</span>"
            "</label>"
            "<label style='display:flex;align-items:center;font-weight:600;font-size:13px;cursor:pointer;'>"
            "<input type='radio' name='rep_sec' id='sec-custom' value='custom' onchange='onSecModeChange()' style='width:auto;margin-right:8px;'>"
            "<span>Custom password</span>"
            "</label>"
            "</div>"
            "<div id='grp-custom-pass' style='display:none;margin-top:6px;'>"
            "<input type='password' id='inp-ap-pass' placeholder='Enter custom password (min 8 chars)'>"
            "</div>"
            "</div>"
            "<div class='input-group'>"
            "<label>Radio Channel & Band:</label>"
            "<select id='sel-channel' onchange='onChannelSelectChange()'>"
            "<option value='1'>2.4 GHz - Channel 1</option>"
            "<option value='2'>2.4 GHz - Channel 2</option>"
            "<option value='3'>2.4 GHz - Channel 3</option>"
            "<option value='4'>2.4 GHz - Channel 4</option>"
            "<option value='5'>2.4 GHz - Channel 5</option>"
            "<option value='6'>2.4 GHz - Channel 6</option>"
            "<option value='7'>2.4 GHz - Channel 7</option>"
            "<option value='8'>2.4 GHz - Channel 8</option>"
            "<option value='9'>2.4 GHz - Channel 9</option>"
            "<option value='10'>2.4 GHz - Channel 10</option>"
            "<option value='11'>2.4 GHz - Channel 11</option>"
            "<option value='12'>2.4 GHz - Channel 12</option>"
            "<option value='13'>2.4 GHz - Channel 13</option>"
            "<option value='36'>5 GHz - Channel 36</option>"
            "<option value='40'>5 GHz - Channel 40</option>"
            "<option value='44'>5 GHz - Channel 44</option>"
            "<option value='48'>5 GHz - Channel 48</option>"
            "<option value='52'>5 GHz - Channel 52</option>"
            "<option value='56'>5 GHz - Channel 56</option>"
            "<option value='60'>5 GHz - Channel 60</option>"
            "<option value='64'>5 GHz - Channel 64</option>"
            "<option value='149'>5 GHz - Channel 149</option>"
            "<option value='153'>5 GHz - Channel 153</option>"
            "<option value='157'>5 GHz - Channel 157</option>"
            "<option value='161'>5 GHz - Channel 161</option>"
            "<option value='165'>5 GHz - Channel 165</option>"
            "</select>"
            "<div id='chan-hint' style='display:none;font-size:12px;color:#2b6cb0;background:#ebf8ff;border:1px solid #bee3f8;padding:8px 10px;border-radius:6px;margin-top:6px;'></div>"
            "</div>"
            "<div class='input-group'>"
            "<label>Onboard RGB LED:</label>"
            "<select id='sel-led-mode'>"
            "<option value='0'>Auto (Telemetry: Red/Blue/Green)</option>"
            "<option value='1'>Off (Dark / Sleep Mode)</option>"
            "<option value='2'>Solid Green</option>"
            "<option value='3'>Solid Blue</option>"
            "<option value='4'>Solid Red</option>"
            "<option value='5'>Solid Cyan</option>"
            "<option value='6'>Solid Magenta</option>"
            "<option value='7'>Solid Yellow</option>"
            "<option value='8'>Solid White</option>"
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
            "<option value='149'>5 GHz - Channel 149</option>"
            "<option value='153'>5 GHz - Channel 153</option>"
            "<option value='157'>5 GHz - Channel 157</option>"
            "</select>"
            "</div>"
            "<div class='input-group'>"
            "<label>Onboard RGB LED:</label>"
            "<select id='sel-hotspot-led'>"
            "<option value='0'>Auto (Telemetry: Red/Blue/Green)</option>"
            "<option value='1'>Off (Dark / Sleep Mode)</option>"
            "<option value='2'>Solid Green</option>"
            "<option value='3'>Solid Blue</option>"
            "<option value='4'>Solid Red</option>"
            "<option value='5'>Solid Cyan</option>"
            "<option value='6'>Solid Magenta</option>"
            "<option value='7'>Solid Yellow</option>"
            "<option value='8'>Solid White</option>"
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
            "<div class='stat-box'><h4>Onboard LED</h4><select id='dash-led-mode' onchange='changeLedLive(this.value)' style='width:100%;padding:6px;margin-top:4px;font-size:12px;font-weight:700;border:1px solid #cbd5e1;border-radius:4px;background:#fff;cursor:pointer;'><option value='0'>Auto (Status)</option><option value='1'>Off (Dark)</option><option value='2'>Green</option><option value='3'>Blue</option><option value='4'>Red</option><option value='5'>Cyan</option><option value='6'>Magenta</option><option value='7'>Yellow</option><option value='8'>White</option></select></div>"
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

let currentBandFilter = 'all';
window.allNetworks = [];

function setBandFilter(b) {
  currentBandFilter = b;
  document.getElementById('flt-all').className = 'filter-btn' + (b==='all'?' filter-active':'');
  document.getElementById('flt-24').className = 'filter-btn' + (b==='2.4GHz'?' filter-active':'');
  document.getElementById('flt-5').className = 'filter-btn' + (b==='5GHz'?' filter-active':'');
  renderNetworks(window.allNetworks || []);
}

function checkStatus() {
  fetch('/status_json').then(r=>r.json()).then(d=>{
    if (d.led_mode !== undefined) {
      let dl = document.getElementById('dash-led-mode');
      if (dl && document.activeElement !== dl) dl.value = String(d.led_mode);
      let sl = document.getElementById('sel-led-mode');
      if (sl && document.activeElement !== sl) sl.value = String(d.led_mode);
    }
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

function changeLedLive(val) {
  fetch('/set_led', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'mode=' + encodeURIComponent(val)
  }).catch(e=>{});
}

let scanPollTimer = null;
function scanNetworks(force) {
  let url = force ? '/scan_json?refresh=1' : '/scan_json';
  document.getElementById('scan-status').style.display = 'flex';
  if (scanPollTimer) clearTimeout(scanPollTimer);
  fetch(url).then(r=>r.json()).then(d=>{
    let nets = (d.networks || []).sort((a,b)=>b.rssi - a.rssi);
    window.allNetworks = nets;
    renderNetworks(nets);
    if (d.scanning) {
      document.getElementById('scan-status').style.display = 'flex';
      scanPollTimer = setTimeout(()=>{ scanNetworks(false); }, 1500);
    } else {
      document.getElementById('scan-status').style.display = 'none';
    }
  }).catch(e=>{
    document.getElementById('scan-status').style.display = 'none';
  });
}

function renderNetworks(nets) {
  let filtered = nets;
  if (currentBandFilter === '2.4GHz') filtered = nets.filter(n=>n.channel <= 14);
  else if (currentBandFilter === '5GHz') filtered = nets.filter(n=>n.channel > 14);

  let html = '';
  if (filtered.length === 0) {
    html = '<div style="padding:14px;text-align:center;color:#718096;font-size:12px;">' + (nets.length > 0 ? 'No networks in this band. Try another filter.' : 'Scanning Wi-Fi networks... Click "Refresh Scan" if taking long.') + '</div>';
  } else {
    filtered.forEach(n=>{
      let bars = n.rssi >= -60 ? 4 : (n.rssi >= -72 ? 3 : (n.rssi >= -82 ? 2 : 1));
      let bandLabel = n.channel > 14 ? '5 GHz' : '2.4 GHz';
      let badgeClass = n.channel > 14 ? 'badge-5g' : 'badge-24g';
      let secLabel = n.enc ? 'WPA2' : 'OPEN';
      html += '<div class="net-item" data-ssid="' + esc(n.ssid) + '" data-ch="' + (n.channel||1) + '" onclick="pickNet(this)">';
      html += '<div style="display:flex;align-items:center;flex-wrap:wrap;gap:4px;"><strong>' + esc(n.ssid) + '</strong><span class="badge-band ' + badgeClass + '">Ch ' + n.channel + ' (' + bandLabel + ')</span><span class="badge-sec">' + secLabel + '</span></div>';
      html += '<div style="display:flex;align-items:center;"><span style="font-size:11px;color:#718096;margin-right:6px;">' + n.rssi + ' dBm</span>';
      html += '<div class="signal-bar">';
      for (let j=1; j<=4; j++) { html += '<div class="bar-seg ' + (j<=bars?'bar-on':'') + '" style="height:' + (j*3+2) + 'px;"></div>'; }
      html += '</div></div></div>';
    });
  }
  document.getElementById('network-list').innerHTML = html;
}

// Auto-scan immediately on portal load
scanNetworks();

function onChannelSelectChange() {
  let sel = document.getElementById('sel-channel');
  let ch = sel ? parseInt(sel.value) : 1;
  let hint = document.getElementById('chan-hint');
  if (hint) {
    let band = ch > 14 ? '5 GHz' : '2.4 GHz';
    hint.innerHTML = '<strong>Configured Channel:</strong> Ch ' + ch + ' (' + band + '). Make sure your home router is also on this band.';
    hint.style.display = 'block';
  }
}

function pickNet(el) {
  let s = el.getAttribute('data-ssid');
  let ch = parseInt(el.getAttribute('data-ch')) || 1;
  document.getElementById('inp-sta-ssid').value = s;
  document.getElementById('inp-ap-ssid').value = s + '-EXT';
  let sel = document.getElementById('sel-channel');
  if (sel) {
    let found = false;
    for (let i = 0; i < sel.options.length; i++) {
      if (sel.options[i].value == String(ch)) { found = true; break; }
    }
    if (!found) {
      let opt = document.createElement('option');
      opt.value = String(ch);
      opt.innerText = (ch > 14 ? '5 GHz - Channel ' : '2.4 GHz - Channel ') + ch;
      sel.appendChild(opt);
    }
    sel.value = String(ch);
  }
  let hint = document.getElementById('chan-hint');
  if (hint) {
    let band = ch > 14 ? '5 GHz' : '2.4 GHz';
    let warn = '';
    if (ch > 14 && ch > 48) {
      warn = '<br><span style="color:#c53030;font-weight:700;">Note: 5 GHz channels > 48 (e.g. 149-165) have limited SoftAP support on BW16. Connecting to 2.4 GHz (Ch 1-13) or 5 GHz (Ch 36-48) is recommended.</span>';
    }
    hint.innerHTML = '<strong>Locked to Router Channel:</strong> Ch ' + ch + ' (' + band + '). Extender AP will broadcast on this exact channel.' + warn;
    hint.style.display = 'block';
  }
  document.getElementById('inp-sta-pass').focus();
}

function onSecModeChange() {
  let isCustom = document.getElementById('sec-custom') && document.getElementById('sec-custom').checked;
  let grp = document.getElementById('grp-custom-pass');
  if (grp) grp.style.display = isCustom ? 'block' : 'none';
  if (isCustom) {
    let inp = document.getElementById('inp-ap-pass');
    if (inp) inp.focus();
  }
}

function submitRepeater() {
  let err = document.getElementById('form-err');
  if (err) { err.style.display = 'none'; err.innerText = ''; }
  let s = document.getElementById('inp-sta-ssid').value.trim();
  let p = document.getElementById('inp-sta-pass').value;
  let apS = document.getElementById('inp-ap-ssid').value.trim();
  let ch = document.getElementById('sel-channel').value;
  let led = document.getElementById('sel-led-mode') ? document.getElementById('sel-led-mode').value : '0';

  let secMode = 'same';
  if (document.getElementById('sec-open') && document.getElementById('sec-open').checked) secMode = 'open';
  else if (document.getElementById('sec-custom') && document.getElementById('sec-custom').checked) secMode = 'custom';

  let apP = '';
  if (secMode === 'custom') {
    let inp = document.getElementById('inp-ap-pass');
    apP = inp ? inp.value : '';
    if (apP.length < 8) {
      if (err) { err.innerText = 'Custom repeater password must be at least 8 characters'; err.style.display = 'block'; }
      if (inp) inp.focus();
      return;
    }
  }

  if (!s) {
    if (err) { err.innerText = 'Please select or enter home Wi-Fi network'; err.style.display = 'block'; }
    document.getElementById('inp-sta-ssid').focus();
    return;
  }
  if (p && p.length < 8) {
    if (err) { err.innerText = 'Home Wi-Fi password must be at least 8 characters (or empty for open network)'; err.style.display = 'block'; }
    document.getElementById('inp-sta-pass').focus();
    return;
  }

  let targetAp = apS || (s + '-EXT');
  let secText = (secMode === 'open') ? 'Open (No password)' : ((secMode === 'custom') ? 'Custom password' : 'Same as Home Wi-Fi');

  document.getElementById('rep-form').innerHTML = '<div style="text-align:center;padding:24px;"><h3 style="color:#2b220e;">Settings Applied</h3><p style="color:#4a5568;margin-top:10px;font-size:13px;">Repeater is connecting to home router and restarting...<br><br>Network: <strong style="color:#be8d36;font-size:15px;">' + esc(targetAp) + '</strong><br>Security: <strong>' + esc(secText) + '</strong><br>Channel: <strong>Ch ' + esc(ch) + '</strong></p></div>';

  let body = 'ssid=' + encodeURIComponent(s) + '&pass=' + encodeURIComponent(p) + '&ap_ssid=' + encodeURIComponent(targetAp) + '&ap_sec_mode=' + encodeURIComponent(secMode) + '&ap_pass=' + encodeURIComponent(apP) + '&channel=' + encodeURIComponent(ch) + '&led_mode=' + encodeURIComponent(led);
  fetch('/save_wifi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).catch(e=>{});
}

function submitApMode() {
  let apS = document.getElementById('inp-hotspot-ssid').value.trim() || 'BW16-Hotspot';
  let apP = document.getElementById('inp-hotspot-pass').value;
  let ch = document.getElementById('sel-hotspot-channel').value;
  let led = document.getElementById('sel-hotspot-led') ? document.getElementById('sel-hotspot-led').value : '0';

  if (apP && apP.length < 8) {
    let err = document.getElementById('hotspot-err');
    if (!err) {
      err = document.createElement('div');
      err.id = 'hotspot-err';
      err.style = 'color:#c53030;font-size:12px;margin-bottom:12px;background:#fff5f5;padding:8px 12px;border-radius:6px;border:1px solid #feb2b2;';
      document.getElementById('view-ap').insertBefore(err, document.getElementById('view-ap').firstChild);
    }
    err.innerText = 'Hotspot password must be at least 8 characters (or empty for open network)';
    err.style.display = 'block';
    document.getElementById('inp-hotspot-pass').focus();
    return;
  }

  document.getElementById('view-ap').innerHTML = '<div style="text-align:center;padding:24px;"><h3 style="color:#2b220e;">Access Point Started</h3><p style="color:#4a5568;margin-top:10px;font-size:13px;">Board is rebooting into Standalone Access Point mode...<br><br>Connect your devices to: <strong style="color:#be8d36;font-size:15px;">' + esc(apS) + '</strong></p></div>';

  let body = 'ap_ssid=' + encodeURIComponent(apS) + '&ap_pass=' + encodeURIComponent(apP) + '&channel=' + encodeURIComponent(ch) + '&led_mode=' + encodeURIComponent(led);
  fetch('/save_ap', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).catch(e=>{});
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
#endif
}
