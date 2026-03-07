/*
 * Secure Element Module - Implementation
 * Handles ATECC608A cryptographic operations for FIDO2
 *
 * Uses SparkFun ATECCX08A library for all hardware interactions.
 * Slot allocation: 0=attestation, 1-14=passkeys, 15=reserved.
 */

#include "config.h"
#include "debug.h"
#include "secure.h"
#include <string.h>

// ============================================
// PRIVATE STATE
// ============================================

ATECCX08A atecc;
static bool ateccPresent = false;

// Track which passkey slots are in use (slots 1-14)
// Index 0 = slot 1, index 13 = slot 14
static bool slotInUse[ATECC_MAX_KEY_SLOTS] = { false };

// ============================================
// INITIALIZATION
// ============================================

void initSecureElement() {
  debugPrint("Initializing secure element... ");

  if (atecc.begin() == true) {
    ateccPresent = true;
    debugPrintln("OK");

    // Read config zone to populate serial number, lock statuses
    atecc.readConfigZone(false);
    printSecureElementInfo();
    printSlotStatus();
  } else {
    ateccPresent = false;
    debugPrintln("WARNING - ATECC608A not found");
    debugPrintln("  -> FIDO2 functionality will be limited");
    debugPrintln("  -> Continuing without secure element...");
  }
}

bool isDevicePresent() {
  return ateccPresent;
}

// ============================================
// DEVICE PROVISIONING (one-time setup)
// ============================================

bool configureDevice() {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (isConfigLocked()) {
    debugPrintln("Config zone already locked - cannot reconfigure");
    return false;
  }

  debugPrintln("Configuring ATECC608A for FIDO2...");

  // writeConfigSparkFun() sets slots 0 and 1 for ECC P-256:
  //   KeyConfig: 0x3300 (Lockable, ECC, PubInfo, Private)
  //   SlotConfig: 0x8320 (ExtSig, IntSig, IsSecret, WriteNever)
  if (!atecc.writeConfigSparkFun()) {
    debugPrintln("ERROR: Failed to write config for slots 0-1");
    return false;
  }
  debugPrintln("  Slots 0-1 configured (SparkFun defaults)");

  // Configure slots 2-14 with same ECC key pair settings
  // KeyConfig: 0x3300 for each pair of slots (written 4 bytes = 2 slots at a time)
  // SlotConfig: 0x8320 for each pair of slots
  uint8_t keyConfigData[] = {0x33, 0x00, 0x33, 0x00};
  uint8_t slotConfigData[] = {0x83, 0x20, 0x83, 0x20};

  // KeyConfig zone starts at offset 96, each slot uses 2 bytes
  // SlotConfig zone starts at offset 20, each slot uses 2 bytes
  // We write 4 bytes at a time (2 slots), addresses are offset/4

  for (int slot = 2; slot <= 14; slot += 2) {
    // KeyConfig address: (96 + slot*2) / 4
    uint16_t kcAddr = (96 + slot * 2) / 4;
    if (!atecc.write(ZONE_CONFIG, kcAddr, keyConfigData, 4)) {
      debugPrint("ERROR: Failed to write KeyConfig for slots ");
      debugPrint(slot);
      debugPrint("-");
      debugPrintln(slot + 1);
      return false;
    }

    // SlotConfig address: (20 + slot*2) / 4
    uint16_t scAddr = (20 + slot * 2) / 4;
    if (!atecc.write(ZONE_CONFIG, scAddr, slotConfigData, 4)) {
      debugPrint("ERROR: Failed to write SlotConfig for slots ");
      debugPrint(slot);
      debugPrint("-");
      debugPrintln(slot + 1);
      return false;
    }

    debugPrint("  Slots ");
    debugPrint(slot);
    debugPrint("-");
    debugPrint(slot + 1);
    debugPrintln(" configured");
  }

  debugPrintln("Device configuration complete!");
  debugPrintln("Next step: Lock config zone with lockConfiguration()");
  return true;
}

bool lockConfiguration() {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (isConfigLocked()) {
    debugPrintln("Config zone already locked");
    return true;
  }

  debugPrintln("========================================");
  debugPrintln("WARNING: Locking configuration zone!");
  debugPrintln("This is a ONE-TIME IRREVERSIBLE operation.");
  debugPrintln("Slot configurations will be PERMANENT.");
  debugPrintln("(Key data can still be regenerated)");
  debugPrintln("========================================");

  if (!atecc.lockConfig()) {
    debugPrintln("ERROR: Failed to lock config zone");
    return false;
  }

  // Re-read config to update lock status
  atecc.readConfigZone(false);

  debugPrintln("Config zone LOCKED successfully");
  return true;
}

