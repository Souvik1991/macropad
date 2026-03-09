/*
 * Serial Command Parser - Implementation
 * Handles configuration commands via Serial/USB
 */

#include "config.h"
#include "commands.h"
#include "debug.h"
#include "display.h"
#include "leds.h"
#include "settings.h"
#include <Arduino.h>
#include <string.h>

void handleMacroCommand(String args);
void handleSequenceCommand(String args);
void handleLEDCommand(String args);
void handleBrightnessCommand(String args);
void handleOSCommand(String args);
void handleNameCommand(String args);
void handleListCommand();
void handleResetCommand();
void handleHelpCommand();

void processSerialCommands() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd.length() == 0) return;
    
    int spaceIdx = cmd.indexOf(' ');
    String command = (spaceIdx > 0) ? cmd.substring(0, spaceIdx) : cmd;
    String args = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1) : "";
    
    if (command == "MACRO") {
      handleMacroCommand(args);
    } else if (command == "SEQUENCE") {
      handleSequenceCommand(args);
    } else if (command == "LED") {
      handleLEDCommand(args);
    } else if (command == "BRIGHTNESS") {
      handleBrightnessCommand(args);
    } else if (command == "OS") {
      handleOSCommand(args);
    } else if (command == "NAME") {
      handleNameCommand(args);
    } else if (command == "LIST") {
      handleListCommand();
    } else if (command == "RESET") {
      handleResetCommand();
    } else if (command == "HELP") {
      handleHelpCommand();
    } else {
      debugPrintln("Unknown command. Type HELP for commands.");
    }
  }
}

void handleMacroCommand(String args) {
  int keyNum = args.substring(0, args.indexOf(' ')).toInt();
  args = args.substring(args.indexOf(' ') + 1);
  
  int type = args.substring(0, args.indexOf(' ')).toInt();
  args = args.substring(args.indexOf(' ') + 1);
  
  int modifier = args.substring(0, args.indexOf(' ')).toInt();
  args = args.substring(args.indexOf(' ') + 1);
  
  int keycode = args.substring(0, args.indexOf(' ')).toInt();
  String data = args.indexOf(' ') > 0 ? args.substring(args.indexOf(' ') + 1) : "";
  
  if (keyNum < 1 || keyNum > NUM_MACROS) {
    debugPrintln("ERROR: Key number must be 1-12");
    return;
  }
  
  MacroConfig macro;
  macro.type = type;
  macro.modifier = modifier;
  macro.keycode = keycode;
  memset(macro.data, 0, 5);
  
  if (data.length() > 0) {
    int len = (data.length() < 5) ? (int)data.length() : 5;
    for (int i = 0; i < len; i++) {
      macro.data[i] = data[i];
    }
  }
  
  saveMacro(keyNum, macro);
  loadMacros();
  
  debugPrint("OK: Macro saved for Key ");
  debugPrintln(keyNum);
}

void handleSequenceCommand(String args) {
  int firstSpace = args.indexOf(' ');
  if (firstSpace <= 0) {
    debugPrintln("ERROR: Format: SEQUENCE <key> <os> <steps>");
    return;
  }

  int keyNum = args.substring(0, firstSpace).toInt();
  String remaining = args.substring(firstSpace + 1);

  int secondSpace = remaining.indexOf(' ');
  if (secondSpace <= 0) {
    debugPrintln("ERROR: Format: SEQUENCE <key> <os> <steps>");
    return;
  }

  String osStr = remaining.substring(0, secondSpace);
  String sequenceStr = remaining.substring(secondSpace + 1);

  if (keyNum < 1 || keyNum > NUM_MACROS) {
    debugPrintln("ERROR: Key number must be 1-12");
    return;
  }

  int osOffset = 0;
  if (osStr.equalsIgnoreCase("mac")) {
    osOffset = 1;
  } else if (osStr.equalsIgnoreCase("win")) {
    osOffset = 0;
  } else {
    debugPrintln("ERROR: OS must be 'win' or 'mac'");
    return;
  }

  int seqAddr = EEPROM_ADDR_SEQUENCES + ((keyNum - 1) * 2 + osOffset) * MAX_SEQUENCE_LENGTH;
  int len = (sequenceStr.length() < (unsigned int)(MAX_SEQUENCE_LENGTH - 1)) ? (int)sequenceStr.length() : (MAX_SEQUENCE_LENGTH - 1);

  for (int i = 0; i < len; i++) {
    EEPROM.write(seqAddr + i, sequenceStr[i]);
  }
  EEPROM.write(seqAddr + len, 0);
  commitEEPROM("handleSequenceCommand");

  debugPrint("OK: Sequence saved for Key ");
  debugPrint(keyNum);
  debugPrint(" (");
  debugPrint(osStr.c_str());
  debugPrint(") - ");
  debugPrint(len);
  debugPrintln(" bytes)");
}

