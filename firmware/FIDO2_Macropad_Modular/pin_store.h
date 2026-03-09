/*
 * pin_store.h — FIDO2 ClientPIN storage
 * Stores PIN hash in NVS. PIN is never stored in plaintext.
 */

#ifndef PIN_STORE_H
#define PIN_STORE_H

#include <stdint.h>
#include <stdbool.h>

#define PIN_HASH_LEN 16   // LEFT(SHA-256(pin), 16)
#define PIN_MAX_RETRIES 8

// Check if PIN has been set
bool pinIsSet(void);

// Get/set PIN retries remaining (0 = locked)
uint8_t pinGetRetries(void);
void pinSetRetries(uint8_t retries);
void pinDecrementRetries(void);
void pinResetRetries(void);

// Store PIN hash (16 bytes). Call after successful setPIN/changePIN.
bool pinStoreHash(const uint8_t hash[PIN_HASH_LEN]);

// Verify PIN: compare hash of provided PIN against stored hash.
// Returns true if match.
bool pinVerifyHash(const uint8_t hash[PIN_HASH_LEN]);

// Clear PIN (e.g. on factory reset)
void pinClear(void);

#endif // PIN_STORE_H
