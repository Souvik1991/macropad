/*
 * Key Matrix Module
 * Handles 3x4 key matrix scanning and macro execution
 */

// Forward declarations
extern MacroConfig storedMacros[NUM_MACROS];
extern Adafruit_USBD_HID usb_hid;

// ============================================
// STATE VARIABLES
// ============================================

bool keyStates[MATRIX_ROWS][MATRIX_COLS] = {0};
bool lastKeyStates[MATRIX_ROWS][MATRIX_COLS] = {0};

// Double-tap detection for Delete key (Key 8)
unsigned long lastDeletePressTime = 0;
const unsigned long doubleTapDelay = 500;  // 500ms window for double-tap

// Double-tap detection for OS switch (Key 12)
unsigned long lastOSSwitchPressTime = 0;

// ============================================
// INITIALIZATION
// ============================================

void initKeyMatrix() {
  debugPrint("Initializing key matrix... ");
  
  // Configure row pins as outputs (HIGH = inactive)
  for (int i = 0; i < MATRIX_ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  
  // Configure column pins as inputs with pull-ups
  for (int i = 0; i < MATRIX_COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  
  debugPrintln("OK");
}

// ============================================
// KEY SCANNING
// ============================================

void scanKeyMatrix() {
  for (int row = 0; row < MATRIX_ROWS; row++) {
    // Activate current row (set LOW)
    digitalWrite(rowPins[row], LOW);
    delayMicroseconds(10);  // Signal settle time
    
    // Read all columns
    for (int col = 0; col < MATRIX_COLS; col++) {
      keyStates[row][col] = !digitalRead(colPins[col]);  // Invert (LOW = pressed)
      
      // Detect key press (rising edge)
      if (keyStates[row][col] && !lastKeyStates[row][col]) {
        handleKeyPress(row, col);
      }
      
      // Detect key release (falling edge)
      if (!keyStates[row][col] && lastKeyStates[row][col]) {
        handleKeyRelease(row, col);
      }
      
      lastKeyStates[row][col] = keyStates[row][col];
    }
    
    // Deactivate current row (set HIGH)
    digitalWrite(rowPins[row], HIGH);
  }
}

// ============================================
// KEY EVENT HANDLERS
// ============================================

void handleKeyPress(int row, int col) {
  int keyNum = (row * MATRIX_COLS) + col + 1;
  
  debugPrint("Key pressed: ");
  debugPrint(keyNum);
  debugPrint(" (R");
  debugPrint(row + 1);
  debugPrint("C");
  debugPrint(col + 1);
  debugPrintln(")");
  
  // Execute macro
  executeMacro(keyNum);
  
  // Visual feedback
  flashLED(col % NUM_LEDS);
}

void handleKeyRelease(int row, int col) {
  // Optional: handle key release events
  // Currently not used, but available for future features
}

// ============================================
// MACRO EXECUTION
// ============================================

void executeMacro(int keyNum) {
  // Try to execute macro from EEPROM first
  if (keyNum >= 1 && keyNum <= NUM_MACROS) {
    MacroConfig &macro = storedMacros[keyNum - 1];
    
    // If macro is disabled or invalid, fall back to hardcoded
    if (macro.type == MACRO_TYPE_DISABLED) {
      debugPrintln("  -> Macro disabled, using hardcoded fallback");
      executeHardcodedMacro(keyNum);
      return;
    }
    
    // Execute stored macro
    if (macro.type == MACRO_TYPE_KEYCOMBO) {
      uint8_t modifier = macro.modifier;
      
      // If modifier is 0, use OS-aware modifier
      if (modifier == 0) {
        modifier = (currentOS == OS_MAC) ? HID_KEY_GUI_LEFT : HID_KEY_CONTROL_LEFT;
      } else {
        // Convert bitmask to HID modifiers
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
      // Execute string macro
      char str[6] = {0};
      for (int i = 0; i < 5 && macro.data[i] != 0; i++) {
        str[i] = macro.data[i];
      }
      sendString(str);
      debugPrint("  -> Executed string macro: ");
      debugPrintln(str);
      return;
      
    } else if (macro.type == MACRO_TYPE_SPECIAL) {
      // Special functions - map to hardcoded equivalents
      executeHardcodedMacro(keyNum);
      return;
      
    } else if (macro.type == MACRO_TYPE_SEQUENCE) {
      // Execute sequence macro
      executeSequenceMacro(keyNum);
      debugPrint("  -> Executed sequence macro: Key ");
      debugPrintln(keyNum);
      return;
    }
  }
  
  // Fallback to hardcoded macro
  executeHardcodedMacro(keyNum);
}

void executeSequenceMacro(int keyNum) {
  // Calculate sequence address based on current OS
  // OS offset: 0 for Windows, 1 for Mac
  int osOffset = (currentOS == OS_MAC) ? 1 : 0;
  int seqAddr = EEPROM_ADDR_SEQUENCES + ((keyNum - 1) * 2 + osOffset) * MAX_SEQUENCE_LENGTH;

  // Read sequence string from EEPROM (null-terminated)
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
    return; // No sequence defined, will fall back to hardcoded
  }
  
  // Parse and execute sequence
  // Format: K:modifier:keycode|T:text|D:delay|R:release
  String seq = String(sequenceStr);
  int startIdx = 0;
  
  while (startIdx < seq.length()) {
    int pipeIdx = seq.indexOf('|', startIdx);
    String step = (pipeIdx > 0) ? seq.substring(startIdx, pipeIdx) : seq.substring(startIdx);
    
    if (step.length() == 0) break;
    
    // Parse step type
    if (step.startsWith("K:")) {
      // Key press: K:modifier:keycode1,keycode2,...
      int colon1 = 2; // First colon after 'K'
      int colon2 = step.indexOf(':', colon1 + 1);

      if (colon2 > colon1) {
        uint8_t modifier = step.substring(colon1 + 1, colon2).toInt();
        String keycodesStr = step.substring(colon2 + 1);

        // Convert modifier bitmask to HID modifiers
        uint8_t hidMod = 0;
        if (modifier & 1) hidMod |= HID_KEY_CONTROL_LEFT;
        if (modifier & 2) hidMod |= HID_KEY_SHIFT_LEFT;
        if (modifier & 4) hidMod |= HID_KEY_ALT_LEFT;
        if (modifier & 8) hidMod |= HID_KEY_GUI_LEFT;

        // If modifier is 0, use OS-aware modifier
        if (modifier == 0) {
          hidMod = (currentOS == OS_MAC) ? HID_KEY_GUI_LEFT : HID_KEY_CONTROL_LEFT;
        }

        // Handle multiple keys separated by commas
        uint8_t keycodes[6] = {0}; // HID keyboard report supports up to 6 simultaneous keys
        int keyCount = 0;

        int startIdx = 0;
        while (startIdx < keycodesStr.length() && keyCount < 6) {
          int commaIdx = keycodesStr.indexOf(',', startIdx);
          String keycodeStr = (commaIdx > 0) ? keycodesStr.substring(startIdx, commaIdx) : keycodesStr.substring(startIdx);
          uint8_t keycode = keycodeStr.toInt();

          if (keycode > 0) {
            keycodes[keyCount++] = keycode;
          }

          startIdx = (commaIdx > 0) ? commaIdx + 1 : keycodesStr.length();
        }

        // Send the keyboard report with modifiers and multiple keys
        usb_hid.keyboardReport(hidMod, keycodes[0], keycodes[1], keycodes[2], keycodes[3], keycodes[4], keycodes[5]);
        delay(10);
        usb_hid.keyboardRelease(0); // Release all keys
        delay(10);
      }
      
    } else if (step.startsWith("T:")) {
      // Text: T:text
      String text = step.substring(2);
      sendString(text.c_str());
      
    } else if (step.startsWith("D:")) {
      // Delay: D:milliseconds
      int delayMs = step.substring(2).toInt();
      delayMs = constrain(delayMs, 0, 5000); // Limit to 5 seconds
      delay(delayMs);
      
    } else if (step == "R") {
      // Release all keys
      usb_hid.keyboardRelease(0);
      delay(10);
    }
    
    startIdx = (pipeIdx > 0) ? pipeIdx + 1 : seq.length();
  }
}

void executeHardcodedMacro(int keyNum) {
  // Get modifier key based on OS (Cmd for Mac, Ctrl for Windows)
  uint8_t modKey = (currentOS == OS_MAC) ? HID_KEY_GUI_LEFT : HID_KEY_CONTROL_LEFT;
  
  switch(keyNum) {
    // ========== ROW 1 ==========
    
    case 1:  // Screenshot
      if (currentOS == OS_MAC) {
        // Mac: Cmd + Shift + 4 (area screenshot)
        sendKeyCombo(HID_KEY_GUI_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_4);
        debugPrintln("  -> Screenshot (Mac: Cmd+Shift+4)");
      } else {
        // Windows: Win + Shift + S (Snipping tool)
        sendKeyCombo(HID_KEY_GUI_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_S);
        debugPrintln("  -> Screenshot (Win: Win+Shift+S)");
      }
      break;
      
    case 2:  // Mic Mute/Unmute
      // Uses global hotkeys - must be configured in each app
      // Common: Ctrl+D (Zoom), Ctrl+M (GMeet), Ctrl+Shift+M (Webex)
      // We'll use Ctrl+Shift+M as most universal
      sendKeyCombo(HID_KEY_CONTROL_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_M);
      debugPrintln("  -> Mic Toggle (Ctrl+Shift+M)");
      debugPrintln("     Note: Configure this shortcut in Zoom/Webex/GMeet");
      break;
      
    case 3:  // Video On/Off
      // Common: Ctrl+Shift+V (Zoom/GMeet), Ctrl+Shift+V (Webex)
      sendKeyCombo(HID_KEY_CONTROL_LEFT | HID_KEY_SHIFT_LEFT, HID_KEY_V);
      debugPrintln("  -> Video Toggle (Ctrl+Shift+V)");
      debugPrintln("     Note: Configure this shortcut in Zoom/Webex/GMeet");
      break;
      
    case 4:  // Lock Screen
      if (currentOS == OS_MAC) {
        // Mac: Cmd + Ctrl + Q
        sendKeyCombo(HID_KEY_GUI_LEFT | HID_KEY_CONTROL_LEFT, HID_KEY_Q);
        debugPrintln("  -> Lock Screen (Mac: Cmd+Ctrl+Q)");
      } else {
        // Windows: Win + L
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_L);
        debugPrintln("  -> Lock Screen (Win: Win+L)");
      }
      break;
      
    // ========== ROW 2 ==========
      
    case 5:  // Copy
      sendKeyCombo(modKey, HID_KEY_C);
      debugPrint("  -> Copy (");
      debugPrint(currentOS == OS_MAC ? "Cmd+C" : "Ctrl+C");
      debugPrintln(")");
      break;
      
    case 6:  // Cut
      sendKeyCombo(modKey, HID_KEY_X);
      debugPrint("  -> Cut (");
      debugPrint(currentOS == OS_MAC ? "Cmd+X" : "Ctrl+X");
      debugPrintln(")");
      break;
      
    case 7:  // Paste
      sendKeyCombo(modKey, HID_KEY_V);
      debugPrint("  -> Paste (");
      debugPrint(currentOS == OS_MAC ? "Cmd+V" : "Ctrl+V");
      debugPrintln(")");
      break;
      
    case 8:  // Delete (double-tap for Shift+Delete)
      if ((millis() - lastDeletePressTime) < doubleTapDelay) {
        // Double-tap detected - Shift+Delete (permanent delete)
        sendKeyCombo(HID_KEY_SHIFT_LEFT, HID_KEY_DELETE);
        debugPrintln("  -> PERMANENT DELETE (Shift+Delete)");
        lastDeletePressTime = 0;  // Reset
      } else {
        // Single tap - normal Delete
        sendKey(HID_KEY_DELETE);
        debugPrintln("  -> Delete (tap again for Shift+Delete)");
        lastDeletePressTime = millis();
      }
      break;
      
    // ========== ROW 3 ==========
      
    case 9:  // Open Cursor
      if (currentOS == OS_MAC) {
        // Mac: Open via Spotlight
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_SPACE);
        delay(200);
        sendString("cursor");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Cursor (Mac)");
      } else {
        // Windows: Win+R, then type cursor
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_R);
        delay(200);
        sendString("cursor");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Cursor (Windows)");
      }
      break;
      
    case 10:  // Inspect Element
      // F12 works on all browsers (Chrome, Firefox, Edge, Safari)
      sendKey(HID_KEY_F12);
      debugPrintln("  -> Inspect Element (F12)");
      break;
      
    case 11:  // Open Terminal
      if (currentOS == OS_MAC) {
        // Mac: Cmd+Space, type "terminal"
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_SPACE);
        delay(200);
        sendString("terminal");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Terminal (Mac)");
      } else {
        // Windows: Win+R, type "cmd"
        sendKeyCombo(HID_KEY_GUI_LEFT, HID_KEY_R);
        delay(200);
        sendString("cmd");
        delay(100);
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Opening Terminal (Windows)");
      }
      break;
      
    case 12:  // OS Switch (double-tap) / Git Pull (single tap)
      if ((millis() - lastOSSwitchPressTime) < doubleTapDelay) {
        // ========== DOUBLE-TAP: SWITCH OS MODE ==========
        toggleOSMode();
        
        debugPrintln();
        debugPrintln("========================================");
        debugPrint("  OS SWITCHED TO: ");
        debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
        debugPrintln("========================================");
        
        // Visual feedback on OLED
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 20);
        display.println("OS MODE:");
        display.setTextSize(3);
        display.setCursor(10, 40);
        display.println(currentOS == OS_MAC ? " MAC" : " WIN");
        display.display();
        
        // LED feedback: Cycle through all LEDs
        for (int i = 0; i < NUM_LEDS; i++) {
          setAllLEDs(CRGB::Blue);
          FastLED.show();
          delay(100);
          setAllLEDs(CRGB::Black);
          FastLED.show();
          delay(50);
        }
        
        // Show confirmation for 2 seconds
        delay(2000);
        
        // Return to idle screen
        updateDisplay();
        
        lastOSSwitchPressTime = 0;  // Reset double-tap timer
      } else {
        // ========== SINGLE TAP: GIT PULL ==========
        // This requires terminal to be open and in the right directory
        sendString("git pull");
        sendKey(HID_KEY_ENTER);
        debugPrintln("  -> Git Pull");
        debugPrintln("     Note: Terminal must be open in git repo directory");
        debugPrintln("     Tip: Double-tap Key 12 to switch OS mode");
        
        lastOSSwitchPressTime = millis();  // Start double-tap timer
      }
      break;
      
    default:
      debugPrintln("  -> Macro: Not assigned");
      break;
  }
}

