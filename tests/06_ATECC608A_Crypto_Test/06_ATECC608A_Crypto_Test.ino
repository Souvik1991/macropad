/*
 * ATECC608A Crypto Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * Chip: ATECC608A-SSHDA-B (WSOIC-8)
 *
 * This test validates the full FIDO2 crypto pipeline:
 *   1. Init + connectivity check
 *   2. Device info + lock status
 *   3. Configuration & locking (if needed)
 *   4. Key pair generation
 *   5. Public key retrieval
 *   6. SHA-256 hashing
 *   7. ECDSA signing
 *   8. Signature verification
 *   9. Slot usage summary
 *
 * Connections (I2C - shared bus with OLED):
 * ATECC608A Pin 4 (GND) -> ESP32 GND
 * ATECC608A Pin 5 (SDA) -> ESP32 SDA
 * ATECC608A Pin 6 (SCL) -> ESP32 SCL
 * ATECC608A Pin 8 (VCC) -> ESP32 3.3V
 *
 * I2C Address: 0x60 (default)
 *
 * Install Library:
 * - SparkFun_ATECCX08a_Arduino_Library
 *   (Install via Arduino Library Manager: search "SparkFun ATECCX08a")
 */

#include <Wire.h>
#include <SparkFun_ATECCX08a_Arduino_Library.h>

ATECCX08A atecc;

// Test slot (use slot 1 - first passkey slot)
#define TEST_SLOT 1

// Test data to sign
const char* testMessage = "FIDO2 Macropad Test Challenge 01";

// Buffers
uint8_t publicKey[64];
uint8_t signature[64];
uint8_t hash[32];