bool lockDataZone() {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (!isConfigLocked()) {
    debugPrintln("ERROR: Config zone must be locked first");
    return false;
  }

  if (isDataLocked()) {
    debugPrintln("Data zone already locked");
    return true;
  }

  debugPrintln("========================================");
  debugPrintln("WARNING: Locking Data/OTP zone!");
  debugPrintln("This is a ONE-TIME IRREVERSIBLE operation.");
  debugPrintln("========================================");

  if (!atecc.lockDataAndOTP()) {
    debugPrintln("ERROR: Failed to lock data zone");
    return false;
  }

  // Re-read config to update lock status
  atecc.readConfigZone(false);

  debugPrintln("Data/OTP zone LOCKED successfully");
  return true;
}

// ============================================
// KEY MANAGEMENT
// ============================================

bool generateKeyPair(uint8_t slot) {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (slot > ATECC_SLOT_LAST_KEY) {
    debugPrint("ERROR: Invalid slot ");
    debugPrintln(slot);
    return false;
  }

  if (!isConfigLocked()) {
    debugPrintln("ERROR: Config zone must be locked before key generation");
    return false;
  }

  debugPrint("Generating ECC P-256 key pair in slot ");
  debugPrint(slot);
  debugPrintln("...");

  if (!atecc.createNewKeyPair(slot)) {
    debugPrintln("ERROR: Key pair generation failed!");
    return false;
  }

  // Mark slot as in use (for passkey slots 1-14)
  if (slot >= ATECC_SLOT_FIRST_KEY && slot <= ATECC_SLOT_LAST_KEY) {
    markSlotUsed(slot);
  }

  debugPrintln("Key pair generated successfully");
  debugPrint("  Public key (first 8 bytes): ");
  for (int i = 0; i < 8; i++) {
    if (atecc.publicKey64Bytes[i] < 0x10) debugPrint("0");
    debugPrint((int)atecc.publicKey64Bytes[i], HEX);
  }
  debugPrintln("...");

  return true;
}

bool getPublicKey(uint8_t slot, uint8_t* publicKey) {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (slot > ATECC_SLOT_LAST_KEY) {
    debugPrint("ERROR: Invalid slot ");
    debugPrintln(slot);
    return false;
  }

  debugPrint("Reading public key from slot ");
  debugPrint(slot);
  debugPrintln("...");

  // generatePublicKey computes the public key from the stored private key
  if (!atecc.generatePublicKey(slot, false)) {
    debugPrintln("ERROR: Failed to read public key (no key in slot?)");
    return false;
  }

  // Copy from library's buffer to caller's buffer
  memcpy(publicKey, atecc.publicKey64Bytes, ATECC_PUBLIC_KEY_SIZE);

  debugPrintln("Public key retrieved");
  return true;
}

// ============================================
// CRYPTOGRAPHIC OPERATIONS
// ============================================

bool signChallenge(uint8_t slot, uint8_t* challenge, size_t challengeLen, uint8_t* signature) {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (slot > ATECC_SLOT_LAST_KEY) {
    debugPrint("ERROR: Invalid slot ");
    debugPrintln(slot);
    return false;
  }

  debugPrint("Signing challenge with slot ");
  debugPrint(slot);
  debugPrintln("...");

  uint8_t hash[ATECC_SHA256_SIZE];

  if (challengeLen == ATECC_SHA256_SIZE) {
    // Challenge is already 32 bytes (pre-hashed), use directly
    memcpy(hash, challenge, ATECC_SHA256_SIZE);
  } else {
    // Need to hash the challenge first using ATECC's hardware SHA-256
    debugPrintln("  Hashing challenge with SHA-256...");
    if (!atecc.sha256(challenge, challengeLen, hash)) {
      debugPrintln("ERROR: SHA-256 hashing failed!");
      return false;
    }
  }

  // createSignature: loads hash into TempKey then signs with private key in slot
  if (!atecc.createSignature(hash, slot)) {
    debugPrintln("ERROR: Signature creation failed!");
    return false;
  }

  // Copy signature from library's buffer
  memcpy(signature, atecc.signature, ATECC_SIGNATURE_SIZE);

  debugPrintln("Signature created successfully");
  debugPrint("  Signature (first 8 bytes): ");
  for (int i = 0; i < 8; i++) {
    if (signature[i] < 0x10) debugPrint("0");
    debugPrint((int)signature[i], HEX);
  }
  debugPrintln("...");

  return true;
}

