/*
 * cred_store.cpp — NVS-backed FIDO2 credential storage implementation
 *
 * Uses ESP32 Preferences library (NVS namespace "fido2creds").
 * Keys per credential: c{idx}_id, c{idx}_rph, c{idx}_prv,
 *                      c{idx}_px, c{idx}_py, c{idx}_cnt
 * Global key: "cnt" (credential count, uint8_t)
 */
#include <Arduino.h>
#include <Preferences.h>
#include "cred_store.h"

static Cred      _creds[MAX_CREDS];
       uint8_t   cred_count = 0;
static Preferences _prefs;

void cred_load() {
  _prefs.begin("fido2creds", false);
  cred_count = _prefs.getUChar("cnt", 0);
  if (cred_count > MAX_CREDS) cred_count = 0;

  for (uint8_t i = 0; i < cred_count; i++) {
    char k[14];
    snprintf(k,sizeof(k),"c%d_id",  i); _prefs.getBytes(k,_creds[i].id,       CRED_ID_LEN);
    snprintf(k,sizeof(k),"c%d_rph", i); _prefs.getBytes(k,_creds[i].rpIdHash,  32);
    snprintf(k,sizeof(k),"c%d_prv", i); _prefs.getBytes(k,_creds[i].privKey,   32);
    snprintf(k,sizeof(k),"c%d_px",  i); _prefs.getBytes(k,_creds[i].pubX,      32);
    snprintf(k,sizeof(k),"c%d_py",  i); _prefs.getBytes(k,_creds[i].pubY,      32);
    snprintf(k,sizeof(k),"c%d_cnt", i); _creds[i].signCount = _prefs.getUInt(k, 0);
    _creds[i].valid = true;
  }
  Serial.printf("[CRED] %d credential(s) loaded from NVS\n", cred_count);
}

Cred* cred_find_by_rpidhash(const uint8_t rpIdHash[32]) {
  for (uint8_t i = 0; i < cred_count; i++)
    if (_creds[i].valid && memcmp(_creds[i].rpIdHash, rpIdHash, 32) == 0)
      return &_creds[i];
  return nullptr;
}

Cred* cred_find_by_id(const uint8_t credId[CRED_ID_LEN]) {
  for (uint8_t i = 0; i < cred_count; i++)
    if (_creds[i].valid && memcmp(_creds[i].id, credId, CRED_ID_LEN) == 0)
      return &_creds[i];
  return nullptr;
}

Cred* cred_alloc(const uint8_t rpIdHash[32]) {
  Cred* ex = cred_find_by_rpidhash(rpIdHash);
  if (ex) return ex;
  if (cred_count >= MAX_CREDS) return nullptr;
  memset(&_creds[cred_count], 0, sizeof(Cred));
  return &_creds[cred_count];
}

bool cred_save(Cred* c) {
  int idx = (int)(c - _creds);
  if (idx < 0 || idx >= MAX_CREDS) return false;
  if (idx == cred_count) cred_count++;

  char k[14];
  snprintf(k,sizeof(k),"c%d_id",  idx); _prefs.putBytes(k,c->id,       CRED_ID_LEN);
  snprintf(k,sizeof(k),"c%d_rph", idx); _prefs.putBytes(k,c->rpIdHash,  32);
  snprintf(k,sizeof(k),"c%d_prv", idx); _prefs.putBytes(k,c->privKey,   32);
  snprintf(k,sizeof(k),"c%d_px",  idx); _prefs.putBytes(k,c->pubX,      32);
  snprintf(k,sizeof(k),"c%d_py",  idx); _prefs.putBytes(k,c->pubY,      32);
  snprintf(k,sizeof(k),"c%d_cnt", idx); _prefs.putUInt(k, c->signCount);
  _prefs.putUChar("cnt", cred_count);
  return true;
}

void cred_update_counter(Cred* c) {
  int idx = (int)(c - _creds);
  if (idx < 0 || idx >= MAX_CREDS) return;
  char k[14];
  snprintf(k,sizeof(k),"c%d_cnt", idx);
  _prefs.putUInt(k, c->signCount);
}
