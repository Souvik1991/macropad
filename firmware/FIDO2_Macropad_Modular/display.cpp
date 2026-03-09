/*
 * Display Module - Implementation
 * Handles OLED display initialization and rendering
 */

#include "config.h"
#include "debug.h"
#include "display.h"
#include "encoders.h"
#include "fingerprint.h"
#include "keymatrix.h"
#include "secure.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void initDisplay() {
  debugPrint("Initializing display... ");
  Wire.begin();
  
  if(!display.begin(SCREEN_ADDRESS, true)) {
    debugPrintln("FAILED");
    debugPrintln("Check OLED connections!");
    while(1) {
      delay(1000);  // Halt if display fails
    }
  }
  
  // Show boot screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("ESP32-S3 Macropad"));
  display.println(F("Booting..."));
  display.display();
  
  debugPrintln("OK");
}

// Helper: draw text centered horizontally on screen
void centerText(const __FlashStringHelper* text, int y, int textSize) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(textSize);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int centerX = (SCREEN_WIDTH - (int)w) / 2;
  display.setCursor(centerX, y);
  display.print(text);
}
void centerText(const char* text, int y, int textSize) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(textSize);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int centerX = (SCREEN_WIDTH - (int)w) / 2;
  display.setCursor(centerX, y);
  display.print(text);
}
// Helper: draw text centered within a horizontal span (e.g. a grid cell)
static void centerTextInRect(const char* text, int rectX, int rectY, int rectW, int textSize = 1) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(textSize);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int centerX = rectX + ((int)rectW - (int)w) / 2;
  display.setCursor(centerX, rectY);
  display.print(text);
}

// Forward declarations for screen renderers
void drawStatusBar();
void displayIdleScreen();
void displayFIDO2Screen();
void displayFIDO2VerifyingScreen();
void displaySuccessScreen();
void displayErrorScreen();
void displayFIDO2RetryScreen();
void displayFIDO2TimeoutScreen();
void displayFIDO2SensorErrorScreen();
void displayFIDO2FailedScreen();
void displayFPEnrollScreen();
void displayFPEnrollOKScreen();
void displayFPEnrollFailScreen();
void displayFPDeleteScreen();
void displayFPDeleteOKScreen();
void displayFPDeleteFailScreen();
void displayFPFullScreen();
void displayFPDeleteMenuScreen();
void displayFPDeleteConfirmScreen();
void displayVolumeOverlay();
void displayBootCheckScreen();
void displayBootFailScreen();
void displayATECCSetupNeededScreen();
void displayATECCProvisioningFailScreen();
void displayFPRequiredScreen();
void displaySetupNeededScreen();
void displaySystemMenuScreen();
void displaySystemResetConfirmScreen();

void updateDisplay() {
  display.clearDisplay();
  
  // Always draw status bar at top
  drawStatusBar();
  
  // Draw separator line
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
  
  // Draw main content (starts at row 12)
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 12);
  
  switch(currentMode) {
    case MODE_IDLE:
      // Show volume overlay if encoder was just used
      if (millis() < volumeOverlayUntil) {
        displayVolumeOverlay();
      } else {
        displayIdleScreen();
      }
      break;
    case MODE_FIDO2_WAITING:
      displayFIDO2Screen();
      break;
    case MODE_FIDO2_VERIFYING:
      displayFIDO2VerifyingScreen();
      break;
    case MODE_FIDO2_SUCCESS:
      displaySuccessScreen();
      break;
    case MODE_FIDO2_ERROR:
      displayErrorScreen();
      break;
    case MODE_FIDO2_FP_RETRY:
      displayFIDO2RetryScreen();
      break;
    case MODE_FIDO2_FP_TIMEOUT:
      displayFIDO2TimeoutScreen();
      break;
    case MODE_FIDO2_FP_SENSOR_ERR:
      displayFIDO2SensorErrorScreen();
      break;
    case MODE_FIDO2_FP_FAILED:
      displayFIDO2FailedScreen();
      break;
    case MODE_FP_ENROLL:
      displayFPEnrollScreen();
      break;
    case MODE_FP_ENROLL_OK:
      displayFPEnrollOKScreen();
      break;
    case MODE_FP_ENROLL_FAIL:
      displayFPEnrollFailScreen();
      break;
    case MODE_FP_DELETE:
      displayFPDeleteScreen();
      break;
    case MODE_FP_DELETE_OK:
      displayFPDeleteOKScreen();
      break;
    case MODE_FP_DELETE_FAIL:
      displayFPDeleteFailScreen();
      break;
    case MODE_FP_FULL:
      displayFPFullScreen();
      break;
    case MODE_FP_DELETE_MENU:
      displayFPDeleteMenuScreen();
      break;
    case MODE_FP_DELETE_CONFIRM:
      displayFPDeleteConfirmScreen();
      break;
    case MODE_BOOT_CHECK:
      displayBootCheckScreen();
      break;
    case MODE_BOOT_FAIL:
      displayBootFailScreen();
      break;
    case MODE_ATECC_SETUP_NEEDED:
      displayATECCSetupNeededScreen();
      break;
    case MODE_ATECC_PROVISIONING_FAIL:
      displayATECCProvisioningFailScreen();
      break;
    case MODE_FP_REQUIRED:
      displayFPRequiredScreen();
      break;
    case MODE_SETUP_NEEDED:
      displaySetupNeededScreen();
      break;
    case MODE_SYSTEM_MENU:
      displaySystemMenuScreen();
      break;
    case MODE_SYSTEM_RESET_CONFIRM:
      displaySystemResetConfirmScreen();
      break;
    default:
      displayIdleScreen();
      break;
  }
  
  display.display();
}

