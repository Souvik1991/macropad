/*
 * Settings Module - Implementation
 * Handles EEPROM storage for persistent configuration
 */

#include "config.h"
#include "debug.h"
#include "display.h"
#include "leds.h"
#include "settings.h"
#include <string.h>

bool autoSwitchOnReconnect = false;

MacroConfig storedMacros[NUM_MACROS];

bool commitEEPROM(const char* context) {
  if (EEPROM.commit()) return true;
  debugPrint("ERROR: EEPROM commit FAILED (");
  debugPrint(context);
  debugPrintln(")");
  return false;
}

void initSettings() {
  debugPrint("Initializing settings... ");
  
  EEPROM.begin(EEPROM_SIZE);
  
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    debugPrintln("First boot detected - Initializing defaults");
    saveOSMode(OS_WINDOWS);
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_LED_BRIGHTNESS, LED_BRIGHTNESS);
    initDefaultMacros();
    initDefaultLEDColors();
    initDefaultKeyNames();
    commitEEPROM("initSettings first boot");
  } else {
    if (autoSwitchOnReconnect) {
      debugPrintln("USB reconnect detected - Auto-switching OS");
      autoToggleOS();
    }
  }
  
  loadOSMode();
  loadLEDBrightness();
  loadLEDColors();
  loadMacros();
  
  debugPrintln("OK");
  debugPrintln();
  debugPrintln("========================================");
  debugPrint("  Current OS: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
  debugPrintln("  Hold K9+K12 for menu (OS switch)");
  debugPrintln("========================================");
}

void saveOSMode(OperatingSystem os) {
  EEPROM.write(EEPROM_ADDR_OS_MODE, (uint8_t)os);
  commitEEPROM("saveOSMode");
  debugPrint("Saved OS mode: ");
  debugPrintln(os == OS_MAC ? "macOS" : "Windows");
}

void loadOSMode() {
  uint8_t savedOS = EEPROM.read(EEPROM_ADDR_OS_MODE);
  
  if (savedOS == OS_MAC || savedOS == OS_WINDOWS) {
    currentOS = (OperatingSystem)savedOS;
    debugPrint("Loaded OS mode: ");
    debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
  } else {
    currentOS = OS_WINDOWS;
    saveOSMode(OS_WINDOWS);
  }
}

void toggleOSMode() {
  currentOS = (currentOS == OS_WINDOWS) ? OS_MAC : OS_WINDOWS;
  saveOSMode(currentOS);
  debugPrint("Manual OS toggle: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
  flashAllLEDs(currentOS == OS_MAC ? CRGB::Blue : CRGB::Green, 200);
  updateDisplay();
}

void autoToggleOS() {
  uint8_t savedOS = EEPROM.read(EEPROM_ADDR_OS_MODE);
  currentOS = (savedOS == OS_WINDOWS) ? OS_MAC : OS_WINDOWS;
  saveOSMode(currentOS);
  debugPrint("Auto-switched to: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
}

void setAutoSwitch(bool enabled) {
  autoSwitchOnReconnect = enabled;
  debugPrint("Auto-switch on reconnect: ");
  debugPrintln(enabled ? "ENABLED" : "DISABLED");
}

void saveLEDBrightness(uint8_t brightness) {
  brightness = constrain(brightness, 0, 255);
  EEPROM.write(EEPROM_ADDR_LED_BRIGHTNESS, brightness);
  commitEEPROM("saveLEDBrightness");
  FastLED.setBrightness(brightness);
  FastLED.show();  // Apply new brightness immediately (updateLEDs doesn't call show() when idle)
  debugPrint("Saved LED brightness: ");
  debugPrintln(brightness);
}

void loadLEDBrightness() {
  uint8_t savedBrightness = EEPROM.read(EEPROM_ADDR_LED_BRIGHTNESS);
  if (savedBrightness == 0 || savedBrightness > 255) {
    savedBrightness = LED_BRIGHTNESS;
  }
  FastLED.setBrightness(savedBrightness);
  debugPrint("Loaded LED brightness: ");
  debugPrintln(savedBrightness);
}

void saveLEDColor(int ledIndex, CRGB color) {
  if (ledIndex < 0 || ledIndex >= NUM_LEDS) return;
  
  int addr = EEPROM_ADDR_LED_COLORS + (ledIndex * 3);
  EEPROM.write(addr, color.r);
  EEPROM.write(addr + 1, color.g);
  EEPROM.write(addr + 2, color.b);
  commitEEPROM("saveLEDColor");
  
  debugPrint("Saved LED ");
  debugPrint(ledIndex);
  debugPrint(" color: R=");
  debugPrint(color.r);
  debugPrint(" G=");
  debugPrint(color.g);
  debugPrint(" B=");
  debugPrintln(color.b);
}

void loadLEDColor(int ledIndex, CRGB &color) {
  if (ledIndex < 0 || ledIndex >= NUM_LEDS) return;
  
  int addr = EEPROM_ADDR_LED_COLORS + (ledIndex * 3);
  color.r = EEPROM.read(addr);
  color.g = EEPROM.read(addr + 1);
  color.b = EEPROM.read(addr + 2);
}

void initDefaultLEDColors() {
  CRGB defaultColor = CRGB(0, 0, 50);
  for (int i = 0; i < NUM_LEDS; i++) {
    saveLEDColor(i, defaultColor);
  }
  debugPrintln("Initialized default LED colors");
}

void loadLEDColors() {
  for (int i = 0; i < NUM_LEDS; i++) {
    CRGB color;
    loadLEDColor(i, color);
    leds[i] = color;
  }
  FastLED.show();
  debugPrintln("Loaded LED colors from EEPROM");
}

void saveMacro(int keyNum, MacroConfig &macro) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  
  int addr = EEPROM_ADDR_MACROS + ((keyNum - 1) * MACRO_SIZE);
  EEPROM.write(addr, macro.type);
  EEPROM.write(addr + 1, macro.modifier);
  EEPROM.write(addr + 2, macro.keycode);
  for (int i = 0; i < 5; i++) {
    EEPROM.write(addr + 3 + i, macro.data[i]);
  }
  commitEEPROM("saveMacro");
  debugPrint("Saved macro for Key ");
  debugPrintln(keyNum);
}

void loadMacro(int keyNum, MacroConfig &macro) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  
  int addr = EEPROM_ADDR_MACROS + ((keyNum - 1) * MACRO_SIZE);
  macro.type = EEPROM.read(addr);
  macro.modifier = EEPROM.read(addr + 1);
  macro.keycode = EEPROM.read(addr + 2);
  for (int i = 0; i < 5; i++) {
    macro.data[i] = EEPROM.read(addr + 3 + i);
  }
}

