/*
 * LED Module - Implementation
 * Handles WS2812B RGB LED control
 * 
 * Status patterns (active/idle/error) are temporary overlays.
 * After a timeout, LEDs automatically revert to the user's saved
 * per-LED colors stored in EEPROM (loaded via settings.cpp).
 */

#include "config.h"
#include "debug.h"
#include "leds.h"
#include "settings.h"

CRGB leds[NUM_LEDS];

// Track when to revert from a temporary status pattern
static unsigned long patternStartTime = 0;
static unsigned long patternDuration = 0;
static bool patternActive = false;

// Duration (ms) for status patterns before reverting to saved colors
#define PATTERN_ACTIVE_DURATION 500
#define PATTERN_ERROR_DURATION 1000

void initLEDs() {
  debugPrint("Initializing LEDs... ");
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  // Note: brightness and per-LED colors are loaded by initSettings()
  // which runs BEFORE initLEDs(). We do NOT override them here.
  FastLED.show();
  debugPrintln("OK");
}

void restoreSavedLEDs() {
  loadLEDColors();
  patternActive = false;
}

void updateLEDs() {
  // Auto-revert from temporary status pattern after timeout
  EVERY_N_MILLISECONDS(50) {
    if (patternActive) {
      unsigned long elapsed = millis() - patternStartTime;
      if (elapsed > patternDuration) {
        restoreSavedLEDs();
      }
    }
  }
}

void setLEDPattern(int pattern) {
  switch(pattern) {
    case PATTERN_IDLE:  // Restore user's saved LED colors
      restoreSavedLEDs();
      break;
      
    case PATTERN_ACTIVE:  // Temporary green flash
      fill_solid(leds, NUM_LEDS, CRGB(0, 100, 0));
      patternActive = true;
      patternStartTime = millis();
      patternDuration = PATTERN_ACTIVE_DURATION;
      break;
      
    case PATTERN_ERROR:  // Temporary red flash
      fill_solid(leds, NUM_LEDS, CRGB(100, 0, 0));
      patternActive = true;
      patternStartTime = millis();
      patternDuration = PATTERN_ERROR_DURATION;
      break;
      
    default:
      restoreSavedLEDs();
      break;
  }
  FastLED.show();
}

void setAllLEDs(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void setLED(int index, CRGB color) {
  if (index >= 0 && index < NUM_LEDS) {
    leds[index] = color;
    FastLED.show();
  }
}

void flashLED(int index) {
  if (index >= 0 && index < NUM_LEDS) {
    // Save the current color for this LED
    CRGB savedColor = leds[index];
    
    // Flash white
    leds[index] = CRGB::White;
    FastLED.show();
    delay(50);
    
    // Restore just this LED to its saved color (not all LEDs)
    leds[index] = savedColor;
    FastLED.show();
  }
}

void flashAllLEDs(CRGB color, int duration) {
  // Save current LED state
  CRGB savedLeds[NUM_LEDS];
  for (int i = 0; i < NUM_LEDS; i++) {
    savedLeds[i] = leds[i];
  }
  
  // Flash the requested color
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  delay(duration);
  
  // Restore saved colors
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = savedLeds[i];
  }
  FastLED.show();
}

void rainbowCycle(uint8_t speed) {
  static uint8_t hue = 0;
  fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
  FastLED.show();
  hue += speed;
}

void breathingEffect(CRGB color) {
  uint8_t breath = beatsin8(30, 50, 255);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
    leds[i].fadeToBlackBy(255 - breath);
  }
  FastLED.show();
}
