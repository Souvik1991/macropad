/*
 * 07_Multi_HID_Test.ino — Main sketch
 * =====================================
 * USB Composite HID: Keyboard + FIDO2 (CTAP2)
 *
 * Companion files (in this sketch folder):
 *   cbor_mini.h      — CBOR encoder/decoder (header-only)
 *   crypto_p256.h/cpp — P-256 ECDSA via mbedtls
 *   cred_store.h/cpp  — NVS credential storage
 *
 * Phase 2: MakeCredential (0x01) + GetAssertion (0x02)
 *
 * Companion files in this sketch folder:
 *   cbor_mini.ino   — CBOR encoder/decoder
 *   crypto_p256.ino — P-256 ECDSA via mbedtls (public API, 2.x/3.x compatible)
 *   cred_store.ino  — NVS credential storage (up to 8 credentials)
 *
 * Hardware: ESP32-S3 module + USB-C cable
 * BOOT button (GPIO 0): press to type test string via keyboard interface
 *
 * ─────────────────────────────────────
 * VERIFICATION STEPS AFTER FLASHING
 * ─────────────────────────────────────
 * 1. Serial Monitor (115200 baud):
 *    Look for "[USB] Device MOUNTED - both interfaces active!"
 *
 * 2. Python test (pip install fido2 cryptography):
 *    python tools/test_phase2.py
 *
 * 3. Browser: https://webauthn.io → Register
 *    Serial should show MakeCredential → DONE
 *
 * 4. Keyboard: open Notepad, press BOOT button
 *    "FIDO2 Macropad OK!" types itself
 */

#include <Adafruit_TinyUSB.h>

#include "cbor_mini.h"    // CBOR encoder/decoder (header-only)
#include "crypto_p256.h"  // crypto_init, crypto_generate_p256, crypto_sign_p256
#include "cred_store.h"   // Cred, cred_load, cred_alloc, cred_save, cred_update_counter

// ─── CTAP2 error codes (subset) ──────────────────────────────────────────
#define CTAP2_OK                  0x00
#define CTAP1_ERR_INVALID_CMD     0x01
#define CTAP1_ERR_INVALID_LEN     0x03
#define CTAP2_ERR_INVALID_CBOR    0x12
#define CTAP2_ERR_MISSING_PARAM   0x14
#define CTAP2_ERR_UNSUPPORTED_ALG 0x26
#define CTAP2_ERR_NO_CREDENTIALS  0x2E
#define CTAP2_ERR_KEY_STORE_FULL  0x28
#define CTAP2_ERR_OPERATION_DENIED 0x27

// User Presence timeout (ms) — how long to wait for BOOT button
#define UP_TIMEOUT_MS  30000
#define BOOT_BUTTON_PIN 0

// CTAP2 commands
#define CTAP2_CMD_MAKE_CREDENTIAL 0x01
#define CTAP2_CMD_GET_ASSERTION   0x02
#define CTAP2_CMD_GET_INFO        0x04

// CTAPHID transport commands
#define CTAPHID_PING         0x81
#define CTAPHID_MSG          0x83
#define CTAPHID_CBOR         0x90   // 0x10 | 0x80
#define CTAPHID_INIT         0x86
#define CTAPHID_KEEPALIVE    0xBB
#define CTAPHID_ERROR        0xBF
#define CTAPHID_STATUS_PROCESSING  1
#define CTAPHID_STATUS_UPNEEDED    2
#define CTAPHID_BROADCAST_CID 0xFFFFFFFF
#define CTAPHID_CAP_CBOR     0x04
#define CTAP_PACKET_SIZE     64

// ─── HID report descriptors ───────────────────────────────────────────────
uint8_t const desc_keyboard_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD()
};

// FIDO2: Usage Page 0xF1D0, 64-byte IN+OUT, no Report ID
uint8_t const desc_fido_report[] = {
  0x06, 0xD0, 0xF1, 0x09, 0x01, 0xA1, 0x01,
  0x09, 0x20, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x40, 0x81, 0x02,
  0x09, 0x21, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x40, 0x91, 0x02,
  0xC0
};