void handleLEDCommand(String args) {
  int index = args.substring(0, args.indexOf(' ')).toInt();
  args = args.substring(args.indexOf(' ') + 1);
  
  int r = args.substring(0, args.indexOf(' ')).toInt();
  args = args.substring(args.indexOf(' ') + 1);
  
  int g = args.substring(0, args.indexOf(' ')).toInt();
  args = args.substring(args.indexOf(' ') + 1);
  
  int b = args.toInt();
  
  if (index < 0 || index >= NUM_LEDS) {
    debugPrintln("ERROR: LED index must be 0-4");
    return;
  }
  
  CRGB color = CRGB(r, g, b);
  saveLEDColor(index, color);
  setLED(index, color);
  
  debugPrint("OK: LED ");
  debugPrint(index);
  debugPrint(" set to RGB(");
  debugPrint(r);
  debugPrint(", ");
  debugPrint(g);
  debugPrint(", ");
  debugPrint(b);
  debugPrintln(")");
}

void handleBrightnessCommand(String args) {
  int brightness = args.toInt();
  brightness = constrain(brightness, 0, 255);
  saveLEDBrightness(brightness);
  debugPrint("OK: Brightness set to ");
  debugPrintln(brightness);
}

void handleOSCommand(String args) {
  args.toLowerCase();
  
  if (args == "mac" || args == "macos") {
    currentOS = OS_MAC;
    saveOSMode(OS_MAC);
    debugPrintln("OK: OS mode set to macOS");
  } else if (args == "win" || args == "windows") {
    currentOS = OS_WINDOWS;
    saveOSMode(OS_WINDOWS);
    debugPrintln("OK: OS mode set to Windows");
  } else {
    debugPrintln("ERROR: OS must be 'mac' or 'win'");
  }
  
  updateDisplay();
}

void handleNameCommand(String args) {
  int spaceIdx = args.indexOf(' ');
  if (spaceIdx <= 0) {
    debugPrintln("ERROR: Format: NAME <key> <name>");
    return;
  }
  
  int keyNum = args.substring(0, spaceIdx).toInt();
  String name = args.substring(spaceIdx + 1);
  
  if (keyNum < 1 || keyNum > NUM_MACROS) {
    debugPrintln("ERROR: Key number must be 1-12");
    return;
  }
  
  if (name.length() > MAX_KEY_NAME_LENGTH) {
    debugPrint("ERROR: Name too long (max ");
    debugPrint(MAX_KEY_NAME_LENGTH);
    debugPrintln(" characters)");
    return;
  }
  
  saveKeyName(keyNum, name.c_str());
  
  debugPrint("OK: Key ");
  debugPrint(keyNum);
  debugPrint(" renamed to: ");
  debugPrintln(name.c_str());
}

