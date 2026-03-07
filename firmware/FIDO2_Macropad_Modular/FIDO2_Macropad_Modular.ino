/*
 * ============================================
 *     ESP32-S3 FIDO2 Fingerprint Macropad
 *     MAIN FILE - Modular Version
 * ============================================
 * 
 * This is the main entry point. Component logic is split into:
 * - config.h          - Pin definitions and constants
 * - display.h/cpp      - OLED display management
 * - leds.h/cpp         - WS2812B LED control
 * - keymatrix.h/cpp    - Key scanning and macro execution
 * - encoders.h/cpp     - Rotary encoder handling
 * - fingerprint.h/cpp  - Fingerprint sensor management
 * - secure.h/cpp       - ATECC608A secure element
 * - usb_hid.h/cpp     - USB HID keyboard functionality
 * - fido2.h/cpp       - FIDO2/CTAP2 protocol (future)
 * - settings.h/cpp    - EEPROM configuration storage
 * - commands.h/cpp    - Serial command parser
 * - debug.h/cpp       - Debug output utilities
 * 
 * Author: [Your Name]
 * Version: 2.0.0 (Modular)
 * Date: 2024
 */

#include "config.h"
#include "commands.h"
#include "cred_store.h"
#include "debug.h"
#include "display.h"
#include "encoders.h"
#include "fingerprint.h"
#include "fido2.h"
#include "keymatrix.h"
#include "leds.h"
#include "secure.h"
#include "settings.h"
#include "usb_hid.h"

// ============================================
// GLOBAL STATE
// ============================================

SystemMode currentMode = MODE_IDLE;
OperatingSystem currentOS = OS_WINDOWS;  // Change to OS_MAC if using Mac
int volumeLevel = 50;
unsigned long volumeOverlayUntil = 0;
uint8_t fpEnrollStep = 0;  // Fingerprint enrollment scan step (0=idle, 1-3=scanning)
int8_t fpMenuSelection = 0;      // Highlighted item in delete menu
uint8_t fpMenuItemCount = 0;     // Total items in delete menu
uint8_t fpDeleteTargetId = 0;    // ID selected for deletion
bool fpConfirmYes = false;       // Confirm dialog selection
uint8_t fpAuthAttempt = 0;       // Current fingerprint auth attempt (1-3)

// ============================================
// SETUP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  debugPrintln("========================================");
  debugPrintln("   ESP32-S3 FIDO2 Macropad Starting");
  debugPrintln("   Modular Version 2.0");
  debugPrintln("========================================");
  
  // Initialize all components
  initSettings();      // Load saved settings first
  initDisplay();
  initLEDs();
  initEncoders();
  initKeyMatrix();
  initSecureElement();
  initFingerprint();
  initUSB();
  initFIDO2();                // Load FIDO2 credentials from NVS
  
  debugPrintln("✓ All systems initialized!");
  debugPrintln("========================================");
  
  // Show ready screen
  currentMode = MODE_IDLE;
  updateDisplay();
  // LED colors are already loaded from EEPROM by initSettings() → loadLEDColors()
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
  // Process serial commands (for configuration)
  processSerialCommands();
  
  // Update all components
  scanKeyMatrix();
  
  // Route encoder input: interactive menus take priority over volume control
  if (currentMode == MODE_FP_DELETE_MENU || currentMode == MODE_FP_DELETE_CONFIRM
      || currentMode == MODE_KEY_SLOT_REPLACE_MENU || currentMode == MODE_KEY_SLOT_REPLACE_CONFIRM) {
    updateFingerprintDeleteMenu();
  } else {
    updateEncoders();
  }
  
  checkFIDO2Requests();
  updateDisplay();
  updateLEDs();
  
  delay(10);  // 100Hz update rate
}

