/*
 * Key Matrix Module - Implementation
 * Handles 3x4 key matrix scanning and macro execution
 */

#include "config.h"
#include "cred_store.h"
#include "debug.h"
#include "display.h"
#include "encoders.h"
#include "fingerprint.h"
#include "keymatrix.h"
#include "leds.h"
#include "settings.h"
#include "usb_hid.h"
#include <Arduino.h>
#include <ESP.h>

bool keyStates[MATRIX_ROWS][MATRIX_COLS] = {0};
bool lastKeyStates[MATRIX_ROWS][MATRIX_COLS] = {0};

// Key 9+12 long-press combo state
static unsigned long comboStartTime = 0;
static bool comboTriggered = false;
static bool comboActive = false;  // true while both keys are held (suppresses macros)
static const unsigned long COMBO_HOLD_MS = 2000;  // 2 seconds to activate

// Sequence macro state machine (non-blocking delays via millis)
static bool seqMacroActive = false;
static char seqMacroSequence[MAX_SEQUENCE_LENGTH + 1] = {0};
static int seqMacroStartIdx = 0;
static unsigned long seqMacroDelayEnd = 0;  // 0 = not waiting, else millis() when delay ends

void handleKeyPress(int row, int col);
void handleKeyRelease(int row, int col);

bool isKeyPressed(int row, int col) {
  if (row < 0 || row >= MATRIX_ROWS || col < 0 || col >= MATRIX_COLS) return false;
  return keyStates[row][col];
}
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

// Runs sequence steps from seqMacroStartIdx. Returns true if paused for delay (resume via tickSequenceMacro), false when done.
static bool runSequenceSteps() {
  String seq = String(seqMacroSequence);

  while (seqMacroStartIdx < seq.length()) {
    int pipeIdx = seq.indexOf('|', seqMacroStartIdx);
    String step = (pipeIdx >= 0) ? seq.substring(seqMacroStartIdx, pipeIdx) : seq.substring(seqMacroStartIdx);
    int nextStartIdx = (pipeIdx >= 0) ? pipeIdx + 1 : (int)seq.length();

    if (step.length() == 0) break;

    if (step.startsWith("K:")) {
      int colon1 = 2;
      int colon2 = step.indexOf(':', colon1 + 1);

      if (colon2 > colon1) {
        uint8_t modifier = step.substring(colon1, colon2).toInt();
        String keycodesStr = step.substring(colon2 + 1);

        uint8_t hidMod = (modifier == 0) ? ((currentOS == OS_MAC) ? 0x08 : 0x01) : (modifier & 0x0F);

        uint8_t keycodes[6] = {0};
        int keyCount = 0;
        int keyIdx = 0;

        while (keyIdx < keycodesStr.length() && keyCount < 6) {
          int commaIdx = keycodesStr.indexOf(',', keyIdx);
          String keycodeStr = (commaIdx >= 0) ? keycodesStr.substring(keyIdx, commaIdx) : keycodesStr.substring(keyIdx);
          uint8_t keycode = keycodeStr.toInt();

          if (keycode > 0) {
            keycodes[keyCount++] = keycode;
          }

          keyIdx = (commaIdx >= 0) ? commaIdx + 1 : keycodesStr.length();
        }

        if (keyCount == 1) {
          if (usb_kbd.ready()) {
            sendKeyCombo(hidMod, keycodes[0]);
            delay(10);
          } else {
            debugPrintln("  -> USB keyboard not ready, skipping");
          }
        } else if (keyCount > 0) {
          if (usb_kbd.ready()) {
            usb_kbd.keyboardReport(RID_KEYBOARD, hidMod, keycodes);
            delay(10);
            usb_kbd.keyboardRelease(RID_KEYBOARD);
            delay(10);
          } else {
            debugPrintln("  -> USB keyboard not ready, skipping");
          }
        }
      }

    } else if (step.startsWith("T:")) {
      String text = step.substring(2);
      sendString(text.c_str());

    } else if (step.startsWith("D:")) {
      int delayMs = step.substring(2).toInt();
      delayMs = constrain(delayMs, 0, 5000);
      seqMacroDelayEnd = millis() + (unsigned long)delayMs;
      seqMacroStartIdx = nextStartIdx;
      return true;  // Paused for delay — tickSequenceMacro will resume

    } else if (step == "R") {
      usb_kbd.keyboardRelease(RID_KEYBOARD);
      delay(10);
    }

    seqMacroStartIdx = nextStartIdx;
  }

  return false;  // Sequence complete
}

