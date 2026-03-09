/*
 * cred_store.h — ATECC608A-backed FIDO2 credential storage API
 *
 * Credentials map to ATECC608A ECC slots (1–14).
 * Private keys NEVER leave the secure element.
 * Only metadata (rpIdHash, credId, signCount) is stored in NVS.
 *
 * Implementation: cred_store.cpp
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_CREDS   14     // ATECC608A slots 1–14
#define CRED_ID_LEN 16     // Random credential ID returned to host

struct Cred {
  bool     valid;
  uint8_t  slot;            // ATECC608A slot number (1–14)
  uint8_t  id[CRED_ID_LEN]; // Random credential ID returned to host
  uint8_t  rpIdHash[32];    // SHA-256(rpId)
  uint32_t signCount;
  // NOTE: No private key stored here — keys live in ATECC608A
};

extern uint8_t cred_count;   // Current number of stored credentials

// Load all credential metadata from NVS — call once in setup().
void cred_load();

// Find a credential by RP ID hash. Returns nullptr if not found.
Cred* cred_find_by_rpidhash(const uint8_t rpIdHash[32]);

// Find a credential by credential ID. Returns nullptr if not found.
Cred* cred_find_by_id(const uint8_t credId[CRED_ID_LEN]);

// Allocate a slot for a new credential (or return existing slot for same RP).
// Returns nullptr if the store is full.
Cred* cred_alloc(const uint8_t rpIdHash[32]);

// Persist credential metadata to NVS.
bool cred_save(Cred* c);

// Update sign counter only — faster than a full cred_save().
void cred_update_counter(Cred* c);

// Clear all credentials (NVS + slot tracking). Use for factory reset.
void cred_clear();