// ─── HID interface objects ────────────────────────────────────────────────
Adafruit_USBD_HID usb_kbd;
Adafruit_USBD_HID usb_fido;
bool kbd_ok  = false;
bool fido_ok = false;
volatile bool fidoBusy = false;   // suppress keyboard while FIDO2 is active

// ─── CTAPHID channel state ────────────────────────────────────────────────
uint32_t nextCID = 0x00000001;

// ─── CTAP reassembly buffer ───────────────────────────────────────────────
#define CTAP_MAX_PAYLOAD 1024
struct {
  uint32_t cid; uint8_t cmd;
  uint16_t bcnt, received;
  uint8_t  buf[CTAP_MAX_PAYLOAD];
  bool     active;
} ctap_asm;

// ─── RX ring buffer (ISR-safe SPSC, lock-free) ───────────────────────────
#define RX_QUEUE_DEPTH 8
struct RxPacket { uint8_t data[CTAP_PACKET_SIZE]; uint16_t len; };
static RxPacket     rx_queue[RX_QUEUE_DEPTH];
static volatile int rx_head = 0, rx_tail = 0;

// ─── Utilities ───────────────────────────────────────────────────────────

void hexDump(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  for (size_t i = 0; i < len; i++) {
    if (i % 16 == 0) { Serial.println(); Serial.print("    "); }
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX); Serial.print(' ');
  }
  Serial.println();
}

// ─── CTAPHID packet senders ───────────────────────────────────────────────

// Single init packet (fits in one 64-byte HID report, payload ≤ 57 bytes)
bool sendCTAPHIDResponse(uint32_t cid, uint8_t cmd,
                          const uint8_t* payload, uint16_t payloadLen) {
  uint8_t pkt[CTAP_PACKET_SIZE] = {0};
  pkt[0] = (cid>>24)&0xFF; pkt[1] = (cid>>16)&0xFF;
  pkt[2] = (cid>> 8)&0xFF; pkt[3] =  cid     &0xFF;
  pkt[4] = cmd;
  pkt[5] = (payloadLen>>8)&0xFF; pkt[6] = payloadLen&0xFF;
  if (payload && payloadLen > 0) {
    uint16_t n = payloadLen < 57 ? payloadLen : 57;
    memcpy(pkt + 7, payload, n);
  }
  return usb_fido.sendReport(0, pkt, CTAP_PACKET_SIZE);
}

// Send a single HID report with retry — waits for endpoint to be ready.
static bool sendReportRetry(const uint8_t* pkt, int maxRetries = 50) {
  for (int attempt = 0; attempt < maxRetries; attempt++) {
    if (usb_fido.sendReport(0, pkt, CTAP_PACKET_SIZE)) return true;
    delay(5);
  }
  Serial.println("[FIDO2] sendReport FAILED after retries");
  return false;
}

// Multi-packet sender — init packet + continuation packets as needed.
// Used for MakeCredential (≈160 bytes) and GetAssertion (≈120 bytes).
bool sendCTAPHIDLarge(uint32_t cid, uint8_t cmd,
                       const uint8_t* payload, uint16_t payloadLen) {
  uint8_t  pkt[CTAP_PACKET_SIZE];
  uint16_t sent = 0;
  uint8_t  seq  = 0;

  // Init packet: 7-byte header + up to 57 bytes
  memset(pkt, 0, CTAP_PACKET_SIZE);
  pkt[0]=(cid>>24)&0xFF; pkt[1]=(cid>>16)&0xFF;
  pkt[2]=(cid>> 8)&0xFF; pkt[3]= cid     &0xFF;
  pkt[4]=cmd;
  pkt[5]=(payloadLen>>8)&0xFF; pkt[6]=payloadLen&0xFF;
  uint16_t chunk = payloadLen < 57 ? payloadLen : 57;
  memcpy(pkt + 7, payload, chunk);
  sent = chunk;
  if (!sendReportRetry(pkt)) return false;

  // Continuation packets: 5-byte header + up to 59 bytes
  while (sent < payloadLen) {
    delay(3);
    memset(pkt, 0, CTAP_PACKET_SIZE);
    pkt[0]=(cid>>24)&0xFF; pkt[1]=(cid>>16)&0xFF;
    pkt[2]=(cid>> 8)&0xFF; pkt[3]= cid     &0xFF;
    pkt[4] = seq++;                              // bit7=0 → continuation
    chunk = (payloadLen - sent) < 59 ? (payloadLen - sent) : 59;
    memcpy(pkt + 5, payload + sent, chunk);
    sent += chunk;
    if (!sendReportRetry(pkt)) return false;
  }
  Serial.printf("[FIDO2] TX CID=0x%08X CMD=0x%02X BCNT=%d\n", cid, cmd, payloadLen);
  return true;
}

