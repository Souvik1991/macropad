/*
 * FIDO2/CTAP2 Module - Implementation
 * Full CTAP2 protocol with ATECC608A hardware crypto + fingerprint auth
 *
 * Ported from tested code in tests/07_Multi_HID_Test/07_Multi_HID_Test.ino
 * Key differences from test:
 *   - Uses ATECC608A for key generation and signing (not mbedtls)
 *   - Uses fingerprint sensor for user verification (not BOOT button)
 *   - OLED display feedback throughout the flow
 *   - 3-retry fingerprint authentication with per-attempt feedback
 */

#include "config.h"
#include "debug.h"
#include "display.h"
#include "fingerprint.h"
#include "fido2.h"
#include "leds.h"
#include "secure.h"
#include "usb_hid.h"
#include "cred_store.h"
#include "cbor_mini.h"
#include "crypto_utils.h"
#include "pin_store.h"
#include "pin_protocol.h"
#include <Arduino.h>
#include <string.h>

// ─── CTAPHID reassembly state ─────────────────────────────────────────────
#define CTAP_MAX_PAYLOAD 1024

static struct {
  uint32_t cid; uint8_t cmd;
  uint16_t bcnt, received;
  uint8_t  buf[CTAP_MAX_PAYLOAD];
  bool     active;
} ctap_asm;

static uint32_t nextCID = 0x00000001;

// Session: skip fingerprint for duplicate GetAssertion (same rpId) within 5 seconds
#define GET_ASSERTION_SESSION_MS  5000
static uint8_t  lastGetAssertionRpIdHash[32] = {0};
static uint32_t lastGetAssertionTime = 0;

// Persist fingerprint attempt across host retries (CANCEL + new GetAssertion)
// so "2/3" doesn't reset to "1/3" when browser cancels and retries
#define FP_AUTH_RETRY_WINDOW_MS  10000
static uint8_t  persistedFpAttempt = 0;
static uint32_t persistedFpAttemptAt = 0;

// ClientPIN: pinUvAuthToken session (16 bytes). Set when getPINToken returns.
#define PIN_TOKEN_VALID_MS  30000
static uint8_t  pinUvAuthToken[16] = {0};
static uint32_t pinTokenSetAt = 0;

// ─── Initialization ──────────────────────────────────────────────────────

void initFIDO2() {
  debugPrintln("Initializing FIDO2 credential store...");
  cred_load();
}

// Reset reassembly state (test does this when draining). Call after drainStalePackets on abort.
static void resetCtapReassembly() {
  ctap_asm.active = false;
}

// ─── CTAPHID command handlers ─────────────────────────────────────────────

static void handleInit(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  debugPrintln("[FIDO2] CTAPHID_INIT");
  if (bcnt < 8) { sendCTAPError(CTAPHID_BROADCAST_CID, CTAP1_ERR_INVALID_LEN); return; }
  uint32_t newCID = nextCID++;
  uint8_t resp[17];
  memcpy(resp, payload, 8);
  resp[8]=(newCID>>24)&0xFF; resp[9]=(newCID>>16)&0xFF;
  resp[10]=(newCID>>8)&0xFF; resp[11]=newCID&0xFF;
  resp[12]=2; resp[13]=1; resp[14]=0; resp[15]=0;
  resp[16]=CTAPHID_CAP_CBOR;
  sendCTAPHIDResponse(CTAPHID_BROADCAST_CID, CTAPHID_INIT, resp, 17);
  debugPrintf("  -> CID=0x%08X assigned\n", newCID);
}

static void handlePing(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  debugPrintf("[FIDO2] PING %d bytes\n", bcnt);
  sendCTAPHIDResponse(cid, CTAPHID_PING, payload, bcnt);
}

static void handleMsg(uint32_t cid) {
  debugPrintln("[FIDO2] MSG (U2F) - not supported");
  sendCTAPError(cid, CTAP1_ERR_INVALID_CMD);
}

// ─── Fingerprint Authentication with 3-Retry Logic ──────────────────────
// Replaces waitForUserPresence() from the test (which used BOOT button).
// Shows OLED feedback for each attempt and sends KEEPALIVE to host.

