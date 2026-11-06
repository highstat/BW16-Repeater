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

enum class LedUserMode : uint8_t {
  MODE_AUTO = 0,
  MODE_OFF = 1,
  MODE_GREEN = 2,
  MODE_BLUE = 3,
  MODE_RED = 4,
  MODE_CYAN = 5,
  MODE_MAGENTA = 6,
  MODE_YELLOW = 7,
  MODE_WHITE = 8
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

  static void setConfig(uint8_t cfg) {
    userConfig = cfg;
    update();
  }

  static uint8_t getConfig() {
    return userConfig;
  }

  static void set(LedMode mode) {
    currentMode = mode;
    blinkState = true;
    update();
  }

  static void tick() {
    if (userConfig != 0) return; // Custom static color or OFF does not blink
    if (currentMode == LedMode::CONNECTING || currentMode == LedMode::SETUP_PORTAL) {
      if (millis() - lastBlink > 400) {
        lastBlink = millis();
        blinkState = !blinkState;
        update();
      }
    }
  }

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

 private:
  static void update() {
    if (userConfig == 1) { // OFF
      setRgb(false, false, false);
      return;
    } else if (userConfig == 2) { // Green
      setRgb(false, true, false);
      return;
    } else if (userConfig == 3) { // Blue
      setRgb(false, false, true);
      return;
    } else if (userConfig == 4) { // Red
      setRgb(true, false, false);
      return;
    } else if (userConfig == 5) { // Cyan
      setRgb(false, true, true);
      return;
    } else if (userConfig == 6) { // Magenta
      setRgb(true, false, true);
      return;
    } else if (userConfig == 7) { // Yellow
      setRgb(true, true, false);
      return;
    } else if (userConfig == 8) { // White
      setRgb(true, true, true);
      return;
    }

    // Default: AUTO (telemetry)
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
  static uint8_t userConfig;
  static bool blinkState;
  static uint32_t lastBlink;
};

__attribute__((weak)) LedMode LedStatus::currentMode = LedMode::ALL_OFF;
__attribute__((weak)) uint8_t LedStatus::userConfig = 0;
__attribute__((weak)) bool LedStatus::blinkState = false;
__attribute__((weak)) uint32_t LedStatus::lastBlink = 0;
