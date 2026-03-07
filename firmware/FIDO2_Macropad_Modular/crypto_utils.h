/*
 * crypto_utils.h — Signature format conversion utilities
 *
 * ATECC608A outputs raw 64-byte (R || S) ECDSA signatures.
 * CTAP2/WebAuthn requires DER-encoded ECDSA signatures.
 * This header-only helper converts between the two formats.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/**
 * Convert raw P-256 ECDSA signature (R || S, 64 bytes) to DER format.
 *
 * DER format: 0x30 <total_len> 0x02 <r_len> <R> 0x02 <s_len> <S>
 * Each integer may need a leading 0x00 pad if the high bit is set.
 *
 * @param raw     Input: 64 bytes (R[32] || S[32])
 * @param der     Output: DER-encoded signature (max 72 bytes)
 * @param derLen  Output: actual DER length
 */
static inline void rawToDER(const uint8_t raw[64], uint8_t* der, size_t* derLen) {
  const uint8_t* r = raw;
  const uint8_t* s = raw + 32;

  // Strip leading zeros from R (but keep at least 1 byte)
  int rOff = 0;
  while (rOff < 31 && r[rOff] == 0) rOff++;
  int rLen = 32 - rOff;
  bool rPad = (r[rOff] & 0x80) != 0;  // need 0x00 prefix if high bit set
  if (rPad) rLen++;

  // Strip leading zeros from S
  int sOff = 0;
  while (sOff < 31 && s[sOff] == 0) sOff++;
  int sLen = 32 - sOff;
  bool sPad = (s[sOff] & 0x80) != 0;
  if (sPad) sLen++;

  int totalLen = 2 + rLen + 2 + sLen;  // 0x02 + len + data for each

  size_t p = 0;
  der[p++] = 0x30;  // SEQUENCE
  der[p++] = (uint8_t)totalLen;

  // R integer
  der[p++] = 0x02;  // INTEGER
  der[p++] = (uint8_t)rLen;
  if (rPad) der[p++] = 0x00;
  memcpy(der + p, r + rOff, 32 - rOff);
  p += 32 - rOff;

  // S integer
  der[p++] = 0x02;  // INTEGER
  der[p++] = (uint8_t)sLen;
  if (sPad) der[p++] = 0x00;
  memcpy(der + p, s + sOff, 32 - sOff);
  p += 32 - sOff;

  *derLen = p;
}
