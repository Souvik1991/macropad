/*
 * crypto_p256.cpp — P-256 ECDSA implementation via mbedtls
 *
 * Uses ONLY public mbedtls API — no MBEDTLS_PRIVATE() needed.
 * Works with mbedtls 2.x (ESP32 Arduino core ≤2.x) AND
 *            mbedtls 3.x (ESP32 Arduino core ≥3.x / ESP-IDF 5.x).
 *
 * DER encoding logic adapted from:
 *   f:\code\uru-card\src\crypto\ecdsa.cpp :: encodeSignature()
 */
#include <Arduino.h>
#include "crypto_p256.h"

#include <mbedtls/ecp.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/sha256.h>

static mbedtls_entropy_context  _entropy;
static mbedtls_ctr_drbg_context _ctr_drbg;

// ─── Init ────────────────────────────────────────────────────────────────
void crypto_init() {
  mbedtls_entropy_init(&_entropy);
  mbedtls_ctr_drbg_init(&_ctr_drbg);
  const char* pers = "fido2_macropad_v1";
  int ret = mbedtls_ctr_drbg_seed(&_ctr_drbg, mbedtls_entropy_func, &_entropy,
                                    (const uint8_t*)pers, strlen(pers));
  Serial.printf("[CRYPTO] mbedtls init %s\n", ret == 0 ? "OK" : "FAILED");
}

// ─── SHA-256 ─────────────────────────────────────────────────────────────
void crypto_sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, data, len);
  mbedtls_sha256_finish(&sha, out);
  mbedtls_sha256_free(&sha);
}

// ─── DER-encode raw (r‖s) → ASN.1 signature ─────────────────────────────
// Lifted from uru-card/src/crypto/ecdsa.cpp :: encodeSignature()
// r_bin, s_bin are 32-byte big-endian. out must be >= 72 bytes.
static size_t _der_encode_rs(const uint8_t r[32], const uint8_t s[32], uint8_t* out) {
  memset(out, 0, 72);

  uint8_t lead_s = 0;
  for (; lead_s < 32 && s[lead_s] == 0; lead_s++);
  int pad_s = ((s[lead_s] & 0x80) == 0x80) ? 1 : 0;

  uint8_t lead_r = 0;
  for (; lead_r < 32 && r[lead_r] == 0; lead_r++);
  int pad_r = ((r[lead_r] & 0x80) == 0x80) ? 1 : 0;

  out[0] = 0x30;
  out[1] = (uint8_t)(0x44 + pad_s + pad_r - lead_s - lead_r);

  // R
  out[2] = 0x02;
  out[3] = (uint8_t)(0x20 + pad_r - lead_r);
  if (pad_r) out[4] = 0x00;
  memmove(out + 4 + pad_r, r + lead_r, 32 - lead_r);

  // S
  size_t s_off = 4 + pad_r + (32 - lead_r);
  out[s_off]   = 0x02;
  out[s_off+1] = (uint8_t)(0x20 + pad_s - lead_s);
  if (pad_s) out[s_off+2] = 0x00;
  memmove(out + s_off + 2 + pad_s, s + lead_s, 32 - lead_s);

  return (size_t)(2 + out[1]);
}

// ─── Key generation ──────────────────────────────────────────────────────
bool crypto_generate_p256(uint8_t priv[32], uint8_t pub_x[32], uint8_t pub_y[32]) {
  mbedtls_ecp_group grp;
  mbedtls_mpi       d;
  mbedtls_ecp_point Q;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d);
  mbedtls_ecp_point_init(&Q);

  int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
  if (ret == 0) ret = mbedtls_ecp_gen_keypair(&grp, &d, &Q,
                                                mbedtls_ctr_drbg_random, &_ctr_drbg);
  if (ret == 0) ret = mbedtls_mpi_write_binary(&d, priv, 32);

  uint8_t pub65[65]; size_t olen = 0;
  if (ret == 0) ret = mbedtls_ecp_point_write_binary(&grp, &Q,
                        MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, pub65, sizeof(pub65));
  if (ret == 0 && olen == 65) {
    memcpy(pub_x, pub65 + 1,  32);
    memcpy(pub_y, pub65 + 33, 32);
  } else { ret = -1; }

  mbedtls_ecp_point_free(&Q);
  mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ret == 0;
}

// ─── Signing ─────────────────────────────────────────────────────────────
bool crypto_sign_p256(const uint8_t priv[32],
                       const uint8_t* authData,          size_t authDataLen,
                       const uint8_t  clientDataHash[32],
                       uint8_t*       sig_der,     size_t* sig_len) {
  // 1. SHA-256(authData || clientDataHash)
  uint8_t hash[32];
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, authData, authDataLen);
  mbedtls_sha256_update(&sha, clientDataHash, 32);
  mbedtls_sha256_finish(&sha, hash);
  mbedtls_sha256_free(&sha);

  // 2. Load key and group (public API only)
  mbedtls_ecp_group grp;
  mbedtls_mpi       d, r, s;
  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&d); mbedtls_mpi_init(&r); mbedtls_mpi_init(&s);

  int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
  if (ret == 0) ret = mbedtls_mpi_read_binary(&d, priv, 32);

  // 3. Sign → (r, s)
  if (ret == 0) ret = mbedtls_ecdsa_sign(&grp, &r, &s, &d,
                                           hash, 32,
                                           mbedtls_ctr_drbg_random, &_ctr_drbg);
  // 4. Export (r, s) and DER-encode
  if (ret == 0) {
    uint8_t r_bin[32], s_bin[32];
    ret  = mbedtls_mpi_write_binary(&r, r_bin, 32);
    ret |= mbedtls_mpi_write_binary(&s, s_bin, 32);
    if (ret == 0) *sig_len = _der_encode_rs(r_bin, s_bin, sig_der);
  }

  mbedtls_mpi_free(&s); mbedtls_mpi_free(&r); mbedtls_mpi_free(&d);
  mbedtls_ecp_group_free(&grp);
  return ret == 0;
}
