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
 * Author: Souvik Maity + Cursor AI
 * Version: 2.0.0 (Modular)
 * Date: 2026
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

#include <esp_bt.h>
#include <esp_wifi.h>

// ============================================
// GLOBAL STATE
// ============================================
SystemMode currentMode = MODE_IDLE;
OperatingSystem currentOS = OS_WINDOWS;  // Change to OS_MAC if using Mac
float volumeLevel = 50.0f;
unsigned long volumeOverlayUntil = 0;
uint8_t fpEnrollStep = 0;  // Fingerprint enrollment scan step (0=idle, 1-3=scanning)
uint8_t fpStoredCount = 0;  // Cached fingerprint count during enrollment (set at enroll start)
int8_t fpMenuSelection = 0;      // Highlighted item in delete menu
uint8_t fpMenuItemCount = 0;     // Total items in delete menu
uint8_t fpDeleteTargetId = 0;    // ID selected for deletion
bool fpConfirmYes = false;       // Confirm dialog selection
uint8_t fpAuthAttempt = 0;       // Current fingerprint auth attempt (1-3)
char fido2RpId[64] = "";         // Requester/relying party ID for FIDO2 display (e.g. github.com)
int8_t sysMenuSelection = 0;     // Highlighted item in system menu
uint8_t sysMenuItemCount = 4;    // Total items: Toggle OS, Enroll FP, Delete FP, Exit

// ============================================
// BOOT HEALTH CHECK
// ============================================
void runBootChecks() {
  // --- Step 1: Show health check diagnostics ---
  currentMode = MODE_BOOT_CHECK;
  updateDisplay();
  delay(2000);

  bool ateccOk = isDevicePresent();
  bool fpOk = isFingerprintSensorPresent();

  // --- Step 2: If critical failure, stay on fail screen ---
  if (!ateccOk || !fpOk) {
    debugPrintln("BOOT CHECK FAILED:");
    if (!ateccOk) debugPrintln("  - ATECC608A not found");
    if (!fpOk) debugPrintln("  - Fingerprint sensor not found");

    currentMode = MODE_BOOT_FAIL;
    updateDisplay();

    // Stay on fail screen until user presses encoder to acknowledge
    while (true) {
      if (digitalRead(ENCODER_SW) == LOW) {
        delay(300);  // debounce
        break;
      }
      delay(50);
    }
    // Continue anyway — the device can still serve as a basic macropad
    debugPrintln("Boot failure acknowledged. Continuing...");
  }

  // --- Step 3: ATECC provisioning gate (one-time setup) ---
  if (ateccOk && !isConfigLocked()) {
    debugPrintln("ATECC config unlocked — entering provisioning gate");
    currentMode = MODE_ATECC_SETUP_NEEDED;
    updateDisplay();

    bool provisioned = false;
    while (!provisioned) {
      if (digitalRead(ENCODER_SW) == LOW) {
        delay(300);  // debounce
        debugPrintln("User pressed encoder — provisioning ATECC...");

        displayMessage("Configuring...", "Please wait");
        bool cfgOk = configureDevice();
        bool lockOk = cfgOk && lockConfiguration();

        if (lockOk) {
          debugPrintln("ATECC provisioning complete");
          displayMessage("Done!", "Config locked");
          delay(1500);
          provisioned = true;
        } else {
          debugPrintln("ATECC provisioning failed");
          currentMode = MODE_ATECC_PROVISIONING_FAIL;
          updateDisplay();
          delay(500);  // debounce before retry
        }
      }
      delay(50);
    }
    debugPrintln("ATECC provisioning gate passed");
  }

  // --- Step 4: Fingerprint enrollment gate ---
  if (fpOk) {
    int fpCount = getFingerprintCount();
    if (fpCount <= 0) {
      debugPrintln("No fingerprints enrolled — entering enrollment gate");
      currentMode = MODE_FP_REQUIRED;
      updateDisplay();

      // Wait for encoder press to start enrollment, loop until ≥1 enrolled
      while (getFingerprintCount() <= 0) {
        if (digitalRead(ENCODER_SW) == LOW) {
          delay(300);  // debounce
          enrollFingerprint(0);  // Auto-assign ID
          // enrollFingerprint sets mode back to MODE_FP_REQUIRED on failure
          // or MODE_IDLE on success (via enrollReturnMode)
          if (currentMode == MODE_FP_REQUIRED) {
            updateDisplay();  // Re-show the enrollment prompt
          }
        }
        delay(50);
      }
      debugPrintln("Fingerprint enrollment gate passed");
    }
  }

  // --- Step 5: Check macro configuration ---
  if (!hasAnyMacroConfigured()) {
    debugPrintln("No macros configured — showing setup URL");
    currentMode = MODE_SETUP_NEEDED;
    updateDisplay();
    // Don't block here — fall through to loop() so serial commands can configure macros
    return;
  }

  // --- All checks passed ---
  currentMode = MODE_IDLE;
  updateDisplay();
}

// ============================================
// SETUP
// ============================================
void setup() {
  // ─── CRITICAL: HID interfaces must be registered BEFORE Serial.begin() ───
  // On ESP32-S3 with TinyUSB, Serial.begin() triggers USB stack enumeration.
  // Any HID .begin() calls after that point are silently ignored, resulting
  // in a device with only CDC serial and zero HID interfaces.
  initUSB();

  Serial.begin(115200);
  delay(2000);
  
  debugPrintln("========================================");
  debugPrintln("   ESP32-S3 FIDO2 Macropad Starting");
  debugPrintln("   Modular Version 2.0");
  debugPrintln("========================================");
  
  // Wait for USB mount now that Serial is active
  waitUSBReady();

  // Disable WiFi and Bluetooth (not needed for USB HID macropad; saves power and reduces attack surface)
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  debugPrintln("WiFi and Bluetooth disabled");

  // Initialize all components
  initSettings();      // Load saved settings first
  initDisplay();
  initLEDs();
  initEncoders();
  initKeyMatrix();
  initSecureElement();
  initFingerprint();
  initFIDO2();                // Load FIDO2 credentials from NVS
  
  debugPrintln("All components initialized");
  debugPrintln("========================================");
  
  // Run boot-time health checks and gates
  runBootChecks();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Process serial commands (for configuration)
  processSerialCommands();

  // If in setup-needed mode, check if a macro was configured via serial
  if (currentMode == MODE_SETUP_NEEDED) {
    if (hasAnyMacroConfigured()) {
      debugPrintln("Macro configured — switching to idle");
      currentMode = MODE_IDLE;
    }
  }
  
  // Update all components
  scanKeyMatrix();
  
  // Route encoder input: interactive menus take priority over volume control
  if (currentMode == MODE_SYSTEM_MENU) {
    updateSystemMenu();
  } else if (currentMode == MODE_SYSTEM_RESET_CONFIRM) {
    updateSystemResetConfirm();
  } else if (currentMode == MODE_FP_DELETE_MENU || currentMode == MODE_FP_DELETE_CONFIRM
      || currentMode == MODE_KEY_SLOT_REPLACE_MENU || currentMode == MODE_KEY_SLOT_REPLACE_CONFIRM) {
    updateFingerprintDeleteMenu();
  } else {
    updateEncoders();
  }
  
  checkFIDO2Requests();
  tickSequenceMacro();  // Non-blocking sequence macro continuation (millis-based delays)
  updateDisplay();
  updateLEDs();
  
  delay(10);  // 100Hz update rate
}
