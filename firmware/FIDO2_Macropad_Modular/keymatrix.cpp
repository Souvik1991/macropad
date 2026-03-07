/*
 * Key Matrix Module - Implementation
 * Handles 3x4 key matrix scanning and macro execution
 */

#include "config.h"
#include "debug.h"
#include "display.h"
#include "encoders.h"
#include "fingerprint.h"
#include "keymatrix.h"
#include "leds.h"
#include "settings.h"
#include "usb_hid.h"
#include <Arduino.h>

bool keyStates[MATRIX_ROWS][MATRIX_COLS] = {0};
bool lastKeyStates[MATRIX_ROWS][MATRIX_COLS] = {0};

// Key 9+12 long-press combo state
static unsigned long comboStartTime = 0;
static bool comboTriggered = false;
static bool comboActive = false;  // true while both keys are held (suppresses macros)
static const unsigned long COMBO_HOLD_MS = 2000;  // 2 seconds to activate

void handleKeyPress(int row, int col);
void handleKeyRelease(int row, int col);
void executeMacro(int keyNum);
void executeSequenceMacro(int keyNum);

void initKeyMatrix() {
  debugPrint("Initializing key matrix... ");
  
  for (int i = 0; i < MATRIX_ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  
  for (int i = 0; i < MATRIX_COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  
  debugPrintln("OK");
}

void scanKeyMatrix() {
  for (int row = 0; row < MATRIX_ROWS; row++) {
    digitalWrite(rowPins[row], LOW);
    delayMicroseconds(10);
    
    for (int col = 0; col < MATRIX_COLS; col++) {
      keyStates[row][col] = !digitalRead(colPins[col]);
      
      if (keyStates[row][col] && !lastKeyStates[row][col]) {
        handleKeyPress(row, col);
      }
      
      if (!keyStates[row][col] && lastKeyStates[row][col]) {
        handleKeyRelease(row, col);
      }
      
      lastKeyStates[row][col] = keyStates[row][col];
    }
    
    digitalWrite(rowPins[row], HIGH);
  }

  // --- Key 9+12 combo detection (after full scan so keyStates are fresh) ---
  // Key 9 = Row 2, Col 0 | Key 12 = Row 2, Col 3
  bool key9Held = keyStates[2][0];
  bool key12Held = keyStates[2][3];

  if (key9Held && key12Held) {
    if (!comboTriggered) {
      if (comboStartTime == 0) {
        // Just started holding both keys
        comboStartTime = millis();
        comboActive = true;
        debugPrintln("  Combo hold started (K9+K12)");
      }

      unsigned long held = millis() - comboStartTime;

      // LED fill-up animation: light LEDs 0→4 progressively over 2 seconds
      int ledsLit = (int)((held * NUM_LEDS) / COMBO_HOLD_MS);
      if (ledsLit > NUM_LEDS) ledsLit = NUM_LEDS;
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i < ledsLit) {
          leds[i] = CRGB(0, 180, 200);  // Cyan fill
        } else {
          leds[i] = CRGB::Black;
        }
      }
      FastLED.show();

      if (held >= COMBO_HOLD_MS) {
        // Combo activated!
        comboTriggered = true;
        debugPrintln("  => System Menu activated!");
        flashAllLEDs(CRGB::Green, 150);
        restoreSavedLEDs();
        enterSystemMenu();
      }
    }
  } else {
    // One or both keys released
    if (comboActive && !comboTriggered) {
      // Released before 2s — cancel, restore LEDs
      restoreSavedLEDs();
      debugPrintln("  Combo cancelled (released early)");
    }
    comboStartTime = 0;
    comboTriggered = false;
    comboActive = false;
  }
}

void handleKeyPress(int row, int col) {
  int keyNum = (row * MATRIX_COLS) + col + 1;
  
  debugPrint("Key pressed: ");
  debugPrint(keyNum);
  debugPrint(" (R");
  debugPrint(row + 1);
  debugPrint("C");
  debugPrint(col + 1);
  debugPrintln(")");
  
  executeMacro(keyNum);
  flashLED(col % NUM_LEDS);
}