void sendCTAPError(uint32_t cid, uint8_t errCode) {
  uint8_t p[1] = { errCode };
  sendCTAPHIDResponse(cid, CTAPHID_ERROR, p, 1);
}

// ─── Drain stale RX packets ──────────────────────────────────────────────
// After waitForUserPresence blocks loop(), packets pile up in the ring buffer.
// Drain them and check if Windows sent CANCEL.
bool drainStalePackets() {
  bool cancelled = false;
  while (rx_tail != rx_head) {
    uint16_t len = rx_queue[rx_tail].len;
    if (len >= 5 && rx_queue[rx_tail].data[4] == 0x91) {
      cancelled = true;
      Serial.println("[FIDO2] CANCEL found in stale queue");
    }
    rx_tail = (rx_tail + 1) % RX_QUEUE_DEPTH;
  }
  ctap_asm.active = false;  // reset reassembly state
  return cancelled;
}

// ─── User Presence check (BOOT button = GPIO 0) ──────────────────────────
// Blinks built-in LED and waits for BOOT button press.
// Returns true if user pressed the button, false on timeout.

bool waitForUserPresence(uint32_t cid, uint32_t timeoutMs) {
  Serial.println("[FIDO2] >>> Press BOOT button to confirm <<<");
  uint32_t start = millis();
  uint32_t lastKeepalive = 0;
  bool ledState = false;

  // Wait for button to be released first (in case already held)
  while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    if (millis() - start > timeoutMs) {
      Serial.println("[FIDO2] UP timeout (button held)");
      return false;
    }
    delay(10);
  }

  // Now wait for press — send KEEPALIVE every 100ms
  while (millis() - start < timeoutMs) {
    // Check for CTAPHID_CANCEL from host
    if (rx_tail != rx_head) {
      int peek = rx_tail;
      if (rx_queue[peek].len >= 5 && rx_queue[peek].data[4] == 0x91) {
        rx_tail = (rx_tail + 1) % RX_QUEUE_DEPTH;
        Serial.println("[FIDO2] CANCEL received during UP wait");
        #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
        #endif
        return false;
      }
    }

    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      delay(30);  // debounce
      if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        Serial.println("[FIDO2] User presence confirmed (BOOT button)");
        // Wait for button release to prevent keyboard trigger in loop()
        while (digitalRead(BOOT_BUTTON_PIN) == LOW) delay(10);
        delay(20);  // short debounce after release
        #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
        #endif
        return true;
      }
    }

    // Send CTAPHID_KEEPALIVE every 100ms — tells host we're waiting for UP
    uint32_t elapsed = millis() - start;
    if (elapsed - lastKeepalive >= 100) {
      lastKeepalive = elapsed;
      uint8_t status = CTAPHID_STATUS_UPNEEDED;
      sendCTAPHIDResponse(cid, CTAPHID_KEEPALIVE, &status, 1);
    }

    // Blink LED at ~2 Hz
    #ifdef LED_BUILTIN
    if ((millis() - start) % 250 < 125) {
      if (!ledState) { digitalWrite(LED_BUILTIN, HIGH); ledState = true; }
    } else {
      if (ledState) { digitalWrite(LED_BUILTIN, LOW); ledState = false; }
    }
    #endif
    delay(10);
  }
  Serial.println("[FIDO2] UP timeout");
  #ifdef LED_BUILTIN
  digitalWrite(LED_BUILTIN, LOW);
  #endif
  return false;
}

