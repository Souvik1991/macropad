/*
 * Settings Module
 * Handles EEPROM storage for persistent configuration
 */

#include <string.h>  // For memset

// ============================================
// STATE VARIABLES
// ============================================

// Auto-switch on USB reconnect (KVM/hub button)
// DISABLED by default - use double-tap Key 12 instead (more reliable)
bool autoSwitchOnReconnect = false;

// ============================================
// INITIALIZATION
// ============================================

void initSettings() {
  debugPrint("Initializing settings... ");
  
  EEPROM.begin(EEPROM_SIZE);
  
  // Check if EEPROM is initialized
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    // First boot - initialize with defaults
    debugPrintln("First boot detected - Initializing defaults");
    saveOSMode(OS_WINDOWS);  // Default to Windows (first system)
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_LED_BRIGHTNESS, LED_BRIGHTNESS);
    initDefaultMacros();  // Initialize default macros
    initDefaultLEDColors();  // Initialize default LED colors
    initDefaultKeyNames();  // Initialize default key names
    EEPROM.commit();
  } else {
    // Subsequent boot - check auto-switch setting
    if (autoSwitchOnReconnect) {
      debugPrintln("USB reconnect detected - Auto-switching OS");
      autoToggleOS();  // Toggle to other system
    } else {
      debugPrintln("Auto-switch disabled");
    }
  }
  
  // Load all settings
  loadOSMode();
  loadLEDBrightness();
  loadLEDColors();
  loadMacros();
  
  debugPrintln("OK");
  debugPrintln();
  debugPrintln("========================================");
  debugPrint("  Current OS: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
  debugPrintln("  Double-tap Key 12 to switch OS");
  debugPrintln("========================================");
}

// ============================================
// OS MODE STORAGE
// ============================================

void saveOSMode(OperatingSystem os) {
  EEPROM.write(EEPROM_ADDR_OS_MODE, (uint8_t)os);
  EEPROM.commit();
  
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
    // Invalid value, default to Windows
    currentOS = OS_WINDOWS;
    saveOSMode(OS_WINDOWS);
  }
}

// ============================================
// HELPER FUNCTIONS
// ============================================