bool waitForFingerprintAuth(uint32_t cid, uint32_t timeoutMs) {
  debugPrintln("[FIDO2] >>> Fingerprint auth started <<<");

  // Resume from persisted attempt if host cancelled and retried recently
  int startAttempt = 1;
  if (persistedFpAttempt > 0 && persistedFpAttempt <= FP_AUTH_MAX_RETRIES &&
      (millis() - persistedFpAttemptAt) < FP_AUTH_RETRY_WINDOW_MS) {
    startAttempt = persistedFpAttempt;
    debugPrintf("[FIDO2] Resuming from attempt %d (host retry)\n", startAttempt);
  }
  persistedFpAttempt = 0;  // Clear so next fresh auth starts at 1

  for (int attempt = startAttempt; attempt <= FP_AUTH_MAX_RETRIES; attempt++) {
    fpAuthAttempt = (uint8_t)attempt;

    // Show scan prompt on OLED
    currentMode = MODE_FIDO2_WAITING;
    updateDisplay();
    setLEDPattern(PATTERN_ACTIVE);

    debugPrintf("[FIDO2] Fingerprint attempt %d/%d\n", attempt, FP_AUTH_MAX_RETRIES);

    // Send KEEPALIVE packets while waiting — tell host we're waiting for UV
    uint32_t start = millis();
    uint32_t lastKeepalive = 0;

    // Send initial KEEPALIVE
    {
      uint8_t status = CTAPHID_STATUS_UPNEEDED;
      sendCTAPHIDResponse(cid, CTAPHID_KEEPALIVE, &status, 1);
    }

    // Start fingerprint verification (blocking call, up to 5s timeout in sensor)
    FpResult fpResult = verifyFingerprint();

    // --- Handle each result type ---
    // IMPORTANT: Check FP_MATCH first. If user matched, succeed even if CANCEL
    // arrived (host may have timed out but user completed auth — send assertion).
    if (fpResult == FP_MATCH) {
      debugPrintf("[FIDO2] Fingerprint matched on attempt %d\n", attempt);
      persistedFpAttempt = 0;
      fpAuthAttempt = 0;
      return true;
    }

    // Check for CANCEL from host (only when we didn't match — if matched, we succeeded above)
    if (rx_tail != rx_head) {
      int peek = rx_tail;
      if (rx_queue[peek].len >= 5 && rx_queue[peek].data[4] == 0x91) {
        rx_tail = (rx_tail + 1) % RX_QUEUE_DEPTH;
        debugPrintln("[FIDO2] CANCEL received during fingerprint scan");
        // Host may retry — resume from next attempt (current one was used)
        persistedFpAttempt = (uint8_t)(attempt < FP_AUTH_MAX_RETRIES ? attempt + 1 : attempt);
        persistedFpAttemptAt = millis();
        fpAuthAttempt = 0;
        currentMode = MODE_IDLE;
        updateDisplay();
        return false;
      }
    }

    if (fpResult == FP_SENSOR_ERROR) {
      // Hardware failure — abort immediately, don't waste remaining retries
      debugPrintln("[FIDO2] Sensor error — aborting auth");
      persistedFpAttempt = 0;
      fpAuthAttempt = 0;
      currentMode = MODE_FIDO2_FP_SENSOR_ERR;
      updateDisplay();
      setLEDPattern(PATTERN_ERROR);
      delay(2500);  // Let user read the error message
      return false;
    }

    // FP_NO_MATCH or FP_TIMEOUT — show appropriate feedback and retry
    if (attempt < FP_AUTH_MAX_RETRIES) {
      if (fpResult == FP_TIMEOUT) {
        debugPrintf("[FIDO2] Timeout (no finger), %d retries left\n",
                    FP_AUTH_MAX_RETRIES - attempt);
        currentMode = MODE_FIDO2_FP_TIMEOUT;
      } else {
        // FP_NO_MATCH
        debugPrintf("[FIDO2] Fingerprint failed, %d retries left\n",
                    FP_AUTH_MAX_RETRIES - attempt);
        currentMode = MODE_FIDO2_FP_RETRY;
      }
      updateDisplay();
      setLEDPattern(PATTERN_ERROR);
      delay(2000);  // Let user see the retry message before next scan

      // Send KEEPALIVE to keep the host happy during the retry delay
      uint8_t status = CTAPHID_STATUS_UPNEEDED;
      sendCTAPHIDResponse(cid, CTAPHID_KEEPALIVE, &status, 1);
    }
  }

  // All attempts exhausted
  debugPrintln("[FIDO2] All fingerprint attempts failed");
  persistedFpAttempt = 0;
  fpAuthAttempt = 0;
  currentMode = MODE_FIDO2_FP_FAILED;
  updateDisplay();
  setLEDPattern(PATTERN_ERROR);
  delay(2000);  // Show failure screen

  return false;
}

// ─── CTAP2: authenticatorGetInfo (0x04) ──────────────────────────────────

void handleGetInfo(uint32_t cid) {
  debugPrintln("[FIDO2] GetInfo");

  uint8_t r[200];
  size_t p = 0;
  r[p++] = 0x00;  // status OK
  r[p++] = 0xA8;  // 8-item map

  // key 1: versions array
  p += cbor_enc_uint(r+p, 1);
  r[p++] = 0x82;  // 2-element array: FIDO_2_0, FIDO_2_1
  p += cbor_enc_text(r+p, "FIDO_2_0");
  p += cbor_enc_text(r+p, "FIDO_2_1");

  // key 3: aaguid (16 zero bytes)
  p += cbor_enc_uint(r+p, 3);
  uint8_t aaguid[16] = {0};
  p += cbor_enc_bytes(r+p, aaguid, 16);

  // key 4: options map {up, uv, plat, rk, clientPin, pinUvAuthToken}
  p += cbor_enc_uint(r+p, 4);
  r[p++] = 0xA6;  // 6-item map
  p += cbor_enc_text(r+p, "up");             r[p++] = 0xF5; // true
  p += cbor_enc_text(r+p, "uv");             r[p++] = 0xF5; // true
  p += cbor_enc_text(r+p, "plat");           r[p++] = 0xF4; // false
  p += cbor_enc_text(r+p, "rk");             r[p++] = 0xF5; // true
  p += cbor_enc_text(r+p, "clientPin");      r[p++] = pinIsSet() ? 0xF5 : 0xF4; // true if PIN set
  p += cbor_enc_text(r+p, "pinUvAuthToken"); r[p++] = 0xF5; // true: supports getPinUvAuthTokenUsingUv

  // key 5: maxMsgSize
  p += cbor_enc_uint(r+p, 5);
  p += cbor_enc_uint(r+p, 1024);

  // key 6: pinUvAuthProtocols [1]
  p += cbor_enc_uint(r+p, 6);
  r[p++] = 0x81;  // 1-element array
  p += cbor_enc_uint(r+p, 1);  // protocol v1

  // key 7: maxCredentialCountInList (optional, for credential list)
  p += cbor_enc_uint(r+p, 7);
  p += cbor_enc_uint(r+p, 8);

  // key 9: transports ["usb"]
  p += cbor_enc_uint(r+p, 9);
  r[p++] = 0x81;  // 1-element array
  p += cbor_enc_text(r+p, "usb");

  // key 13 (0x0D): minPINLength (required when clientPin supported)
  p += cbor_enc_uint(r+p, 13);
  p += cbor_enc_uint(r+p, 4);

  sendCTAPHIDLarge(cid, CTAPHID_CBOR, r, (uint16_t)p);
}

// ─── CTAP2: authenticatorClientPIN (0x06) ────────────────────────────────