// ─── CTAPHID command handlers ─────────────────────────────────────────────

void handleInit(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  Serial.println("[FIDO2] CTAPHID_INIT");
  if (bcnt < 8) { sendCTAPError(CTAPHID_BROADCAST_CID, CTAP1_ERR_INVALID_LEN); return; }
  uint32_t newCID = nextCID++;
  uint8_t resp[17];
  memcpy(resp, payload, 8);
  resp[8]=(newCID>>24)&0xFF; resp[9]=(newCID>>16)&0xFF;
  resp[10]=(newCID>>8)&0xFF; resp[11]=newCID&0xFF;
  resp[12]=2; resp[13]=1; resp[14]=0; resp[15]=0;
  resp[16]=CTAPHID_CAP_CBOR;
  sendCTAPHIDResponse(CTAPHID_BROADCAST_CID, CTAPHID_INIT, resp, 17);
  Serial.printf("  -> CID=0x%08X assigned\n", newCID);
}

void handlePing(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  Serial.printf("[FIDO2] PING %d bytes\n", bcnt);
  sendCTAPHIDResponse(cid, CTAPHID_PING, payload, bcnt);
}

void handleMsg(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  Serial.println("[FIDO2] MSG (U2F) — not supported");
  sendCTAPError(cid, CTAP1_ERR_INVALID_CMD);
}

// ─── CTAP2: authenticatorGetInfo (0x04) ──────────────────────────────────

void handleGetInfo(uint32_t cid) {
  Serial.println("[FIDO2] GetInfo");
  // Build CTAP2 GetInfo response with fields Windows requires:
  //   1 (versions): ["FIDO_2_0"]
  //   3 (aaguid):   16 zero bytes
  //   4 (options):  {up:true, uv:true, plat:false, rk:true}
  //   7 (maxMsgSize): 1024
  uint8_t r[128];
  size_t p = 0;
  r[p++] = 0x00;  // status OK
  r[p++] = 0xA4;  // 4-item map

  // key 1: versions array
  p += cbor_enc_uint(r+p, 1);
  r[p++] = 0x81;  // 1-element array
  p += cbor_enc_text(r+p, "FIDO_2_0");

  // key 3: aaguid (16 zero bytes)
  p += cbor_enc_uint(r+p, 3);
  p += cbor_enc_bytes(r+p, (const uint8_t*)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);

  // key 4: options map {up:true, uv:true, plat:false, rk:true}
  p += cbor_enc_uint(r+p, 4);
  r[p++] = 0xA4;  // 4-item map
  p += cbor_enc_text(r+p, "up");   r[p++] = 0xF5; // true
  p += cbor_enc_text(r+p, "uv");   r[p++] = 0xF5; // true
  p += cbor_enc_text(r+p, "plat"); r[p++] = 0xF4; // false
  p += cbor_enc_text(r+p, "rk");   r[p++] = 0xF5; // true

  // key 7: maxMsgSize
  p += cbor_enc_uint(r+p, 7);
  p += cbor_enc_uint(r+p, 1024);

  sendCTAPHIDLarge(cid, CTAPHID_CBOR, r, (uint16_t)p);
}

// ─── CTAP2: authenticatorMakeCredential (0x01) ───────────────────────────
// Reference: f:\code\uru-card\src\fido2\authenticator\makecredential.cpp
//
// authData layout (148 bytes with AT flag):
//  [0-31]  rpIdHash
//  [32]    flags = 0x45 (UP|UV|AT)
//  [33-36] signCount = 0 (big-endian)
//  [37-52] aaguid (zeros)
//  [53-54] credentialIdLen = 0x0010
//  [55-70] credentialId (16 bytes random)
//  [71-147] COSE public key (77 bytes)

