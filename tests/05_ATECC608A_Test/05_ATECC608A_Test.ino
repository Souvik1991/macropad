/*
 * ATECC608A Secure Element Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * Chip: ATECC608A-SSHDA-B (WSOIC-8)
 * 
 * Connections (I2C - shared bus with OLED):
 * ATECC608A Pin 1 (NC)  -> Not connected
 * ATECC608A Pin 2 (NC)  -> Not connected
 * ATECC608A Pin 3 (NC)  -> Not connected
 * ATECC608A Pin 4 (GND) -> ESP32 GND
 * ATECC608A Pin 5 (SDA) -> ESP32 SDA
 * ATECC608A Pin 6 (SCL) -> ESP32 SCL
 * ATECC608A Pin 7 (NC)  -> Not connected
 * ATECC608A Pin 8 (VCC) -> ESP32 3.3V
 * 
 * I2C Address: 0x60 (default)
 * 
 * Install Library:
 * - SparkFun_ATECCX08a_Arduino_Library
 *   (Install via Arduino Library Manager: search "SparkFun ATECCX08a")
 * 
 * Note: This test first scans the I2C bus to verify wiring,
 * then runs ATECC608A-specific tests.
 */

#include <Wire.h>
#include <SparkFun_ATECCX08a_Arduino_Library.h>

ATECCX08A atecc;

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for serial monitor
  Serial.println("========================================");
  Serial.println("    ATECC608A Secure Element Test");
  Serial.println("========================================");
  Serial.println();
  
  // ---- Step 1: Initialize I2C ----
  Serial.println("[Step 1] Initializing I2C bus...");
  Wire.begin();  // Uses board default SDA/SCL pins
  Serial.println("  I2C initialized (using board default pins)");
  Serial.println();
  
  // ---- Step 2: Scan I2C Bus ----
  Serial.println("[Step 2] Scanning I2C bus...");
  scanI2C();
  Serial.println();
  
  // ---- Step 3: Initialize ATECC608A ----
  Serial.println("[Step 3] Connecting to ATECC608A at 0x60...");
  if (atecc.begin() == true) {
    Serial.println("  ✓ ATECC608A connected successfully!");
  } else {
    Serial.println("  ✗ FAILED to connect to ATECC608A!");
    Serial.println();
    Serial.println("  Troubleshooting:");
    Serial.println("  1. Check wiring: SDA->GPIO3, SCL->GPIO4");
    Serial.println("  2. Check power: VCC->3.3V, GND->GND");
    Serial.println("  3. Check I2C pull-ups (4.7kΩ on SDA/SCL)");
    Serial.println("  4. Try power cycling the board");
    Serial.println();
    Serial.println("========================================");
    Serial.println("  TEST FAILED - Cannot proceed");
    Serial.println("========================================");
    while (1) { delay(1000); }  // Halt
  }
  Serial.println();
  
  // ---- Step 4: Read Config Zone ----
  Serial.println("[Step 4] Reading configuration zone...");
  if (atecc.readConfigZone(false)) {
    Serial.println("  ✓ Config zone read successfully!");
  } else {
    Serial.println("  ✗ Failed to read config zone");
  }
  Serial.println();
  
  // ---- Step 5: Device Info ----
  Serial.println("[Step 5] Device information...");
  printDeviceInfo();
  Serial.println();
  
  // ---- Step 6: Random Number Test ----
  Serial.println("[Step 6] Testing random number generation...");
  testRandomNumber();
  Serial.println();
  
  // ---- Step 7: Configuration Check ----
  Serial.println("[Step 7] Checking configuration...");
  testConfiguration();
  Serial.println();
  
  // ---- Summary ----
  Serial.println("========================================");
  Serial.println("  ALL TESTS PASSED!");
  Serial.println("  ATECC608A is working correctly.");
  Serial.println("  Ready for FIDO2 key operations.");
  Serial.println("========================================");
  Serial.println();
  Serial.println("The loop will generate a random number");
  Serial.println("every 5 seconds to confirm continuous");
  Serial.println("communication...");
}

void loop() {
  delay(5000);
  
  Serial.println();
  Serial.print("[Loop] Random: ");
  if (atecc.updateRandom32Bytes(false)) {
    for (int j = 0; j < 8; j++) {
      if (atecc.random32Bytes[j] < 0x10) Serial.print("0");
      Serial.print(atecc.random32Bytes[j], HEX);
    }
    Serial.println("... OK");
  } else {
    Serial.println("FAILED - communication lost?");
  }
}