void drawStatusBar() {
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  // Left: OS indicator
  display.setCursor(0, 0);
  display.print(F("["));
  display.print(currentOS == OS_MAC ? F("Mac") : F("Win"));
  display.print(F("]"));
  
  // Right: Volume with mute icon (aligned to extreme right)
  const int volBlockW = 48;  // Icon ~16px + "100%" or "MUTE" (~28px) - extra space to prevent wrap
  const int volIconX = SCREEN_WIDTH - volBlockW;
  display.setCursor(volIconX, 0);
  if (isMutedState()) {
    // Draw mute icon (filled speaker + X)
    display.fillRect(volIconX, 2, 3, 5, SH110X_WHITE);
    display.fillTriangle(volIconX + 2, 2, volIconX + 7, 0, volIconX + 7, 8, SH110X_WHITE);
    display.fillTriangle(volIconX + 2, 2, volIconX + 2, 6, volIconX + 7, 8, SH110X_WHITE);
    display.drawLine(volIconX + 10, 2, volIconX + 14, 6, SH110X_WHITE);
    display.drawLine(volIconX + 14, 2, volIconX + 10, 6, SH110X_WHITE);
    display.setCursor(volIconX + 17, 0);
    display.print(F("MUTE"));
  } else {
    // Draw volume icon: filled speaker + sound waves (arcs)
    display.fillRect(volIconX, 2, 3, 5, SH110X_WHITE);
    display.fillTriangle(volIconX + 2, 2, volIconX + 7, 0, volIconX + 7, 8, SH110X_WHITE);
    display.fillTriangle(volIconX + 2, 2, volIconX + 2, 6, volIconX + 7, 8, SH110X_WHITE);
    // Sound waves: right-facing arcs )
    display.drawCircleHelper(volIconX + 9, 4, 2, 0x06, SH110X_WHITE);
    display.drawCircleHelper(volIconX + 9, 4, 4, 0x06, SH110X_WHITE);
    display.setCursor(volIconX + 16, 0);
    display.print((int)(volumeLevel + 0.5f));
    display.print(F("%"));
  }
}

void displayIdleScreen() {
  display.setTextSize(1);

  // Dynamic 3x4 macro grid matching the physical key matrix
  // Each cell shows first 4 chars of key name, or [X] if disabled
  char name[MAX_KEY_NAME_LENGTH + 1];
  int cellW = 32;  // 128px / 4 cols = 32px per cell
  int startY = 13;
  int rowH = 11;

  char cellText[8];
  for (int row = 0; row < MATRIX_ROWS; row++) {
    for (int col = 0; col < MATRIX_COLS; col++) {
      int keyIdx = row * MATRIX_COLS + col;  // 0-based index into storedMacros[]
      int cellX = col * cellW;
      int cellY = startY + row * rowH;
      bool pressed = isKeyPressed(row, col);

      if (pressed) {
        // Invert: fill cell background white, text black
        display.fillRect(cellX, cellY, cellW, rowH, SH110X_WHITE);
        display.setTextColor(SH110X_BLACK);
      } else {
        display.setTextColor(SH110X_WHITE);
      }

      // Build cell text for centering
      if (storedMacros[keyIdx].type != MACRO_TYPE_DISABLED) {
        loadKeyName(keyIdx + 1, name, sizeof(name));
        if (name[0] != 0) {
          name[4] = '\0';
          strncpy(cellText, name, sizeof(cellText) - 1);
          cellText[sizeof(cellText) - 1] = '\0';
        } else {
          snprintf(cellText, sizeof(cellText), "K%d", keyIdx + 1);
        }
      } else {
        strcpy(cellText, "[X]");
      }

      centerTextInRect(cellText, cellX, cellY + 2, cellW);

      display.setTextColor(SH110X_WHITE);  // Reset for next cell
    }
  }

  // Divider line below the grid
  int gridBottom = startY + MATRIX_ROWS * rowH + 1;
  display.drawLine(0, gridBottom, SCREEN_WIDTH, gridBottom, SH110X_WHITE);

  // Bottom area: helpful info (centered)
  centerText(F("Hold K9+K12 = Menu"), gridBottom + 3);
}

