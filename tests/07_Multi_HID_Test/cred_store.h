/*
 * cred_store.h — NVS-backed FIDO2 credential storage API
 *
 * Stores up to MAX_CREDS P-256 credentials in ESP32 NVS (Preferences).
 * Implementation: cred_store.cpp
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_CREDS   8
#define CRED_ID_LEN 16

struct Cred {
  bool     valid;
  uint8_t  id[CRED_ID_LEN];  // random credential ID returned to host
  uint8_t  rpIdHash[32];      // SHA-256(rpId)
  uint8_t  privKey[32];       // P-256 private scalar
  uint8_t  pubX[32];          // P-256 public key X
  uint8_t  pubY[32];          // P-256 public key Y
  uint32_t signCount;
};

extern uint8_t cred_count;   // current number of stored credentials

// Load all credentials from NVS — call once in setup().
void cred_load();

// Find a credential by RP ID hash. Returns nullptr if not found.
Cred* cred_find_by_rpidhash(const uint8_t rpIdHash[32]);

// Find a credential by credential ID. Returns nullptr if not found.
Cred* cred_find_by_id(const uint8_t credId[CRED_ID_LEN]);

// Allocate a slot for a new credential (or return existing slot for same RP).
// Returns nullptr if the store is full.
Cred* cred_alloc(const uint8_t rpIdHash[32]);

// Persist a credential (and any changes) to NVS.
bool cred_save(Cred* c);

// Update sign counter only — faster than a full cred_save().
void cred_update_counter(Cred* c);