bool computeSHA256(uint8_t* data, size_t len, uint8_t* hash) {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  if (!atecc.sha256(data, len, hash)) {
    debugPrintln("ERROR: SHA-256 computation failed");
    return false;
  }

  return true;
}

bool verifySignatureOnDevice(uint8_t* message, uint8_t* sig, uint8_t* pubKey) {
  if (!ateccPresent) {
    debugPrintln("ERROR: ATECC608A not present");
    return false;
  }

  debugPrintln("Verifying signature on device...");

  // verifySignature expects 32-byte message (hash), 64-byte signature, 64-byte public key
  if (!atecc.verifySignature(message, sig, pubKey)) {
    debugPrintln("Signature verification FAILED");
    return false;
  }

  debugPrintln("Signature verification PASSED");
  return true;
}

bool generateRandom(uint8_t* buffer, size_t length) {
  if (!ateccPresent) {
    return false;
  }

  if (length > 32) {
    return false;
  }

  if (atecc.updateRandom32Bytes(false)) {
    memcpy(buffer, atecc.random32Bytes, length);
    return true;
  }

  return false;
}

// ============================================
// LOCK STATUS
// ============================================

bool isConfigLocked() {
  return ateccPresent && atecc.configLockStatus;
}

bool isDataLocked() {
  return ateccPresent && atecc.dataOTPLockStatus;
}

// ============================================
// SLOT MANAGEMENT
// ============================================

uint8_t getTotalKeySlots() {
  return ATECC_MAX_KEY_SLOTS;
}

uint8_t getUsedSlots() {
  uint8_t count = 0;
  for (int i = 0; i < ATECC_MAX_KEY_SLOTS; i++) {
    if (slotInUse[i]) count++;
  }
  return count;
}

uint8_t getAvailableSlots() {
  return getTotalKeySlots() - getUsedSlots();
}

bool isSlotUsed(uint8_t slot) {
  if (slot < ATECC_SLOT_FIRST_KEY || slot > ATECC_SLOT_LAST_KEY) {
    return false;
  }
  return slotInUse[slot - ATECC_SLOT_FIRST_KEY];
}

void markSlotUsed(uint8_t slot) {
  if (slot >= ATECC_SLOT_FIRST_KEY && slot <= ATECC_SLOT_LAST_KEY) {
    slotInUse[slot - ATECC_SLOT_FIRST_KEY] = true;
  }
}

void markSlotFree(uint8_t slot) {
  if (slot >= ATECC_SLOT_FIRST_KEY && slot <= ATECC_SLOT_LAST_KEY) {
    slotInUse[slot - ATECC_SLOT_FIRST_KEY] = false;
  }
}

// ============================================
// DEBUG / INFO
// ============================================

void printSecureElementInfo() {
  debugPrintln("  Secure Element Info:");

  debugPrint("  Serial: ");
  for (int i = 0; i < 9; i++) {
    if (atecc.serialNumber[i] < 0x10) debugPrint("0");
    debugPrint((int)atecc.serialNumber[i], HEX);
    if (i < 8) debugPrint(":");
  }
  debugPrintln();

  debugPrint("  Revision: ");
  for (int i = 0; i < 4; i++) {
    if (atecc.revisionNumber[i] < 0x10) debugPrint("0");
    debugPrint((int)atecc.revisionNumber[i], HEX);
    if (i < 3) debugPrint(":");
  }
  debugPrintln();

  debugPrint("  Config Zone: ");
  debugPrintln(isConfigLocked() ? "LOCKED" : "UNLOCKED");

  debugPrint("  Data Zone:   ");
  debugPrintln(isDataLocked() ? "LOCKED" : "UNLOCKED");
}

void printSlotStatus() {
  debugPrint("  Passkey Slots: ");
  debugPrint(getUsedSlots());
  debugPrint("/");
  debugPrint(getTotalKeySlots());
  debugPrint(" used, ");
  debugPrint(getAvailableSlots());
  debugPrintln(" available");
}
