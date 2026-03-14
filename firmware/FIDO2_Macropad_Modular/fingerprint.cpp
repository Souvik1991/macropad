/*
 * Fingerprint Module - Implementation
 * Handles Waveshare fingerprint sensor communication
 * Protocol ported from tested code in tests/06_Fingerprint_Test/finger.cpp
 *
 * UART1: RX=GPIO38, TX=GPIO39 @ 19200 baud
 * Sensor: Waveshare UART Fingerprint Sensor (D) - max 150 fingerprints
 */

#include <string.h>
#include "config.h"
#include "debug.h"
#include "display.h"
#include "encoders.h"
#include "fingerprint.h"

// =====================================================
// Internal State
// =====================================================

HardwareSerial fingerprintSerial(1);

static uint8_t fp_RxBuf[9];
static uint8_t fp_TxBuf[9];
static int fp_matchedUserId = 0;
static bool fpSensorPresent = false;

// Delete menu internal state
static int lastMenuEncoderPos = 0;

// Throttle: minimum ms between wake/sleep cycles for getFingerprintCount
#define FP_COUNT_CACHE_MS  2000
static int fpCountCache = -1;
static unsigned long fpCountCacheTime = 0;

bool isFingerprintSensorPresent() {
  return fpSensorPresent;
}

/**
 * Helper: determine the correct mode to return to after enrollment
 * completes or fails. If no fingerprints are enrolled, return to
 * MODE_FP_REQUIRED (boot gate); otherwise MODE_IDLE.
 */
static SystemMode enrollReturnMode() {
  // Quick check without wake/sleep (caller already has sensor awake or just finished)
  return (getFingerprintCount() <= 0) ? MODE_FP_REQUIRED : MODE_IDLE;
}

// =====================================================
// Sleep / Wake (RST pin control)
// =====================================================

/** Wake the fingerprint sensor. RST HIGH, wait 200ms. Call before any UART command. */
static void fpWake() {
  digitalWrite(FINGERPRINT_RST, HIGH);
  delay(300);
}

/** Put the fingerprint sensor to sleep. RST LOW. Call after UART operations complete. */
static void fpSleep() {
  digitalWrite(FINGERPRINT_RST, LOW);
}

// =====================================================
// Low-level UART Helpers (ported from finger.cpp)
// =====================================================

static void fpTxByte(uint8_t val) {
  fingerprintSerial.write(val);
}

/**
 * Send a framed command and wait for response.
 * @param sendCount  Number of payload bytes to send (from fp_TxBuf)
 * @param recvCount  Expected total response frame length
 * @param timeoutMs  Timeout in milliseconds
 * @return ACK_SUCCESS or error code
 */
static uint8_t fpTxAndRxCmd(uint8_t sendCount, uint8_t recvCount, uint16_t timeoutMs) {
  uint8_t i, j, checkSum;
  uint16_t rxCount = 0;
  unsigned long tStart, tNow;
  uint8_t overflowFlag = 0;

  // Send: HEAD + payload + checksum + TAIL
  fpTxByte(CMD_HEAD);
  checkSum = 0;
  for (i = 0; i < sendCount; i++) {
    fpTxByte(fp_TxBuf[i]);
    checkSum ^= fp_TxBuf[i];
  }
  fpTxByte(checkSum);
  fpTxByte(CMD_TAIL);

  memset(fp_RxBuf, 0, sizeof(fp_RxBuf));
  fingerprintSerial.flush();

  // Receive with timeout
  tStart = millis();
  do {
    overflowFlag = 0;
    if (fingerprintSerial.available()) {
      fp_RxBuf[rxCount++] = fingerprintSerial.read();
    }
    tNow = millis();
    if (tStart > tNow) {  // millis() overflow guard
      tStart = millis();
      overflowFlag = 1;
    }
  } while (((rxCount < recvCount) && (tNow - tStart < timeoutMs)) || (overflowFlag == 1));

  fp_matchedUserId = fp_RxBuf[3];

  if (rxCount != recvCount) return ACK_TIMEOUT;
  if (fp_RxBuf[0] != CMD_HEAD) return ACK_FAIL;
  if (fp_RxBuf[recvCount - 1] != CMD_TAIL) return ACK_FAIL;
  if (fp_RxBuf[1] != fp_TxBuf[0]) return ACK_FAIL;

  checkSum = 0;
  for (j = 1; j < rxCount - 1; j++) checkSum ^= fp_RxBuf[j];
  if (checkSum != 0) return ACK_FAIL;

  return ACK_SUCCESS;
}

