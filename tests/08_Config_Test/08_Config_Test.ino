/*
 * Configuration System Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * 
 * Tests the full configuration pipeline that the web configurator relies on:
 *   1. EEPROM initialization and default loading
 *   2. Saving and reading back macros, sequences, key names, LEDs, OS mode
 *   3. Modifying an existing config and verifying the update
 *   4. Persistence across simulated reboots (save -> clear RAM -> reload)
 *   5. Serial command parsing (the same protocol the web configurator uses)
 *   6. RESET to defaults
 *
 * USB Device Name:
 *   The device identifies as "FIDO2 Macropad" (manufacturer "Passkey")
 *   so it's easy to spot in the browser's serial port picker.
 *
 * Usage:
 *   - Upload and open Serial Monitor at 115200 baud
 *   - Automated test suite runs on boot
 *   - After automated tests, enter interactive mode for manual testing
 *   - Type HELP in Serial Monitor for available commands
 *
 * Board settings (Arduino IDE / PlatformIO):
 *   - Board: Adafruit ESP32-S3 Feather
 *   - USB CDC On Boot: Enabled
 *   - USB Mode: Hardware CDC and JTAG
 */

#include <EEPROM.h>
#include "USB.h"

// ============================================
// EEPROM LAYOUT (mirrors config.h)
// ============================================

#define EEPROM_SIZE 8192
#define EEPROM_ADDR_OS_MODE 0
#define EEPROM_ADDR_MAGIC 1
#define EEPROM_ADDR_LED_BRIGHTNESS 2
#define EEPROM_ADDR_LED_COLORS 3       // 5 LEDs × 3 bytes = 15 bytes (3-17)
#define EEPROM_ADDR_MACROS 20          // 12 keys × 8 bytes = 96 bytes (20-115)
#define EEPROM_ADDR_SEQUENCES 116      // 12 keys × 2 OS × 200 bytes = 4800 bytes (116-4915)
#define EEPROM_ADDR_KEY_NAMES 4916     // 12 keys × 13 bytes = 156 bytes (4916-5071)
#define EEPROM_MAGIC 0xAB
#define MAX_SEQUENCE_LENGTH 200
#define MAX_KEY_NAME_LENGTH 12

#define MACRO_TYPE_DISABLED 0
#define MACRO_TYPE_KEYCOMBO 1
#define MACRO_TYPE_STRING 2
#define MACRO_TYPE_SPECIAL 3
#define MACRO_TYPE_SEQUENCE 4

#define MACRO_SIZE 8
#define NUM_MACROS 12
#define NUM_LEDS 5
#define LED_BRIGHTNESS_DEFAULT 50

enum OperatingSystem { OS_WINDOWS = 0, OS_MAC = 1 };

struct MacroConfig {
  uint8_t type;
  uint8_t modifier;
  uint8_t keycode;
  uint8_t data[5];
};

// ============================================
// TEST STATE
// ============================================

MacroConfig storedMacros[NUM_MACROS];
OperatingSystem currentOS = OS_WINDOWS;
int testsRun = 0;
int testsPassed = 0;
int testsFailed = 0;

// ============================================
// EEPROM HELPERS (same logic as settings.ino)
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

void loadAllMacros() {
  for (int i = 0; i < NUM_MACROS; i++) {
    loadMacro(i + 1, storedMacros[i]);
  }
}

void saveKeyName(int keyNum, const char* name) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  int addr = EEPROM_ADDR_KEY_NAMES + ((keyNum - 1) * (MAX_KEY_NAME_LENGTH + 1));
  int len = min((int)strlen(name), MAX_KEY_NAME_LENGTH);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, name[i]);
  }
  EEPROM.write(addr + len, 0);
  EEPROM.commit();
}