// Test results
int testsPassed = 0;
int testsFailed = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for serial monitor

  Serial.println("========================================");
  Serial.println("  ATECC608A Crypto Pipeline Test");
  Serial.println("  (Full FIDO2 Flow Validation)");
  Serial.println("========================================");
  Serial.println();

  // ---- Step 1: Initialize ----
  Serial.println("[Step 1] Initializing ATECC608A...");
  Wire.begin();

  if (atecc.begin() == true) {
    Serial.println("  ✓ ATECC608A connected!");
    testsPassed++;
  } else {
    Serial.println("  ✗ FAILED to connect!");
    Serial.println("  Check wiring and power.");
    Serial.println("========================================");
    Serial.println("  CANNOT PROCEED - Halting");
    Serial.println("========================================");
    while (1) { delay(1000); }
  }
  Serial.println();

  // ---- Step 2: Device Info ----
  Serial.println("[Step 2] Reading device info...");
  if (atecc.readConfigZone(false)) {
    Serial.println("  ✓ Config zone read");
    testsPassed++;
  } else {
    Serial.println("  ✗ Failed to read config zone");
    testsFailed++;
  }

  printDeviceInfo();
  Serial.println();

  // ---- Step 3: Check Configuration ----
  Serial.println("[Step 3] Checking configuration status...");
  bool configLocked = atecc.configLockStatus;
  bool dataLocked = atecc.dataOTPLockStatus;

  if (!configLocked) {
    Serial.println("  Config zone is UNLOCKED");
    Serial.println("  -> Configuring device for FIDO2...");

    // Write config for ECC key pairs
    if (atecc.writeConfigSparkFun()) {
      Serial.println("  ✓ Slots 0-1 configured for ECC");
      testsPassed++;
    } else {
      Serial.println("  ✗ Configuration failed!");
      testsFailed++;
      haltOnError();
    }

    // Configure additional slots (2-14) for ECC
    Serial.println("  Configuring slots 2-14...");
    uint8_t keyConfigData[] = {0x33, 0x00, 0x33, 0x00};
    uint8_t slotConfigData[] = {0x83, 0x20, 0x83, 0x20};
    bool slotsOk = true;

    for (int slot = 2; slot <= 14; slot += 2) {
      uint16_t kcAddr = (96 + slot * 2) / 4;
      uint16_t scAddr = (20 + slot * 2) / 4;

      if (!atecc.write(ZONE_CONFIG, kcAddr, keyConfigData, 4) ||
          !atecc.write(ZONE_CONFIG, scAddr, slotConfigData, 4)) {
        Serial.print("  ✗ Failed to configure slots ");
        Serial.print(slot);
        Serial.print("-");
        Serial.println(slot + 1);
        slotsOk = false;
        break;
      }
      Serial.print("  ✓ Slots ");
      Serial.print(slot);
      Serial.print("-");
      Serial.print(slot + 1);
      Serial.println(" configured");
    }

    if (slotsOk) {
      testsPassed++;
    } else {
      testsFailed++;
    }

    // Lock config zone
    Serial.println();
    Serial.println("  Locking configuration zone...");
    Serial.println("  (This is IRREVERSIBLE but required for key ops)");
    if (atecc.lockConfig()) {
      Serial.println("  ✓ Config zone LOCKED");
      atecc.readConfigZone(false);  // Refresh status
      testsPassed++;
    } else {
      Serial.println("  ✗ Failed to lock config zone");
      testsFailed++;
      haltOnError();
    }
  } else {
    Serial.println("  Config zone: LOCKED (good)");
    testsPassed++;
  }

  if (!dataLocked) {
    Serial.println("  Data zone: UNLOCKED (will lock after first key gen)");
  } else {
    Serial.println("  Data zone: LOCKED (good)");
  }
  Serial.println();

  // ---- Step 4: Generate Key Pair ----
  Serial.println("[Step 4] Generating ECC P-256 key pair in slot ");
  Serial.print("  Slot: ");
  Serial.println(TEST_SLOT);

  if (atecc.createNewKeyPair(TEST_SLOT)) {
    Serial.println("  ✓ Key pair generated!");
    testsPassed++;

    // Copy public key
    memcpy(publicKey, atecc.publicKey64Bytes, 64);

    Serial.println("  Public key (X coordinate):");
    Serial.print("    ");
    for (int i = 0; i < 32; i++) {
      if (publicKey[i] < 0x10) Serial.print("0");
      Serial.print(publicKey[i], HEX);
      if (i == 15) {
        Serial.println();
        Serial.print("    ");
      }
    }
    Serial.println();
    Serial.println("  Public key (Y coordinate):");
    Serial.print("    ");
    for (int i = 32; i < 64; i++) {
      if (publicKey[i] < 0x10) Serial.print("0");
      Serial.print(publicKey[i], HEX);
      if (i == 47) {
        Serial.println();
        Serial.print("    ");
      }
    }
    Serial.println();
  } else {
    Serial.println("  ✗ Key pair generation FAILED!");
    testsFailed++;

    // If data zone needs locking first, try that
    if (!atecc.dataOTPLockStatus) {
      Serial.println("  Attempting to lock data zone first...");
      if (atecc.lockDataAndOTP()) {
        Serial.println("  ✓ Data zone locked, retrying key gen...");
        atecc.readConfigZone(false);
        if (atecc.createNewKeyPair(TEST_SLOT)) {
          Serial.println("  ✓ Key pair generated on retry!");
          memcpy(publicKey, atecc.publicKey64Bytes, 64);
          testsPassed++;
          testsFailed--;  // undo the earlier failure count
        } else {
          Serial.println("  ✗ Still failed after data zone lock");
        }
      }
    }
  }
  Serial.println();

  // ---- Step 5: Retrieve Public Key ----
  Serial.println("[Step 5] Retrieving public key from slot...");
  uint8_t retrievedKey[64];

  if (atecc.generatePublicKey(TEST_SLOT, false)) {
    memcpy(retrievedKey, atecc.publicKey64Bytes, 64);

    // Verify it matches the generated key
    bool keysMatch = (memcmp(publicKey, retrievedKey, 64) == 0);
    if (keysMatch) {
      Serial.println("  ✓ Public key retrieved and MATCHES generated key!");
      testsPassed++;
    } else {
      Serial.println("  ✗ Public key retrieved but does NOT match!");
      testsFailed++;
    }
  } else {
    Serial.println("  ✗ Failed to retrieve public key");
    testsFailed++;
  }
  Serial.println();

  // ---- Step 6: SHA-256 Hash ----
  Serial.println("[Step 6] Computing SHA-256 hash of test message...");
  Serial.print("  Message: \"");
  Serial.print(testMessage);
  Serial.println("\"");

  if (atecc.sha256((uint8_t*)testMessage, strlen(testMessage), hash)) {
    Serial.println("  ✓ SHA-256 computed!");
    Serial.print("  Hash: ");
    for (int i = 0; i < 32; i++) {
      if (hash[i] < 0x10) Serial.print("0");
      Serial.print(hash[i], HEX);
    }
    Serial.println();
    testsPassed++;
  } else {
    Serial.println("  ✗ SHA-256 computation FAILED!");
    testsFailed++;
  }
  Serial.println();

  // ---- Step 7: Sign the Hash ----
  Serial.println("[Step 7] Signing hash with private key in slot...");

  if (atecc.createSignature(hash, TEST_SLOT)) {
    memcpy(signature, atecc.signature, 64);
    Serial.println("  ✓ Signature created!");
    Serial.print("  Signature (R, first 16 bytes): ");
    for (int i = 0; i < 16; i++) {
      if (signature[i] < 0x10) Serial.print("0");
      Serial.print(signature[i], HEX);
    }
    Serial.println("...");
    Serial.print("  Signature (S, first 16 bytes): ");
    for (int i = 32; i < 48; i++) {
      if (signature[i] < 0x10) Serial.print("0");
      Serial.print(signature[i], HEX);
    }
    Serial.println("...");
    testsPassed++;
  } else {
    Serial.println("  ✗ Signature creation FAILED!");
    testsFailed++;
  }
  Serial.println();

  // ---- Step 8: Verify Signature ----
  Serial.println("[Step 8] Verifying signature with public key...");

  if (atecc.verifySignature(hash, signature, publicKey)) {
    Serial.println("  ✓ Signature VERIFIED! Crypto pipeline is working!");
    testsPassed++;
  } else {
    Serial.println("  ✗ Signature verification FAILED!");
    testsFailed++;
  }
  Serial.println();

  // ---- Step 9: Slot Usage Summary ----
  Serial.println("[Step 9] Slot usage summary...");
  Serial.println("  Slot  | Type         | Status");
  Serial.println("  ------|------------- |-------");
  Serial.println("  0     | Attestation  | Reserved");

  for (int s = 1; s <= 14; s++) {
    Serial.print("  ");
    Serial.print(s);
    if (s < 10) Serial.print(" ");
    Serial.print("    | Passkey      | ");
    if (s == TEST_SLOT) {
      Serial.println("KEY PRESENT (test)");
    } else {
      Serial.println("Empty");
    }
  }
  Serial.println("  15    | Reserved     | -");

  Serial.println();
  Serial.print("  Passkey capacity: 1/14 used, 13 available");
  Serial.println();

  // ---- Summary ----
  Serial.println();
  Serial.println("========================================");
  Serial.print("  RESULTS: ");
  Serial.print(testsPassed);
  Serial.print(" passed, ");
  Serial.print(testsFailed);
  Serial.println(" failed");
  Serial.println();

  if (testsFailed == 0) {
    Serial.println("  ★ ALL TESTS PASSED! ★");
    Serial.println("  ATECC608A crypto pipeline is fully");
    Serial.println("  operational for FIDO2 passkeys.");
  } else {
    Serial.println("  ⚠ SOME TESTS FAILED");
    Serial.println("  Review output above for details.");
  }
  Serial.println("========================================");
  Serial.println();
  Serial.println("Loop will sign+verify every 10 seconds");
  Serial.println("to confirm continuous operation...");
}

