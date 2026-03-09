/*
 * pin_store.cpp — FIDO2 ClientPIN storage in NVS
 */

#include "pin_store.h"
#include "debug.h"
#include <Preferences.h>
#include <string.h>

static Preferences prefs;
static uint8_t cachedRetries = PIN_MAX_RETRIES;
static bool retriesLoaded = false;

static void loadRetries() {
  if (retriesLoaded) return;
  prefs.begin("fido2_pin", true);
  cachedRetries = prefs.getUChar("retries", PIN_MAX_RETRIES);
  prefs.end();
  retriesLoaded = true;
}

bool pinIsSet(void) {
  prefs.begin("fido2_pin", true);
  size_t len = prefs.getBytesLength("hash");
  prefs.end();
  return (len == PIN_HASH_LEN);
}

uint8_t pinGetRetries(void) {
  loadRetries();
  return cachedRetries;
}

void pinSetRetries(uint8_t retries) {
  cachedRetries = retries;
  prefs.begin("fido2_pin", false);
  prefs.putUChar("retries", retries);
  prefs.end();
}

void pinDecrementRetries(void) {
  loadRetries();
  if (cachedRetries > 0) cachedRetries--;
  prefs.begin("fido2_pin", false);
  prefs.putUChar("retries", cachedRetries);
  prefs.end();
}

void pinResetRetries(void) {
  pinSetRetries(PIN_MAX_RETRIES);
}

bool pinStoreHash(const uint8_t hash[PIN_HASH_LEN]) {
  prefs.begin("fido2_pin", false);
  prefs.putBytes("hash", hash, PIN_HASH_LEN);
  prefs.putUChar("retries", PIN_MAX_RETRIES);
  prefs.end();
  cachedRetries = PIN_MAX_RETRIES;
  return true;
}

bool pinVerifyHash(const uint8_t hash[PIN_HASH_LEN]) {
  uint8_t stored[PIN_HASH_LEN];
  prefs.begin("fido2_pin", true);
  size_t len = prefs.getBytes("hash", stored, PIN_HASH_LEN);
  prefs.end();
  if (len != PIN_HASH_LEN) return false;
  return (memcmp(stored, hash, PIN_HASH_LEN) == 0);
}

void pinClear(void) {
  prefs.begin("fido2_pin", false);
  prefs.clear();
  prefs.end();
  cachedRetries = PIN_MAX_RETRIES;
  retriesLoaded = true;
}
