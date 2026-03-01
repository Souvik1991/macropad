/*
 * Debug Utility Module
 * Provides centralized debug printing with enable/disable control
 */

// ============================================
// DEBUG FUNCTIONS
// ============================================

// Print without newline (same line)
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

// Print with newline
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

// Print empty line
void debugPrintln() {
  #if DEBUG_ENABLED
    Serial.println();
  #endif
}

// Print F() macro strings (for PROGMEM strings)
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

