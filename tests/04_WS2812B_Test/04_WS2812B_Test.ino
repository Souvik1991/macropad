/*
 * WS2812B RGB LED Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * LEDs: 4-5x WS2812B-5050 or 2020 (underglow)
 * 
 * Connections (with SN74AHCT1G125 Level Shifter):
 * ESP32 GPIO17 -> SN74AHCT1G125 Pin 2 (A - Input)
 * Pin 1 (OE) -> GND (always enabled)
 * Pin 3 (GND) -> GND
 * Pin 5 (VCC) -> 5V (USB VBUS)
 * Pin 4 (Y - Output) -> WS2812B DIN
 * 
 * NOTE: GPIO33 is reserved for onboard NeoPixel on ESP32-S3 Feather!
 * 
 * WS2812B Power:
 * VCC -> 5V (USB VBUS)
 * GND -> GND
 * 
 * Install Library:
 * - FastLED (recommended) or Adafruit_NeoPixel
 */

#include <FastLED.h>

#define LED_PIN     17      // GPIO17 -> Level Shifter (GPIO33 reserved)
#define NUM_LEDS    5       // Number of underglow LEDs (adjust as needed)
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB     // WS2812B-2020 color order
#define BRIGHTNESS  50      // 0-255 (start with lower brightness)

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("WS2812B LED Test Starting...");
  
  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  Serial.println("LEDs initialized!");
  Serial.print("Number of LEDs: ");
  Serial.println(NUM_LEDS);
  Serial.println("Running test patterns...");
  Serial.println();
}

void loop() {
  Serial.println("Test 1: Solid Colors");
  testSolidColors();
  delay(2000);
  
  Serial.println("Test 2: Rainbow Cycle");
  testRainbow();
  delay(2000);
  
  Serial.println("Test 3: Breathing Effect");
  testBreathing();
  delay(2000);
  
  Serial.println("Test 4: Chase Effect");
  testChase();
  delay(2000);
  
  Serial.println("Test 5: Individual LED Control");
  testIndividual();
  delay(2000);
}

void testSolidColors() {
  // Test each color
  setAllLEDs(CRGB::Red);
  Serial.println("  -> Red");
  delay(1000);
  
  setAllLEDs(CRGB::Green);
  Serial.println("  -> Green");
  delay(1000);
  
  setAllLEDs(CRGB::Blue);
  Serial.println("  -> Blue");
  delay(1000);
  
  setAllLEDs(CRGB::White);
  Serial.println("  -> White");
  delay(1000);
  
  setAllLEDs(CRGB::Purple);
  Serial.println("  -> Purple");
  delay(1000);
  
  setAllLEDs(CRGB::Cyan);
  Serial.println("  -> Cyan");
  delay(1000);
  
  setAllLEDs(CRGB::Yellow);
  Serial.println("  -> Yellow");
  delay(1000);
  
  setAllLEDs(CRGB::Black);  // Turn off
  delay(500);
}

void testRainbow() {
  for (int hue = 0; hue < 256; hue += 2) {
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    FastLED.show();
    delay(20);
  }
}

void testBreathing() {
  for (int i = 0; i < 3; i++) {  // 3 breath cycles
    // Fade in
    for (int brightness = 0; brightness < 150; brightness += 2) {
      setAllLEDs(CRGB(0, 0, brightness));
      delay(10);
    }
    // Fade out
    for (int brightness = 150; brightness > 0; brightness -= 2) {
      setAllLEDs(CRGB(0, 0, brightness));
      delay(10);
    }
  }
}

void testChase() {
  for (int cycle = 0; cycle < 3; cycle++) {  // 3 chase cycles
    for (int i = 0; i < NUM_LEDS; i++) {
      FastLED.clear();
      leds[i] = CRGB::Purple;
      FastLED.show();
      delay(150);
    }
  }
  FastLED.clear();
  FastLED.show();
}

void testIndividual() {
  // Light up each LED one by one
  FastLED.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(255, 0, 255);  // Magenta
    FastLED.show();
    Serial.print("  -> LED ");
    Serial.print(i + 1);
    Serial.println(" ON");
    delay(500);
  }
  
  // Turn off each LED one by one
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
    FastLED.show();
    Serial.print("  -> LED ");
    Serial.print(i + 1);
    Serial.println(" OFF");
    delay(500);
  }
}

void setAllLEDs(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