void handleMakeCredential(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  Serial.println("[FIDO2] === MakeCredential ===");
  if (bcnt < 2) { sendCTAPError(cid, CTAP2_ERR_INVALID_CBOR); return; }

  // User Presence check — wait for BOOT button (sends KEEPALIVE to host)
  fidoBusy = true;
  if (!waitForUserPresence(cid, UP_TIMEOUT_MS)) {
    fidoBusy = false;
    drainStalePackets();
    sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
    return;
  }
  // Drain any packets that arrived during the UP wait
  if (drainStalePackets()) {
    fidoBusy = false;
    sendCTAPError(cid, 0x2D); // CTAP2_ERR_KEEPALIVE_CANCEL
    return;
  }

  const uint8_t* cbor    = payload + 1;
  size_t         cborLen = bcnt - 1;

  // 1. clientDataHash (key 0x01)
  uint8_t clientDataHash[32];
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x01, &v, &va) ||
        cbor_get_bytes(v, va, clientDataHash, 32) != 32) {
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM); return;
    }
  }

  // 2. rpId from rp map (key 0x02 -> "id")
  char rpId[128] = {};
  {
    const uint8_t* rpMap; size_t rpLen;
    const uint8_t* v;     size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x02, &rpMap, &rpLen) ||
        !cbor_map_find_text(rpMap, rpLen, "id", &v, &va) ||
        !cbor_get_text(v, va, rpId, sizeof(rpId))) {
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM); return;
    }
  }
  Serial.printf("  rpId: %s\n", rpId);

  // 3. Verify ES256 (-7) in pubKeyCredParams (key 0x04)
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
    if (!ok) { sendCTAPError(cid, CTAP2_ERR_UNSUPPORTED_ALG); return; }
  }

  // 4. rpIdHash
  uint8_t rpIdHash[32];
  crypto_sha256((const uint8_t*)rpId, strlen(rpId), rpIdHash);

  // 5. Allocate credential slot
  Cred* cred = cred_alloc(rpIdHash);
  if (!cred) { sendCTAPError(cid, CTAP2_ERR_KEY_STORE_FULL); return; }

  // 6. Generate P-256 key pair
  uint8_t pub_x[32], pub_y[32];
  if (!crypto_generate_p256(cred->privKey, pub_x, pub_y)) {
    Serial.println("  ERR: keygen failed");
    sendCTAPError(cid, 0x7F); return;
  }
  memcpy(cred->pubX, pub_x, 32);
  memcpy(cred->pubY, pub_y, 32);
  esp_fill_random(cred->id, CRED_ID_LEN);
  memcpy(cred->rpIdHash, rpIdHash, 32);
  cred->signCount = 0;
  cred->valid     = true;

  // 7. Build authenticatorData (148 bytes)
  uint8_t authData[148] = {};
  memcpy(authData, rpIdHash, 32);           // rpIdHash
  authData[32] = 0x45;                       // flags: UP|UV|AT
  // [33-36] signCount = 0x00000000
  // [37-52] aaguid = zeros
  authData[53] = 0x00;
  authData[54] = CRED_ID_LEN;               // credentialIdLen = 16
  memcpy(authData + 55, cred->id, CRED_ID_LEN);
  cbor_enc_cose_key(authData + 71, pub_x, pub_y); // 77 bytes

  Serial.print("  credId: ");
  for (int i = 0; i < CRED_ID_LEN; i++) Serial.printf("%02X", cred->id[i]);
  Serial.println();

  // 8. Sign: DER(ECDSA(SHA256(authData || clientDataHash)))
  uint8_t sig_der[72]; size_t sig_len = 72;
  if (!crypto_sign_p256(cred->privKey, authData, sizeof(authData),
                         clientDataHash, sig_der, &sig_len)) {
    Serial.println("  ERR: signing failed");
    sendCTAPError(cid, 0x7F); return;
  }
  Serial.printf("  Signature: %d bytes (DER)\n", (int)sig_len);

  // 9. Save to NVS
  cred_save(cred);

  // 10. Encode "none" attestation response and send
  uint8_t resp[300];
  size_t  respLen = cbor_enc_make_credential_resp(resp, authData, sizeof(authData));
  sendCTAPHIDLarge(cid, CTAPHID_CBOR, resp, (uint16_t)respLen);
  fidoBusy = false;
  Serial.println("[FIDO2] MakeCredential DONE \xe2\x9c\x93");
}

