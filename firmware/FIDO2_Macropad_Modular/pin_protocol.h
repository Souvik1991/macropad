/*
 * pin_protocol.h — CTAP2 PIN/UV Protocol v1
 * ECDH, AES-256-CBC, HMAC-SHA-256 for ClientPIN
 */

#ifndef PIN_PROTOCOL_H
#define PIN_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PIN_PROTOCOL_MAX_PUBKEY 91   // COSE key size
#define PIN_PROTOCOL_SHARED_LEN 32

// Generate ephemeral ECDH keypair, return public key in COSE format.
// Returns length of public key in out, or 0 on failure.
size_t pinProtocolGetKeyAgreement(uint8_t* out, size_t maxOut);

// Set platform's public key for this session (from keyAgreement in request).
// Call before setPIN, changePIN, getPINToken.
bool pinProtocolSetPlatformKey(const uint8_t* coseKey, size_t len);

// Derive shared secret from ECDH. Call after SetPlatformKey.
// sharedSecret = SHA-256(ECDH_result.x)
bool pinProtocolDeriveSharedSecret(uint8_t* sharedSecret, size_t maxLen);

// Authenticate: HMAC-SHA-256(key, message)[:16]
void pinProtocolAuthenticate(const uint8_t* key, size_t keyLen,
                            const uint8_t* msg, size_t msgLen,
                            uint8_t* out);

// Encrypt: AES-256-CBC with IV=zeros
bool pinProtocolEncrypt(const uint8_t* key, size_t keyLen,
                        const uint8_t* plain, size_t plainLen,
                        uint8_t* cipher, size_t* cipherLen);

// Decrypt: AES-256-CBC with IV=zeros
bool pinProtocolDecrypt(const uint8_t* key, size_t keyLen,
                        const uint8_t* cipher, size_t cipherLen,
                        uint8_t* plain, size_t* plainLen);

#endif // PIN_PROTOCOL_H