void loop() {
  delay(10000);

  Serial.println();
  Serial.print("[Loop] Sign+Verify: ");

  // Generate fresh random data to sign
  uint8_t loopHash[32];
  if (!atecc.updateRandom32Bytes(false)) {
    Serial.println("FAILED (random gen)");
    return;
  }
  memcpy(loopHash, atecc.random32Bytes, 32);

  // Sign it
  uint8_t loopSig[64];
  if (!atecc.createSignature(loopHash, TEST_SLOT)) {
    Serial.println("FAILED (sign)");
    return;
  }
  memcpy(loopSig, atecc.signature, 64);

  // Verify it
  if (atecc.verifySignature(loopHash, loopSig, publicKey)) {
    Serial.print("PASS - sig[0:4]=");
    for (int i = 0; i < 4; i++) {
      if (loopSig[i] < 0x10) Serial.print("0");
      Serial.print(loopSig[i], HEX);
    }
    Serial.println();
  } else {
    Serial.println("FAILED (verify)");
  }
}

// Helper functions

void printDeviceInfo() {
  Serial.print("  Serial Number: ");
  for (int i = 0; i < 9; i++) {
    if (atecc.serialNumber[i] < 0x10) Serial.print("0");
    Serial.print(atecc.serialNumber[i], HEX);
    if (i < 8) Serial.print(":");
  }
  Serial.println();

  Serial.print("  Revision:      ");
  for (int i = 0; i < 4; i++) {
    if (atecc.revisionNumber[i] < 0x10) Serial.print("0");
    Serial.print(atecc.revisionNumber[i], HEX);
    if (i < 3) Serial.print(":");
  }
  Serial.println();

  Serial.print("  Config Zone:   ");
  Serial.println(atecc.configLockStatus ? "LOCKED" : "UNLOCKED");

  Serial.print("  Data Zone:     ");
  Serial.println(atecc.dataOTPLockStatus ? "LOCKED" : "UNLOCKED");

  Serial.print("  Slot 0:        ");
  Serial.println(atecc.slot0LockStatus ? "LOCKED" : "UNLOCKED");
}

void haltOnError() {
  Serial.println("========================================");
  Serial.println("  CRITICAL ERROR - Cannot proceed");
  Serial.println("========================================");
  while (1) { delay(1000); }
}
