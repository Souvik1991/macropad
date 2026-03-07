/*
 * Settings Module - Header
 * Handles EEPROM storage for persistent configuration
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"

void initSettings();
void saveOSMode(OperatingSystem os);
void loadOSMode();
void toggleOSMode();
void autoToggleOS();
void setAutoSwitch(bool enabled);
void saveLEDBrightness(uint8_t brightness);
void loadLEDBrightness();
void saveLEDColor(int ledIndex, CRGB color);
void loadLEDColor(int ledIndex, CRGB &color);
void initDefaultLEDColors();
void loadLEDColors();
void saveMacro(int keyNum, MacroConfig &macro);
void loadMacro(int keyNum, MacroConfig &macro);
void initDefaultMacros();
void loadMacros();
void saveKeyName(int keyNum, const char* name);
void loadKeyName(int keyNum, char* name, int maxLen);
void initDefaultKeyNames();
bool hasAnyMacroConfigured();

#endif // SETTINGS_H