static void handleClientPIN(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  debugPrintf("[FIDO2] ClientPIN bcnt=%d\n", bcnt);
  if (bcnt < 2) { sendCTAPError(cid, CTAP2_ERR_INVALID_CBOR); return; }

  const uint8_t* cbor = payload + 1;
  size_t cborLen = bcnt - 1;

  // subCommand is key 0x02 per CTAP2 spec (0x01 = pinUvAuthProtocol)
  uint8_t subCmd = 0;
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x02, &v, &va)) {
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
      return;
    }
    int32_t sc;
    if (!cbor_get_int(v, va, &sc) || sc < 0 || sc > 255) {
      sendCTAPError(cid, CTAP2_ERR_INVALID_CBOR);
      return;
    }
    subCmd = (uint8_t)sc;
  }
  debugPrintf("  ClientPIN subCmd=0x%02X\n", subCmd);

  uint8_t resp[128];
  size_t rp = 0;
  resp[rp++] = 0x00;  // status OK

  switch (subCmd) {
    case 0x01: {  // getPINRetries
      uint8_t retries = pinGetRetries();
      if (retries == 0) {
        resp[rp++] = 0xA2;  // 2-item map
        rp += cbor_enc_uint(resp+rp, 3);  // key 3: pinRetries
        rp += cbor_enc_uint(resp+rp, 0);
        rp += cbor_enc_uint(resp+rp, 4);  // key 4: powerCycleState
        resp[rp++] = 0xF5;  // true
      } else {
        resp[rp++] = 0xA1;  // 1-item map
        rp += cbor_enc_uint(resp+rp, 3);  // key 3: pinRetries
        rp += cbor_enc_uint(resp+rp, retries);
      }
      break;
    }
    case 0x07: {  // getUVRetries — fingerprint attempts remaining before lockout
      resp[rp++] = 0xA1;  // 1-item map
      rp += cbor_enc_uint(resp+rp, 5);  // key 5: uvRetries
      rp += cbor_enc_uint(resp+rp, 5);  // 5 attempts (we don't lock out UV)
      break;
    }
    case 0x02: {  // getKeyAgreement — keyAgreement is COSE_Key (embedded map), not bstr
      uint8_t keyAgree[91];
      size_t kaLen = pinProtocolGetKeyAgreement(keyAgree, sizeof(keyAgree));
      if (kaLen == 0) {
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      resp[rp++] = 0xA1;  // 1-item map
      rp += cbor_enc_uint(resp+rp, 1);  // key 1: keyAgreement
      memcpy(resp + rp, keyAgree, kaLen);  // value: COSE_Key map (embedded, not bstr)
      rp += kaLen;
      break;
    }
    case 0x03: {  // setPIN — keys: 0x03 keyAgreement, 0x05 newPinEnc, 0x04 pinUvAuthParam
      debugPrintln("  setPIN: parsing...");
      if (pinIsSet()) {
        debugPrintln("  setPIN: PIN already set");
        sendCTAPError(cid, CTAP2_ERR_PIN_AUTH_INVALID);
        return;
      }
      const uint8_t* keyAgreeV, *newPinEncV, *pinUvParamV;
      size_t kaVa, neVa, pvVa;
      if (!cbor_map_find_uint(cbor, cborLen, 0x03, &keyAgreeV, &kaVa) ||
          !cbor_map_find_uint(cbor, cborLen, 0x05, &newPinEncV, &neVa) ||
          !cbor_map_find_uint(cbor, cborLen, 0x04, &pinUvParamV, &pvVa)) {
        debugPrintln("  setPIN: missing params");
        sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
        return;
      }
      uint8_t keyAgree[91], newPinEnc[96], pinUvParam[16];
      // keyAgreement is COSE_Key (CBOR map), not bstr — get encoded map bytes via cbor_skip
      size_t kaLen = cbor_skip(keyAgreeV, kaVa);
      if (kaLen == 0 || kaLen > sizeof(keyAgree)) kaLen = 0;
      else memcpy(keyAgree, keyAgreeV, kaLen);
      size_t neLen = cbor_get_bytes(newPinEncV, neVa, newPinEnc, sizeof(newPinEnc));
      size_t pvLen = cbor_get_bytes(pinUvParamV, pvVa, pinUvParam, 16);
      debugPrintf("  setPIN: kaLen=%d neLen=%d pvLen=%d\n", (int)kaLen, (int)neLen, (int)pvLen);
      if (kaLen == 0 || neLen == 0 || pvLen != 16) {
        debugPrintln("  setPIN: bad param lengths");
        sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
        return;
      }
      pinProtocolSetPlatformKey(keyAgree, kaLen);
      uint8_t sharedSecret[32];
      if (!pinProtocolDeriveSharedSecret(sharedSecret, 32)) {
        debugPrintln("  setPIN: deriveSharedSecret failed");
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      uint8_t expectedAuth[16];
      pinProtocolAuthenticate(sharedSecret, 32, newPinEnc, neLen, expectedAuth);
      if (memcmp(expectedAuth, pinUvParam, 16) != 0) {
        debugPrintln("  setPIN: pinUvAuthParam verify failed");
        sendCTAPError(cid, CTAP2_ERR_PIN_AUTH_INVALID);
        return;
      }
      uint8_t newPinPadded[96];
      size_t npLen;
      if (!pinProtocolDecrypt(sharedSecret, 32, newPinEnc, neLen, newPinPadded, &npLen)) {
        debugPrintln("  setPIN: decrypt failed");
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      size_t pinLen = 0;
      while (pinLen < npLen && newPinPadded[pinLen] != 0) pinLen++;
      if (pinLen < 4) {
        debugPrintln("  setPIN: PIN too short");
        sendCTAPError(cid, CTAP2_ERR_PIN_POLICY_VIOLATION);
        return;
      }
      uint8_t pinHash[32];
      if (!computeSHA256(newPinPadded, pinLen, pinHash)) {
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      pinStoreHash(pinHash);
      pinResetRetries();
      resp[rp++] = 0xA0;  // empty map
      debugPrintln("  setPIN: OK");
      break;
    }
    case 0x05: {  // getPINToken (legacy) — keys: 0x03 keyAgreement, 0x06 pinHashEnc
      if (!pinIsSet()) {
        sendCTAPError(cid, CTAP2_ERR_PIN_NOT_SET);
        return;
      }
      if (pinGetRetries() == 0) {
        sendCTAPError(cid, CTAP2_ERR_PIN_BLOCKED);
        return;
      }
      const uint8_t* keyAgreeV, *pinHashEncV;
      size_t kaVa, phVa;
      if (!cbor_map_find_uint(cbor, cborLen, 0x03, &keyAgreeV, &kaVa) ||
          !cbor_map_find_uint(cbor, cborLen, 0x06, &pinHashEncV, &phVa)) {
        sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
        return;
      }
      uint8_t keyAgree[91], pinHashEnc[32];
      size_t kaLen = cbor_get_bytes(keyAgreeV, kaVa, keyAgree, sizeof(keyAgree));
      size_t phLen = cbor_get_bytes(pinHashEncV, phVa, pinHashEnc, sizeof(pinHashEnc));
      if (kaLen == 0 || phLen != 16) {
        sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
        return;
      }
      pinProtocolSetPlatformKey(keyAgree, kaLen);
      uint8_t sharedSecret[32];
      if (!pinProtocolDeriveSharedSecret(sharedSecret, 32)) {
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      uint8_t pinHashDec[16];
      size_t pdLen;
      if (!pinProtocolDecrypt(sharedSecret, 32, pinHashEnc, 16, pinHashDec, &pdLen) || pdLen != 16) {
        pinDecrementRetries();
        sendCTAPError(cid, CTAP2_ERR_PIN_INVALID);
        return;
      }
      if (!pinVerifyHash(pinHashDec)) {
        pinDecrementRetries();
        sendCTAPError(cid, CTAP2_ERR_PIN_INVALID);
        return;
      }
      pinResetRetries();
      esp_fill_random(pinUvAuthToken, 16);
      pinTokenSetAt = millis();
      uint8_t tokenEnc[32];
      size_t teLen;
      if (!pinProtocolEncrypt(sharedSecret, 32, pinUvAuthToken, 16, tokenEnc, &teLen)) {
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      resp[rp++] = 0xA1;
      rp += cbor_enc_uint(resp+rp, 2);  // key 2: pinUvAuthToken
      rp += cbor_enc_bytes(resp+rp, tokenEnc, 16);
      break;
    }
    case 0x06: {  // getPinUvAuthTokenUsingUv (fingerprint) — key 0x03 keyAgreement
      fidoBusy = true;
      if (!waitForFingerprintAuth(cid, FP_AUTH_TIMEOUT_MS)) {
        fidoBusy = false;
        sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
        return;
      }
      const uint8_t* keyAgreeV;
      size_t kaVa;
      if (!cbor_map_find_uint(cbor, cborLen, 0x03, &keyAgreeV, &kaVa)) {
        fidoBusy = false;
        sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
        return;
      }
      uint8_t keyAgree[91];
      size_t kaLen = cbor_get_bytes(keyAgreeV, kaVa, keyAgree, sizeof(keyAgree));
      if (kaLen == 0) {
        fidoBusy = false;
        sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
        return;
      }
      pinProtocolSetPlatformKey(keyAgree, kaLen);
      uint8_t sharedSecret[32];
      if (!pinProtocolDeriveSharedSecret(sharedSecret, 32)) {
        fidoBusy = false;
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      esp_fill_random(pinUvAuthToken, 16);
      pinTokenSetAt = millis();
      uint8_t tokenEnc[32];
      size_t teLen;
      if (!pinProtocolEncrypt(sharedSecret, 32, pinUvAuthToken, 16, tokenEnc, &teLen)) {
        fidoBusy = false;
        sendCTAPError(cid, CTAP1_ERR_OTHER);
        return;
      }
      fidoBusy = false;
      resp[rp++] = 0xA1;
      rp += cbor_enc_uint(resp+rp, 2);
      rp += cbor_enc_bytes(resp+rp, tokenEnc, 16);
      break;
    }
    default:
      sendCTAPError(cid, CTAP1_ERR_INVALID_PARAMETER);
      return;
  }

  // getKeyAgreement response (~84 bytes) exceeds 57-byte single-packet limit; use Large
  if (rp > 57) {
    sendCTAPHIDLarge(cid, CTAPHID_CBOR, resp, (uint16_t)rp);
  } else {
    sendCTAPHIDResponse(cid, CTAPHID_CBOR, resp, (uint16_t)rp);
  }
}

// ─── CTAP2: authenticatorMakeCredential (0x01) ───────────────────────────

void handleMakeCredential(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  debugPrintln("[FIDO2] === MakeCredential ===");
  if (bcnt < 2) { sendCTAPError(cid, CTAP2_ERR_INVALID_CBOR); return; }

  const uint8_t* cbor    = payload + 1;
  size_t         cborLen = bcnt - 1;

  // --- Parse pinUvAuthParam (0x08) and pinUvAuthProtocol (0x09) for ClientPIN flow ---
  const uint8_t* pinUvParamV = nullptr;
  size_t pinUvParamVa = 0;
  bool hasPinUvParam = cbor_map_find_uint(cbor, cborLen, 0x08, &pinUvParamV, &pinUvParamVa);

  if (hasPinUvParam && pinUvParamVa > 0) {
    uint8_t paramBuf[32];
    size_t paramLen = cbor_get_bytes(pinUvParamV, pinUvParamVa, paramBuf, sizeof(paramBuf));
    if (paramLen == 16) {
      // Verify pinUvAuthParam = HMAC-SHA-256(pinUvAuthToken, clientDataHash)[:16]
      const uint8_t* cdHashV; size_t cdHashVa;
      if (cbor_map_find_uint(cbor, cborLen, 0x01, &cdHashV, &cdHashVa)) {
        uint8_t clientDataHash[32];
        if (cbor_get_bytes(cdHashV, cdHashVa, clientDataHash, 32) == 32) {
          uint8_t expected[16];
          pinProtocolAuthenticate(pinUvAuthToken, 16, clientDataHash, 32, expected);
          if (memcmp(expected, paramBuf, 16) == 0 &&
              (millis() - pinTokenSetAt) < PIN_TOKEN_VALID_MS) {
            // PIN auth valid — skip fingerprint
            fidoBusy = true;
            currentMode = MODE_FIDO2_VERIFYING;
            updateDisplay();
            goto make_cred_after_auth;
          }
        }
      }
      sendCTAPError(cid, CTAP2_ERR_PIN_AUTH_INVALID);
      return;
    }
  } else if (hasPinUvParam) {
    // Zero-length pinUvAuthParam: device selection probe — wait for touch, return PIN_NOT_SET
    fidoBusy = true;
    if (!waitForFingerprintAuth(cid, FP_AUTH_TIMEOUT_MS)) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
      return;
    }
    fidoBusy = false;
    currentMode = MODE_IDLE;
    updateDisplay();
    sendCTAPError(cid, pinIsSet() ? CTAP2_ERR_PIN_INVALID : CTAP2_ERR_PIN_NOT_SET);
    return;
  }

  // --- Parse rpId early so display can show site name during fingerprint ---
  {
    char rpId[128] = {};
    const uint8_t* rpMap; size_t rpLen;
    const uint8_t* v;     size_t va;
    if (cbor_map_find_uint(cbor, cborLen, 0x02, &rpMap, &rpLen) &&
        cbor_map_find_text(rpMap, rpLen, "id", &v, &va) &&
        cbor_get_text(v, va, rpId, sizeof(rpId))) {
      strncpy(fido2RpId, rpId, sizeof(fido2RpId) - 1);
      fido2RpId[sizeof(fido2RpId) - 1] = '\0';
    } else {
      fido2RpId[0] = '\0';
    }
  }

  // --- Step 1: Fingerprint authentication (3 retries) ---
  fidoBusy = true;
  if (!waitForFingerprintAuth(cid, FP_AUTH_TIMEOUT_MS)) {
    fidoBusy = false;
    drainStalePackets(cid);
    currentMode = MODE_FIDO2_ERROR;
    updateDisplay();
    delay(1500);
    currentMode = MODE_IDLE;
    updateDisplay();
    sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
    return;
  }

  if (drainStalePackets(cid)) {
    fidoBusy = false;
    currentMode = MODE_IDLE;
    updateDisplay();
    sendCTAPError(cid, 0x2D);
    return;
  }

  currentMode = MODE_FIDO2_VERIFYING;
  updateDisplay();

make_cred_after_auth:

  // --- Step 2: Parse clientDataHash (key 0x01) ---
  uint8_t clientDataHash[32];
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x01, &v, &va) ||
        cbor_get_bytes(v, va, clientDataHash, 32) != 32) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
      currentMode = MODE_IDLE; updateDisplay();
      return;
    }
  }

  // --- Step 3: Parse rpId (key 0x02 -> "id") ---
  char rpId[128] = {};
  {
    const uint8_t* rpMap; size_t rpLen;
    const uint8_t* v;     size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x02, &rpMap, &rpLen) ||
        !cbor_map_find_text(rpMap, rpLen, "id", &v, &va) ||
        !cbor_get_text(v, va, rpId, sizeof(rpId))) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
      currentMode = MODE_IDLE; updateDisplay();
      return;
    }
  }
  debugPrintf("  rpId: %s\n", rpId);
  strncpy(fido2RpId, rpId, sizeof(fido2RpId) - 1);
  fido2RpId[sizeof(fido2RpId) - 1] = '\0';

  // --- SelectDevice: Windows device-selection probe — do NOT create credential ---
  if (strcmp(rpId, "SelectDevice") == 0) {
    debugPrintln("  [SelectDevice] Device selection probe — returning PIN_NOT_SET");
    fidoBusy = false;
    currentMode = MODE_IDLE;
    updateDisplay();
    sendCTAPError(cid, pinIsSet() ? CTAP2_ERR_PIN_INVALID : CTAP2_ERR_PIN_NOT_SET);
    return;
  }

  // --- Step 4: Verify ES256 (-7) in pubKeyCredParams (key 0x04) ---
  {
    const uint8_t* arr; size_t arrLen;
    bool ok = false;
    if (cbor_map_find_uint(cbor, cborLen, 0x04, &arr, &arrLen)) {
      uint8_t maj; size_t n;
      size_t h = cbor_decode_head(arr, arrLen, &maj, &n);
      if (h && maj == CBOR_ARRAY) {
        size_t pos = h;
        for (size_t i = 0; i < n && !ok; i++) {
          const uint8_t* av; size_t aa;
          if (cbor_map_find_text(arr+pos, arrLen-pos, "alg", &av, &aa)) {
            int32_t alg;
            if (cbor_get_int(av, aa, &alg) && alg == -7) ok = true;
          }
          size_t s = cbor_skip(arr+pos, arrLen-pos); if (!s) break; pos += s;
        }
      }
    }
    if (!ok) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_UNSUPPORTED_ALG);
      currentMode = MODE_IDLE; updateDisplay();
      return;
    }
  }

  // --- Step 5: Compute rpIdHash using ATECC608A ---
  uint8_t rpIdHash[32];
  if (!computeSHA256((uint8_t*)rpId, strlen(rpId), rpIdHash)) {
    debugPrintln("  ERR: SHA-256 failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  // --- Step 6: Allocate credential slot ---
  Cred* cred = cred_alloc(rpIdHash);
  if (!cred) {
    debugPrintln("  ERR: key store full");
    fidoBusy = false;
    sendCTAPError(cid, CTAP2_ERR_KEY_STORE_FULL);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  // --- Step 7: Generate ECC P-256 key pair in ATECC608A ---
  if (!generateKeyPair(cred->slot)) {
    debugPrintln("  ERR: key generation failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  // Get the public key from ATECC608A (64 bytes: X || Y)
  uint8_t pubKey[64];
  if (!getPublicKey(cred->slot, pubKey)) {
    debugPrintln("  ERR: failed to read public key");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  // Generate random credential ID
  if (!generateRandom(cred->id, CRED_ID_LEN)) {
    // Fallback to ESP32 RNG if ATECC608A random fails
    esp_fill_random(cred->id, CRED_ID_LEN);
  }
  memcpy(cred->rpIdHash, rpIdHash, 32);
  cred->signCount = 0;
  cred->valid = true;

  debugPrint("  credId: ");
  for (int i = 0; i < CRED_ID_LEN; i++) debugPrintf("%02X", cred->id[i]);
  debugPrintln();

  // --- Step 8: Build authenticatorData (148 bytes with AT flag) ---
  uint8_t authData[148] = {};
  memcpy(authData, rpIdHash, 32);           // rpIdHash
  authData[32] = 0x45;                       // flags: UP|UV|AT
  // [33-36] signCount = 0x00000000
  // [37-52] aaguid = zeros
  authData[53] = 0x00;
  authData[54] = CRED_ID_LEN;               // credentialIdLen = 16
  memcpy(authData + 55, cred->id, CRED_ID_LEN);
  cbor_enc_cose_key(authData + 71, pubKey, pubKey + 32); // 77 bytes (X, Y)

  // --- Step 9: Sign authData || clientDataHash ---
  // Compute SHA-256(authData || clientDataHash) for signing
  // ATECC608A expects a 32-byte pre-hashed message
  uint8_t signInput[sizeof(authData) + 32];
  memcpy(signInput, authData, sizeof(authData));
  memcpy(signInput + sizeof(authData), clientDataHash, 32);

  uint8_t hashToSign[32];
  if (!computeSHA256(signInput, sizeof(signInput), hashToSign)) {
    debugPrintln("  ERR: hash for signing failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  uint8_t rawSig[64];  // ATECC608A raw R||S
  if (!signChallenge(cred->slot, hashToSign, 32, rawSig)) {
    debugPrintln("  ERR: signing failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  // Convert raw signature to DER format
  uint8_t sig_der[72]; size_t sig_len;
  rawToDER(rawSig, sig_der, &sig_len);
  debugPrintf("  Signature: %d bytes (DER)\n", (int)sig_len);

  // --- Step 10: Save credential metadata to NVS ---
  cred_save(cred);

  // --- Step 11: Encode "none" attestation response ---
  uint8_t resp[300];
  size_t respLen = cbor_enc_make_credential_resp(resp, authData, sizeof(authData));
  sendCTAPHIDLarge(cid, CTAPHID_CBOR, resp, (uint16_t)respLen);

  fidoBusy = false;
  debugPrintln("[FIDO2] MakeCredential DONE ✓");

  // Show success on OLED
  currentMode = MODE_FIDO2_SUCCESS;
  updateDisplay();
  setLEDPattern(PATTERN_ACTIVE);
  delay(2000);
  fido2RpId[0] = '\0';  // Clear requester when returning to idle
  currentMode = MODE_IDLE;
  updateDisplay();
  setLEDPattern(PATTERN_IDLE);
}

// ─── CTAP2: authenticatorGetAssertion (0x02) ──────────────────────────────

void handleGetAssertion(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  debugPrintln("[FIDO2] === GetAssertion ===");
  if (bcnt < 2) { sendCTAPError(cid, CTAP2_ERR_INVALID_CBOR); return; }

  fidoBusy = true;

  const uint8_t* cbor    = payload + 1;
  size_t         cborLen = bcnt - 1;

  // --- Parse rpId, clientDataHash, find credential FIRST (before fingerprint) ---
  char rpId[128] = {};
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x01, &v, &va) ||
        !cbor_get_text(v, va, rpId, sizeof(rpId))) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
      currentMode = MODE_IDLE; updateDisplay();
      return;
    }
  }
  debugPrintf("  rpId: %s\n", rpId);
  strncpy(fido2RpId, rpId, sizeof(fido2RpId) - 1);
  fido2RpId[sizeof(fido2RpId) - 1] = '\0';

  uint8_t clientDataHash[32];
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x02, &v, &va) ||
        cbor_get_bytes(v, va, clientDataHash, 32) != 32) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM);
      currentMode = MODE_IDLE; updateDisplay();
      return;
    }
  }

  // --- ClientPIN: pinUvAuthParam (0x06) — verify or device selection ---
  const uint8_t* pinUvParamV = nullptr;
  size_t pinUvParamVa = 0;
  bool hasPinUvParam = cbor_map_find_uint(cbor, cborLen, 0x06, &pinUvParamV, &pinUvParamVa);
  bool pinAuthValid = false;

  if (hasPinUvParam && pinUvParamVa > 0) {
    uint8_t paramBuf[32];
    size_t paramLen = cbor_get_bytes(pinUvParamV, pinUvParamVa, paramBuf, sizeof(paramBuf));
    if (paramLen == 16) {
      uint8_t expected[16];
      pinProtocolAuthenticate(pinUvAuthToken, 16, clientDataHash, 32, expected);
      if (memcmp(expected, paramBuf, 16) == 0 &&
          (millis() - pinTokenSetAt) < PIN_TOKEN_VALID_MS) {
        pinAuthValid = true;  // Skip fingerprint
      } else {
        fidoBusy = false;
        sendCTAPError(cid, CTAP2_ERR_PIN_AUTH_INVALID);
        currentMode = MODE_IDLE; updateDisplay();
        return;
      }
    }
  } else if (hasPinUvParam) {
    // Zero-length: device selection — wait for touch, return PIN_NOT_SET
    if (!waitForFingerprintAuth(cid, FP_AUTH_TIMEOUT_MS)) {
      fidoBusy = false;
      sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
      currentMode = MODE_IDLE; updateDisplay();
      return;
    }
    fidoBusy = false;
    currentMode = MODE_IDLE;
    updateDisplay();
    sendCTAPError(cid, pinIsSet() ? CTAP2_ERR_PIN_INVALID : CTAP2_ERR_PIN_NOT_SET);
    return;
  }

  uint8_t rpIdHash[32];
  if (!computeSHA256((uint8_t*)rpId, strlen(rpId), rpIdHash)) {
    debugPrintln("  ERR: SHA-256 failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  Cred* cred = nullptr;
  const uint8_t* arr; size_t arrLen;
  if (cbor_map_find_uint(cbor, cborLen, 0x03, &arr, &arrLen)) {
    uint8_t maj; size_t n;
    size_t h = cbor_decode_head(arr, arrLen, &maj, &n);
    if (h && maj == CBOR_ARRAY) {
      size_t pos = h;
      for (size_t i = 0; i < n && !cred; i++) {
        const uint8_t* iv; size_t ia;
        if (cbor_map_find_text(arr+pos, arrLen-pos, "id", &iv, &ia)) {
          uint8_t credId[CRED_ID_LEN];
          if (cbor_get_bytes(iv, ia, credId, CRED_ID_LEN) == CRED_ID_LEN)
            cred = cred_find_by_id(credId);
        }
        size_t s = cbor_skip(arr+pos, arrLen-pos); if (!s) break; pos += s;
      }
    }
  }
  if (!cred) cred = cred_find_by_rpidhash(rpIdHash);

  if (!cred) {
    debugPrintln("  ERR: no matching credential");
    fidoBusy = false;
    sendCTAPError(cid, CTAP2_ERR_NO_CREDENTIALS);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  // Session: skip fingerprint if PIN auth valid, or user just authenticated for same rpId
  bool fpSkipped = pinAuthValid ||
    (memcmp(rpIdHash, lastGetAssertionRpIdHash, 32) == 0 &&
     (millis() - lastGetAssertionTime) < GET_ASSERTION_SESSION_MS);
  if (fpSkipped) {
    debugPrintln("  [Session] Skipping fingerprint (recent auth for same site)");
  } else {
    if (!waitForFingerprintAuth(cid, FP_AUTH_TIMEOUT_MS)) {
      fidoBusy = false;
      drainStalePackets(cid);
      resetCtapReassembly();
      currentMode = MODE_FIDO2_ERROR;
      updateDisplay();
      delay(1500);
      currentMode = MODE_IDLE;
      updateDisplay();
      sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
      return;
    }
    if (drainStalePackets(cid)) {
      fidoBusy = false;
      resetCtapReassembly();
      currentMode = MODE_IDLE;
      updateDisplay();
      sendCTAPError(cid, 0x2D);
      return;
    }
  }

  currentMode = MODE_FIDO2_VERIFYING;
  updateDisplay();

  debugPrint("  credId: ");
  for (int i = 0; i < CRED_ID_LEN; i++) debugPrintf("%02X", cred->id[i]);
  debugPrintln();

  // --- Step 5: Build short authenticatorData (37 bytes) ---
  uint8_t authData[37] = {};
  memcpy(authData, rpIdHash, 32);
  authData[32] = 0x05; // flags: UP|UV
  cred->signCount++;
  authData[33] = (cred->signCount >> 24) & 0xFF;
  authData[34] = (cred->signCount >> 16) & 0xFF;
  authData[35] = (cred->signCount >>  8) & 0xFF;
  authData[36] =  cred->signCount        & 0xFF;

  // --- Step 6: Sign authData || clientDataHash ---
  uint8_t signInput[sizeof(authData) + 32];
  memcpy(signInput, authData, sizeof(authData));
  memcpy(signInput + sizeof(authData), clientDataHash, 32);

  uint8_t hashToSign[32];
  if (!computeSHA256(signInput, sizeof(signInput), hashToSign)) {
    debugPrintln("  ERR: hash for signing failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  uint8_t rawSig[64];
  if (!signChallenge(cred->slot, hashToSign, 32, rawSig)) {
    debugPrintln("  ERR: signing failed");
    fidoBusy = false;
    sendCTAPError(cid, 0x7F);
    currentMode = MODE_FIDO2_ERROR; updateDisplay(); delay(2000);
    currentMode = MODE_IDLE; updateDisplay();
    return;
  }

  uint8_t sig_der[72]; size_t sig_len;
  rawToDER(rawSig, sig_der, &sig_len);

  // Update counter in NVS
  cred_update_counter(cred);

  // --- Step 7: Encode CBOR response and send ---
  uint8_t resp[300];
  size_t respLen = cbor_enc_get_assertion_resp(resp,
                                                cred->id, CRED_ID_LEN,
                                                authData, sizeof(authData),
                                                sig_der, sig_len);
  sendCTAPHIDLarge(cid, CTAPHID_CBOR, resp, (uint16_t)respLen);

  fidoBusy = false;
  debugPrintf("[FIDO2] GetAssertion DONE ✓ (signCount=%d)\n", cred->signCount);

  // Update session so duplicate GetAssertion (same rpId) can skip fingerprint
  memcpy(lastGetAssertionRpIdHash, rpIdHash, 32);
  lastGetAssertionTime = millis();

  // Always show success on OLED after successful auth (fix: was only shown when
  // fpSkipped, leaving UI stuck at "Signing..." when user did fingerprint)
  currentMode = MODE_FIDO2_SUCCESS;
  updateDisplay();
  setLEDPattern(PATTERN_ACTIVE);
  delay(2000);
  fido2RpId[0] = '\0';  // Clear requester when returning to idle
  currentMode = MODE_IDLE;
  updateDisplay();
  setLEDPattern(PATTERN_IDLE);
}

// ─── CTAPHID_CBOR dispatcher ─────────────────────────────────────────────

static void handleCBOR(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  if (bcnt < 1) { sendCTAPError(cid, CTAP1_ERR_INVALID_LEN); return; }
  uint8_t cmd = payload[0];
  debugPrintf("[FIDO2] CBOR cmd=0x%02X\n", cmd);
  switch (cmd) {
    case CTAP2_CMD_MAKE_CREDENTIAL: handleMakeCredential(cid, payload, bcnt); break;
    case CTAP2_CMD_GET_ASSERTION:   handleGetAssertion  (cid, payload, bcnt); break;
    case CTAP2_CMD_GET_INFO:        handleGetInfo(cid);                        break;
    case CTAP2_CMD_CLIENT_PIN:      handleClientPIN    (cid, payload, bcnt); break;
    default:
      debugPrintf("  cmd 0x%02X not implemented\n", cmd);
      { uint8_t e[1]={0x27}; sendCTAPHIDResponse(cid, CTAPHID_CBOR, e, 1); }
  }
}

// ─── Main loop: Check for FIDO2 requests ──────────────────────────────────
// Reassembles CTAPHID packets and dispatches. Process ONE complete message per
// call (like the test) so there's a natural gap between responses — the test
// has human delay (BOOT button) between the two GetAssertions.

void checkFIDO2Requests() {
  while (rx_tail != rx_head) {
    RxPacket pkt;
    pkt.len = rx_queue[rx_tail].len;
    memcpy(pkt.data, rx_queue[rx_tail].data, pkt.len);
    rx_tail = (rx_tail + 1) % RX_QUEUE_DEPTH;
    if (pkt.len < 5) continue;

    uint32_t cid = ((uint32_t)pkt.data[0]<<24)|((uint32_t)pkt.data[1]<<16)
                 | ((uint32_t)pkt.data[2]<< 8)| (uint32_t)pkt.data[3];
    uint8_t  b4  = pkt.data[4];

    if (b4 & 0x80) {
      // Initialization packet
      uint16_t bcnt  = ((uint16_t)pkt.data[5]<<8)|pkt.data[6];
      uint16_t first = pkt.len > 7 ? (uint16_t)(pkt.len-7) : 0;
      if (bcnt > CTAP_MAX_PAYLOAD) bcnt = CTAP_MAX_PAYLOAD;
      if (first > bcnt) first = bcnt;

      // INIT is always single-packet; handle it without touching ctap_asm so
      // we don't corrupt an in-progress multi-packet reassembly (e.g. GetAssertion)
      if (b4 == CTAPHID_INIT && first >= bcnt) {
        debugPrintf("\n[FIDO2] PKT CID=0x%08X CMD=0x%02X BCNT=%d\n", cid, b4, bcnt);
        handleInit(cid, pkt.data + 7, bcnt);
        return;
      }

      ctap_asm = {cid, b4, bcnt, first, {}, true};
      memcpy(ctap_asm.buf, pkt.data+7, first);
      debugPrintf("\n[FIDO2] PKT CID=0x%08X CMD=0x%02X BCNT=%d\n", cid, b4, bcnt);
      if (ctap_asm.received >= ctap_asm.bcnt) {
        ctap_asm.active = false;
        switch (b4) {
          case CTAPHID_INIT: handleInit(cid, ctap_asm.buf, bcnt); break;
          case CTAPHID_PING: handlePing(cid, ctap_asm.buf, bcnt); break;
          case CTAPHID_CBOR: handleCBOR(cid, ctap_asm.buf, bcnt); break;
          case CTAPHID_MSG:  handleMsg (cid);                      break;
          case 0x91: debugPrintln("[FIDO2] CTAPHID_CANCEL"); break;
          default: sendCTAPError(cid, CTAP1_ERR_INVALID_CMD);
        }
        return;  // One message per call — let main loop run before next
      }
    } else if (ctap_asm.active && cid == ctap_asm.cid) {
      // Continuation packet
      uint16_t chunk = pkt.len > 5 ? (uint16_t)(pkt.len-5) : 0;
      uint16_t rem   = ctap_asm.bcnt - ctap_asm.received;
      if (chunk > rem) chunk = rem;
      if (ctap_asm.received + chunk <= CTAP_MAX_PAYLOAD)
        memcpy(ctap_asm.buf + ctap_asm.received, pkt.data+5, chunk);
      ctap_asm.received += chunk;
      if (ctap_asm.received >= ctap_asm.bcnt) {
        ctap_asm.active = false;
        uint32_t dc=ctap_asm.cid; uint8_t dx=ctap_asm.cmd; uint16_t dl=ctap_asm.bcnt;
        switch (dx) {
          case CTAPHID_INIT: handleInit(dc, ctap_asm.buf, dl); break;
          case CTAPHID_PING: handlePing(dc, ctap_asm.buf, dl); break;
          case CTAPHID_CBOR: handleCBOR(dc, ctap_asm.buf, dl); break;
          case CTAPHID_MSG:  handleMsg (dc);                    break;
          default: sendCTAPError(dc, CTAP1_ERR_INVALID_CMD);
        }
        return;  // One message per call
      }
    }
  }
}

// ─── Reset ────────────────────────────────────────────────────────────────

void handleReset() {
  debugPrintln("FIDO2: Reset requested");

  displayMessage("Reset device?", "Press Enc button");

  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    if (digitalRead(ENCODER_SW) == LOW) {
      clearAllFingerprints();
      displayMessage("Device reset", "complete!");
      delay(2000);
      return;
    }
    delay(100);
  }

  displayMessage("Reset cancelled", "");
  delay(1000);
}