void tickSequenceMacro() {
  if (!seqMacroActive) return;
  if (seqMacroDelayEnd != 0 && millis() < seqMacroDelayEnd) return;  // Still waiting

  seqMacroDelayEnd = 0;
  if (!runSequenceSteps()) {
    seqMacroActive = false;
  }
}

void executeSequenceMacro(int keyNum) {
  if (seqMacroActive) {
    debugPrintln("  -> Sequence already running, ignoring");
    return;
  }

  int osOffset = (currentOS == OS_MAC) ? 1 : 0;
  char sequenceStr[MAX_SEQUENCE_LENGTH + 1] = {0};
  loadSequence(keyNum, osOffset, sequenceStr, sizeof(sequenceStr));

  if (sequenceStr[0] == 0) {
    debugPrintln("  -> No sequence defined for current OS");
    return;
  }

  debugPrint("  -> Seq[");
  debugPrint(currentOS == OS_MAC ? F("Mac") : F("Win"));
  debugPrint(F("]: "));
  debugPrintln(sequenceStr);

  strncpy(seqMacroSequence, sequenceStr, MAX_SEQUENCE_LENGTH);
  seqMacroSequence[MAX_SEQUENCE_LENGTH] = 0;
  seqMacroStartIdx = 0;
  seqMacroDelayEnd = 0;
  seqMacroActive = true;

  if (!runSequenceSteps()) {
    seqMacroActive = false;
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
    // HID modifier byte: bitmask 0x01=Ctrl, 0x02=Shift, 0x04=Alt, 0x08=GUI (NOT HID_KEY_* keycodes).
    uint8_t modifier = (macro.modifier == 0) ? ((currentOS == OS_MAC) ? 0x08 : 0x01) : (macro.modifier & 0x0F);

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
  sysMenuItemCount = 5;  // Toggle OS, Enroll FP, Delete FP, System Reset, Exit
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

          // Brief confirmation on OLED (centered)
          display.clearDisplay();
          display.setTextColor(SH110X_WHITE);
          centerText("OS MODE:", 10, 2);
          centerText(currentOS == OS_MAC ? " MAC" : " WIN", 35, 3);
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

        case 3: {
          // System Reset — show confirmation
          debugPrintln("  System Menu -> System Reset (confirm)");
          fpConfirmYes = false;
          currentMode = MODE_SYSTEM_RESET_CONFIRM;
          lastSysMenuEncoderPos = encoderPosition;
          updateDisplay();
          break;
        }

        case 4:
          // Exit
          debugPrintln("  System Menu -> Exit");
          currentMode = MODE_IDLE;
          updateDisplay();
          break;
      }
    }
  }
}

void updateSystemResetConfirm() {
  static unsigned long lastButtonTime = 0;
  const unsigned long debounceDelay = 300;

  // Encoder rotation toggles Yes/No
  int currentPos = encoderPosition;
  if (currentPos != lastSysMenuEncoderPos) {
    lastSysMenuEncoderPos = currentPos;
    fpConfirmYes = !fpConfirmYes;
    updateDisplay();
  }

  // Button press
  if (digitalRead(ENCODER_SW) == LOW) {
    if ((millis() - lastButtonTime) > debounceDelay) {
      lastButtonTime = millis();

      if (fpConfirmYes) {
        debugPrintln("  System Reset CONFIRMED — wiping all data...");

        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        centerText(F("RESETTING..."), 14);
        display.setCursor(4, 28);
        display.print(F("Clearing data"));
        display.display();

        // 1. Clear all fingerprints
        clearAllFingerprints();
        // clearAllFingerprints sets its own mode and delays; we override to continue

        // 2. Reset EEPROM settings (macros, LEDs, key names, sequences)
        resetAllSettings();

        // 3. Clear FIDO2 credentials
        cred_clear();

        display.clearDisplay();
        display.setTextColor(SH110X_WHITE);
        centerText(F("RESET COMPLETE"), 14);
        display.setCursor(4, 28);
        display.print(F("Rebooting..."));
        display.display();

        delay(2000);
        ESP.restart();
      } else {
        debugPrintln("  System Reset cancelled");
        currentMode = MODE_SYSTEM_MENU;
        lastSysMenuEncoderPos = encoderPosition;
        updateDisplay();
      }
    }
  }
}