// =====================================================
// Volume Overlay (temporary, auto-reverts to idle)
// =====================================================

void displayVolumeOverlay() {
  display.setTextSize(1);

  // Shared layout: icon + text block (centered, shifted down)
  const int spkY = 22;
  const int blockW = isMutedState() ? 88 : 77;  // Muted: icon+"MUTED", Volume: icon+"100%"
  const int spkX = (SCREEN_WIDTH - blockW) / 2;

  // Speaker icon (body + cone) - same for both muted and volume
  display.fillRect(spkX, spkY + 2, 4, 7, SH110X_WHITE);
  display.fillTriangle(spkX + 3, spkY + 2, spkX + 11, spkY - 2, spkX + 11, spkY + 12, SH110X_WHITE);
  display.fillTriangle(spkX + 3, spkY + 2, spkX + 3, spkY + 8, spkX + 11, spkY + 12, SH110X_WHITE);

  display.setTextSize(2);
  if (isMutedState()) {
    display.drawLine(spkX + 16, spkY, spkX + 24, spkY + 10, SH110X_WHITE);
    display.drawLine(spkX + 24, spkY, spkX + 16, spkY + 10, SH110X_WHITE);
    display.setCursor(spkX + 28, spkY);
    display.print(F("MUTED"));
  } else {
    display.drawCircleHelper(spkX + 14, spkY + 5, 3, 0x06, SH110X_WHITE);
    display.drawCircleHelper(spkX + 14, spkY + 5, 6, 0x06, SH110X_WHITE);
    display.drawCircleHelper(spkX + 14, spkY + 5, 9, 0x06, SH110X_WHITE);
    display.setCursor(spkX + 28, spkY);
    display.print((int)(volumeLevel + 0.5f));
    display.print(F("%"));

    // Progress bar (volume only)
    const int barW = 108;
    const int barH = 12;
    const int barX = (SCREEN_WIDTH - barW) / 2;
    const int barY = 42;
    display.drawRect(barX, barY, barW, barH, SH110X_WHITE);
    display.drawRect(barX + 1, barY + 1, barW - 2, barH - 2, SH110X_WHITE);
    int fillW = (int)((barW - 4) * volumeLevel / 100.0f);
    if (fillW > 0) {
      display.fillRect(barX + 2, barY + 2, fillW, barH - 4, SH110X_WHITE);
    }
    for (int pct = 25; pct < 100; pct += 25) {
      int tickX = barX + 2 + ((barW - 4) * pct) / 100;
      display.drawLine(tickX, barY + barH + 1, tickX, barY + barH + 3, SH110X_WHITE);
    }
  }
  display.setTextSize(1);
}

void displayFIDO2Screen() {
  display.setTextSize(1);

  // Draw lock icon (bigger, centered)
  int lockX = 10;
  int lockY = 20;
  display.fillRect(lockX + 2, lockY, 10, 12, SH110X_WHITE);
  display.fillRect(lockX + 4, lockY + 2, 6, 8, SH110X_BLACK);
  display.drawRect(lockX, lockY + 7, 14, 10, SH110X_WHITE);

  display.setCursor(30, 20);
  display.setTextSize(1);
  display.println(F("PASSKEY AUTH"));
  display.setCursor(30, 30);
  display.println(F("Please scan"));
  display.setCursor(30, 40);
  display.println(F("your finger"));

  // Show attempt counter if retrying
  if (fpAuthAttempt > 0) {
    display.setCursor(30, 52);
    display.print(F("Attempt "));
    display.print(fpAuthAttempt);
    display.print(F("/3"));
  }
}

// =====================================================
// FIDO2 Verifying/Processing Screen
// =====================================================

void displayFIDO2VerifyingScreen() {
  display.setTextSize(1);

  // Draw lock icon
  int lockX = 10;
  int lockY = 22;
  display.fillRect(lockX + 2, lockY, 10, 12, SH110X_WHITE);
  display.fillRect(lockX + 4, lockY + 2, 6, 8, SH110X_BLACK);
  display.drawRect(lockX, lockY + 7, 14, 10, SH110X_WHITE);

  display.setCursor(30, 22);
  display.println(F("PASSKEY AUTH"));
  display.setCursor(30, 34);
  display.println(F("Signing..."));

  // Animated dots
  int dots = (millis() / 500) % 4;
  display.setCursor(30, 46);
  for (int i = 0; i < dots; i++) display.print(F("."));
}

// =====================================================
// FIDO2 Fingerprint Retry Screen
// =====================================================

