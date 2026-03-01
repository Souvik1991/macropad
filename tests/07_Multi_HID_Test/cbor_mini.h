/*
 * cbor_mini.h — Minimal CBOR encoder/decoder (header-only)
 *
 * All functions are static inline — no .cpp needed.
 * Include this in any .ino or .cpp that needs CBOR.
 */
#pragma once
#include <Arduino.h>

#define CBOR_UINT  0
#define CBOR_NEG   1
#define CBOR_BYTES 2
#define CBOR_TEXT  3
#define CBOR_ARRAY 4
#define CBOR_MAP   5

// ─── Decoder ─────────────────────────────────────────────────────────────

static inline size_t cbor_decode_head(const uint8_t* p, size_t avail,
                                       uint8_t* major, size_t* arg) {
  if (avail < 1) return 0;
  *major = (*p) >> 5;
  uint8_t ai = (*p) & 0x1F;
  if (ai <= 23) { *arg = ai; return 1; }
  if (ai == 24 && avail >= 2) { *arg = p[1]; return 2; }
  if (ai == 25 && avail >= 3) { *arg = ((size_t)p[1]<<8)|p[2]; return 3; }
  if (ai == 26 && avail >= 5) {
    *arg=((size_t)p[1]<<24)|((size_t)p[2]<<16)|((size_t)p[3]<<8)|p[4]; return 5;
  }
  return 0;
}

static inline size_t cbor_skip(const uint8_t* p, size_t avail) {
  uint8_t maj; size_t arg;
  size_t h = cbor_decode_head(p, avail, &maj, &arg);
  if (!h) return 0;
  if (maj == CBOR_UINT || maj == CBOR_NEG || maj == 7) return h;
  if (maj == CBOR_BYTES || maj == CBOR_TEXT) {
    return (h + arg > avail) ? 0 : h + arg;
  }
  size_t pos = h, items = (maj == CBOR_ARRAY) ? arg : arg*2;
  for (size_t i = 0; i < items; i++) {
    size_t s = cbor_skip(p+pos, avail-pos); if (!s) return 0; pos += s;
  }
  return pos;
}

static inline bool cbor_map_find_uint(const uint8_t* p, size_t avail, uint8_t k,
                                       const uint8_t** val, size_t* val_avail) {
  uint8_t maj; size_t n;
  size_t h = cbor_decode_head(p, avail, &maj, &n);
  if (!h || maj != CBOR_MAP) return false;
  size_t pos = h;
  for (size_t i = 0; i < n; i++) {
    uint8_t km; size_t ka;
    size_t kh = cbor_decode_head(p+pos, avail-pos, &km, &ka);
    if (!kh) return false;
    if (km == CBOR_UINT && ka == k) { *val=p+pos+kh; *val_avail=avail-pos-kh; return true; }
    size_t ks=cbor_skip(p+pos,avail-pos); if(!ks) return false; pos+=ks;
    size_t vs=cbor_skip(p+pos,avail-pos); if(!vs) return false; pos+=vs;
  }
  return false;
}

static inline bool cbor_map_find_text(const uint8_t* p, size_t avail, const char* key,
                                       const uint8_t** val, size_t* val_avail) {
  uint8_t maj; size_t n;
  size_t h = cbor_decode_head(p, avail, &maj, &n);
  if (!h || maj != CBOR_MAP) return false;
  size_t klen=strlen(key), pos=h;
  for (size_t i = 0; i < n; i++) {
    uint8_t km; size_t ka;
    size_t kh = cbor_decode_head(p+pos, avail-pos, &km, &ka);
    if (!kh) return false;
    if (km==CBOR_TEXT && ka==klen && memcmp(p+pos+kh,key,klen)==0) {
      *val=p+pos+kh+klen; *val_avail=avail-pos-kh-klen; return true;
    }
    size_t ks=cbor_skip(p+pos,avail-pos); if(!ks) return false; pos+=ks;
    size_t vs=cbor_skip(p+pos,avail-pos); if(!vs) return false; pos+=vs;
  }
  return false;
}

static inline size_t cbor_get_bytes(const uint8_t* p, size_t avail,
                                     uint8_t* out, size_t maxLen) {
  uint8_t maj; size_t len;
  size_t h = cbor_decode_head(p, avail, &maj, &len);
  if (!h || maj!=CBOR_BYTES || h+len>avail) return 0;
  size_t n=(len<maxLen)?len:maxLen; memcpy(out,p+h,n); return n;
}

static inline size_t cbor_get_text(const uint8_t* p, size_t avail,
                                    char* out, size_t maxLen) {
  uint8_t maj; size_t len;
  size_t h = cbor_decode_head(p, avail, &maj, &len);
  if (!h || maj!=CBOR_TEXT || h+len>avail) return 0;
  size_t n=(len<maxLen-1)?len:maxLen-1; memcpy(out,p+h,n); out[n]='\0'; return n;
}

