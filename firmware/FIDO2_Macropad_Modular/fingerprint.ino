/*
 * Fingerprint Module
 * Handles Waveshare fingerprint sensor communication
 */

// ============================================
// GLOBAL OBJECTS
// ============================================

HardwareSerial fingerprintSerial(1);

// ============================================
// INITIALIZATION
// ============================================

void initFingerprint() {
  debugPrint("Initializing fingerprint sensor... ");
  fingerprintSerial.begin(FINGERPRINT_BAUD, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  delay(100);
  debugPrintln("OK");
}

// ============================================
// FINGERPRINT FUNCTIONS
// ============================================

bool verifyFingerprint() {
  /*
   * Verify fingerprint against stored templates
   * Returns: true if match found, false otherwise
   * 
   * TODO: Implement using Waveshare SDK
   * 1. Send verification command to sensor
   * 2. Wait for sensor response
   * 3. Parse match result
   * 4. Return match status
   */
  
  debugPrintln("Verifying fingerprint...");
  
  // Placeholder - implement with actual sensor protocol
  // For testing, you can return true
  
  return false;  // No match (placeholder)
}

bool enrollFingerprint(uint8_t id) {
  /*
   * Enroll a new fingerprint
   * id: Storage ID for the fingerprint template
   * Returns: true if enrollment successful
   * 
   * TODO: Implement using Waveshare SDK
   * Typical process:
   * 1. Request first scan
   * 2. Generate first template
   * 3. Request second scan
   * 4. Generate second template
   * 5. Merge templates and store with ID
   */
  
  debugPrint("Enrolling fingerprint ID: ");
  debugPrintln(id);
  
  // Placeholder
  return false;
}

bool deleteFingerprint(uint8_t id) {
  /*
   * Delete a stored fingerprint
   * id: ID of fingerprint to delete
   * Returns: true if deletion successful
   */
  
  debugPrint("Deleting fingerprint ID: ");
  debugPrintln(id);
  
  // Placeholder
  return false;
}

int getFingerprintCount() {
  /*
   * Get number of enrolled fingerprints
   * Returns: Number of stored templates
   */
  
  // Placeholder
  return 0;
}

void clearAllFingerprints() {
  /*
   * Delete all stored fingerprints
   */
  
  debugPrintln("Clearing all fingerprints...");
  
  // Placeholder
}

// ============================================
// HELPER FUNCTIONS
// ============================================

void waitForFinger() {
  /*
   * Wait for finger to be placed on sensor
   * Shows status on display
   */
  
  debugPrintln("Waiting for finger...");
  displayMessage("Place finger", "on sensor");
  
  // TODO: Poll sensor until finger detected
}

/*
 * IMPLEMENTATION NOTES:
 * =====================
 * 
 * Waveshare fingerprint sensors typically use UART protocol.
 * Download the official SDK/library from:
 * https://www.waveshare.com/wiki/Round_Capacitive_Fingerprint_Sensor
 * 
 * Key commands to implement:
 * - Initialize sensor
 * - Enroll fingerprint (multi-step process)
 * - Verify fingerprint (1:N matching)
 * - Delete fingerprint
 * - Get template count
 * 
 * The sensor stores templates internally and returns match IDs.
 * No fingerprint data is sent to ESP32 (secure by design).
 */