void handleListCommand() {
  debugPrintln("\n=== MACRO CONFIGURATION ===");
  debugPrintln("Key | Name              | Type      | Modifier | Keycode | Data");
  debugPrintln("----|-------------------|-----------|----------|---------|-----");
  
  for (int i = 0; i < NUM_MACROS; i++) {
    MacroConfig &macro = storedMacros[i];
    char keyName[MAX_KEY_NAME_LENGTH + 1];
    loadKeyName(i + 1, keyName, sizeof(keyName));
    
    debugPrint(i + 1);
    debugPrint("   | ");
    
    debugPrint(keyName);
    int nameLen = strlen(keyName);
    for (int j = nameLen; j < 17; j++) {
      debugPrint(" ");
    }
    
    debugPrint("| ");
    
    switch(macro.type) {
      case MACRO_TYPE_DISABLED: debugPrint("DISABLED "); break;
      case MACRO_TYPE_KEYCOMBO: debugPrint("KEYCOMBO  "); break;
      case MACRO_TYPE_STRING: debugPrint("STRING    "); break;
      case MACRO_TYPE_SPECIAL: debugPrint("SPECIAL   "); break;
      case MACRO_TYPE_SEQUENCE: debugPrint("SEQUENCE  "); break;
      default: debugPrint("UNKNOWN   "); break;
    }
    
    debugPrint("| ");
    debugPrint(macro.modifier);
    debugPrint("       | ");
    debugPrint(macro.keycode);
    debugPrint("      | ");
    
    if (macro.type == MACRO_TYPE_STRING) {
      for (int j = 0; j < 5 && macro.data[j] != 0; j++) {
        debugPrint((char)macro.data[j]);
      }
    } else {
      debugPrint("-");
    }
    
    debugPrintln();
  }
  
  debugPrintln("\n=== SEQUENCES ===");
  for (int i = 0; i < NUM_MACROS; i++) {
    if (storedMacros[i].type == MACRO_TYPE_SEQUENCE) {
      char seqBuf[MAX_SEQUENCE_LENGTH + 1];
      loadSequence(i + 1, 0, seqBuf, sizeof(seqBuf));  // Windows (osOffset=0)
      if (seqBuf[0] != 0) {
        debugPrint(i + 1);
        debugPrint(":win:");
        debugPrintln(seqBuf);
      }
      loadSequence(i + 1, 1, seqBuf, sizeof(seqBuf));  // Mac (osOffset=1)
      if (seqBuf[0] != 0) {
        debugPrint(i + 1);
        debugPrint(":mac:");
        debugPrintln(seqBuf);
      }
    }
  }
  
  debugPrintln("\n=== LED COLORS ===");
  for (int i = 0; i < NUM_LEDS; i++) {
    debugPrint("LED ");
    debugPrint(i);
    debugPrint(": RGB(");
    debugPrint(leds[i].r);
    debugPrint(", ");
    debugPrint(leds[i].g);
    debugPrint(", ");
    debugPrint(leds[i].b);
    debugPrintln(")");
  }
  
  debugPrint("\nOS Mode: ");
  debugPrintln(currentOS == OS_MAC ? "macOS" : "Windows");
  
  debugPrint("LED Brightness: ");
  debugPrintln(FastLED.getBrightness());
}

void handleResetCommand() {
  debugPrintln("Resetting to defaults...");
  
  initDefaultMacros();
  loadMacros();
  initDefaultLEDColors();
  loadLEDColors();
  initDefaultKeyNames();
  saveOSMode(OS_WINDOWS);
  loadOSMode();
  saveLEDBrightness(LED_BRIGHTNESS);
  
  debugPrintln("OK: Reset complete");
  updateDisplay();
}

void handleHelpCommand() {
  debugPrintln("\n=== MACROPAD CONFIGURATION COMMANDS ===");
  debugPrintln();
  debugPrintln("MACRO <key> <type> <modifier> <keycode> [data]");
  debugPrintln("  Set macro for key (1-12)");
  debugPrintln("  Types: 0=disabled, 1=keycombo, 2=string, 3=special");
  debugPrintln("  Modifier bitmask: Ctrl=1, Shift=2, Alt=4, GUI=8");
  debugPrintln("  Example: MACRO 1 1 0 4");
  debugPrintln();
  debugPrintln("SEQUENCE <key> <os> <steps>");
  debugPrintln("  Set complex sequence macro for key (1-12) and OS (win/mac)");
  debugPrintln("  Steps: K:modifier:keycode (key press), T:text (type text), D:ms (delay), R (release)");
  debugPrintln("  Example: SEQUENCE 1 win K:8:18|D:200|T:calc|D:100|K:0:40");
  debugPrintln("           (Win+R, delay 200ms, type 'calc', delay 100ms, Enter)");
  debugPrintln();
  debugPrintln("NAME <key> <name>");
  debugPrintln("  Set custom name for key (1-12, max 12 characters)");
  debugPrintln("  Example: NAME 1 Calculator");
  debugPrintln();
  debugPrintln("LED <index> <r> <g> <b>");
  debugPrintln("  Set LED color (index 0-4, RGB 0-255)");
  debugPrintln("  Example: LED 0 255 0 0");
  debugPrintln();
  debugPrintln("BRIGHTNESS <value>");
  debugPrintln("  Set LED brightness (0-255)");
  debugPrintln("  Example: BRIGHTNESS 100");
  debugPrintln();
  debugPrintln("OS <mode>");
  debugPrintln("  Set OS mode (mac/win)");
  debugPrintln("  Example: OS mac");
  debugPrintln();
  debugPrintln("LIST");
  debugPrintln("  List all macros and settings");
  debugPrintln();
  debugPrintln("RESET");
  debugPrintln("  Reset all settings to defaults");
  debugPrintln();
  debugPrintln("HELP");
  debugPrintln("  Show this help message");
  debugPrintln();
}