void toggleOSMode() {
  // Manual toggle - switch between Windows and Mac
  currentOS = (currentOS == OS_WINDOWS) ? OS_MAC : OS_WINDOWS;
  
  // Save to EEPROM
  saveOSMode(currentOS);
  
  debugPrint("Manual OS toggle: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
  
  // Visual feedback
  flashAllLEDs(currentOS == OS_MAC ? CRGB::Blue : CRGB::Green, 200);
  updateDisplay();
}

void autoToggleOS() {
  // Auto-toggle when KVM/hub button is pressed (USB reconnect)
  uint8_t savedOS = EEPROM.read(EEPROM_ADDR_OS_MODE);
  
  // Toggle to opposite system
  currentOS = (savedOS == OS_WINDOWS) ? OS_MAC : OS_WINDOWS;
  
  // Save new state
  saveOSMode(currentOS);
  
  debugPrint("Auto-switched to: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
}

void setAutoSwitch(bool enabled) {
  autoSwitchOnReconnect = enabled;
  debugPrint("Auto-switch on reconnect: ");
  debugPrintln(enabled ? "ENABLED" : "DISABLED");
}

// ============================================
// LED BRIGHTNESS STORAGE
// ============================================

void saveLEDBrightness(uint8_t brightness) {
  brightness = constrain(brightness, 0, 255);
  EEPROM.write(EEPROM_ADDR_LED_BRIGHTNESS, brightness);
  EEPROM.commit();
  FastLED.setBrightness(brightness);
  debugPrint("Saved LED brightness: ");
  debugPrintln(brightness);
}

void loadLEDBrightness() {
  uint8_t savedBrightness = EEPROM.read(EEPROM_ADDR_LED_BRIGHTNESS);
  if (savedBrightness == 0 || savedBrightness > 255) {
    savedBrightness = LED_BRIGHTNESS;  // Default
  }
  FastLED.setBrightness(savedBrightness);
  debugPrint("Loaded LED brightness: ");
  debugPrintln(savedBrightness);
}

// ============================================
// LED COLOR STORAGE
// ============================================

void saveLEDColor(int ledIndex, CRGB color) {
  if (ledIndex < 0 || ledIndex >= NUM_LEDS) return;
  
  int addr = EEPROM_ADDR_LED_COLORS + (ledIndex * 3);
  EEPROM.write(addr, color.r);
  EEPROM.write(addr + 1, color.g);
  EEPROM.write(addr + 2, color.b);
  EEPROM.commit();
  
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
  // Default: Soft blue for all LEDs
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

// ============================================
// MACRO STORAGE
// ============================================

void saveMacro(int keyNum, MacroConfig &macro) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  
  int addr = EEPROM_ADDR_MACROS + ((keyNum - 1) * MACRO_SIZE);
  EEPROM.write(addr, macro.type);
  EEPROM.write(addr + 1, macro.modifier);
  EEPROM.write(addr + 2, macro.keycode);
  for (int i = 0; i < 5; i++) {
    EEPROM.write(addr + 3 + i, macro.data[i]);
  }
  EEPROM.commit();
  
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
  // Initialize with hardcoded defaults
  MacroConfig macro;
  
  // Key 1: Screenshot (will be OS-aware at runtime)
  macro.type = MACRO_TYPE_SPECIAL;
  macro.modifier = 0;
  macro.keycode = 1;  // Special code for screenshot
  memset(macro.data, 0, 5);
  saveMacro(1, macro);
  
  // Key 2: Mic Mute
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 5;  // Ctrl + Shift
  macro.keycode = HID_KEY_M;
  memset(macro.data, 0, 5);
  saveMacro(2, macro);
  
  // Key 3: Video Toggle
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 5;  // Ctrl + Shift
  macro.keycode = HID_KEY_V;
  memset(macro.data, 0, 5);
  saveMacro(3, macro);
  
  // Key 4: Lock Screen (OS-aware)
  macro.type = MACRO_TYPE_SPECIAL;
  macro.modifier = 0;
  macro.keycode = 2;  // Special code for lock screen
  memset(macro.data, 0, 5);
  saveMacro(4, macro);
  
  // Key 5: Copy
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 0;  // Will use OS-aware modifier
  macro.keycode = HID_KEY_C;
  memset(macro.data, 0, 5);
  saveMacro(5, macro);
  
  // Key 6: Cut
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 0;
  macro.keycode = HID_KEY_X;
  memset(macro.data, 0, 5);
  saveMacro(6, macro);
  
  // Key 7: Paste
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 0;
  macro.keycode = HID_KEY_V;
  memset(macro.data, 0, 5);
  saveMacro(7, macro);
  
  // Key 8: Delete
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 0;
  macro.keycode = HID_KEY_DELETE;
  memset(macro.data, 0, 5);
  saveMacro(8, macro);
  
  // Key 9: Open Cursor (OS-aware)
  macro.type = MACRO_TYPE_SPECIAL;
  macro.modifier = 0;
  macro.keycode = 3;  // Special code for open cursor
  memset(macro.data, 0, 5);
  saveMacro(9, macro);
  
  // Key 10: Inspect Element
  macro.type = MACRO_TYPE_KEYCOMBO;
  macro.modifier = 0;
  macro.keycode = HID_KEY_F12;
  memset(macro.data, 0, 5);
  saveMacro(10, macro);
  
  // Key 11: Open Terminal (OS-aware)
  macro.type = MACRO_TYPE_SPECIAL;
  macro.modifier = 0;
  macro.keycode = 4;  // Special code for open terminal
  memset(macro.data, 0, 5);
  saveMacro(11, macro);
  
  // Key 12: Git Pull / OS Switch (special)
  macro.type = MACRO_TYPE_SPECIAL;
  macro.modifier = 0;
  macro.keycode = 5;  // Special code for git pull / OS switch
  memset(macro.data, 0, 5);
  saveMacro(12, macro);
  
  debugPrintln("Initialized default macros");
}

// Global macro storage (loaded from EEPROM)
MacroConfig storedMacros[NUM_MACROS];

void loadMacros() {
  for (int i = 0; i < NUM_MACROS; i++) {
    loadMacro(i + 1, storedMacros[i]);
  }
  debugPrintln("Loaded macros from EEPROM");
}

// ============================================
// KEY NAME STORAGE
// ============================================

void saveKeyName(int keyNum, const char* name) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  
  int addr = EEPROM_ADDR_KEY_NAMES + ((keyNum - 1) * (MAX_KEY_NAME_LENGTH + 1));
  int len = min(strlen(name), MAX_KEY_NAME_LENGTH);
  
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, name[i]);
  }
  EEPROM.write(addr + len, 0); // Null terminator
  EEPROM.commit();
  
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
  
  // Read until null terminator or max length
  for (int i = 0; i < maxLen - 1 && i < MAX_KEY_NAME_LENGTH; i++) {
    char c = EEPROM.read(addr + i);
    if (c == 0) break;
    name[i] = c;
    len++;
  }
  name[len] = 0; // Null terminator
}

void initDefaultKeyNames() {
  const char* defaultNames[] = {
    "Screenshot", "Mic Mute", "Video Toggle", "Lock Screen",
    "Copy", "Cut", "Paste", "Delete",
    "Open Cursor", "Inspect Element", "Open Terminal", "Git Pull / OS Switch"
  };
  
  for (int i = 0; i < NUM_MACROS; i++) {
    saveKeyName(i + 1, defaultNames[i]);
  }
  
  debugPrintln("Initialized default key names");
}

/*
 * HOW AUTO-SWITCH WORKS:
 * ======================
 * 
 * When you press your KVM/USB hub button:
 * 1. Macropad loses USB connection
 * 2. Hub switches to other computer
 * 3. Macropad reconnects to new computer
 * 4. ESP32 reboots (power cycle from USB)
 * 5. initSettings() detects it's not first boot
 * 6. Auto-toggles OS mode (Windows ↔ Mac)
 * 7. Loads new OS mode
 * 8. All shortcuts now work for the new system!
 * 
 * First Power-On Behavior:
 * - Defaults to Windows (System 1)
 * - Does NOT auto-switch
 * - Subsequent switches auto-toggle
 * 
 * To Disable Auto-Switch:
 * - Hold Encoder 1 button during boot
 * - Or call: setAutoSwitch(false)
 * 
 * FUTURE SETTINGS TO ADD:
 * =======================
 * 
 * - LED brightness preference
 * - Default volume level
 * - Custom macro assignments
 * - Fingerprint IDs
 * - FIDO2 credentials
 * 
 * EEPROM Layout:
 * Address 0: OS Mode (1 byte)
 * Address 1: Magic byte (1 byte)
 * Address 2: LED brightness (1 byte)
 * Address 3: Volume level (1 byte)
 * Address 4: Auto-switch enabled (1 byte)
 * Address 5-511: Reserved for credentials/macros
 */