static inline bool cbor_get_int(const uint8_t* p, size_t avail, int32_t* out) {
  uint8_t maj; size_t arg;
  size_t h = cbor_decode_head(p, avail, &maj, &arg);
  if (!h) return false;
  if (maj==CBOR_UINT) { *out= (int32_t)arg;     return true; }
  if (maj==CBOR_NEG)  { *out=-(int32_t)arg-1;   return true; }
  return false;
}

// ─── Encoder ─────────────────────────────────────────────────────────────

static inline size_t cbor_enc_head(uint8_t* o, uint8_t maj, size_t arg) {
  uint8_t m = maj<<5;
  if (arg<=23)     { o[0]=m|(uint8_t)arg; return 1; }
  if (arg<=0xFF)   { o[0]=m|24; o[1]=(uint8_t)arg; return 2; }
  if (arg<=0xFFFF) { o[0]=m|25; o[1]=arg>>8; o[2]=arg&0xFF; return 3; }
  o[0]=m|26; o[1]=(arg>>24)&0xFF; o[2]=(arg>>16)&0xFF;
  o[3]=(arg>>8)&0xFF; o[4]=arg&0xFF; return 5;
}
static inline size_t cbor_enc_uint(uint8_t* o, uint32_t v)     { return cbor_enc_head(o,CBOR_UINT,v); }
static inline size_t cbor_enc_text(uint8_t* o, const char* s)  { size_t l=strlen(s); size_t h=cbor_enc_head(o,CBOR_TEXT,l); memcpy(o+h,s,l); return h+l; }
static inline size_t cbor_enc_bytes(uint8_t* o, const uint8_t* b, size_t l) { size_t h=cbor_enc_head(o,CBOR_BYTES,l); memcpy(o+h,b,l); return h+l; }

// COSE ES256 P-256 public key — always exactly 77 bytes
// {1:2, 3:-7, -1:1, -2:<x 32B>, -3:<y 32B>}
static inline void cbor_enc_cose_key(uint8_t* o, const uint8_t x[32], const uint8_t y[32]) {
  size_t p=0;
  o[p++]=0xA5;
  o[p++]=0x01; o[p++]=0x02;  // kty: EC2
  o[p++]=0x03; o[p++]=0x26;  // alg: -7 (ES256)
  o[p++]=0x20; o[p++]=0x01;  // crv: P-256
  o[p++]=0x21; o[p++]=0x58; o[p++]=0x20; memcpy(o+p,x,32); p+=32;
  o[p++]=0x22; o[p++]=0x58; o[p++]=0x20; memcpy(o+p,y,32); p+=32;
}

// "none" attestation MakeCredential response (status byte + CBOR).  out >= 200 bytes.
static inline size_t cbor_enc_make_credential_resp(uint8_t* out,
                                                    const uint8_t* authData,
                                                    size_t authDataLen) {
  size_t p=0;
  out[p++]=0x00; out[p++]=0xA3;
  p+=cbor_enc_uint(out+p,1); p+=cbor_enc_text(out+p,"none");
  p+=cbor_enc_uint(out+p,2); p+=cbor_enc_bytes(out+p,authData,authDataLen);
  p+=cbor_enc_uint(out+p,3); out[p++]=0xA0;
  return p;
}

// GetAssertion response.  out >= 300 bytes.
// Includes credential descriptor (key 0x01) — required by Windows/Chrome.
static inline size_t cbor_enc_get_assertion_resp(uint8_t* out,
                                                  const uint8_t* credId,   size_t credIdLen,
                                                  const uint8_t* authData, size_t authDataLen,
                                                  const uint8_t* sig,      size_t sigLen) {
  size_t p=0;
  out[p++]=0x00; out[p++]=0xA4;  // status OK + 4-item map
  // 0x01: credential {type: "public-key", id: <credId>}
  p+=cbor_enc_uint(out+p,1);
  out[p++]=0xA2;  // 2-item map
  p+=cbor_enc_text(out+p,"type"); p+=cbor_enc_text(out+p,"public-key");
  p+=cbor_enc_text(out+p,"id");   p+=cbor_enc_bytes(out+p,credId,credIdLen);
  // 0x02: authData
  p+=cbor_enc_uint(out+p,2); p+=cbor_enc_bytes(out+p,authData,authDataLen);
  // 0x03: signature
  p+=cbor_enc_uint(out+p,3); p+=cbor_enc_bytes(out+p,sig,sigLen);
  // 0x05: numberOfCredentials (required by Windows)
  p+=cbor_enc_uint(out+p,5); p+=cbor_enc_uint(out+p,1);
  return p;
}
