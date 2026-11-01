/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            `https://github.com/highstat/BW16-Repeater` 
    Name: Base64 
    Description: Clean, safe Base64 encoding helper 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#include "Base64.h"

String b64(const String& in) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out = "";
  size_t len = in.length();
  if (len == 0) return out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while (i < len) {
    uint32_t b0 = (uint8_t)in[i++];
    uint32_t b1 = (i < len) ? (uint8_t)in[i++] : 0;
    uint32_t b2 = (i < len) ? (uint8_t)in[i++] : 0;
    uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
    out += T[(triple >> 18) & 0x3F];
    out += T[(triple >> 12) & 0x3F];
    out += (i > len + 1) ? '=' : T[(triple >> 6) & 0x3F];
    out += (i > len) ? '=' : T[triple & 0x3F];
  }
  return out;
}
