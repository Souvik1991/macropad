/*
 * Display Module
 * Handles OLED display initialization and rendering
 */

// ============================================
// GLOBAL OBJECTS
// ============================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// INITIALIZATION
// ============================================

void initDisplay() {
  debugPrint("Initializing display... ");
  Wire.begin();
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    debugPrintln("FAILED");
    debugPrintln("Check OLED connections!");
    while(1) {
      delay(1000);  // Halt if display fails
    }
  }
  
  // Show boot screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("ESP32-S3 Macropad"));
  display.println(F("Booting..."));
  display.display();
  
  debugPrintln("OK");
}

// ============================================
// DISPLAY UPDATE
// ============================================

void updateDisplay() {
  display.clearDisplay();
  
  // Always draw status bar at top
  drawStatusBar();
  
  // Draw separator line
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  
  // Draw main content (starts at row 12)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 12);
  
  switch(currentMode) {
    case MODE_IDLE:
      displayIdleScreen();
      break;
    case MODE_FIDO2_WAITING:
      displayFIDO2Screen();
      break;
    case MODE_FIDO2_SUCCESS:
      displaySuccessScreen();
      break;
    case MODE_FIDO2_ERROR:
      displayErrorScreen();
      break;
    default:
      displayIdleScreen();
      break;
  }
  
  display.display();
}

// ============================================
// STATUS BAR (Always visible at top)
// ============================================

void drawStatusBar() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Left: OS indicator
  display.setCursor(0, 0);
  display.print(F("["));
  display.print(currentOS == OS_MAC ? F("Mac") : F("Win"));
  display.print(F("]"));
  
  // Middle: Volume with mute icon
  display.setCursor(48, 0);
  if (isMutedState()) {
    // Draw mute icon (speaker with X)
    display.fillRect(48, 1, 3, 6, SSD1306_WHITE);
    display.fillTriangle(51, 1, 51, 7, 55, 4, SSD1306_WHITE);
    display.drawLine(57, 2, 60, 6, SSD1306_WHITE);
    display.drawLine(60, 2, 57, 6, SSD1306_WHITE);
    display.setCursor(65, 0);
    display.print(F("MUTE"));
  } else {
    // Draw speaker icon
    display.fillRect(48, 1, 3, 6, SSD1306_WHITE);
    display.fillTriangle(51, 1, 51, 7, 55, 4, SSD1306_WHITE);
    display.setCursor(58, 0);
    display.print(volumeLevel);
    display.print(F("%"));
  }
  
  // Right side: Signal indicator (future: WiFi/BT)
  // display.setCursor(110, 0);
  // display.print(F("USB"));
}

// ============================================
// SCREEN MODES
// ============================================

void displayIdleScreen() {
  display.println(F("READY FOR ACTION"));
  display.println(F("---------------"));
  display.println(F("R1:Shot Mic Vid Lock"));
  display.println(F("R2:Copy Cut Pst Del"));
  display.println(F("R3:Cur Insp Trm Git"));
  display.println();
  display.println(F("Double-tap Key 12"));
  display.print(F("to switch OS mode"));
}

void displayFIDO2Screen() {
  display.setTextSize(1);
  
  // Draw lock icon (bigger, centered)
  int lockX = 10;
  int lockY = 20;
  display.fillRect(lockX + 2, lockY, 10, 12, SSD1306_WHITE);
  display.fillRect(lockX + 4, lockY + 2, 6, 8, SSD1306_BLACK);
  display.drawRect(lockX, lockY + 7, 14, 10, SSD1306_WHITE);
  
  display.setCursor(30, 20);
  display.setTextSize(1);
  display.println(F("PASSKEY AUTH"));
  display.setCursor(30, 30);
  display.println(F("Please scan"));
  display.setCursor(30, 40);
  display.println(F("your finger"));
}

void displaySuccessScreen() {
  display.setTextSize(2);
  
  // Draw bigger checkmark
  int checkY = 25;
  display.drawLine(10, checkY, 20, checkY + 10, SSD1306_WHITE);
  display.drawLine(20, checkY + 10, 40, checkY - 10, SSD1306_WHITE);
  display.drawLine(11, checkY, 21, checkY + 10, SSD1306_WHITE);
  display.drawLine(21, checkY + 10, 41, checkY - 10, SSD1306_WHITE);
  display.drawLine(12, checkY, 22, checkY + 10, SSD1306_WHITE);
  display.drawLine(22, checkY + 10, 42, checkY - 10, SSD1306_WHITE);
  
  display.setCursor(50, 20);
  display.println(F("AUTH"));
  display.setCursor(50, 38);
  display.println(F("OK!"));
  
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.println(F("Welcome back!"));
}

void displayErrorScreen() {
  display.setTextSize(2);
  
  // Draw bigger X
  int xY = 20;
  display.drawLine(10, xY, 30, xY + 20, SSD1306_WHITE);
  display.drawLine(30, xY, 10, xY + 20, SSD1306_WHITE);
  display.drawLine(11, xY, 31, xY + 20, SSD1306_WHITE);
  display.drawLine(31, xY, 11, xY + 20, SSD1306_WHITE);
  display.drawLine(12, xY, 32, xY + 20, SSD1306_WHITE);
  display.drawLine(32, xY, 12, xY + 20, SSD1306_WHITE);
  
  display.setCursor(45, 20);
  display.println(F("AUTH"));
  display.setCursor(45, 38);
  display.println(F("ERR"));
  
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.println(F("Try again..."));
}

// ============================================
// HELPER FUNCTIONS
// ============================================

void displayMessage(const char* line1, const char* line2 = NULL) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2) {
    display.println(line2);
  }
  display.display();
}

