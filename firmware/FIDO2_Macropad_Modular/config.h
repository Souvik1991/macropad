/*
 * Configuration Header
 * Contains pin definitions, constants, and global includes
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// LIBRARY INCLUDES
// ============================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <FastLED.h>
#include <SparkFun_ATECCX08a_Arduino_Library.h>
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>

// ============================================
// DISPLAY CONFIGURATION
// ============================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3c

// ============================================
// LED CONFIGURATION
// ============================================

#define LED_PIN 17    // Changed from GPIO33 (internal NeoPixel conflict)
#define NUM_LEDS 5
#define LED_BRIGHTNESS 50

// LED Pattern IDs
#define PATTERN_IDLE 0
#define PATTERN_ACTIVE 1
#define PATTERN_ERROR 2

// ============================================
// ENCODER CONFIGURATION (Single Encoder)
// ============================================

// Rotary Encoder (Volume Control + Mute)
#define ENCODER_CLK 10
#define ENCODER_DT  11
#define ENCODER_SW  12

#define ENCODER_STEP_SIZE 5  // Volume change per encoder tick

// ============================================
// KEY MATRIX CONFIGURATION
// ============================================

#define MATRIX_ROWS 3
#define MATRIX_COLS 4
#define MATRIX_SCAN_DELAY 10  // Debounce delay in ms

// Row pins (outputs) - Using only available GPIOs (not GPIO7/21/33)
const int rowPins[MATRIX_ROWS] = {5, 6, 9};

// Column pins (inputs with pull-ups) - Avoiding GPIO21 (internal NeoPixel)
const int colPins[MATRIX_COLS] = {13, 14, 15, 16};

// ============================================
// FINGERPRINT SENSOR CONFIGURATION
// ============================================

#define FINGERPRINT_RX 38  // ESP32 UART1 RX (U1RXD)
#define FINGERPRINT_TX 39  // ESP32 UART1 TX (U1TXD)
#define FINGERPRINT_RST 18 // Sleep/wake control (output): LOW=sleep, HIGH=active
#define FINGERPRINT_WAKE 8 // Finger detect (input, interrupt)
#define FINGERPRINT_BAUD 19200
#define USER_MAX_CNT 500   // Max fingerprint templates (Waveshare sensor)

// ============================================
// EEPROM CONFIGURATION
// ============================================

#define EEPROM_SIZE 8192
#define EEPROM_ADDR_OS_MODE 0      // OS mode (1 byte)
#define EEPROM_ADDR_MAGIC 1        // Magic byte (1 byte)
#define EEPROM_ADDR_LED_BRIGHTNESS 2  // LED brightness (1 byte)
#define EEPROM_ADDR_LED_COLORS 3   // LED colors start (5 LEDs × 3 bytes = 15 bytes)
#define EEPROM_ADDR_MACROS 20      // Macros start (12 keys × 8 bytes = 96 bytes)
#define EEPROM_ADDR_SEQUENCES 116  // Sequences start (12 keys × 2 OS × 200 bytes = 4800 bytes, 116-4915)
#define EEPROM_ADDR_KEY_NAMES 4916 // Key names start (12 keys × 13 bytes = 156 bytes, 4916-5071)
#define EEPROM_MAGIC 0xAB          // Magic byte to verify EEPROM is initialized
#define MAX_SEQUENCE_LENGTH 200    // Max sequence string length
#define MAX_KEY_NAME_LENGTH 12     // Max key name length (12 chars + null terminator = 13 bytes)

// Macro structure: 8 bytes per macro
// Byte 0: Macro type (0=disabled, 1=keycombo, 2=string, 3=special)
// Byte 1: Modifier keys (bitmask: Ctrl=1, Shift=2, Alt=4, GUI=8)
// Byte 2: Key code (HID key code)
// Byte 3-7: String data or additional params (5 bytes)

#define MACRO_TYPE_DISABLED 0
#define MACRO_TYPE_KEYCOMBO 1
#define MACRO_TYPE_STRING 2
#define MACRO_TYPE_SPECIAL 3  // Special functions (OS switch, etc.)
#define MACRO_TYPE_SEQUENCE 4  // Complex key sequences

#define MACRO_SIZE 8
#define NUM_MACROS 12

// ============================================
// SYSTEM STATE
// ============================================

enum SystemMode {
  MODE_IDLE,
  MODE_MACRO,
  MODE_FIDO2_WAITING,
  MODE_FIDO2_VERIFYING,
  MODE_FIDO2_SUCCESS,
  MODE_FIDO2_ERROR,
  MODE_FP_ENROLL,        // Fingerprint enrollment in progress
  MODE_FP_ENROLL_OK,     // Enrollment succeeded
  MODE_FP_ENROLL_FAIL,   // Enrollment failed
  MODE_FP_DELETE,        // Delete in progress
  MODE_FP_DELETE_OK,     // Delete succeeded
  MODE_FP_DELETE_FAIL,   // Delete failed
  MODE_FP_FULL,          // Storage full - cannot enroll
  MODE_FP_DELETE_MENU,   // Interactive delete list
  MODE_FP_DELETE_CONFIRM,// Confirm delete dialog
  MODE_KEY_SLOTS_FULL,       // All 14 passkey slots used
  MODE_KEY_SLOT_REPLACE_MENU,    // Pick a slot to replace
  MODE_KEY_SLOT_REPLACE_CONFIRM, // Confirm slot replacement
  MODE_FIDO2_FP_RETRY,         // Fingerprint scan failed, retry prompt
  MODE_FIDO2_FP_TIMEOUT,       // No finger detected (timeout)
  MODE_FIDO2_FP_SENSOR_ERR,    // Sensor hardware/UART error
  MODE_FIDO2_FP_FAILED         // All 3 fingerprint attempts failed
};

enum OperatingSystem {
  OS_WINDOWS,
  OS_MAC
};

// ============================================
// DEBUG CONFIGURATION
// ============================================

#define DEBUG_ENABLED true  // Set to false to disable all debug output

// ============================================
// GLOBAL VARIABLES (declared here, defined in main .ino)
// ============================================

extern SystemMode currentMode;
extern OperatingSystem currentOS;
extern int volumeLevel;
extern unsigned long volumeOverlayUntil;  // millis() timestamp for volume overlay auto-hide
extern uint8_t fpEnrollStep;   // 0 = not enrolling, 1-3 = current scan step
extern int8_t fpMenuSelection;     // Highlighted item in delete menu (0=Back, 1..N=fingerprint IDs)
extern uint8_t fpMenuItemCount;    // Total items in delete menu
extern uint8_t fpDeleteTargetId;   // ID selected for deletion
extern bool fpConfirmYes;          // Confirm dialog selection (true=Yes)
extern int8_t keySlotMenuSelection;   // Selected slot in replace menu
extern uint8_t keySlotMenuItemCount;  // Total items in slot replace menu
extern uint8_t keySlotReplaceTarget;  // Slot selected for replacement
extern bool keySlotConfirmYes;        // Confirm dialog selection
extern uint8_t fpAuthAttempt;          // Current fingerprint auth attempt (1-3) for FIDO2

// ============================================
// GLOBAL OBJECTS (defined in respective .ino files)
// ============================================

extern Adafruit_SH1106G display;
extern CRGB leds[NUM_LEDS];
extern ATECCX08A atecc;
extern HardwareSerial fingerprintSerial;
extern Adafruit_USBD_HID usb_kbd;   // keyboard interface
extern Adafruit_USBD_HID usb_fido;  // FIDO2 interface

// ============================================
// MACRO STRUCTURE
// ============================================

struct MacroConfig {
  uint8_t type;        // Macro type
  uint8_t modifier;    // Modifier keys (bitmask)
  uint8_t keycode;     // HID key code
  uint8_t data[5];     // String data or additional params
};

extern MacroConfig storedMacros[NUM_MACROS];

#endif // CONFIG_H

