/*
 * ============================================
 *     ESP32-S3 FIDO2 Fingerprint Macropad
 *     MAIN FILE - Modular Version
 * ============================================
 * 
 * This is the main entry point. Component logic is split into:
 * - config.h          - Pin definitions and constants
 * - display.ino       - OLED display management
 * - leds.ino          - WS2812B LED control
 * - keymatrix.ino     - Key scanning and macro execution
 * - encoders.ino      - Rotary encoder handling
 * - fingerprint.ino   - Fingerprint sensor management
 * - secure.ino        - ATECC608A secure element
 * - usb_hid.ino       - USB HID keyboard functionality
 * - fido2.ino         - FIDO2/CTAP2 protocol (future)
 * 
 * Author: [Your Name]
 * Version: 2.0.0 (Modular)
 * Date: 2024
 */

#include "config.h"

// ============================================
// GLOBAL STATE
// ============================================

SystemMode currentMode = MODE_IDLE;
OperatingSystem currentOS = OS_WINDOWS;  // Change to OS_MAC if using Mac
int volumeLevel = 50;

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
  
  debugPrintln("✓ All systems initialized!");
  debugPrintln("========================================");
  
  // Show ready screen
  currentMode = MODE_IDLE;
  updateDisplay();
  setLEDPattern(PATTERN_IDLE);
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
  // Process serial commands (for configuration)
  processSerialCommands();
  
  // Update all components
  scanKeyMatrix();
  updateEncoders();
  checkFIDO2Requests();
  updateDisplay();
  updateLEDs();
  
  delay(10);  // 100Hz update rate
}

