/*
 * Display Module - Header
 * Handles OLED display initialization and rendering
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

void initDisplay();
void updateDisplay();
void displayMessage(const char* line1, const char* line2 = NULL);

// Draw text centered horizontally on screen (textSize defaults to 1)
void centerText(const char* text, int y, int textSize = 1);
void centerText(const __FlashStringHelper* text, int y, int textSize = 1);

#endif // DISPLAY_H
