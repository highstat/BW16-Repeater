/*                                      _     
        ___ _ __  _ __ ___ _ __  ___(_)___ 
        / __| '_ \| '__/ _ \ '_ \/ __| / __| 
        \__ \ |_) | | |  __/ | | \__ \ \__ \ 
        |___/ .__/|_|  \___|_| |_|___/_|___/ 
            |_|                             
                © Copyright 2025-2026 
            ✈ `https://github.com/highstat` 
    Name: LedStatus 
    Description: RGB LED telemetry for BW16 (RTL8720DN) 
    Author: @highstat 
    Platform: BW16 (RTL8720dn) - Ameba Arduino 
    License: MIT 
*/

#pragma once
#include <Arduino.h>

enum class LedMode {
  ALL_OFF,
  SETUP_PORTAL,   // Red blinking - setup mode (Captive Portal)
  CONNECTING,     // Blue blinking - connecting to router
  BRIDGE_ACTIVE   // Solid Green - bridge active
};

class LedStatus {
 public:
  static void begin() {
#ifdef LED_R
    pinMode(LED_R, OUTPUT);
    digitalWrite(LED_R, LOW);
#endif
#ifdef LED_G
    pinMode(LED_G, OUTPUT);
    digitalWrite(LED_G, LOW);
#endif
#ifdef LED_B
    pinMode(LED_B, OUTPUT);
    digitalWrite(LED_B, LOW);
#endif
  }

  static void set(LedMode mode) {
    currentMode = mode;
    blinkState = true;
    update();
  }

  static void tick() {
    if (currentMode == LedMode::CONNECTING || currentMode == LedMode::SETUP_PORTAL) {
      if (millis() - lastBlink > 400) {
        lastBlink = millis();
        blinkState = !blinkState;
        update();
      }
    }
  }

 private:
  static void setRgb(bool r, bool g, bool b) {
#ifdef LED_R
    digitalWrite(LED_R, r ? HIGH : LOW);
#endif
#ifdef LED_G
    digitalWrite(LED_G, g ? HIGH : LOW);
#endif
#ifdef LED_B
    digitalWrite(LED_B, b ? HIGH : LOW);
#endif
  }

  static void update() {
    switch (currentMode) {
      case LedMode::SETUP_PORTAL:
        setRgb(blinkState, false, false);
        break;
      case LedMode::CONNECTING:
        setRgb(false, false, blinkState);
        break;
      case LedMode::BRIDGE_ACTIVE:
        setRgb(false, true, false);
        break;
      case LedMode::ALL_OFF:
      default:
        setRgb(false, false, false);
        break;
    }
  }

  static LedMode currentMode;
  static bool blinkState;
  static uint32_t lastBlink;
};

__attribute__((weak)) LedMode LedStatus::currentMode = LedMode::ALL_OFF;
__attribute__((weak)) bool LedStatus::blinkState = false;
__attribute__((weak)) uint32_t LedStatus::lastBlink = 0;
