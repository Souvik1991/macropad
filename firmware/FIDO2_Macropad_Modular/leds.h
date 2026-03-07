/*
 * LED Module - Header
 * Handles WS2812B RGB LED control
 */

#ifndef LEDS_H
#define LEDS_H

#include "config.h"

void initLEDs();
void updateLEDs();
void setLEDPattern(int pattern);
void restoreSavedLEDs();
void setAllLEDs(CRGB color);
void setLED(int index, CRGB color);
void flashLED(int index);
void flashAllLEDs(CRGB color, int duration = 100);
void rainbowCycle(uint8_t speed = 2);
void breathingEffect(CRGB color);

#endif // LEDS_H