// =====================================================
// Sensor Query Functions
// =====================================================

/** Get fingerprint count (internal, assumes sensor is awake). */
static int getFingerprintCountInternal() {
  uint8_t m;

  fp_TxBuf[0] = CMD_USER_CNT;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = 0;
  fp_TxBuf[3] = 0;
  fp_TxBuf[4] = 0;

  m = fpTxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && fp_RxBuf[4] == ACK_SUCCESS) {
    return (int)fp_RxBuf[3];
  }
  return -1;  // Error
}

void invalidateFingerprintCountCache() {
  fpCountCache = -1;
  fpCountCacheTime = 0;
}

int getFingerprintCount() {
  unsigned long now = millis();
  if (fpCountCache >= 0 && (now - fpCountCacheTime) < FP_COUNT_CACHE_MS) {
    return fpCountCache;
  }
  fpWake();
  int count = getFingerprintCountInternal();
  fpSleep();
  fpCountCache = count;
  fpCountCacheTime = now;
  return count;
}

uint8_t fpGetCompareLevel() {
  uint8_t m;

  fp_TxBuf[0] = CMD_COM_LEV;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = 0;
  fp_TxBuf[3] = 1;   // Read mode
  fp_TxBuf[4] = 0;

  m = fpTxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && fp_RxBuf[4] == ACK_SUCCESS) {
    return fp_RxBuf[3];
  }
  return 0xFF;
}

uint8_t fpSetCompareLevel(uint8_t level) {
  uint8_t m;

  fp_TxBuf[0] = CMD_COM_LEV;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = level;
  fp_TxBuf[3] = 0;   // Write mode
  fp_TxBuf[4] = 0;

  m = fpTxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && fp_RxBuf[4] == ACK_SUCCESS) {
    return fp_RxBuf[3];
  }
  return 0xFF;
}

static uint8_t isMasterUser(uint8_t userId) {
  return (userId == 1 || userId == 2 || userId == 3) ? 1 : 0;
}

// =====================================================
// Initialization
// =====================================================

