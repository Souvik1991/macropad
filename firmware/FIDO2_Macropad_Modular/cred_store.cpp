/*
 * cred_store.cpp — ATECC608A-backed FIDO2 credential storage
 *
 * Stores credential METADATA in ESP32 NVS (Preferences library).
 * Private keys are generated and stored inside ATECC608A slots (1–14).
 *
 * NVS namespace: "fido2_creds"
 * Keys: "cred0" .. "cred13" — each stores a serialised Cred struct.
 */

#include "cred_store.h"
#include "secure.h"
#include "debug.h"
#include <Preferences.h>
#include <string.h>
#include <esp_random.h>

static Preferences prefs;
static Cred creds[MAX_CREDS];
uint8_t cred_count = 0;

// ─── Serialization helpers ────────────────────────────────────────────────
// Pack a Cred into a flat byte buffer for NVS storage.
// Format: [valid:1][slot:1][id:16][rpIdHash:32][signCount:4] = 54 bytes

#define CRED_BLOB_SIZE 54

static void cred_serialize(const Cred* c, uint8_t* blob) {
  blob[0] = c->valid ? 1 : 0;
  blob[1] = c->slot;
  memcpy(blob + 2, c->id, CRED_ID_LEN);
  memcpy(blob + 18, c->rpIdHash, 32);
  blob[50] = (c->signCount >> 24) & 0xFF;
  blob[51] = (c->signCount >> 16) & 0xFF;
  blob[52] = (c->signCount >>  8) & 0xFF;
  blob[53] =  c->signCount        & 0xFF;
}

static void cred_deserialize(Cred* c, const uint8_t* blob) {
  c->valid     = (blob[0] != 0);
  c->slot      = blob[1];
  memcpy(c->id, blob + 2, CRED_ID_LEN);
  memcpy(c->rpIdHash, blob + 18, 32);
  c->signCount = ((uint32_t)blob[50] << 24) | ((uint32_t)blob[51] << 16)
               | ((uint32_t)blob[52] <<  8) |  (uint32_t)blob[53];
}

// ─── Public API ───────────────────────────────────────────────────────────

void cred_load() {
  prefs.begin("fido2_creds", true);  // read-only

  cred_count = 0;
  for (int i = 0; i < MAX_CREDS; i++) {
    char key[8];
    snprintf(key, sizeof(key), "cred%d", i);

    uint8_t blob[CRED_BLOB_SIZE];
    size_t len = prefs.getBytes(key, blob, CRED_BLOB_SIZE);
    if (len == CRED_BLOB_SIZE) {
      cred_deserialize(&creds[i], blob);
      if (creds[i].valid) {
        cred_count++;
        // Sync slot tracking with secure element
        markSlotUsed(creds[i].slot);
      }
    } else {
      memset(&creds[i], 0, sizeof(Cred));
    }
  }

  prefs.end();
  debugPrint("  Loaded credentials: ");
  debugPrintln(cred_count);
}

Cred* cred_find_by_rpidhash(const uint8_t rpIdHash[32]) {
  for (int i = 0; i < MAX_CREDS; i++) {
    if (creds[i].valid && memcmp(creds[i].rpIdHash, rpIdHash, 32) == 0) {
      return &creds[i];
    }
  }
  return nullptr;
}

Cred* cred_find_by_id(const uint8_t credId[CRED_ID_LEN]) {
  for (int i = 0; i < MAX_CREDS; i++) {
    if (creds[i].valid && memcmp(creds[i].id, credId, CRED_ID_LEN) == 0) {
      return &creds[i];
    }
  }
  return nullptr;
}

Cred* cred_alloc(const uint8_t rpIdHash[32]) {
  // First check if we already have a credential for this RP
  Cred* existing = cred_find_by_rpidhash(rpIdHash);
  if (existing) return existing;

  // Find first available ATECC608A slot + cred array entry
  for (int i = 0; i < MAX_CREDS; i++) {
    if (!creds[i].valid) {
      uint8_t slot = i + 1;  // ATECC608A slots 1–14
      if (!isSlotUsed(slot)) {
        creds[i].slot = slot;
        cred_count++;
        return &creds[i];
      }
    }
  }

  return nullptr;  // All slots full
}

bool cred_save(Cred* c) {
  int idx = (int)(c - creds);
  if (idx < 0 || idx >= MAX_CREDS) return false;

  prefs.begin("fido2_creds", false);  // read-write
  char key[8];
  snprintf(key, sizeof(key), "cred%d", idx);

  uint8_t blob[CRED_BLOB_SIZE];
  cred_serialize(c, blob);
  size_t written = prefs.putBytes(key, blob, CRED_BLOB_SIZE);
  prefs.end();

  return (written == CRED_BLOB_SIZE);
}

void cred_update_counter(Cred* c) {
  cred_save(c);  // Just re-save the whole blob (54 bytes, fast enough)
}

void cred_clear() {
  // Mark all passkey slots (1-14) as free in secure element tracking
  if (isDevicePresent()) {
    for (uint8_t slot = 1; slot <= MAX_CREDS; slot++) {
      markSlotFree(slot);
    }
  }

  prefs.begin("fido2_creds", false);
  prefs.clear();
  prefs.end();

  memset(creds, 0, sizeof(creds));
  cred_count = 0;

  debugPrintln("  All FIDO2 credentials cleared");
}