void displayFIDO2RetryScreen() {
  display.setTextSize(1);

  // Draw fingerprint icon with X
  int fpX = 6;
  int fpY = 16;
  display.drawRoundRect(fpX, fpY, 18, 22, 6, SH110X_WHITE);
  display.drawRoundRect(fpX + 4, fpY + 5, 10, 12, 4, SH110X_WHITE);
  // X over it
  display.drawLine(fpX, fpY, fpX + 18, fpY + 22, SH110X_WHITE);
  display.drawLine(fpX + 18, fpY, fpX, fpY + 22, SH110X_WHITE);

  display.setCursor(30, 16);
  display.println(F("SCAN FAILED"));

  display.setCursor(30, 28);
  display.println(F("Try again"));

  // Show retry count
  display.setCursor(30, 40);
  display.print(F("Attempt "));
  display.print(fpAuthAttempt);
  display.print(F("/3"));

  // Progress bar showing attempts used
  int barX = 30;
  int barY = 52;
  int barW = 80;
  int barH = 6;
  display.drawRect(barX, barY, barW, barH, SH110X_WHITE);
  int fillW = (fpAuthAttempt * barW) / 3;
  display.fillRect(barX, barY, fillW, barH, SH110X_WHITE);
}

// =====================================================
// FIDO2 Fingerprint Timeout Screen (no finger detected)
// =====================================================

void displayFIDO2TimeoutScreen() {
  display.setTextSize(1);

  // Draw clock/timeout icon
  int cX = 17;
  int cY = 27;
  display.drawCircle(cX, cY, 10, SH110X_WHITE);
  display.drawCircle(cX, cY, 11, SH110X_WHITE);
  display.drawLine(cX, cY, cX, cY - 6, SH110X_WHITE);  // minute hand
  display.drawLine(cX, cY, cX + 5, cY + 2, SH110X_WHITE);  // hour hand

  display.setCursor(34, 16);
  display.println(F("NO FINGER"));

  display.setCursor(34, 28);
  display.println(F("Place finger"));
  display.setCursor(34, 38);
  display.println(F("on sensor"));

  // Show attempt counter
  if (fpAuthAttempt > 0) {
    display.setCursor(34, 52);
    display.print(F("Attempt "));
    display.print(fpAuthAttempt);
    display.print(F("/3"));
  }
}

// =====================================================
// FIDO2 Sensor Error Screen (hardware/UART failure)
// =====================================================

void displayFIDO2SensorErrorScreen() {
  display.setTextSize(1);

  // Draw warning triangle with !
  int triX = 17;
  int triTop = 16;
  display.drawTriangle(triX, triTop, triX - 12, triTop + 20, triX + 12, triTop + 20, SH110X_WHITE);
  display.drawTriangle(triX + 1, triTop, triX - 11, triTop + 20, triX + 13, triTop + 20, SH110X_WHITE);
  display.setCursor(triX - 2, triTop + 8);
  display.print(F("!"));

  display.setTextSize(1);
  display.setCursor(36, 16);
  display.println(F("SENSOR ERROR"));

  display.setCursor(36, 30);
  display.println(F("Check sensor"));
  display.setCursor(36, 40);
  display.println(F("connection"));

  display.setCursor(10, 56);
  display.println(F("Auth aborted"));
}

// =====================================================
// FIDO2 All Attempts Failed Screen
// =====================================================

void displayFIDO2FailedScreen() {
  display.setTextSize(2);

  // Draw big X
  int xY = 18;
  display.drawLine(10, xY, 30, xY + 20, SH110X_WHITE);
  display.drawLine(30, xY, 10, xY + 20, SH110X_WHITE);
  display.drawLine(11, xY, 31, xY + 20, SH110X_WHITE);
  display.drawLine(31, xY, 11, xY + 20, SH110X_WHITE);
  display.drawLine(12, xY, 32, xY + 20, SH110X_WHITE);
  display.drawLine(32, xY, 12, xY + 20, SH110X_WHITE);

  display.setCursor(40, 18);
  display.println(F("AUTH"));
  display.setCursor(40, 36);
  display.println(F("DENY"));

  display.setTextSize(1);
  centerText(F("3 attempts failed"), 56);
}

void displaySuccessScreen() {
  display.setTextSize(2);
  
  // Draw bigger checkmark
  int checkY = 25;
  display.drawLine(10, checkY, 20, checkY + 10, SH110X_WHITE);
  display.drawLine(20, checkY + 10, 40, checkY - 10, SH110X_WHITE);
  display.drawLine(11, checkY, 21, checkY + 10, SH110X_WHITE);
  display.drawLine(21, checkY + 10, 41, checkY - 10, SH110X_WHITE);
  display.drawLine(12, checkY, 22, checkY + 10, SH110X_WHITE);
  display.drawLine(22, checkY + 10, 42, checkY - 10, SH110X_WHITE);
  
  display.setCursor(50, 20);
  display.println(F("AUTH"));
  display.setCursor(50, 38);
  display.println(F("OK!"));
  
  display.setTextSize(1);
  centerText(F("Welcome back!"), 56);
}