void initFingerprint() {
  debugPrint("Initializing fingerprint sensor... ");

  // RST pin: keep sensor sleeping until we need it
  pinMode(FINGERPRINT_RST, OUTPUT);
  digitalWrite(FINGERPRINT_RST, LOW);

  fingerprintSerial.begin(FINGERPRINT_BAUD, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  delay(100);

  fpWake();

  // Verify communication by setting compare level (same as test code)
  int retries = 0;
  while (fpSetCompareLevel(5) != 5) {
    retries++;
    debugPrint("  Fingerprint sensor retry ");
    debugPrintln(retries);
    delay(500);
    if (retries >= 10) {
      fpSensorPresent = false;
      debugPrintln("FAILED - sensor not responding");
      displayMessage("FP Sensor", "NOT FOUND!");
      fpSleep();
      delay(2000);
      return;
    }
  }

  fpSensorPresent = true;

  int count = getFingerprintCountInternal();
  debugPrintln("OK");
  debugPrint("  Enrolled fingerprints: ");
  debugPrintln(count >= 0 ? count : 0);
  debugPrint("  Max capacity: ");
  debugPrintln(USER_MAX_CNT);

  fpSleep();
}

// =====================================================
// Verify Fingerprint
// =====================================================

FpResult verifyFingerprint() {
  debugPrintln("Verifying fingerprint...");

  fpWake();

  fp_TxBuf[0] = CMD_MATCH;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = 0;
  fp_TxBuf[3] = 0;
  fp_TxBuf[4] = 0;

  uint8_t m = fpTxAndRxCmd(5, 8, 5000);

  FpResult result;
  if ((m == ACK_SUCCESS) && (isMasterUser(fp_RxBuf[4])) && fp_RxBuf[3] != 0) {
    debugPrint("  Matched user ID: ");
    debugPrintln(fp_matchedUserId);
    result = FP_MATCH;
  } else if (fp_RxBuf[4] == ACK_NO_USER) {
    debugPrintln("  No match found");
    result = FP_NO_MATCH;
  } else if (fp_RxBuf[4] == ACK_TIMEOUT) {
    debugPrintln("  Timeout - no finger detected");
    result = FP_TIMEOUT;
  } else {
    debugPrintln("  Sensor error or unexpected response");
    result = FP_SENSOR_ERROR;
  }

  fpSleep();
  return result;
}

// =====================================================
// Enroll Fingerprint (3-scan process with OLED feedback)
// =====================================================

bool enrollFingerprint(uint8_t id) {
  uint8_t m;

  debugPrint("Enrolling fingerprint ID: ");
  debugPrintln(id);

  fpWake();

  // Check capacity and show count
  int count = getFingerprintCountInternal();
  if (count < 0) count = 0;

  if (count >= USER_MAX_CNT) {
    debugPrintln("  Storage full!");
    fpSleep();
    invalidateFingerprintCountCache();
    currentMode = MODE_FP_FULL;
    updateDisplay();
    delay(3000);
    currentMode = enrollReturnMode();
    return false;
  }

  // If id is 0, auto-assign next available ID
  uint8_t newID = (id == 0) ? (uint8_t)(count + 1) : id;

  fpStoredCount = (uint8_t)count;  // Cache for display (can't call getFingerprintCount during draw)

  // --- Scan 1 of 3 ---
  fpEnrollStep = 1;
  currentMode = MODE_FP_ENROLL;
  updateDisplay();

  debugPrintln("  [Scan 1/3] Waiting for finger...");

  fp_TxBuf[0] = CMD_ADD_1;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = newID;
  fp_TxBuf[3] = 3;   // Master user privilege
  fp_TxBuf[4] = 0;

  m = fpTxAndRxCmd(5, 8, 10000);
  if (m != ACK_SUCCESS || fp_RxBuf[4] != ACK_SUCCESS) {
    debugPrintln("  Scan 1 failed!");
    fpSleep();
    invalidateFingerprintCountCache();
    fpEnrollStep = 0;
    currentMode = MODE_FP_ENROLL_FAIL;
    updateDisplay();
    delay(2000);
    currentMode = enrollReturnMode();
    return false;
  }
  debugPrintln("  Scan 1 OK");
  delay(1500);  // Give user time to lift finger

  // --- Scan 2 of 3 ---
  fpEnrollStep = 2;
  currentMode = MODE_FP_ENROLL;
  updateDisplay();

  debugPrintln("  [Scan 2/3] Place same finger again...");

  fp_TxBuf[0] = CMD_ADD_2;

  m = fpTxAndRxCmd(5, 8, 10000);
  if (m != ACK_SUCCESS || fp_RxBuf[4] != ACK_SUCCESS) {
    debugPrintln("  Scan 2 failed!");
    fpSleep();
    invalidateFingerprintCountCache();
    fpEnrollStep = 0;
    currentMode = MODE_FP_ENROLL_FAIL;
    updateDisplay();
    delay(2000);
    currentMode = enrollReturnMode();
    return false;
  }
  debugPrintln("  Scan 2 OK");
  delay(1500);

  // --- Scan 3 of 3 ---
  fpEnrollStep = 3;
  currentMode = MODE_FP_ENROLL;
  updateDisplay();

  debugPrintln("  [Scan 3/3] Place same finger one last time...");

  fp_TxBuf[0] = CMD_ADD_3;

  m = fpTxAndRxCmd(5, 8, 10000);
  if (m != ACK_SUCCESS || fp_RxBuf[4] != ACK_SUCCESS) {
    debugPrintln("  Scan 3 failed!");
    fpSleep();
    invalidateFingerprintCountCache();
    fpEnrollStep = 0;
    currentMode = MODE_FP_ENROLL_FAIL;
    updateDisplay();
    delay(2000);
    currentMode = enrollReturnMode();
    return false;
  }

  // Success!
  debugPrintln("  Fingerprint enrolled successfully!");
  fpSleep();
  invalidateFingerprintCountCache();
  fpEnrollStep = 0;
  currentMode = MODE_FP_ENROLL_OK;
  updateDisplay();
  delay(2000);
  currentMode = enrollReturnMode();
  return true;
}

// =====================================================
// Delete Single Fingerprint
// =====================================================

bool deleteFingerprint(uint8_t id) {
  uint8_t m;

  debugPrint("Deleting fingerprint ID: ");
  debugPrintln(id);

  currentMode = MODE_FP_DELETE;
  updateDisplay();

  fpWake();

  fp_TxBuf[0] = CMD_DEL;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = id;
  fp_TxBuf[3] = 0;
  fp_TxBuf[4] = 0;

  m = fpTxAndRxCmd(5, 8, 500);

  fpSleep();
  invalidateFingerprintCountCache();

  if (m == ACK_SUCCESS && fp_RxBuf[4] == ACK_SUCCESS) {
    debugPrintln("  Deleted successfully");
    currentMode = MODE_FP_DELETE_OK;
    updateDisplay();
    delay(2000);
    currentMode = MODE_IDLE;
    return true;
  } else {
    debugPrintln("  Delete failed");
    currentMode = MODE_FP_DELETE_FAIL;
    updateDisplay();
    delay(2000);
    currentMode = MODE_IDLE;
    return false;
  }
}

// =====================================================
// Clear All Fingerprints
// =====================================================

bool clearAllFingerprints() {
  uint8_t m;

  debugPrintln("Clearing all fingerprints...");

  currentMode = MODE_FP_DELETE;
  updateDisplay();

  fpWake();

  fp_TxBuf[0] = CMD_DEL_ALL;
  fp_TxBuf[1] = 0;
  fp_TxBuf[2] = 0;
  fp_TxBuf[3] = 0;
  fp_TxBuf[4] = 0;

  m = fpTxAndRxCmd(5, 8, 500);

  fpSleep();
  invalidateFingerprintCountCache();

  if (m == ACK_SUCCESS && fp_RxBuf[4] == ACK_SUCCESS) {
    debugPrintln("  All fingerprints cleared");
    currentMode = MODE_FP_DELETE_OK;
    updateDisplay();
    delay(2000);
    currentMode = MODE_IDLE;
    return true;
  } else {
    debugPrintln("  Clear all failed");
    currentMode = MODE_FP_DELETE_FAIL;
    updateDisplay();
    delay(2000);
    currentMode = MODE_IDLE;
    return false;
  }
}

// =====================================================
// Interactive Delete Menu (Rotary Encoder Driven)
// =====================================================

/**
 * Enter the interactive fingerprint delete menu.
 * Shows a scrollable list of stored fingerprint IDs with a "Back" option.
 * User navigates with rotary encoder and presses to select.
 */
void enterFingerprintDeleteMenu() {
  debugPrintln("Entering fingerprint delete menu...");

  int count = getFingerprintCount();
  if (count < 0) count = 0;

  if (count == 0) {
    displayMessage("No fingerprints", "stored yet!");
    delay(2000);
    currentMode = MODE_IDLE;
    return;
  }

  // Menu: item 0 = "Back", items 1..count = fingerprint IDs
  fpMenuItemCount = (uint8_t)(count + 1);
  fpMenuSelection = 0;  // Start on "Back"
  fpConfirmYes = false;

  // Snapshot encoder position
  lastMenuEncoderPos = encoderPosition;

  currentMode = MODE_FP_DELETE_MENU;
  updateDisplay();
}

/**
 * Process rotary encoder input for the delete menu.
 * Called from the main loop when currentMode is MODE_FP_DELETE_MENU or MODE_FP_DELETE_CONFIRM.
 */
void updateFingerprintDeleteMenu() {
  static unsigned long lastButtonTime = 0;
  const unsigned long debounceDelay = 300;

  if (currentMode == MODE_FP_DELETE_MENU) {
    // --- List Navigation ---

    // Check encoder rotation
    int currentPos = encoderPosition;
    if (currentPos != lastMenuEncoderPos) {
      int diff = currentPos - lastMenuEncoderPos;
      lastMenuEncoderPos = currentPos;

      fpMenuSelection += diff;
      // Clamp to valid range
      if (fpMenuSelection < 0) fpMenuSelection = 0;
      if (fpMenuSelection >= (int8_t)fpMenuItemCount) fpMenuSelection = fpMenuItemCount - 1;

      updateDisplay();
    }

    // Check button press
    if (digitalRead(ENCODER_SW) == LOW) {
      if ((millis() - lastButtonTime) > debounceDelay) {
        lastButtonTime = millis();

        if (fpMenuSelection == 0) {
          // "Back" selected — exit menu
          debugPrintln("  Delete menu: Back");
          currentMode = MODE_IDLE;
          updateDisplay();
        } else {
          // Fingerprint ID selected — show confirm dialog
          fpDeleteTargetId = (uint8_t)fpMenuSelection;  // ID = selection index (1-based)
          fpConfirmYes = false;  // Default to "No"
          currentMode = MODE_FP_DELETE_CONFIRM;
          lastMenuEncoderPos = encoderPosition;
          updateDisplay();
          debugPrint("  Selected ID: ");
          debugPrintln(fpDeleteTargetId);
        }
      }
    }

  } else if (currentMode == MODE_FP_DELETE_CONFIRM) {
    // --- Confirm Dialog Navigation ---

    // Check encoder rotation (toggles Yes/No)
    int currentPos = encoderPosition;
    if (currentPos != lastMenuEncoderPos) {
      lastMenuEncoderPos = currentPos;
      fpConfirmYes = !fpConfirmYes;
      updateDisplay();
    }

    // Check button press
    if (digitalRead(ENCODER_SW) == LOW) {
      if ((millis() - lastButtonTime) > debounceDelay) {
        lastButtonTime = millis();

        if (fpConfirmYes) {
          // Confirmed deletion
          debugPrint("  Confirming delete ID: ");
          debugPrintln(fpDeleteTargetId);

          // Perform the deletion (this sets mode to DELETE_OK or DELETE_FAIL)
          deleteFingerprint(fpDeleteTargetId);

          // After deletion, re-enter the menu with updated list
          int newCount = getFingerprintCount();
          if (newCount <= 0) {
            // No more fingerprints
            displayMessage("All fingerprints", "deleted!");
            delay(2000);
            currentMode = MODE_IDLE;
            updateDisplay();
          } else {
            // Refresh menu
            fpMenuItemCount = (uint8_t)(newCount + 1);
            if (fpMenuSelection >= (int8_t)fpMenuItemCount) {
              fpMenuSelection = fpMenuItemCount - 1;
            }
            currentMode = MODE_FP_DELETE_MENU;
            lastMenuEncoderPos = encoderPosition;
            updateDisplay();
          }
        } else {
          // Cancelled — go back to list
          debugPrintln("  Delete cancelled");
          currentMode = MODE_FP_DELETE_MENU;
          lastMenuEncoderPos = encoderPosition;
          updateDisplay();
        }
      }
    }
  }
}
