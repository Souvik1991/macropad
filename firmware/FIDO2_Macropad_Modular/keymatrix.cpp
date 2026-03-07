/*
 * Key Matrix Module - Implementation
 * Handles 3x4 key matrix scanning and macro execution
 */

#include "config.h"
#include "debug.h"
#include "display.h"
#include "keymatrix.h"
#include "leds.h"
#include "settings.h"
#include "usb_hid.h"
#include <Arduino.h>

bool keyStates[MATRIX_ROWS][MATRIX_COLS] = {0};
bool lastKeyStates[MATRIX_ROWS][MATRIX_COLS] = {0};
unsigned long lastDeletePressTime = 0;
const unsigned long doubleTapDelay = 500;
unsigned long lastOSSwitchPressTime = 0;

void handleKeyPress(int row, int col);
void handleKeyRelease(int row, int col);
void executeMacro(int keyNum);
void executeSequenceMacro(int keyNum);
void executeHardcodedMacro(int keyNum);

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

void executeMacro(int keyNum) {
  if (keyNum >= 1 && keyNum <= NUM_MACROS) {
    MacroConfig &macro = storedMacros[keyNum - 1];
    
    if (macro.type == MACRO_TYPE_DISABLED) {
      debugPrintln("  -> Macro disabled, using hardcoded fallback");
      executeHardcodedMacro(keyNum);
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
      debugPrint("  -> Executed stored macro: Key ");
      debugPrintln(keyNum);
      return;
      
    } else if (macro.type == MACRO_TYPE_STRING) {
      char str[6] = {0};
      for (int i = 0; i < 5 && macro.data[i] != 0; i++) {
        str[i] = macro.data[i];
      }
      sendString(str);
      debugPrint("  -> Executed string macro: ");
      debugPrintln(str);
      return;
      
    } else if (macro.type == MACRO_TYPE_SPECIAL) {
      executeHardcodedMacro(keyNum);
      return;
      
    } else if (macro.type == MACRO_TYPE_SEQUENCE) {
      executeSequenceMacro(keyNum);
      debugPrint("  -> Executed sequence macro: Key ");
      debugPrintln(keyNum);
      return;
    }
  }
  
  executeHardcodedMacro(keyNum);
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
    debugPrintln("  -> No sequence defined for current OS, using fallback");
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

void executeHardcodedMacro(int keyNum) {
  uint8_t modKey = (currentOS == OS_MAC) ? HID_KEY_GUI_LEFT : HID_KEY_CONTROL_LEFT;
  
  switch(keyNum) {
    case 1:
      if (currentOS == OS_MAC) {
        sendKeyCombo(HID_KEY_GUI_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_4);
        debugPrintln("  -> Screenshot (Mac: Cmd+Shift+4)");
      } else {
        sendKeyCombo(HID_KEY_GUI_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_S);
        debugPrintln("  -> Screenshot (Win: Win+Shift+S)");
      }
      break;
      
    case 2:
      sendKeyCombo(HID_KEY_CONTROL_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_M);
      debugPrintln("  -> Mic Toggle (Ctrl+Shift+M)");
      debugPrintln("     Note: Configure this shortcut in Zoom/Webex/GMeet");
      break;
      
    case 3:
      sendKeyCombo(HID_KEY_CONTROL_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_V);
      debugPrintln("  -> Video Toggle (Ctrl+Shift+V)");
      debugPrintln("     Note: Configure this shortcut in Zoom/Webex/GMeet");
      break;
      
    case 4:
      if (currentOS == OS_MAC) {
        sendKeyCombo(HID_KEY_GUI_LEFT | HID_KEY_CONTROL_LEFT, HID_KEY_Q);
        debugPrintln("  -> Lock Screen (Mac: Cmd+Ctrl+Q)");
      } else {
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_L);
        debugPrintln("  -> Lock Screen (Win: Win+L)");
      }
      break;
      
    case 5:
      sendKeyCombo(modKey, HID_KEY_C);
      debugPrint("  -> Copy (");
      debugPrint(currentOS == OS_MAC ? "Cmd+C" : "Ctrl+C");
      debugPrintln(")");
      break;
      
    case 6:
      sendKeyCombo(modKey, HID_KEY_X);
      debugPrint("  -> Cut (");
      debugPrint(currentOS == OS_MAC ? "Cmd+X" : "Ctrl+X");
      debugPrintln(")");
      break;
      
    case 7:
      sendKeyCombo(modKey, HID_KEY_V);
      debugPrint("  -> Paste (");
      debugPrint(currentOS == OS_MAC ? "Cmd+V" : "Ctrl+V");
      debugPrintln(")");
      break;
      
    case 8:
      if ((millis() - lastDeletePressTime) < doubleTapDelay) {
        sendKeyCombo(HID_KEY_SHIFT_LEFT, HID_KEY_DELETE);
        debugPrintln("  -> PERMANENT DELETE (Shift+Delete)");
        lastDeletePressTime = 0;
      } else {
        sendKey(HID_KEY_DELETE);
        debugPrintln("  -> Delete (tap again for Shift+Delete)");
        lastDeletePressTime = millis();
      }
      break;
      
    case 9:
      if (currentOS == OS_MAC) {
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_SPACE);
        delay(200);
        sendString("cursor");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Cursor (Mac)");
      } else {
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_R);
        delay(200);
        sendString("cursor");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Cursor (Windows)");
      }
      break;
      
    case 10:
      sendKey(HID_KEY_F12);
      debugPrintln("  -> Inspect Element (F12)");
      break;
      
    case 11:
      if (currentOS == OS_MAC) {
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_SPACE);
        delay(200);
        sendString("terminal");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Terminal (Mac)");
      } else {
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_R);
        delay(200);
        sendString("cmd");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Terminal (Windows)");
      }
      break;
      
    case 12:
      if ((millis() - lastOSSwitchPressTime) < doubleTapDelay) {
        toggleOSMode();
        
        debugPrintln();
        debugPrintln("========================================");
        debugPrint("  OS SWITCHED TO: ");
        debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
        debugPrintln("========================================");
        
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 20);
        display.println("OS MODE:");
        display.setTextSize(3);
        display.setCursor(10, 40);
        display.println(currentOS == OS_MAC ? " MAC" : " WIN");
        display.display();
        
        for (int i = 0; i < NUM_LEDS; i++) {
          setAllLEDs(CRGB::Blue);
          FastLED.show();
          delay(100);
          setAllLEDs(CRGB::Black);
          FastLED.show();
          delay(50);
        }
        
        delay(2000);
        updateDisplay();
        lastOSSwitchPressTime = 0;
      } else {
        sendString("git pull");
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Git Pull");
        debugPrintln("     Note: Terminal must be open in git repo directory");
        debugPrintln("     Tip: Double-tap Key 12 to switch OS mode");
        
        lastOSSwitchPressTime = millis();
      }
      break;
      
    default:
      debugPrintln("  -> Macro: Not assigned");
      break;
  }
}