void displayErrorScreen() {
  display.setTextSize(2);
  
  // Draw bigger X
  int xY = 20;
  display.drawLine(10, xY, 30, xY + 20, SH110X_WHITE);
  display.drawLine(30, xY, 10, xY + 20, SH110X_WHITE);
  display.drawLine(11, xY, 31, xY + 20, SH110X_WHITE);
  display.drawLine(31, xY, 11, xY + 20, SH110X_WHITE);
  display.drawLine(12, xY, 32, xY + 20, SH110X_WHITE);
  display.drawLine(32, xY, 12, xY + 20, SH110X_WHITE);
  
  display.setCursor(45, 20);
  display.println(F("AUTH"));
  display.setCursor(45, 38);
  display.println(F("ERR"));
  
  display.setTextSize(1);
  centerText(F("try again"), 56);
}

// =====================================================
// Fingerprint Enrollment Screen (with progress bar + capacity)
// =====================================================

void displayFPEnrollScreen() {
  display.setTextSize(1);

  // Draw fingerprint icon (stylized)
  int fpX = 6;
  int fpY = 14;
  display.drawRoundRect(fpX, fpY, 18, 22, 6, SH110X_WHITE);
  display.drawRoundRect(fpX + 4, fpY + 5, 10, 12, 4, SH110X_WHITE);
  display.drawRoundRect(fpX + 7, fpY + 8, 4, 6, 2, SH110X_WHITE);

  // Title
  display.setCursor(30, 13);
  display.println(F("ENROLL FINGER"));

  // Scan step indicator
  display.setCursor(30, 24);
  display.print(F("Scan "));
  display.print(fpEnrollStep);
  display.println(F(" of 3"));

  // Instruction
  display.setCursor(30, 35);
  if (fpEnrollStep == 1) {
    display.println(F("Place finger..."));
  } else {
    display.println(F("Same finger..."));
  }

  // --- Progress Bar ---
  int barX = 14;
  int barY = 46;
  int barW = 100;
  int barH = 8;
  int segGap = 3;
  int segW = (barW - 2 * segGap) / 3;

  display.drawRect(barX, barY, barW, barH, SH110X_WHITE);

  for (int seg = 0; seg < 3; seg++) {
    int segX = barX + 1 + seg * (segW + segGap);
    if (seg < fpEnrollStep) {
      display.fillRect(segX, barY + 1, segW, barH - 2, SH110X_WHITE);
    }
  }

  // Capacity info at bottom (compact format to fit 128px: "Stored 5/150")
  char buf[24];
  snprintf(buf, sizeof(buf), "Stored %u/%u", fpStoredCount, (unsigned)USER_MAX_CNT);
  centerText(buf, 57);
}

// =====================================================
// Fingerprint Enrollment Success Screen
// =====================================================

void displayFPEnrollOKScreen() {
  display.setTextSize(1);

  // Draw checkmark
  int checkY = 22;
  display.drawLine(10, checkY, 18, checkY + 8, SH110X_WHITE);
  display.drawLine(18, checkY + 8, 34, checkY - 6, SH110X_WHITE);
  display.drawLine(11, checkY, 19, checkY + 8, SH110X_WHITE);
  display.drawLine(19, checkY + 8, 35, checkY - 6, SH110X_WHITE);

  display.setTextSize(2);
  display.setCursor(42, 16);
  display.println(F("OK!"));

  display.setTextSize(1);
  display.setCursor(10, 40);
  display.println(F("Fingerprint saved"));
  display.setCursor(10, 50);
  display.println(F("successfully!"));
}

// =====================================================
// Fingerprint Enrollment Fail Screen
// =====================================================