void initDefaultMacros() {
  MacroConfig macro;
  memset(&macro, 0, sizeof(MacroConfig));
  macro.type = MACRO_TYPE_DISABLED;

  for (int i = 1; i <= NUM_MACROS; i++) {
    saveMacro(i, macro);
  }

  debugPrintln("Initialized macros (all disabled)");
}

void loadMacros() {
  for (int i = 0; i < NUM_MACROS; i++) {
    loadMacro(i + 1, storedMacros[i]);
  }
  debugPrintln("Loaded macros from EEPROM");
}

void saveKeyName(int keyNum, const char* name) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  
  int addr = EEPROM_ADDR_KEY_NAMES + ((keyNum - 1) * (MAX_KEY_NAME_LENGTH + 1));
  int len = min((int)strlen(name), MAX_KEY_NAME_LENGTH);
  
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, name[i]);
  }
  EEPROM.write(addr + len, 0);
  commitEEPROM("saveKeyName");
  
  debugPrint("Saved name for Key ");
  debugPrint(keyNum);
  debugPrint(": ");
  debugPrintln(name);
}

void loadKeyName(int keyNum, char* name, int maxLen) {
  if (keyNum < 1 || keyNum > NUM_MACROS) {
    name[0] = 0;
    return;
  }
  
  int addr = EEPROM_ADDR_KEY_NAMES + ((keyNum - 1) * (MAX_KEY_NAME_LENGTH + 1));
  int len = 0;
  
  for (int i = 0; i < maxLen - 1 && i < MAX_KEY_NAME_LENGTH; i++) {
    char c = EEPROM.read(addr + i);
    if (c == 0) break;
    name[i] = c;
    len++;
  }
  name[len] = 0;
}

void loadSequence(int keyNum, int osOffset, char* buf, int bufSize) {
  if (keyNum < 1 || keyNum > NUM_MACROS || bufSize < 1) {
    buf[0] = 0;
    return;
  }
  int seqAddr = EEPROM_ADDR_SEQUENCES + ((keyNum - 1) * 2 + osOffset) * MAX_SEQUENCE_LENGTH;
  int len = 0;
  for (int i = 0; i < bufSize - 1 && i < MAX_SEQUENCE_LENGTH; i++) {
    char c = EEPROM.read(seqAddr + i);
    if (c == 0) break;
    buf[i] = c;
    len++;
  }
  buf[len] = 0;
}

void initDefaultKeyNames() {
  for (int i = 1; i <= NUM_MACROS; i++) {
    saveKeyName(i, "");
  }
  debugPrintln("Initialized key names (all empty)");
}

bool hasAnyMacroConfigured() {
  for (int i = 0; i < NUM_MACROS; i++) {
    if (storedMacros[i].type != MACRO_TYPE_DISABLED) {
      return true;
    }
  }
  return false;
}

void resetAllSettings() {
  debugPrintln("Resetting all settings to factory defaults...");

  initDefaultMacros();
  initDefaultLEDColors();
  initDefaultKeyNames();

  // Clear all sequence data (12 keys × 2 OS = 24 sequences)
  for (int i = 0; i < NUM_MACROS * 2; i++) {
    int addr = EEPROM_ADDR_SEQUENCES + (i * MAX_SEQUENCE_LENGTH);
    EEPROM.write(addr, 0);
  }
  commitEEPROM("resetAllSettings sequences");

  saveOSMode(OS_WINDOWS);
  saveLEDBrightness(LED_BRIGHTNESS);

  loadMacros();
  loadLEDColors();

  debugPrintln("  Settings reset complete");
}
