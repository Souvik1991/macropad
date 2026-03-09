/*
 * pin_protocol.cpp — CTAP2 PIN/UV Protocol v1
 * Uses mbedtls (ESP32 built-in) for ECDH, AES-256-CBC, HMAC-SHA-256
 */

#include "pin_protocol.h"
#include "debug.h"
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <string.h>
#include <esp_random.h>

static int rng_wrapper(void*, unsigned char* buf, size_t len) {
  esp_fill_random(buf, len);
  return 0;
}

static mbedtls_ecp_group grp;
static mbedtls_mpi authPriv;
static mbedtls_ecp_point authPub;
static uint8_t platformPubX[32];
static uint8_t platformPubY[32];
static bool platformKeySet = false;

size_t pinProtocolGetKeyAgreement(uint8_t* out, size_t maxOut) {
  size_t p = 0;
  size_t olen = 0;
  uint8_t pointBuf[65];  // 0x04 || x(32) || y(32)

  mbedtls_ecp_group_init(&grp);
  mbedtls_mpi_init(&authPriv);
  mbedtls_ecp_point_init(&authPub);

  int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
  if (ret != 0) { debugPrintln("  pin protocol: ecp_group_load failed"); goto fail; }

  ret = mbedtls_ecp_gen_keypair(&grp, &authPriv, &authPub, rng_wrapper, NULL);
  if (ret != 0) { debugPrintln("  pin protocol: ecp_gen_keypair failed"); goto fail; }

  // Write public key as COSE: {1:2, 3:-25, -1:1, -2:x, -3:y}
  if (maxOut < 77) goto fail;

  ret = mbedtls_ecp_point_write_binary(&grp, &authPub,
        MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, pointBuf, sizeof(pointBuf));
  if (ret != 0 || olen != 65) goto fail;
  out[p++] = 0xA5;  // 5-item map
  out[p++] = 0x01; out[p++] = 0x02;  // kty: EC2
  out[p++] = 0x03; out[p++] = 0x38; out[p++] = 0x18;  // alg: -25
  out[p++] = 0x20; out[p++] = 0x01;  // crv: P-256
  out[p++] = 0x21; out[p++] = 0x58; out[p++] = 0x20;  // -2: x (32 bytes)
  memcpy(out + p, pointBuf + 1, 32);
  p += 32;
  out[p++] = 0x22; out[p++] = 0x58; out[p++] = 0x20;  // -3: y (32 bytes)
  memcpy(out + p, pointBuf + 33, 32);
  p += 32;

  platformKeySet = false;
  return p;

fail:
  mbedtls_ecp_point_free(&authPub);
  mbedtls_mpi_free(&authPriv);
  mbedtls_ecp_group_free(&grp);
  return 0;
}

bool pinProtocolSetPlatformKey(const uint8_t* coseKey, size_t len) {
  if (!coseKey || len < 70) return false;
  // COSE key: scan for -2 (0x21 0x58 0x20 <32>) and -3 (0x22 0x58 0x20 <32>)
  bool gotX = false, gotY = false;
  for (size_t i = 0; i + 35 <= len; i++) {
    if (coseKey[i] == 0x21 && coseKey[i+1] == 0x58 && coseKey[i+2] == 0x20) {
      memcpy(platformPubX, coseKey + i + 3, 32);
      gotX = true;
    } else if (coseKey[i] == 0x22 && coseKey[i+1] == 0x58 && coseKey[i+2] == 0x20) {
      memcpy(platformPubY, coseKey + i + 3, 32);
      gotY = true;
    }
  }
  platformKeySet = (gotX && gotY);
  return platformKeySet;
}

bool pinProtocolDeriveSharedSecret(uint8_t* sharedSecret, size_t maxLen) {
  if (!platformKeySet || maxLen < 32) return false;

  uint8_t platformPoint[65];
  platformPoint[0] = 0x04;
  memcpy(platformPoint + 1, platformPubX, 32);
  memcpy(platformPoint + 33, platformPubY, 32);

  mbedtls_ecp_point platformPub;
  mbedtls_mpi z;
  uint8_t zBin[32];
  mbedtls_sha256_context sha;

  mbedtls_ecp_point_init(&platformPub);
  mbedtls_mpi_init(&z);
  mbedtls_sha256_init(&sha);

  int ret = mbedtls_ecp_point_read_binary(&grp, &platformPub, platformPoint, 65);
  if (ret != 0) { debugPrintln("  pin protocol: point_read_binary failed"); goto fail; }

  ret = mbedtls_ecdh_compute_shared(&grp, &z, &platformPub, &authPriv, rng_wrapper, NULL);
  if (ret != 0) { debugPrintln("  pin protocol: ecdh failed"); goto fail; }

  ret = mbedtls_mpi_write_binary(&z, zBin, 32);
  if (ret != 0) goto fail;

  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, zBin, 32);
  mbedtls_sha256_finish(&sha, sharedSecret);

  mbedtls_sha256_free(&sha);
  mbedtls_mpi_free(&z);
  mbedtls_ecp_point_free(&platformPub);
  return true;

fail:
  mbedtls_sha256_free(&sha);
  mbedtls_mpi_free(&z);
  mbedtls_ecp_point_free(&platformPub);
  return false;
}

void pinProtocolAuthenticate(const uint8_t* key, size_t keyLen,
                             const uint8_t* msg, size_t msgLen,
                             uint8_t* out) {
  uint8_t hmac[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  key, keyLen, msg, msgLen, hmac);
  memcpy(out, hmac, 16);
}

bool pinProtocolEncrypt(const uint8_t* key, size_t keyLen,
                        const uint8_t* plain, size_t plainLen,
                        uint8_t* cipher, size_t* cipherLen) {
  if (keyLen != 32 || plainLen % 16 != 0) return false;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  int ret = mbedtls_aes_setkey_enc(&aes, key, 256);
  if (ret != 0) { mbedtls_aes_free(&aes); return false; }
  uint8_t iv[16] = {0};
  ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, plainLen, iv, plain, cipher);
  mbedtls_aes_free(&aes);
  if (ret != 0) return false;
  *cipherLen = plainLen;
  return true;
}

bool pinProtocolDecrypt(const uint8_t* key, size_t keyLen,
                        const uint8_t* cipher, size_t cipherLen,
                        uint8_t* plain, size_t* plainLen) {
  if (keyLen != 32 || cipherLen % 16 != 0) return false;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  int ret = mbedtls_aes_setkey_dec(&aes, key, 256);
  if (ret != 0) { mbedtls_aes_free(&aes); return false; }
  uint8_t iv[16] = {0};
  ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, cipher, plain);
  mbedtls_aes_free(&aes);
  if (ret != 0) return false;
  *plainLen = cipherLen;
  return true;
}