// ─── CTAP2: authenticatorGetAssertion (0x02) ──────────────────────────────
// Reference: f:\code\uru-card\src\fido2\authenticator\getassertion.cpp
//
// authData layout (37 bytes, no AT flag):
//  [0-31]  rpIdHash
//  [32]    flags = 0x05 (UP|UV)
//  [33-36] signCount (big-endian, incremented)

void handleGetAssertion(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  Serial.println("[FIDO2] === GetAssertion ===");
  if (bcnt < 2) { sendCTAPError(cid, CTAP2_ERR_INVALID_CBOR); return; }

  // User Presence check — wait for BOOT button (sends KEEPALIVE to host)
  fidoBusy = true;
  if (!waitForUserPresence(cid, UP_TIMEOUT_MS)) {
    fidoBusy = false;
    drainStalePackets();
    sendCTAPError(cid, CTAP2_ERR_OPERATION_DENIED);
    return;
  }
  // Drain any packets that arrived during the UP wait
  if (drainStalePackets()) {
    fidoBusy = false;
    sendCTAPError(cid, 0x2D); // CTAP2_ERR_KEEPALIVE_CANCEL
    return;
  }

  const uint8_t* cbor    = payload + 1;
  size_t         cborLen = bcnt - 1;

  // 1. rpId (key 0x01)
  char rpId[128] = {};
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x01, &v, &va) ||
        !cbor_get_text(v, va, rpId, sizeof(rpId))) {
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM); return;
    }
  }
  Serial.printf("  rpId: %s\n", rpId);

  // 2. clientDataHash (key 0x02)
  uint8_t clientDataHash[32];
  {
    const uint8_t* v; size_t va;
    if (!cbor_map_find_uint(cbor, cborLen, 0x02, &v, &va) ||
        cbor_get_bytes(v, va, clientDataHash, 32) != 32) {
      sendCTAPError(cid, CTAP2_ERR_MISSING_PARAM); return;
    }
  }

  // 3. Find credential (try allowList first, then rpIdHash fallback)
  uint8_t rpIdHash[32];
  crypto_sha256((const uint8_t*)rpId, strlen(rpId), rpIdHash);
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
  if (!cred) cred = cred_find_by_rpidhash(rpIdHash);  // fallback

  if (!cred) {
    Serial.println("  ERR: no matching credential");
    sendCTAPError(cid, CTAP2_ERR_NO_CREDENTIALS); return;
  }
  Serial.print("  credId: ");
  for (int i = 0; i < CRED_ID_LEN; i++) Serial.printf("%02X", cred->id[i]);
  Serial.println();

  // 4. Build short authenticatorData (37 bytes)
  uint8_t authData[37] = {};
  memcpy(authData, rpIdHash, 32);
  authData[32] = 0x05; // UP|UV
  cred->signCount++;
  authData[33] = (cred->signCount >> 24) & 0xFF;
  authData[34] = (cred->signCount >> 16) & 0xFF;
  authData[35] = (cred->signCount >>  8) & 0xFF;
  authData[36] =  cred->signCount        & 0xFF;

  // 5. Sign
  uint8_t sig_der[72]; size_t sig_len = 72;
  if (!crypto_sign_p256(cred->privKey, authData, sizeof(authData),
                         clientDataHash, sig_der, &sig_len)) {
    Serial.println("  ERR: signing failed");
    sendCTAPError(cid, 0x7F); return;
  }
  cred_update_counter(cred);

  // 6. Encode CBOR response and send
  uint8_t resp[300];
  size_t  respLen = cbor_enc_get_assertion_resp(resp,
                                                 cred->id, CRED_ID_LEN,
                                                 authData, sizeof(authData),
                                                 sig_der, sig_len);
  sendCTAPHIDLarge(cid, CTAPHID_CBOR, resp, (uint16_t)respLen);
  fidoBusy = false;
  Serial.printf("[FIDO2] GetAssertion DONE \xe2\x9c\x93 (signCount=%d)\n", cred->signCount);
}