void handleKeyRelease(int row, int col) {
  // Optional: handle key release events
}

void executeSequenceMacro(int keyNum) {
  int osOffset = (currentOS == OS_MAC) ? 1 : 0;
  int seqAddr = EEPROM_ADDR_SEQUENCES + ((keyNum - 1) * 2 + osOffset) * MAX_SEQUENCE_LENGTH;

  char sequenceStr[MAX_SEQUENCE_LENGTH + 1] = {0};
  int seqLen = 0;
  for (int i = 0; i < MAX_SEQUENCE_LENGTH; i++) {
    char c = EEPROM.read(seqAddr + i);
    if (c == 0) break;
    sequenceStr[i] = c;
    seqLen++;
  }

  if (seqLen == 0) {
    debugPrintln("  -> No sequence defined for current OS");
    return;
  }

  String seq = String(sequenceStr);
  int startIdx = 0;

  while (startIdx < seq.length()) {
    int pipeIdx = seq.indexOf('|', startIdx);
    String step = (pipeIdx > 0) ? seq.substring(startIdx, pipeIdx) : seq.substring(startIdx);

    if (step.length() == 0) break;

    if (step.startsWith("K:")) {
      int colon1 = 2;
      int colon2 = step.indexOf(':', colon1 + 1);

      if (colon2 > colon1) {
        uint8_t modifier = step.substring(colon1 + 1, colon2).toInt();
        String keycodesStr = step.substring(colon2 + 1);

        uint8_t hidMod = 0;
        if (modifier & 1) hidMod |= HID_KEY_CONTROL_LEFT;
        if (modifier & 2) hidMod |= HID_KEY_SHIFT_LEFT;
        if (modifier & 4) hidMod |= HID_KEY_ALT_LEFT;
        if (modifier & 8) hidMod |= HID_KEY_GUI_LEFT;

        if (modifier == 0) {
          hidMod = (currentOS == OS_MAC) ? HID_KEY_GUI_LEFT : HID_KEY_CONTROL_LEFT;
        }

        uint8_t keycodes[6] = {0};
        int keyCount = 0;
        int keyIdx = 0;

        while (keyIdx < keycodesStr.length() && keyCount < 6) {
          int commaIdx = keycodesStr.indexOf(',', keyIdx);
          String keycodeStr = (commaIdx > 0) ? keycodesStr.substring(keyIdx, commaIdx) : keycodesStr.substring(keyIdx);
          uint8_t keycode = keycodeStr.toInt();

          if (keycode > 0) {
            keycodes[keyCount++] = keycode;
          }

          keyIdx = (commaIdx > 0) ? commaIdx + 1 : keycodesStr.length();
        }

        usb_kbd.keyboardReport(0, hidMod, keycodes);
        delay(10);
        usb_kbd.keyboardRelease(0);
        delay(10);
      }

    } else if (step.startsWith("T:")) {
      String text = step.substring(2);
      sendString(text.c_str());

    } else if (step.startsWith("D:")) {
      int delayMs = step.substring(2).toInt();
      delayMs = constrain(delayMs, 0, 5000);
      delay(delayMs);

    } else if (step == "R") {
      usb_kbd.keyboardRelease(0);
      delay(10);
    }

    startIdx = (pipeIdx > 0) ? pipeIdx + 1 : seq.length();
  }
}

