/*
 * OLED Display Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * Display: 0.96" I2C OLED (SSD1306) 128x64
 * 
 * Connections (I2C - shared bus with ATECC608A):
 * OLED VCC -> ESP32 3.3V
 * OLED GND -> ESP32 GND
 * OLED SCL -> ESP32 SCL (GPIO 4)
 * OLED SDA -> ESP32 SDA (GPIO 3)
 * 
 * I2C Address: 0x3C
 * 
 * Install Libraries:
 * - Adafruit SSD1306
 * - Adafruit GFX Library
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3c // Try 0x3D if this fails!

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  if(!display.begin(SCREEN_ADDRESS, true)) { 
    Serial.println(F("SH1106 allocation failed"));
  }
  Serial.println("OLED Test Starting...");
  
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds
  Serial.println("OLED initialized successfully!");
  
  // Clear the buffer
  display.clearDisplay();
  
  // Display welcome message
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("FIDO2 Macropad"));
  display.println(F("-------------"));
  display.println(F("Display Test"));
  display.println(F(""));
  display.println(F("Status: OK"));
  display.display();
  
  delay(2000);
}

void loop() {
  // Test 1: Display modes
  testIdleMode();
  delay(3000);
  
  testFIDO2Mode();
  delay(3000);
  
  testSuccessMode();
  delay(3000);
  
  testErrorMode();
  delay(3000);
  
  // Test 2: Counter
  testCounter();
  delay(2000);
}

void testIdleMode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  display.setCursor(0, 0);
  display.println(F("MACROPAD READY"));
  display.println(F("=============="));
  display.println(F("[1:Copy] [2:Paste]"));
  display.println(F("[3:Save] [4:Undo]"));
  display.println(F(""));
  display.print(F("Volume: "));
  
  // Draw volume bar
  int volume = 65;
  int barWidth = (volume * 50) / 100;
  display.fillRect(0, 56, barWidth, 6, SH110X_WHITE);
  display.drawRect(0, 56, 50, 6, SH110X_WHITE);
  display.setCursor(54, 56);
  display.print(volume);
  display.print(F("%"));
  
  display.display();
  Serial.println("Displayed: Idle Mode");
}

void testFIDO2Mode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  // Draw lock icon (simple representation)
  display.fillRect(8, 5, 10, 12, SH110X_WHITE);
  display.fillRect(10, 7, 6, 8, SH110X_BLACK);
  display.drawRect(6, 12, 14, 8, SH110X_WHITE);
  
  display.setCursor(22, 8);
  display.setTextSize(1);
  display.println(F("PASSKEY AUTH"));
  
  display.setCursor(0, 24);
  display.println(F("GitHub.com"));
  display.println(F(""));
  display.println(F(">> SCAN FINGER"));
  display.println(F("[Waiting...]"));
  
  display.display();
  Serial.println("Displayed: FIDO2 Auth Mode");
}

void testSuccessMode() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  
  // Draw checkmark
  display.drawLine(10, 20, 15, 25, SH110X_WHITE);
  display.drawLine(15, 25, 25, 10, SH110X_WHITE);
  display.drawLine(11, 20, 16, 25, SH110X_WHITE);
  display.drawLine(16, 25, 26, 10, SH110X_WHITE);
  
  display.setCursor(35, 12);
  display.println(F("LOGIN"));
  display.setCursor(35, 30);
  display.println(F("OK!"));
  
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.println(F("Welcome back!"));
  
  display.display();
  Serial.println("Displayed: Success Mode");
}

void testErrorMode() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  
  // Draw X
  display.drawLine(10, 10, 25, 25, SH110X_WHITE);
  display.drawLine(25, 10, 10, 25, SH110X_WHITE);
  display.drawLine(11, 10, 26, 25, SH110X_WHITE);
  display.drawLine(26, 10, 11, 25, SH110X_WHITE);
  
  display.setCursor(35, 12);
  display.println(F("AUTH"));
  display.setCursor(35, 30);
  display.println(F("FAIL"));
  
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.println(F("Try again..."));
  
  display.display();
  Serial.println("Displayed: Error Mode");
}

void testCounter() {
  for(int i = 0; i <= 10; i++) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(10, 20);
    display.print(F("Count: "));
    display.println(i);
    display.display();
    delay(200);
  }
  Serial.println("Counter test complete");
}