void displayFPEnrollFailScreen() {
  display.setTextSize(1);

  // Draw X mark
  int xY = 18;
  display.drawLine(10, xY, 26, xY + 16, SH110X_WHITE);
  display.drawLine(26, xY, 10, xY + 16, SH110X_WHITE);
  display.drawLine(11, xY, 27, xY + 16, SH110X_WHITE);
  display.drawLine(27, xY, 11, xY + 16, SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(36, 16);
  display.println(F("ENROLL FAILED"));

  display.setCursor(36, 28);
  display.println(F("Try again with"));
  display.setCursor(36, 38);
  display.println(F("flat finger on"));
  display.setCursor(36, 48);
  display.println(F("sensor center"));
}

// =====================================================
// Fingerprint Delete In-Progress Screen
// =====================================================

void displayFPDeleteScreen() {
  display.setTextSize(1);

  // Animated dots effect (using static counter for simple animation)
  display.setCursor(20, 20);
  display.setTextSize(1);
  display.println(F("DELETING"));

  // Draw fingerprint icon with X overlay
  int fpX = 50;
  int fpY = 32;
  display.drawRoundRect(fpX, fpY, 18, 22, 6, SH110X_WHITE);
  display.drawRoundRect(fpX + 4, fpY + 5, 10, 12, 4, SH110X_WHITE);
  // X over it
  display.drawLine(fpX, fpY, fpX + 18, fpY + 22, SH110X_WHITE);
  display.drawLine(fpX + 18, fpY, fpX, fpY + 22, SH110X_WHITE);

  display.setCursor(20, 56);
  display.println(F("Please wait..."));
}

// =====================================================
// Fingerprint Delete Success Screen
// =====================================================

void displayFPDeleteOKScreen() {
  display.setTextSize(1);

  // Draw checkmark
  int checkY = 22;
  display.drawLine(10, checkY, 18, checkY + 8, SH110X_WHITE);
  display.drawLine(18, checkY + 8, 34, checkY - 6, SH110X_WHITE);
  display.drawLine(11, checkY, 19, checkY + 8, SH110X_WHITE);
  display.drawLine(19, checkY + 8, 35, checkY - 6, SH110X_WHITE);

  display.setTextSize(2);
  display.setCursor(42, 16);
  display.println(F("DEL"));

  display.setTextSize(1);
  display.setCursor(10, 40);
  display.println(F("Fingerprint(s)"));
  display.setCursor(10, 50);
  display.println(F("deleted!"));
}

// =====================================================
// Fingerprint Delete Fail Screen
// =====================================================

void displayFPDeleteFailScreen() {
  display.setTextSize(1);

  // Draw X mark
  int xY = 18;
  display.drawLine(10, xY, 26, xY + 16, SH110X_WHITE);
  display.drawLine(26, xY, 10, xY + 16, SH110X_WHITE);
  display.drawLine(11, xY, 27, xY + 16, SH110X_WHITE);
  display.drawLine(27, xY, 11, xY + 16, SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(36, 16);
  display.println(F("DELETE FAILED"));

  display.setCursor(36, 30);
  display.println(F("Check sensor"));
  display.setCursor(36, 42);
  display.println(F("connection"));
}

// =====================================================
// Storage Full Screen
// =====================================================

void displayFPFullScreen() {
  display.setTextSize(1);

  // Warning icon (triangle with !)
  display.drawTriangle(54, 16, 40, 36, 68, 36, SH110X_WHITE);
  display.drawTriangle(55, 16, 41, 36, 69, 36, SH110X_WHITE);
  display.setCursor(52, 26);
  display.print(F("!"));

  display.setCursor(16, 40);
  display.setTextSize(1);
  display.println(F("  STORAGE FULL!"));

  display.setCursor(10, 52);
  display.print(USER_MAX_CNT);
  display.print(F("/"));
  display.print(USER_MAX_CNT);
  display.print(F(" Delete some"));
}

// =====================================================
// Delete Menu Screen (Nokia-style inverted highlight)
// =====================================================

void displayFPDeleteMenuScreen() {
  display.setTextSize(1);

  // Title bar
  display.fillRect(0, 12, SCREEN_WIDTH, 10, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(2, 13);
  display.print(F(" DELETE FINGERPRINT"));
  display.setTextColor(SH110X_WHITE);

  // Menu area: y=24 to y=63 (40px), each row = 10px, so 4 visible rows
  int rowHeight = 10;
  int menuY = 24;
  int visibleRows = 4;

  // Calculate scroll offset so selected item is always visible
  int scrollOffset = 0;
  if (fpMenuSelection >= visibleRows) {
    scrollOffset = fpMenuSelection - visibleRows + 1;
  }

  for (int i = 0; i < visibleRows; i++) {
    int itemIdx = scrollOffset + i;
    if (itemIdx >= fpMenuItemCount) break;

    int rowY = menuY + i * rowHeight;
    bool isSelected = (itemIdx == fpMenuSelection);

    if (isSelected) {
      // Nokia-style: fill row background white, text black
      display.fillRect(0, rowY, SCREEN_WIDTH, rowHeight, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(4, rowY + 1);
    if (itemIdx == 0) {
      display.print(F("< Back"));
    } else {
      display.print(F("Finger ID "));
      display.print(itemIdx);
    }

    // Reset text color
    display.setTextColor(SH110X_WHITE);
  }

  // Scroll indicators
  if (scrollOffset > 0) {
    // Up arrow at top-right
    display.fillTriangle(120, menuY + 2, 124, menuY + 2, 122, menuY - 1, SH110X_WHITE);
  }
  if (scrollOffset + visibleRows < fpMenuItemCount) {
    // Down arrow at bottom-right
    int arrowY = menuY + visibleRows * rowHeight - 4;
    display.fillTriangle(120, arrowY, 124, arrowY, 122, arrowY + 3, SH110X_WHITE);
  }
}

// =====================================================
// Delete Confirm Dialog (Nokia-style button highlight)
// =====================================================

void displayFPDeleteConfirmScreen() {
  display.setTextSize(1);

  // Fingerprint icon
  int fpX = 50;
  int fpY = 14;
  display.drawRoundRect(fpX, fpY, 18, 22, 6, SH110X_WHITE);
  display.drawRoundRect(fpX + 4, fpY + 5, 10, 12, 4, SH110X_WHITE);
  // X over it
  display.drawLine(fpX, fpY, fpX + 18, fpY + 22, SH110X_WHITE);
  display.drawLine(fpX + 18, fpY, fpX, fpY + 22, SH110X_WHITE);

  // Question text
  display.setCursor(4, 16);
  display.print(F("Delete"));
  display.setCursor(4, 26);
  display.print(F("ID "));
  display.print(fpDeleteTargetId);
  display.print(F("?"));

  // --- Yes / No buttons (Nokia style) ---
  int btnY = 44;
  int btnW = 50;
  int btnH = 14;
  int yesX = 10;
  int noX = 68;

  // Yes button
  if (fpConfirmYes) {
    display.fillRoundRect(yesX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  } else {
    display.drawRoundRect(yesX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
  }
  display.setCursor(yesX + 14, btnY + 3);
  display.print(F("Yes"));

  // No button
  display.setTextColor(SH110X_WHITE);  // Reset first
  if (!fpConfirmYes) {
    display.fillRoundRect(noX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  } else {
    display.drawRoundRect(noX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
  }
  display.setCursor(noX + 17, btnY + 3);
  display.print(F("No"));

  // Reset
  display.setTextColor(SH110X_WHITE);
}

// =====================================================
// System Menu Screen (Nokia-style inverted highlight)
// =====================================================

void displaySystemMenuScreen() {
  display.setTextSize(1);

  // Title bar (inverted)
  display.fillRect(0, 12, SCREEN_WIDTH, 10, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(2, 13);
  display.print(F("   SYSTEM MENU"));
  display.setTextColor(SH110X_WHITE);

  // Menu items area: y=24 to y=63 (40px), each row = 10px, 4 visible rows
  int rowHeight = 10;
  int menuY = 24;

  // Menu item labels
  const char* labels[] = {
    "Toggle OS",
    "Enroll Finger",
    "Delete Finger",
    "System Reset",
    "Exit"
  };

  for (int i = 0; i < (int)sysMenuItemCount; i++) {
    int rowY = menuY + i * rowHeight;
    bool isSelected = (i == sysMenuSelection);

    if (isSelected) {
      display.fillRect(0, rowY, SCREEN_WIDTH, rowHeight, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
    } else {
      display.setTextColor(SH110X_WHITE);
    }

    display.setCursor(4, rowY + 1);

    if (i == 0) {
      // Show current OS in the toggle item
      display.print(F("Toggle OS ("));
      display.print(currentOS == OS_MAC ? F("Mac") : F("Win"));
      display.print(F(")"));
    } else {
      display.print(labels[i]);
    }

    display.setTextColor(SH110X_WHITE);
  }
}

// =====================================================
// System Reset Confirm Dialog (Nokia-style Yes/No)
// =====================================================

void displaySystemResetConfirmScreen() {
  display.setTextSize(1);

  // Question text
  centerText(F("RESET ALL DATA?"), 14);
  display.setCursor(4, 26);
  display.print(F("Fingerprints, macros,"));
  display.setCursor(4, 36);
  display.print(F("LEDs, passkeys"));

  // --- Yes / No buttons (Nokia style) ---
  int btnY = 50;
  int btnW = 50;
  int btnH = 12;
  int yesX = 10;
  int noX = 68;

  if (fpConfirmYes) {
    display.fillRoundRect(yesX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  } else {
    display.drawRoundRect(yesX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
  }
  display.setCursor(yesX + 14, btnY + 2);
  display.print(F("Yes"));

  display.setTextColor(SH110X_WHITE);
  if (!fpConfirmYes) {
    display.fillRoundRect(noX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  } else {
    display.drawRoundRect(noX, btnY, btnW, btnH, 3, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE);
  }
  display.setCursor(noX + 17, btnY + 2);
  display.print(F("No"));

  display.setTextColor(SH110X_WHITE);
}

// =====================================================
// Boot Check Screen (component health diagnostics)
// =====================================================

void displayBootCheckScreen() {
  display.setTextSize(1);

  centerText(F("SYSTEM CHECK"), 13);
  display.drawLine(0, 23, SCREEN_WIDTH, 23, SH110X_WHITE);

  // ATECC608A status
  display.setCursor(4, 26);
  display.print(isDevicePresent() ? F("[OK] ") : F("[!!] "));
  display.println(F("Secure Element"));

  // Fingerprint sensor status
  display.setCursor(4, 36);
  display.print(isFingerprintSensorPresent() ? F("[OK] ") : F("[!!] "));
  display.println(F("FP Sensor"));

  // USB (always assumed OK if we got this far)
  display.setCursor(4, 46);
  display.println(F("[OK] USB HID"));

  // Overall status
  display.setCursor(4, 57);
  if (isDevicePresent() && isFingerprintSensorPresent()) {
    display.print(F("All systems GO"));
  } else {
    display.print(F("Issues detected!"));
  }
}

// =====================================================
// Boot Fail Screen (critical component missing)
// =====================================================

void displayBootFailScreen() {
  display.setTextSize(1);

  // Warning icon (triangle with !)
  int triX = 64;
  int triTop = 14;
  display.drawTriangle(triX, triTop, triX - 10, triTop + 16, triX + 10, triTop + 16, SH110X_WHITE);
  display.drawTriangle(triX + 1, triTop, triX - 9, triTop + 16, triX + 11, triTop + 16, SH110X_WHITE);
  display.setCursor(triX - 2, triTop + 6);
  display.print(F("!"));

  display.setCursor(10, 34);
  display.println(F("BOOT FAILED"));

  // Show what's missing
  display.setCursor(4, 46);
  if (!isDevicePresent()) {
    display.println(F("ATECC608A missing"));
  }
  if (!isFingerprintSensorPresent()) {
    display.setCursor(4, 56);
    display.println(F("FP Sensor missing"));
  }
}

// =====================================================
// ATECC Setup Needed Screen (boot provisioning gate)
// =====================================================

void displayATECCSetupNeededScreen() {
  display.setTextSize(1);

  display.setCursor(4, 14);
  display.println(F("ATECC SETUP"));
  display.setCursor(4, 24);
  display.println(F("REQUIRED"));

  // Lock/safe icon (simple)
  int iconX = 85;
  int iconY = 14;
  display.drawRoundRect(iconX, iconY, 22, 18, 4, SH110X_WHITE);
  display.fillRect(iconX + 8, iconY - 2, 6, 4, SH110X_WHITE);
  display.drawRect(iconX + 10, iconY + 4, 2, 6, SH110X_WHITE);

  display.setCursor(4, 40);
  display.println(F("Press encoder to"));
  display.setCursor(4, 50);
  display.println(F("provision (1-time)"));
}

// =====================================================
// ATECC Provisioning Fail Screen (retry prompt)
// =====================================================

void displayATECCProvisioningFailScreen() {
  display.setTextSize(1);

  display.setCursor(4, 14);
  display.println(F("PROVISIONING"));
  display.setCursor(4, 24);
  display.println(F("FAILED"));

  display.setCursor(4, 40);
  display.println(F("Press encoder to"));
  display.setCursor(4, 50);
  display.println(F("retry"));
}

// =====================================================
// Fingerprint Required Screen (boot enrollment gate)
// =====================================================

void displayFPRequiredScreen() {
  display.setTextSize(1);

  // Text on left
  display.setCursor(4, 14);
  display.println(F("NO FINGER"));
  display.setCursor(4, 24);
  display.println(F("ENROLLED"));

  // Fingerprint/encoder icon (3 concentric shapes) - right side to avoid overlap
  int fpX = 78;
  int fpY = 14;
  display.drawRoundRect(fpX, fpY, 18, 22, 6, SH110X_WHITE);
  display.drawRoundRect(fpX + 4, fpY + 5, 10, 12, 4, SH110X_WHITE);
  display.drawRoundRect(fpX + 7, fpY + 8, 4, 6, 2, SH110X_WHITE);

  display.setCursor(4, 40);
  display.println(F("Press encoder to"));
  display.setCursor(4, 50);
  display.println(F("start enrollment"));
}

// =====================================================
// Setup Needed Screen (no macros configured)
// =====================================================

void displaySetupNeededScreen() {
  display.setTextSize(1);

  centerText(F("NO MACROS SET"), 14);
  display.drawLine(0, 24, SCREEN_WIDTH, 24, SH110X_WHITE);

  display.setCursor(4, 28);
  display.println(F("Configure at:"));

  // Print URL with automatic word wrapping for long URLs
  // At text size 1, each char is 6px wide → 21 chars per line
  const char* url = SETUP_URL;
  int urlLen = strlen(url);
  int charsPerLine = 21;
  int y = 40;

  int pos = 0;
  while (pos < urlLen && y < 60) {
    display.setCursor(4, y);
    int remaining = urlLen - pos;
    int lineLen = (remaining > charsPerLine) ? charsPerLine : remaining;

    for (int i = 0; i < lineLen; i++) {
      display.print(url[pos + i]);
    }
    pos += lineLen;
    y += 10;
  }
}

void displayMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2) {
    display.println(line2);
  }
  display.display();
}