// ─── CTAPHID_CBOR dispatcher ──────────────────────────────────────────────

void handleCBOR(uint32_t cid, const uint8_t* payload, uint16_t bcnt) {
  if (bcnt < 1) { sendCTAPError(cid, CTAP1_ERR_INVALID_LEN); return; }
  uint8_t cmd = payload[0];
  Serial.printf("[FIDO2] CBOR cmd=0x%02X\n", cmd);
  switch (cmd) {
    case CTAP2_CMD_MAKE_CREDENTIAL: handleMakeCredential(cid, payload, bcnt); break;
    case CTAP2_CMD_GET_ASSERTION:   handleGetAssertion  (cid, payload, bcnt); break;
    case CTAP2_CMD_GET_INFO:        handleGetInfo(cid);                        break;
    default:
      Serial.printf("  cmd 0x%02X not implemented\n", cmd);
      { uint8_t e[1]={0x27}; sendCTAPHIDResponse(cid, CTAPHID_CBOR, e, 1); }
  }
}

// ─── FIDO2 RX callback (called from TinyUSB task) ─────────────────────────

void fido_set_report(uint8_t, hid_report_type_t,
                     uint8_t const* buffer, uint16_t bufsize) {
  if (!bufsize) return;
  int next = (rx_head + 1) % RX_QUEUE_DEPTH;
  if (next == rx_tail) return; // queue full, drop
  uint16_t n = bufsize < CTAP_PACKET_SIZE ? bufsize : CTAP_PACKET_SIZE;
  memcpy(rx_queue[rx_head].data, buffer, n);
  rx_queue[rx_head].len = n;
  rx_head = next;
}

// ─── setup() ─────────────────────────────────────────────────────────────

void setup() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  #endif

  TinyUSBDevice.setManufacturerDescriptor("DIY Security");
  TinyUSBDevice.setProductDescriptor("FIDO2 Macropad");

  usb_kbd.setPollInterval(2);
  usb_kbd.setReportDescriptor(desc_keyboard_report, sizeof(desc_keyboard_report));
  usb_kbd.setStringDescriptor("FIDO2 Macropad Keyboard");
  usb_kbd.setReportCallback(NULL, [](uint8_t, hid_report_type_t, uint8_t const*, uint16_t sz){
    Serial.printf("[KBD-DIAG] data on kbd instance! %d bytes\n", sz);
  });
  kbd_ok = usb_kbd.begin();

  usb_fido.setPollInterval(5);
  usb_fido.setReportDescriptor(desc_fido_report, sizeof(desc_fido_report));
  usb_fido.setStringDescriptor("FIDO2 Authenticator");
  usb_fido.setReportCallback(NULL, fido_set_report);
  usb_fido.enableOutEndpoint(true);
  fido_ok = usb_fido.begin();

  Serial.begin(115200);

  crypto_init();   // from crypto_p256.ino
  cred_load();     // from cred_store.ino

  Serial.println();
  Serial.println("=========================================");
  Serial.println("  USB Composite HID — CTAP2 Phase 2");
  Serial.println("  Keyboard + FIDO2 Authenticator");
  Serial.println("=========================================");
  Serial.printf("[USB] Keyboard: %s\n", kbd_ok  ? "OK" : "FAILED");
  Serial.printf("[USB] FIDO2:    %s\n", fido_ok ? "OK" : "FAILED");

  Serial.print("[USB] Waiting for mount...");
  uint32_t t0 = millis();
  while (!TinyUSBDevice.mounted()) {
    delay(10);
    if (millis() - t0 > 8000) { Serial.println(" TIMEOUT"); return; }
  }
  Serial.println(" MOUNTED!");
  Serial.println("[USB] Device MOUNTED - both interfaces active!");
  Serial.println("[FIDO2] Ready. Run: python tools/test_phase2.py");
}

