/*
 * Secure Element Module - Header
 * Handles ATECC608A cryptographic operations for FIDO2
 */

#ifndef SECURE_H
#define SECURE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================
// ATECC608A SLOT ALLOCATION
// ============================================
// The ATECC608A has 16 data slots (0-15).
// Slots configured for ECC P-256 key pairs:
//   Slot 0:  Device attestation key (generate once, keep forever)
//   Slot 1-14: FIDO2 passkey storage (14 slots for user credentials)
//   Slot 15: Reserved for future use
//
// IMPORTANT: After lockConfig(), slot configurations are permanent.
// However, key DATA can still be regenerated (new key pair) in any
// ECC-configured slot — so users CAN replace passkeys.
// Only individual slot locks (e.g. lockDataSlot0()) are truly permanent.

#define ATECC_SLOT_ATTESTATION  0
#define ATECC_SLOT_FIRST_KEY    1
#define ATECC_SLOT_LAST_KEY     14
#define ATECC_SLOT_RESERVED     15
#define ATECC_MAX_KEY_SLOTS     (ATECC_SLOT_LAST_KEY - ATECC_SLOT_FIRST_KEY + 1)  // 14

#define ATECC_PUBLIC_KEY_SIZE   64
#define ATECC_SIGNATURE_SIZE    64
#define ATECC_SHA256_SIZE       32

// ============================================
// INITIALIZATION
// ============================================
void initSecureElement();
bool isDevicePresent();

// ============================================
// DEVICE PROVISIONING (one-time setup)
// ============================================
bool configureDevice();       // Write slot configs for ECC key pairs
bool lockConfiguration();     // Lock config zone (IRREVERSIBLE!)
bool lockDataZone();          // Lock data/OTP zone (IRREVERSIBLE!)

// ============================================
// KEY MANAGEMENT
// ============================================
bool generateKeyPair(uint8_t slot);
bool getPublicKey(uint8_t slot, uint8_t* publicKey);

// ============================================
// CRYPTOGRAPHIC OPERATIONS
// ============================================
bool signChallenge(uint8_t slot, uint8_t* challenge, size_t challengeLen, uint8_t* signature);
bool computeSHA256(uint8_t* data, size_t len, uint8_t* hash);
bool verifySignatureOnDevice(uint8_t* message, uint8_t* sig, uint8_t* pubKey);
bool generateRandom(uint8_t* buffer, size_t length);

// ============================================
// LOCK STATUS
// ============================================
bool isConfigLocked();
bool isDataLocked();

// ============================================
// SLOT MANAGEMENT
// ============================================
uint8_t getTotalKeySlots();    // Always returns ATECC_MAX_KEY_SLOTS (14)
uint8_t getUsedSlots();        // Count of slots with keys
uint8_t getAvailableSlots();   // Count of empty slots
bool isSlotUsed(uint8_t slot); // Check if a specific slot has a key
void markSlotUsed(uint8_t slot);    // Mark slot as occupied
void markSlotFree(uint8_t slot);    // Mark slot as free (after key replacement)

// ============================================
// DEBUG / INFO
// ============================================
void printSecureElementInfo();
void printSlotStatus();

#endif // SECURE_H