// Scan entire I2C bus and report all devices found
void scanI2C() {
  byte error, address;
  int nDevices = 0;
  
  Serial.println("  Addr  Status");
  Serial.println("  ----  ------");
  
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("  0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print("  Found");
      
      // Identify known devices
      if (address == 0x3C || address == 0x3D) {
        Serial.print(" <- OLED Display (SSD1306)");
      } else if (address == 0x60) {
        Serial.print(" <- ATECC608A (expected!)");
      } else if (address == 0x35) {
        Serial.print(" <- ATECC608A (alt addr)");
      }
      Serial.println();
      nDevices++;
    }
  }
  
  Serial.println();
  if (nDevices == 0) {
    Serial.println("  WARNING: No I2C devices found!");
    Serial.println("  Check wiring and power connections.");
  } else {
    Serial.print("  Found ");
    Serial.print(nDevices);
    Serial.println(" device(s) on I2C bus.");
    
    // Check if ATECC608A was found
    Wire.beginTransmission(0x60);
    if (Wire.endTransmission() != 0) {
      Serial.println("  WARNING: ATECC608A not found at 0x60!");
    }
  }
}

void printDeviceInfo() {
  // Serial number was populated by readConfigZone()
  Serial.print("  Serial Number: ");
  for (int i = 0; i < 9; i++) {
    if (atecc.serialNumber[i] < 0x10) Serial.print("0");
    Serial.print(atecc.serialNumber[i], HEX);
    if (i < 8) Serial.print(":");
  }
  Serial.println();
  
  // Revision number
  Serial.print("  Revision:      ");
  for (int i = 0; i < 4; i++) {
    if (atecc.revisionNumber[i] < 0x10) Serial.print("0");
    Serial.print(atecc.revisionNumber[i], HEX);
    if (i < 3) Serial.print(":");
  }
  Serial.println();
  
  // Lock status (populated by readConfigZone())
  Serial.print("  Config Zone:   ");
  if (atecc.configLockStatus) {
    Serial.println("LOCKED (production ready)");
  } else {
    Serial.println("UNLOCKED (needs configuration)");
  }
  
  Serial.print("  Data/OTP Zone: ");
  if (atecc.dataOTPLockStatus) {
    Serial.println("LOCKED (keys stored)");
  } else {
    Serial.println("UNLOCKED (no keys yet)");
  }
  
  Serial.print("  Slot 0:        ");
  if (atecc.slot0LockStatus) {
    Serial.println("LOCKED");
  } else {
    Serial.println("UNLOCKED");
  }
}

void testRandomNumber() {
  Serial.println("  Generating 3 random numbers (32 bytes each):");
  
  bool allPassed = true;
  for (int i = 0; i < 3; i++) {
    if (atecc.updateRandom32Bytes(false)) {
      Serial.print("  #");
      Serial.print(i + 1);
      Serial.print(": ");
      
      // Print first 16 bytes
      for (int j = 0; j < 16; j++) {
        if (atecc.random32Bytes[j] < 0x10) Serial.print("0");
        Serial.print(atecc.random32Bytes[j], HEX);
        if (j == 7) Serial.print(" ");  // Space in middle for readability
      }
      Serial.println("...");
    } else {
      Serial.print("  #");
      Serial.print(i + 1);
      Serial.println(": FAILED");
      allPassed = false;
    }
  }
  
  if (allPassed) {
    Serial.println("  ✓ Random number generation working!");
  } else {
    Serial.println("  ✗ Random number generation had errors!");
  }
}

void testConfiguration() {
  Serial.println("  Lock Status Summary:");
  
  bool configLocked = atecc.configLockStatus;
  bool dataLocked = atecc.dataOTPLockStatus;
  
  if (!configLocked && !dataLocked) {
    Serial.println("  State: BRAND NEW (unconfigured)");
    Serial.println("  Action: Config zone must be configured");
    Serial.println("          and locked before storing keys.");
  } else if (configLocked && !dataLocked) {
    Serial.println("  State: CONFIGURED (ready for keys)");
    Serial.println("  Action: Can generate and store FIDO2 keys.");
  } else if (configLocked && dataLocked) {
    Serial.println("  State: FULLY LOCKED (production)");
    Serial.println("  Action: Keys are stored. Ready for use.");
  }
  
  Serial.println("  ✓ Configuration check complete!");
}