void executeMacro(int keyNum) {
  // Suppress macros while the Key 9+12 combo is being held
  if (comboActive) return;

  // =====================================================
  // Normal macro execution
  // =====================================================

  if (keyNum < 1 || keyNum > NUM_MACROS) return;

  MacroConfig &macro = storedMacros[keyNum - 1];

  if (macro.type == MACRO_TYPE_DISABLED) {
    debugPrintln("  -> Key not configured");
    return;
  }

  if (macro.type == MACRO_TYPE_KEYCOMBO) {
    uint8_t modifier = macro.modifier;

    if (modifier == 0) {
      modifier = (currentOS == OS_MAC) ? HID_KEY_GUI_LEFT : HID_KEY_CONTROL_LEFT;
    } else {
      uint8_t hidMod = 0;
      if (modifier & 1) hidMod |= HID_KEY_CONTROL_LEFT;
      if (modifier & 2) hidMod |= HID_KEY_SHIFT_LEFT;
      if (modifier & 4) hidMod |= HID_KEY_ALT_LEFT;
      if (modifier & 8) hidMod |= HID_KEY_GUI_LEFT;
      modifier = hidMod;
    }

    sendKeyCombo(modifier, macro.keycode);
    debugPrint("  -> Executed macro: Key ");
    debugPrintln(keyNum);

  } else if (macro.type == MACRO_TYPE_STRING) {
    char str[6] = {0};
    for (int i = 0; i < 5 && macro.data[i] != 0; i++) {
      str[i] = macro.data[i];
    }
    sendString(str);
    debugPrint("  -> Executed string macro: ");
    debugPrintln(str);

  } else if (macro.type == MACRO_TYPE_SEQUENCE) {
    executeSequenceMacro(keyNum);
    debugPrint("  -> Executed sequence macro: Key ");
    debugPrintln(keyNum);
  }
}

// =====================================================
// System Menu — Enter & Update (encoder-driven)
// =====================================================

static int lastSysMenuEncoderPos = 0;

void enterSystemMenu() {
  sysMenuSelection = 0;
  sysMenuItemCount = 4;  // Toggle OS, Enroll FP, Delete FP, Exit
  lastSysMenuEncoderPos = encoderPosition;
  currentMode = MODE_SYSTEM_MENU;
  updateDisplay();
  debugPrintln("Entered System Menu");
}

void updateSystemMenu() {
  static unsigned long lastButtonTime = 0;
  const unsigned long debounceDelay = 300;

  // --- Rotation: navigate menu items ---
  int currentPos = encoderPosition;
  if (currentPos != lastSysMenuEncoderPos) {
    int diff = currentPos - lastSysMenuEncoderPos;
    lastSysMenuEncoderPos = currentPos;

    sysMenuSelection += diff;
    if (sysMenuSelection < 0) sysMenuSelection = 0;
    if (sysMenuSelection >= (int8_t)sysMenuItemCount) sysMenuSelection = sysMenuItemCount - 1;

    updateDisplay();
  }

  // --- Button press: select action ---
  if (digitalRead(ENCODER_SW) == LOW) {
    if ((millis() - lastButtonTime) > debounceDelay) {
      lastButtonTime = millis();

      switch (sysMenuSelection) {
        case 0: {
          // Toggle OS Mode
          toggleOSMode();
          debugPrintln("========================================");
          debugPrint("  OS SWITCHED TO: ");
          debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
          debugPrintln("========================================");

          // Brief confirmation on OLED
          display.clearDisplay();
          display.setTextSize(2);
          display.setTextColor(SH110X_WHITE);
          display.setCursor(0, 10);
          display.println("OS MODE:");
          display.setTextSize(3);
          display.setCursor(10, 35);
          display.println(currentOS == OS_MAC ? " MAC" : " WIN");
          display.display();

          // Blue flash animation
          for (int i = 0; i < NUM_LEDS; i++) {
            setAllLEDs(CRGB::Blue);
            FastLED.show();
            delay(100);
            setAllLEDs(CRGB::Black);
            FastLED.show();
            delay(50);
          }
          delay(1000);

          currentMode = MODE_IDLE;
          restoreSavedLEDs();
          updateDisplay();
          break;
        }

        case 1:
          // Enroll Fingerprint
          debugPrintln("  System Menu -> Enroll Fingerprint");
          enrollFingerprint(0);  // Auto-assign ID
          // enrollFingerprint sets currentMode to IDLE or appropriate state
          break;

        case 2:
          // Delete Fingerprint (enters existing interactive menu)
          debugPrintln("  System Menu -> Delete Fingerprint");
          enterFingerprintDeleteMenu();
          break;

        case 3:
          // Exit
          debugPrintln("  System Menu -> Exit");
          currentMode = MODE_IDLE;
          updateDisplay();
          break;
      }
    }
  }
}