// ─── loop() ──────────────────────────────────────────────────────────────

void loop() {
  static bool     lastBtn   = HIGH;
  static uint32_t lastHeart = 0;

  // BOOT button → keyboard test (suppressed during FIDO2 auth)
  bool btn = digitalRead(BOOT_BUTTON_PIN);
  if (kbd_ok && !fidoBusy && btn == LOW && lastBtn == HIGH)
    kbdTypeString("FIDO2 Macropad OK!");
  lastBtn = btn;

  // Drain FIDO2 RX ring buffer
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
      ctap_asm = {cid, b4, bcnt, first, {}, true};
      memcpy(ctap_asm.buf, pkt.data+7, first);
      Serial.printf("\n[FIDO2] PKT CID=0x%08X CMD=0x%02X BCNT=%d\n", cid, b4, bcnt);
      if (ctap_asm.received >= ctap_asm.bcnt) {
        ctap_asm.active = false;
        switch (b4) {
          case CTAPHID_INIT: handleInit(cid, ctap_asm.buf, bcnt); break;
          case CTAPHID_PING: handlePing(cid, ctap_asm.buf, bcnt); break;
          case CTAPHID_CBOR: handleCBOR(cid, ctap_asm.buf, bcnt); break;
          case CTAPHID_MSG:  handleMsg (cid, ctap_asm.buf, bcnt); break;
          case 0x91: Serial.println("[FIDO2] CTAPHID_CANCEL"); break;
          default: sendCTAPError(cid, CTAP1_ERR_INVALID_CMD);
        }
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
          case CTAPHID_MSG:  handleMsg (dc, ctap_asm.buf, dl); break;
          default: sendCTAPError(dc, CTAP1_ERR_INVALID_CMD);
        }
      }
    }
  }

  // Heartbeat every 5 s
  if (millis() - lastHeart > 5000) {
    lastHeart = millis();
    Serial.printf("[LOOP] FIDO2=%s KBD=%s creds=%d\n",
                  usb_fido.ready()?"RDY":"---",
                  usb_kbd.ready() ?"RDY":"---", cred_count);
  }
  delay(10);
}

// ─── Keyboard helper ─────────────────────────────────────────────────────

uint8_t asciiToHID(char c, uint8_t& mod) {
  mod = 0;
  if (c >= 'a' && c <= 'z') return HID_KEY_A + (c-'a');
  if (c >= 'A' && c <= 'Z') { mod=KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_A+(c-'A'); }
  if (c >= '1' && c <= '9') return HID_KEY_1+(c-'1');
  if (c == '0') return HID_KEY_0;   if (c == ' ') return HID_KEY_SPACE;
  if (c == '!') { mod=KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_1; }
  if (c == '.') return HID_KEY_PERIOD;
  if (c == '_') { mod=KEYBOARD_MODIFIER_LEFTSHIFT; return HID_KEY_MINUS; }
  return 0;
}

void kbdTypeString(const char* str) {
  if (!TinyUSBDevice.mounted() || !kbd_ok) return;
  Serial.printf("[KBD] Typing: \"%s\"\n", str);
  for (size_t i = 0; str[i]; i++) {
    uint8_t mod = 0, key = asciiToHID(str[i], mod);
    if (!key) continue;
    uint8_t keys[6]={key,0,0,0,0,0};
    usb_kbd.keyboardReport(0, mod, keys); delay(15);
    usb_kbd.keyboardRelease(0);          delay(15);
  }
  uint8_t enter[6]={HID_KEY_ENTER,0,0,0,0,0};
  usb_kbd.keyboardReport(0,0,enter); delay(15);
  usb_kbd.keyboardRelease(0);
}
