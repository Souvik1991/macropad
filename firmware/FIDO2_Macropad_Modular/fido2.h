/*
 * FIDO2/CTAP2 Module - Header
 * Full CTAP2 protocol with ATECC608A crypto + fingerprint auth
 */

#ifndef FIDO2_H
#define FIDO2_H

#include <stdint.h>
#include <stdbool.h>

// ─── CTAP2 error codes ───────────────────────────────────────────────────
#define CTAP2_OK                  0x00
#define CTAP1_ERR_INVALID_CMD     0x01
#define CTAP1_ERR_INVALID_LEN     0x03
#define CTAP2_ERR_INVALID_CBOR    0x12
#define CTAP2_ERR_MISSING_PARAM   0x14
#define CTAP2_ERR_UNSUPPORTED_ALG 0x26
#define CTAP2_ERR_OPERATION_DENIED 0x27
#define CTAP2_ERR_KEY_STORE_FULL  0x28
#define CTAP2_ERR_PIN_NOT_SET     0x30  // Used for Windows SelectDevice probe (device selected)
#define CTAP2_ERR_NO_CREDENTIALS  0x2E

// ─── CTAP2 command IDs ───────────────────────────────────────────────────
#define CTAP2_CMD_MAKE_CREDENTIAL 0x01
#define CTAP2_CMD_GET_ASSERTION   0x02
#define CTAP2_CMD_GET_INFO        0x04

// ─── CTAPHID transport constants ─────────────────────────────────────────
#define CTAPHID_PING         0x81
#define CTAPHID_MSG          0x83
#define CTAPHID_CBOR         0x90
#define CTAPHID_INIT         0x86
#define CTAPHID_KEEPALIVE    0xBB
#define CTAPHID_ERROR        0xBF
#define CTAPHID_STATUS_PROCESSING  1
#define CTAPHID_STATUS_UPNEEDED    2
#define CTAPHID_BROADCAST_CID 0xFFFFFFFF
#define CTAPHID_CAP_CBOR     0x04

// ─── Fingerprint auth config ─────────────────────────────────────────────
#define FP_AUTH_MAX_RETRIES   3
#define FP_AUTH_TIMEOUT_MS    30000

// ─── Public API ──────────────────────────────────────────────────────────
void initFIDO2();              // Called once in setup() — loads credentials
void checkFIDO2Requests();     // Called in loop() — processes CTAPHID packets
void handleMakeCredential(uint32_t cid, const uint8_t* payload, uint16_t bcnt);
void handleGetAssertion(uint32_t cid, const uint8_t* payload, uint16_t bcnt);
void handleGetInfo(uint32_t cid);
void handleReset();

// Fingerprint authentication with 3-retry logic + OLED feedback + KEEPALIVE
bool waitForFingerprintAuth(uint32_t cid, uint32_t timeoutMs);

#endif // FIDO2_H
