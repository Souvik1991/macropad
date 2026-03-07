/*
 * Debug Utility Module - Header
 * Provides centralized debug printing with enable/disable control
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

void debugPrint(const char* str);
void debugPrint(int num);
void debugPrint(int num, int format);  // format: DEC, HEX, BIN, OCT
void debugPrint(unsigned int num);
void debugPrint(long num);
void debugPrint(unsigned long num);
void debugPrint(float num);
void debugPrint(double num);
void debugPrint(const __FlashStringHelper* str);

void debugPrintln(const char* str);
void debugPrintln(int num);
void debugPrintln(unsigned int num);
void debugPrintln(long num);
void debugPrintln(unsigned long num);
void debugPrintln(float num);
void debugPrintln(double num);
void debugPrintln(const __FlashStringHelper* str);
void debugPrintln();
void debugPrintf(const char* fmt, ...);

#endif // DEBUG_H