void loadKeyName(int keyNum, char* name, int maxLen) {
  if (keyNum < 1 || keyNum > NUM_MACROS) { name[0] = 0; return; }
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

void saveSequence(int keyNum, int osOffset, const char* seq) {
  if (keyNum < 1 || keyNum > NUM_MACROS) return;
  int seqAddr = EEPROM_ADDR_SEQUENCES + ((keyNum - 1) * 2 + osOffset) * MAX_SEQUENCE_LENGTH;
  int len = min((int)strlen(seq), MAX_SEQUENCE_LENGTH - 1);
  for (int i = 0; i < len; i++) {
    EEPROM.write(seqAddr + i, seq[i]);
  }
  EEPROM.write(seqAddr + len, 0);
  EEPROM.commit();
}

void loadSequence(int keyNum, int osOffset, char* buf, int bufSize) {
  if (keyNum < 1 || keyNum > NUM_MACROS) { buf[0] = 0; return; }
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

void saveLEDColor(int idx, uint8_t r, uint8_t g, uint8_t b) {
  if (idx < 0 || idx >= NUM_LEDS) return;
  int addr = EEPROM_ADDR_LED_COLORS + (idx * 3);
  EEPROM.write(addr, r);
  EEPROM.write(addr + 1, g);
  EEPROM.write(addr + 2, b);
  EEPROM.commit();
}

void loadLEDColor(int idx, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (idx < 0 || idx >= NUM_LEDS) return;
  int addr = EEPROM_ADDR_LED_COLORS + (idx * 3);
  r = EEPROM.read(addr);
  g = EEPROM.read(addr + 1);
  b = EEPROM.read(addr + 2);
}

void saveOSMode(OperatingSystem os) {
  EEPROM.write(EEPROM_ADDR_OS_MODE, (uint8_t)os);
  EEPROM.commit();
}

OperatingSystem loadOSMode() {
  uint8_t val = EEPROM.read(EEPROM_ADDR_OS_MODE);
  return (val == OS_MAC) ? OS_MAC : OS_WINDOWS;
}

void saveBrightness(uint8_t brightness) {
  EEPROM.write(EEPROM_ADDR_LED_BRIGHTNESS, brightness);
  EEPROM.commit();
}

uint8_t loadBrightness() {
  return EEPROM.read(EEPROM_ADDR_LED_BRIGHTNESS);
}

// ============================================
// TEST FRAMEWORK
// ============================================

void assertEq(const char* testName, int actual, int expected) {
  testsRun++;
  if (actual == expected) {
    testsPassed++;
    Serial.print("  [PASS] ");
  } else {
    testsFailed++;
    Serial.print("  [FAIL] ");
  }
  Serial.print(testName);
  if (actual != expected) {
    Serial.print("  (expected ");
    Serial.print(expected);
    Serial.print(", got ");
    Serial.print(actual);
    Serial.print(")");
  }
  Serial.println();
}

void assertStrEq(const char* testName, const char* actual, const char* expected) {
  testsRun++;
  if (strcmp(actual, expected) == 0) {
    testsPassed++;
    Serial.print("  [PASS] ");
  } else {
    testsFailed++;
    Serial.print("  [FAIL] ");
  }
  Serial.print(testName);
  if (strcmp(actual, expected) != 0) {
    Serial.print("  (expected \"");
    Serial.print(expected);
    Serial.print("\", got \"");
    Serial.print(actual);
    Serial.print("\")");
  }
  Serial.println();
}

// ============================================
// INITIALIZATION HELPERS
// ============================================

void wipeEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
}

void initDefaults() {
  saveOSMode(OS_WINDOWS);
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  saveBrightness(LED_BRIGHTNESS_DEFAULT);

  for (int i = 0; i < NUM_LEDS; i++) {
    saveLEDColor(i, 0, 0, 50);
  }

  // Key 1: Screenshot (special)
  MacroConfig m;
  memset(&m, 0, sizeof(m));
  m.type = MACRO_TYPE_SPECIAL; m.keycode = 1;
  saveMacro(1, m);

  // Key 2: Mic Mute (Ctrl+Shift+M)
  m.type = MACRO_TYPE_KEYCOMBO; m.modifier = 5; m.keycode = 0x10; // HID_KEY_M
  saveMacro(2, m);

  // Keys 3-12: disabled
  m.type = MACRO_TYPE_DISABLED; m.modifier = 0; m.keycode = 0;
  for (int i = 3; i <= NUM_MACROS; i++) saveMacro(i, m);

  const char* defaultNames[] = {
    "Screenshot", "Mic Mute", "Video Tgl", "Lock", "Copy", "Cut",
    "Paste", "Delete", "Cursor", "Inspect", "Terminal", "GitPull"
  };
  for (int i = 0; i < NUM_MACROS; i++) {
    saveKeyName(i + 1, defaultNames[i]);
  }
}

// ============================================
// TEST SUITES
// ============================================

void testEEPROMInit() {
  Serial.println("\n--- Test 1: EEPROM Initialization ---");

  wipeEEPROM();
  uint8_t magic = EEPROM.read(EEPROM_ADDR_MAGIC);
  assertEq("Wiped EEPROM has no magic byte", magic == EEPROM_MAGIC, 0);

  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.commit();
  magic = EEPROM.read(EEPROM_ADDR_MAGIC);
  assertEq("Magic byte written and read back", magic, EEPROM_MAGIC);
}

void testFetchStoredConfigs() {
  Serial.println("\n--- Test 2: Fetch Stored Configs (LIST equivalent) ---");

  wipeEEPROM();
  initDefaults();

  loadAllMacros();
  assertEq("Key 1 type is SPECIAL", storedMacros[0].type, MACRO_TYPE_SPECIAL);
  assertEq("Key 1 keycode is 1", storedMacros[0].keycode, 1);
  assertEq("Key 2 type is KEYCOMBO", storedMacros[1].type, MACRO_TYPE_KEYCOMBO);
  assertEq("Key 2 modifier is 5 (Ctrl+Shift)", storedMacros[1].modifier, 5);
  assertEq("Key 2 keycode is 0x10 (M)", storedMacros[1].keycode, 0x10);
  assertEq("Key 3 type is DISABLED", storedMacros[2].type, MACRO_TYPE_DISABLED);

  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(1, name, sizeof(name));
  assertStrEq("Key 1 name is 'Screenshot'", name, "Screenshot");
  loadKeyName(2, name, sizeof(name));
  assertStrEq("Key 2 name is 'Mic Mute'", name, "Mic Mute");

  OperatingSystem os = loadOSMode();
  assertEq("OS mode is Windows", os, OS_WINDOWS);

  uint8_t bright = loadBrightness();
  assertEq("Brightness is default (50)", bright, LED_BRIGHTNESS_DEFAULT);

  uint8_t r, g, b;
  loadLEDColor(0, r, g, b);
  assertEq("LED 0 R is 0", r, 0);
  assertEq("LED 0 G is 0", g, 0);
  assertEq("LED 0 B is 50", b, 50);
}

void testPushNewConfig() {
  Serial.println("\n--- Test 3: Push New Config via Serial Commands ---");

  // Simulate: MACRO 5 1 8 4 (Key 5, keycombo, GUI modifier, keycode A)
  MacroConfig m;
  memset(&m, 0, sizeof(m));
  m.type = MACRO_TYPE_KEYCOMBO;
  m.modifier = 8;   // GUI
  m.keycode = 4;     // HID key A
  saveMacro(5, m);

  MacroConfig readBack;
  loadMacro(5, readBack);
  assertEq("Key 5 type saved as KEYCOMBO", readBack.type, MACRO_TYPE_KEYCOMBO);
  assertEq("Key 5 modifier saved as 8 (GUI)", readBack.modifier, 8);
  assertEq("Key 5 keycode saved as 4 (A)", readBack.keycode, 4);

  // Simulate: NAME 5 Open App
  saveKeyName(5, "Open App");
  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(5, name, sizeof(name));
  assertStrEq("Key 5 name saved as 'Open App'", name, "Open App");

  // Simulate: LED 2 255 128 0
  saveLEDColor(2, 255, 128, 0);
  uint8_t r, g, b;
  loadLEDColor(2, r, g, b);
  assertEq("LED 2 R saved as 255", r, 255);
  assertEq("LED 2 G saved as 128", g, 128);
  assertEq("LED 2 B saved as 0", b, 0);

  // Simulate: OS mac
  saveOSMode(OS_MAC);
  OperatingSystem os = loadOSMode();
  assertEq("OS mode changed to Mac", os, OS_MAC);

  // Simulate: BRIGHTNESS 180
  saveBrightness(180);
  uint8_t bright = loadBrightness();
  assertEq("Brightness changed to 180", bright, 180);
}

void testSequenceSaveLoad() {
  Serial.println("\n--- Test 4: Sequence Save and Load ---");

  // Simulate: SEQUENCE 1 win K:8:18|D:200|T:calc|D:100|K:0:40
  const char* winSeq = "K:8:18|D:200|T:calc|D:100|K:0:40";
  saveSequence(1, 0, winSeq);  // osOffset 0 = windows

  char buf[MAX_SEQUENCE_LENGTH];
  loadSequence(1, 0, buf, sizeof(buf));
  assertStrEq("Key 1 Win sequence stored correctly", buf, winSeq);

  // Simulate: SEQUENCE 1 mac K:8:44|D:200|T:calc|D:100|K:0:40
  const char* macSeq = "K:8:44|D:200|T:calc|D:100|K:0:40";
  saveSequence(1, 1, macSeq);  // osOffset 1 = mac

  loadSequence(1, 1, buf, sizeof(buf));
  assertStrEq("Key 1 Mac sequence stored correctly", buf, macSeq);

  // Verify Win sequence was not overwritten by Mac sequence
  loadSequence(1, 0, buf, sizeof(buf));
  assertStrEq("Key 1 Win sequence still intact after Mac save", buf, winSeq);

  // Set the macro type to SEQUENCE so executeMacro would trigger it
  MacroConfig m;
  memset(&m, 0, sizeof(m));
  m.type = MACRO_TYPE_SEQUENCE;
  saveMacro(1, m);
  loadMacro(1, m);
  assertEq("Key 1 type is SEQUENCE after update", m.type, MACRO_TYPE_SEQUENCE);
}

void testModifyExistingConfig() {
  Serial.println("\n--- Test 5: Modify Existing Key Config ---");

  // Read current Key 2 config
  MacroConfig original;
  loadMacro(2, original);
  assertEq("Key 2 original type is KEYCOMBO", original.type, MACRO_TYPE_KEYCOMBO);
  assertEq("Key 2 original modifier is 5", original.modifier, 5);

  // Modify Key 2: change from Ctrl+Shift+M to Alt+F4
  MacroConfig updated;
  memset(&updated, 0, sizeof(updated));
  updated.type = MACRO_TYPE_KEYCOMBO;
  updated.modifier = 4;   // Alt
  updated.keycode = 61;   // F4
  saveMacro(2, updated);

  // Read back and verify the update
  MacroConfig readBack;
  loadMacro(2, readBack);
  assertEq("Key 2 modified type still KEYCOMBO", readBack.type, MACRO_TYPE_KEYCOMBO);
  assertEq("Key 2 modified modifier is 4 (Alt)", readBack.modifier, 4);
  assertEq("Key 2 modified keycode is 61 (F4)", readBack.keycode, 61);

  // Rename Key 2
  saveKeyName(2, "Close Window");
  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(2, name, sizeof(name));
  assertStrEq("Key 2 renamed to 'Close Window'", name, "Close Window");

  // Verify other keys were NOT affected
  MacroConfig key1;
  loadMacro(1, key1);
  assertEq("Key 1 unaffected (still SEQUENCE)", key1.type, MACRO_TYPE_SEQUENCE);
  char name1[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(1, name1, sizeof(name1));
  assertStrEq("Key 1 name unaffected", name1, "Screenshot");
}

void testPersistenceAcrossReboot() {
  Serial.println("\n--- Test 6: Persistence Across Simulated Reboot ---");

  // Write known values
  MacroConfig m;
  memset(&m, 0, sizeof(m));
  m.type = MACRO_TYPE_STRING;
  m.modifier = 0;
  m.keycode = 0;
  m.data[0] = 'H'; m.data[1] = 'e'; m.data[2] = 'l'; m.data[3] = 'l'; m.data[4] = 'o';
  saveMacro(8, m);
  saveKeyName(8, "Greet");
  saveLEDColor(4, 0, 255, 0);
  saveOSMode(OS_MAC);
  saveBrightness(200);

  // Clear all RAM state (simulate power cycle)
  memset(storedMacros, 0, sizeof(storedMacros));
  currentOS = OS_WINDOWS;

  // Reload everything from EEPROM (what initSettings() does on boot)
  loadAllMacros();
  currentOS = loadOSMode();

  assertEq("Key 8 type persisted as STRING", storedMacros[7].type, MACRO_TYPE_STRING);
  assertEq("Key 8 data[0] persisted as 'H'", storedMacros[7].data[0], 'H');
  assertEq("Key 8 data[4] persisted as 'o'", storedMacros[7].data[4], 'o');

  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(8, name, sizeof(name));
  assertStrEq("Key 8 name persisted as 'Greet'", name, "Greet");

  uint8_t r, g, b;
  loadLEDColor(4, r, g, b);
  assertEq("LED 4 R persisted as 0", r, 0);
  assertEq("LED 4 G persisted as 255", g, 255);
  assertEq("LED 4 B persisted as 0", b, 0);

  assertEq("OS mode persisted as Mac", currentOS, OS_MAC);
  assertEq("Brightness persisted as 200", loadBrightness(), 200);
}

void testBoundaryAndEdgeCases() {
  Serial.println("\n--- Test 7: Boundary and Edge Cases ---");

  // Key number boundaries
  MacroConfig m;
  memset(&m, 0, sizeof(m));
  m.type = MACRO_TYPE_KEYCOMBO; m.modifier = 1; m.keycode = 4;

  // Key 1 (minimum valid)
  saveMacro(1, m);
  MacroConfig r1;
  loadMacro(1, r1);
  assertEq("Key 1 (min boundary) saves correctly", r1.type, MACRO_TYPE_KEYCOMBO);

  // Key 12 (maximum valid)
  m.keycode = 29;
  saveMacro(12, m);
  MacroConfig r12;
  loadMacro(12, r12);
  assertEq("Key 12 (max boundary) saves correctly", r12.keycode, 29);

  // Max-length key name (exactly 12 chars)
  saveKeyName(7, "TwelveChars!");
  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(7, name, sizeof(name));
  assertStrEq("12-char name stored fully", name, "TwelveChars!");

  // Near-max sequence length
  char longSeq[MAX_SEQUENCE_LENGTH];
  memset(longSeq, 0, sizeof(longSeq));
  // Build a valid long sequence: K:0:4 repeated with pipes
  int pos = 0;
  while (pos + 6 < MAX_SEQUENCE_LENGTH - 1) {
    if (pos > 0) longSeq[pos++] = '|';
    longSeq[pos++] = 'K';
    longSeq[pos++] = ':';
    longSeq[pos++] = '0';
    longSeq[pos++] = ':';
    longSeq[pos++] = '4';
  }
  longSeq[pos] = 0;
  saveSequence(6, 0, longSeq);

  char readSeq[MAX_SEQUENCE_LENGTH];
  loadSequence(6, 0, readSeq, sizeof(readSeq));
  assertEq("Long sequence length preserved", (int)strlen(readSeq), (int)strlen(longSeq));
  assertStrEq("Long sequence content preserved", readSeq, longSeq);

  // Brightness boundaries
  saveBrightness(0);
  assertEq("Brightness 0 saved", loadBrightness(), 0);
  saveBrightness(255);
  assertEq("Brightness 255 saved", loadBrightness(), 255);

  // LED color boundaries
  saveLEDColor(0, 0, 0, 0);
  uint8_t lr, lg, lb;
  loadLEDColor(0, lr, lg, lb);
  assertEq("LED all-zero R", lr, 0);
  assertEq("LED all-zero G", lg, 0);
  assertEq("LED all-zero B", lb, 0);

  saveLEDColor(0, 255, 255, 255);
  loadLEDColor(0, lr, lg, lb);
  assertEq("LED all-max R", lr, 255);
  assertEq("LED all-max G", lg, 255);
  assertEq("LED all-max B", lb, 255);
}

void testSerialCommandParsing() {
  Serial.println("\n--- Test 8: Serial Command Parsing ---");
  Serial.println("  Feeding commands through processTestCommand()...");

  // Start fresh
  wipeEEPROM();
  initDefaults();
  loadAllMacros();

  // Test MACRO command
  processTestCommand("MACRO 3 1 2 22");  // Key 3, keycombo, Shift, S
  MacroConfig m;
  loadMacro(3, m);
  assertEq("CMD: MACRO 3 type set to KEYCOMBO", m.type, MACRO_TYPE_KEYCOMBO);
  assertEq("CMD: MACRO 3 modifier set to 2 (Shift)", m.modifier, 2);
  assertEq("CMD: MACRO 3 keycode set to 22 (S)", m.keycode, 22);

  // Test NAME command
  processTestCommand("NAME 3 Save File");
  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(3, name, sizeof(name));
  assertStrEq("CMD: NAME 3 set to 'SAVE FILE'", name, "SAVE FILE");

  // Test SEQUENCE command
  processTestCommand("SEQUENCE 3 WIN K:8:18|D:200|T:CMD|K:0:40");
  char seq[MAX_SEQUENCE_LENGTH];
  loadSequence(3, 0, seq, sizeof(seq));
  assertStrEq("CMD: SEQUENCE 3 win stored", seq, "K:8:18|D:200|T:CMD|K:0:40");

  // Test LED command
  processTestCommand("LED 1 100 200 50");
  uint8_t r, g, b;
  loadLEDColor(1, r, g, b);
  assertEq("CMD: LED 1 R set to 100", r, 100);
  assertEq("CMD: LED 1 G set to 200", g, 200);
  assertEq("CMD: LED 1 B set to 50", b, 50);

  // Test BRIGHTNESS command
  processTestCommand("BRIGHTNESS 120");
  assertEq("CMD: BRIGHTNESS set to 120", loadBrightness(), 120);

  // Test OS command
  processTestCommand("OS MAC");
  assertEq("CMD: OS set to Mac", loadOSMode(), OS_MAC);

  processTestCommand("OS WIN");
  assertEq("CMD: OS set back to Win", loadOSMode(), OS_WINDOWS);
}

void testResetToDefaults() {
  Serial.println("\n--- Test 9: Reset to Defaults ---");

  // Mess up everything first
  MacroConfig m;
  memset(&m, 0, sizeof(m));
  m.type = MACRO_TYPE_STRING; m.data[0] = 'X';
  for (int i = 1; i <= NUM_MACROS; i++) saveMacro(i, m);
  for (int i = 1; i <= NUM_MACROS; i++) saveKeyName(i, "Overwritten");
  for (int i = 0; i < NUM_LEDS; i++) saveLEDColor(i, 99, 99, 99);
  saveOSMode(OS_MAC);
  saveBrightness(1);

  // Reset
  wipeEEPROM();
  initDefaults();
  loadAllMacros();

  assertEq("After RESET: Key 1 type is SPECIAL", storedMacros[0].type, MACRO_TYPE_SPECIAL);
  assertEq("After RESET: Key 2 type is KEYCOMBO", storedMacros[1].type, MACRO_TYPE_KEYCOMBO);
  assertEq("After RESET: Key 3 type is DISABLED", storedMacros[2].type, MACRO_TYPE_DISABLED);

  char name[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(1, name, sizeof(name));
  assertStrEq("After RESET: Key 1 name restored", name, "Screenshot");

  assertEq("After RESET: OS is Windows", loadOSMode(), OS_WINDOWS);
  assertEq("After RESET: Brightness is default", loadBrightness(), LED_BRIGHTNESS_DEFAULT);

  uint8_t r, g, b;
  loadLEDColor(0, r, g, b);
  assertEq("After RESET: LED 0 B is 50", b, 50);
}

void testConfigIsolation() {
  Serial.println("\n--- Test 10: Config Isolation Between Keys ---");

  wipeEEPROM();
  initDefaults();

  // Write distinct configs to adjacent keys
  MacroConfig m3, m4, m5;
  memset(&m3, 0, sizeof(m3));
  memset(&m4, 0, sizeof(m4));
  memset(&m5, 0, sizeof(m5));

  m3.type = MACRO_TYPE_KEYCOMBO; m3.modifier = 1; m3.keycode = 6;  // Ctrl+C
  m4.type = MACRO_TYPE_STRING;   m4.data[0] = 'A'; m4.data[1] = 'B';
  m5.type = MACRO_TYPE_SEQUENCE; m5.keycode = 99;

  saveMacro(3, m3);
  saveMacro(4, m4);
  saveMacro(5, m5);

  saveKeyName(3, "CopyKey");
  saveKeyName(4, "TypeAB");
  saveKeyName(5, "SeqKey");

  // Read back and cross-check
  MacroConfig r3, r4, r5;
  loadMacro(3, r3);
  loadMacro(4, r4);
  loadMacro(5, r5);

  assertEq("Key 3 isolated: type KEYCOMBO", r3.type, MACRO_TYPE_KEYCOMBO);
  assertEq("Key 4 isolated: type STRING", r4.type, MACRO_TYPE_STRING);
  assertEq("Key 5 isolated: type SEQUENCE", r5.type, MACRO_TYPE_SEQUENCE);
  assertEq("Key 4 data[0] is 'A'", r4.data[0], 'A');
  assertEq("Key 5 keycode is 99", r5.keycode, 99);

  char n3[MAX_KEY_NAME_LENGTH + 1], n4[MAX_KEY_NAME_LENGTH + 1], n5[MAX_KEY_NAME_LENGTH + 1];
  loadKeyName(3, n3, sizeof(n3));
  loadKeyName(4, n4, sizeof(n4));
  loadKeyName(5, n5, sizeof(n5));
  assertStrEq("Key 3 name isolated", n3, "CopyKey");
  assertStrEq("Key 4 name isolated", n4, "TypeAB");
  assertStrEq("Key 5 name isolated", n5, "SeqKey");
}

// ============================================
// SERIAL COMMAND PROCESSOR (mirrors commands.ino)
// ============================================

void processTestCommand(const char* input) {
  String cmd = String(input);
  cmd.trim();
  if (cmd.length() == 0) return;

  int spaceIdx = cmd.indexOf(' ');
  String command = (spaceIdx > 0) ? cmd.substring(0, spaceIdx) : cmd;
  command.toUpperCase();
  String args = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1) : "";

  if (command == "MACRO") {
    int keyNum = args.substring(0, args.indexOf(' ')).toInt();
    args = args.substring(args.indexOf(' ') + 1);
    int type = args.substring(0, args.indexOf(' ')).toInt();
    args = args.substring(args.indexOf(' ') + 1);
    int modifier = args.substring(0, args.indexOf(' ')).toInt();
    args = args.substring(args.indexOf(' ') + 1);
    int keycode = args.toInt();

    if (keyNum >= 1 && keyNum <= NUM_MACROS) {
      MacroConfig m;
      memset(&m, 0, sizeof(m));
      m.type = type;
      m.modifier = modifier;
      m.keycode = keycode;
      saveMacro(keyNum, m);
      loadAllMacros();
      Serial.print("    OK: Macro saved for Key ");
      Serial.println(keyNum);
    }
  } else if (command == "SEQUENCE") {
    int firstSpace = args.indexOf(' ');
    int keyNum = args.substring(0, firstSpace).toInt();
    String remaining = args.substring(firstSpace + 1);
    int secondSpace = remaining.indexOf(' ');
    String osStr = remaining.substring(0, secondSpace);
    osStr.toUpperCase();
    String seqStr = remaining.substring(secondSpace + 1);
    int osOffset = (osStr == "MAC") ? 1 : 0;

    if (keyNum >= 1 && keyNum <= NUM_MACROS) {
      int seqAddr = EEPROM_ADDR_SEQUENCES + ((keyNum - 1) * 2 + osOffset) * MAX_SEQUENCE_LENGTH;
      int len = min((int)seqStr.length(), MAX_SEQUENCE_LENGTH - 1);
      for (int i = 0; i < len; i++) EEPROM.write(seqAddr + i, seqStr[i]);
      EEPROM.write(seqAddr + len, 0);
      EEPROM.commit();
      Serial.print("    OK: Sequence saved for Key ");
      Serial.println(keyNum);
    }
  } else if (command == "NAME") {
    int spIdx = args.indexOf(' ');
    int keyNum = args.substring(0, spIdx).toInt();
    String name = args.substring(spIdx + 1);
    if (keyNum >= 1 && keyNum <= NUM_MACROS && name.length() <= MAX_KEY_NAME_LENGTH) {
      saveKeyName(keyNum, name.c_str());
      Serial.print("    OK: Key ");
      Serial.print(keyNum);
      Serial.print(" renamed to ");
      Serial.println(name);
    }
  } else if (command == "LED") {
    int idx = args.substring(0, args.indexOf(' ')).toInt();
    args = args.substring(args.indexOf(' ') + 1);
    int r = args.substring(0, args.indexOf(' ')).toInt();
    args = args.substring(args.indexOf(' ') + 1);
    int g = args.substring(0, args.indexOf(' ')).toInt();
    args = args.substring(args.indexOf(' ') + 1);
    int b = args.toInt();
    if (idx >= 0 && idx < NUM_LEDS) {
      saveLEDColor(idx, r, g, b);
      Serial.print("    OK: LED ");
      Serial.print(idx);
      Serial.println(" color set");
    }
  } else if (command == "BRIGHTNESS") {
    int val = constrain(args.toInt(), 0, 255);
    saveBrightness(val);
    Serial.print("    OK: Brightness set to ");
    Serial.println(val);
  } else if (command == "OS") {
    args.toUpperCase();
    if (args == "MAC") {
      saveOSMode(OS_MAC);
      currentOS = OS_MAC;
      Serial.println("    OK: OS set to macOS");
    } else {
      saveOSMode(OS_WINDOWS);
      currentOS = OS_WINDOWS;
      Serial.println("    OK: OS set to Windows");
    }
  } else if (command == "LIST") {
    printFullConfig();
  } else if (command == "RESET") {
    wipeEEPROM();
    initDefaults();
    loadAllMacros();
    currentOS = loadOSMode();
    Serial.println("    OK: Reset to defaults");
  } else if (command == "HELP") {
    printHelp();
  } else {
    Serial.print("    Unknown command: ");
    Serial.println(command);
  }
}

// ============================================
// INTERACTIVE MODE HELPERS
// ============================================

void printFullConfig() {
  loadAllMacros();
  currentOS = loadOSMode();

  // Format matches firmware handleListCommand() exactly so the
  // web configurator's parseListOutput() can consume it unchanged.

  Serial.println("\n=== MACRO CONFIGURATION ===");
  Serial.println("Key | Name              | Type      | Modifier | Keycode | Data");
  Serial.println("----|-------------------|-----------|----------|---------|-----");

  for (int i = 0; i < NUM_MACROS; i++) {
    MacroConfig &m = storedMacros[i];
    char name[MAX_KEY_NAME_LENGTH + 1];
    loadKeyName(i + 1, name, sizeof(name));

    Serial.print(i + 1);
    Serial.print("   | ");

    Serial.print(name);
    int nameLen = strlen(name);
    for (int j = nameLen; j < 17; j++) Serial.print(" ");

    Serial.print("| ");

    switch (m.type) {
      case MACRO_TYPE_DISABLED:  Serial.print("DISABLED "); break;
      case MACRO_TYPE_KEYCOMBO:  Serial.print("KEYCOMBO  "); break;
      case MACRO_TYPE_STRING:    Serial.print("STRING    "); break;
      case MACRO_TYPE_SPECIAL:   Serial.print("SPECIAL   "); break;
      case MACRO_TYPE_SEQUENCE:  Serial.print("SEQUENCE  "); break;
      default:                   Serial.print("UNKNOWN   "); break;
    }

    Serial.print("| ");
    Serial.print(m.modifier);
    Serial.print("       | ");
    Serial.print(m.keycode);
    Serial.print("      | ");

    if (m.type == MACRO_TYPE_STRING) {
      for (int j = 0; j < 5 && m.data[j] != 0; j++) Serial.print((char)m.data[j]);
    } else {
      Serial.print("-");
    }

    Serial.println();
  }

  Serial.println("\n=== SEQUENCES ===");
  for (int i = 0; i < NUM_MACROS; i++) {
    char seq[MAX_SEQUENCE_LENGTH];
    loadSequence(i + 1, 0, seq, sizeof(seq));
    if (strlen(seq) > 0) {
      Serial.print(i + 1);
      Serial.print(":win:");
      Serial.println(seq);
    }
    loadSequence(i + 1, 1, seq, sizeof(seq));
    if (strlen(seq) > 0) {
      Serial.print(i + 1);
      Serial.print(":mac:");
      Serial.println(seq);
    }
  }

  Serial.println("\n=== LED COLORS ===");
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t r, g, b;
    loadLEDColor(i, r, g, b);
    Serial.print("LED ");
    Serial.print(i);
    Serial.print(": RGB(");
    Serial.print(r); Serial.print(", ");
    Serial.print(g); Serial.print(", ");
    Serial.print(b); Serial.println(")");
  }

  Serial.print("\nOS Mode: ");
  Serial.println(currentOS == OS_MAC ? "macOS" : "Windows");

  Serial.print("LED Brightness: ");
  Serial.println(loadBrightness());
}

void printHelp() {
  Serial.println("\n=== INTERACTIVE TEST COMMANDS ===");
  Serial.println("MACRO <key> <type> <mod> <keycode>  - Set macro (key 1-12)");
  Serial.println("  Types: 0=disabled, 1=keycombo, 2=string, 3=special, 4=sequence");
  Serial.println("  Mods:  0=none, 1=Ctrl, 2=Shift, 4=Alt, 8=GUI (combine with +)");
  Serial.println("SEQUENCE <key> <os> <steps>         - Set sequence (os: win/mac)");
  Serial.println("  Steps: K:mod:key | T:text | D:ms | R");
  Serial.println("NAME <key> <name>                   - Set key name (max 12 chars)");
  Serial.println("LED <idx> <r> <g> <b>               - Set LED color (idx 0-4)");
  Serial.println("BRIGHTNESS <0-255>                  - Set LED brightness");
  Serial.println("OS <win|mac>                        - Set OS mode");
  Serial.println("LIST                                - Show all stored configs");
  Serial.println("RESET                               - Reset to defaults");
  Serial.println("HELP                                - Show this help");
  Serial.println("RETEST                              - Re-run automated tests");
  Serial.println();
}

// ============================================
// MAIN
// ============================================

void runAllTests() {
  testsRun = 0;
  testsPassed = 0;
  testsFailed = 0;

  Serial.println("╔══════════════════════════════════════════════╗");
  Serial.println("║   CONFIG SYSTEM TEST SUITE                  ║");
  Serial.println("║   Board: ESP32-S3 Feather                   ║");
  Serial.println("╚══════════════════════════════════════════════╝");

  testEEPROMInit();
  testFetchStoredConfigs();
  testPushNewConfig();
  testSequenceSaveLoad();
  testModifyExistingConfig();
  testPersistenceAcrossReboot();
  testBoundaryAndEdgeCases();
  testSerialCommandParsing();
  testResetToDefaults();
  testConfigIsolation();

  Serial.println("\n══════════════════════════════════════════════");
  Serial.print("  RESULTS: ");
  Serial.print(testsPassed);
  Serial.print(" passed, ");
  Serial.print(testsFailed);
  Serial.print(" failed, ");
  Serial.print(testsRun);
  Serial.println(" total");
  if (testsFailed == 0) {
    Serial.println("  ALL TESTS PASSED");
  } else {
    Serial.println("  SOME TESTS FAILED - check output above");
  }
  Serial.println("══════════════════════════════════════════════\n");
}

void setup() {
  USB.productName("FIDO2 Macropad");
  USB.manufacturerName("Passkey");
  USB.begin();

  Serial.begin(115200);
  delay(2000);

  Serial.println("USB Device Name: FIDO2 Macropad");
  Serial.println("Manufacturer:    Passkey\n");

  EEPROM.begin(EEPROM_SIZE);

  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    Serial.println("First boot detected -- initializing defaults...\n");
    initDefaults();
  } else {
    Serial.println("EEPROM already initialized -- loading saved config.\n");
  }

  loadAllMacros();
  currentOS = loadOSMode();

  Serial.println("Entering interactive mode. Type HELP for commands.");
  Serial.println("Use LIST to view current config, or type commands");
  Serial.println("exactly as the web configurator would send them.\n");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("RETEST")) {
      runAllTests();
      initDefaults();
      loadAllMacros();
      currentOS = loadOSMode();
      Serial.println("Back in interactive mode.\n");
    } else {
      processTestCommand(cmd.c_str());
    }
  }
}
