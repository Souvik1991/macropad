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
#include <Adafruit_SSD1306.h>
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
#define SCREEN_ADDRESS 0x3C

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
#define FINGERPRINT_BAUD 57600

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
  MODE_FIDO2_ERROR
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

// ============================================
// GLOBAL OBJECTS (defined in respective .ino files)
// ============================================

extern Adafruit_SSD1306 display;
extern CRGB leds[NUM_LEDS];
extern ATECCX08A atecc;
extern HardwareSerial fingerprintSerial;
extern Adafruit_USBD_HID usb_hid;

// ============================================
// MACRO STRUCTURE
// ============================================

struct MacroConfig {
  uint8_t type;        // Macro type
  uint8_t modifier;    // Modifier keys (bitmask)
  uint8_t keycode;     // HID key code
  uint8_t data[5];     // String data or additional params
};

#endif // CONFIG_H

