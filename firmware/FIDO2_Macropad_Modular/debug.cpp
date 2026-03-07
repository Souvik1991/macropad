/*
 * Debug Utility Module - Implementation
 * Provides centralized debug printing with enable/disable control
 */

#include "config.h"
#include "debug.h"
#include <stdarg.h>

void debugPrint(const char* str) {
  #if DEBUG_ENABLED
    Serial.print(str);
  #endif
}

void debugPrint(int num) {
  #if DEBUG_ENABLED
    Serial.print(num);
  #endif
}

void debugPrint(int num, int format) {
  #if DEBUG_ENABLED
    Serial.print(num, format);
  #endif
}

void debugPrint(unsigned int num) {
  #if DEBUG_ENABLED
    Serial.print(num);
  #endif
}

void debugPrint(long num) {
  #if DEBUG_ENABLED
    Serial.print(num);
  #endif
}

void debugPrint(unsigned long num) {
  #if DEBUG_ENABLED
    Serial.print(num);
  #endif
}

void debugPrint(float num) {
  #if DEBUG_ENABLED
    Serial.print(num);
  #endif
}

void debugPrint(double num) {
  #if DEBUG_ENABLED
    Serial.print(num);
  #endif
}

void debugPrintln(const char* str) {
  #if DEBUG_ENABLED
    Serial.println(str);
  #endif
}

void debugPrintln(int num) {
  #if DEBUG_ENABLED
    Serial.println(num);
  #endif
}

void debugPrintln(unsigned int num) {
  #if DEBUG_ENABLED
    Serial.println(num);
  #endif
}

void debugPrintln(long num) {
  #if DEBUG_ENABLED
    Serial.println(num);
  #endif
}

void debugPrintln(unsigned long num) {
  #if DEBUG_ENABLED
    Serial.println(num);
  #endif
}

void debugPrintln(float num) {
  #if DEBUG_ENABLED
    Serial.println(num);
  #endif
}

void debugPrintln(double num) {
  #if DEBUG_ENABLED
    Serial.println(num);
  #endif
}

void debugPrintln() {
  #if DEBUG_ENABLED
    Serial.println();
  #endif
}

void debugPrint(const __FlashStringHelper* str) {
  #if DEBUG_ENABLED
    Serial.print(str);
  #endif
}

void debugPrintln(const __FlashStringHelper* str) {
  #if DEBUG_ENABLED
    Serial.println(str);
  #endif
}

void debugPrintf(const char* fmt, ...) {
  #if DEBUG_ENABLED
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
  #endif
}
