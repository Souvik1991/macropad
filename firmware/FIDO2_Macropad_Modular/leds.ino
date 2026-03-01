/*
 * LED Module
 * Handles WS2812B RGB LED control
 */

// ============================================
// GLOBAL OBJECTS
// ============================================

CRGB leds[NUM_LEDS];

// ============================================
// INITIALIZATION
// ============================================

void initLEDs() {
  debugPrint("Initializing LEDs... ");
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  debugPrintln("OK");
}

// ============================================
// LED UPDATE (called in main loop)
// ============================================

void updateLEDs() {
  // Add dynamic effects here
  EVERY_N_MILLISECONDS(50) {
    // Breathing effect example
    // uint8_t breath = beatsin8(30, 30, 150);
    // FastLED.setBrightness(breath);
  }
}

// ============================================
// LED PATTERNS
// ============================================

void setLEDPattern(int pattern) {
  switch(pattern) {
    case PATTERN_IDLE:  // Soft blue
      fill_solid(leds, NUM_LEDS, CRGB(0, 0, 50));
      break;
      
    case PATTERN_ACTIVE:  // Green
      fill_solid(leds, NUM_LEDS, CRGB(0, 100, 0));
      break;
      
    case PATTERN_ERROR:  // Red
      fill_solid(leds, NUM_LEDS, CRGB(100, 0, 0));
      break;
      
    default:
      fill_solid(leds, NUM_LEDS, CRGB::Black);
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
    leds[index] = CRGB::White;
    FastLED.show();
    delay(50);
    setLEDPattern(PATTERN_IDLE);
  }
}

void flashAllLEDs(CRGB color, int duration = 100) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  delay(duration);
  setLEDPattern(PATTERN_IDLE);
}

// ============================================
// ADVANCED EFFECTS
// ============================================

void rainbowCycle(uint8_t speed = 2) {
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

