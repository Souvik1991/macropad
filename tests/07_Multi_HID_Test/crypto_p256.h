/*
 * crypto_p256.h — P-256 ECDSA API (mbedtls, public API only)
 *
 * Compatible with mbedtls 2.x and 3.x (no MBEDTLS_PRIVATE required).
 * Implementation: crypto_p256.cpp
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

// Call once in setup() — initialises entropy source and CTR-DRBG.
void crypto_init();

// SHA-256 of a single buffer.
void crypto_sha256(const uint8_t* data, size_t len, uint8_t out[32]);

// Generate a P-256 key pair.
//   priv[32]  — raw private scalar (store in NVS)
//   pub_x[32] — public key X coordinate
//   pub_y[32] — public key Y coordinate
// Returns true on success.
bool crypto_generate_p256(uint8_t priv[32], uint8_t pub_x[32], uint8_t pub_y[32]);

// Sign authData || clientDataHash with a stored P-256 private key.
// Output is a DER-encoded ECDSA signature (max 72 bytes).
// sig_der must be >= 72 bytes; sig_len receives actual length.
// Returns true on success.
bool crypto_sign_p256(const uint8_t priv[32],
                       const uint8_t* authData,       size_t authDataLen,
                       const uint8_t  clientDataHash[32],
                       uint8_t*       sig_der,  size_t* sig_len);
