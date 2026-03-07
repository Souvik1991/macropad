/*
 * Display Module - Header
 * Handles OLED display initialization and rendering
 */

#ifndef DISPLAY_H
#define DISPLAY_H

void initDisplay();
void updateDisplay();
void displayMessage(const char* line1, const char* line2 = NULL);

#endif // DISPLAY_H
