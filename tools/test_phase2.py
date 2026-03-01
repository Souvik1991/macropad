#!/usr/bin/env python3
"""
Phase 2 CTAP2 Test: MakeCredential + GetAssertion
===================================================
Tests the ESP32-S3 FIDO2 Macropad's full credential lifecycle:
  1. List device (transport check)
  2. MakeCredential — register a new P-256 credential
  3. GetAssertion   — authenticate and verify the signature

Install once:
  pip install fido2 cryptography

Run:
  python tools/test_phase2.py
"""

import sys
import os
import hashlib
import secrets

from fido2.hid import CtapHidDevice
from fido2.ctap2 import Ctap2
from fido2.webauthn import (
    PublicKeyCredentialRpEntity,
    PublicKeyCredentialUserEntity,
    PublicKeyCredentialParameters,
    PublicKeyCredentialType,
)
from cryptography.hazmat.primitives.asymmetric.ec import (
    ECDSA, EllipticCurvePublicKey, EllipticCurvePublicNumbers, SECP256R1
)
from cryptography.hazmat.primitives.hashes import SHA256
from cryptography.hazmat.backends import default_backend

# ─── ANSI colours ────────────────────────────────────────────────────────────
GREEN  = "\033[32m"
RED    = "\033[31m"
YELLOW = "\033[33m"
RESET  = "\033[0m"
OK     = f"{GREEN}[PASS]{RESET}"
FAIL   = f"{RED}[FAIL]{RESET}"
INFO   = f"{YELLOW}[INFO]{RESET}"

# ─── Test parameters ─────────────────────────────────────────────────────────
RP_ID   = "test.fido2macropad.local"
RP_NAME = "FIDO2 Macropad Test"

USER_ID      = secrets.token_bytes(16)   # random per run
USER_NAME    = "testuser"
USER_DISPLAY = "Test User"

CLIENT_DATA_HASH_MAKE = hashlib.sha256(b"make-credential-client-data").digest()
CLIENT_DATA_HASH_GET  = hashlib.sha256(b"get-assertion-client-data").digest()

# ─── Step 1: Find device ─────────────────────────────────────────────────────
def find_device():
    print(f"\n{INFO} Scanning for FIDO2 HID devices...")
    devices = list(CtapHidDevice.list_devices())
    if not devices:
        print(f"{FAIL} No FIDO2 HID devices found.")
        print("       Make sure the ESP32-S3 is flashed and connected.")
        sys.exit(1)

    print(f"  Found {len(devices)} device(s):")
    for i, d in enumerate(devices):
        print(f"    [{i}] {d.descriptor.product_name} "
              f"(vid=0x{d.descriptor.vid:04X} pid=0x{d.descriptor.pid:04X})")

    # Prefer our device by product name
    for d in devices:
        if "FIDO2" in (d.descriptor.product_name or "").upper():
            print(f"  -> Using: {d.descriptor.product_name}")
            return d

    # Fallback: first device
    print(f"  -> Using first device")
    return devices[0]


# ─── Step 2: MakeCredential ──────────────────────────────────────────────────
def test_make_credential(ctap2):
    print(f"\n{INFO} MakeCredential (register new P-256 credential)...")

    rp   = PublicKeyCredentialRpEntity(id=RP_ID, name=RP_NAME)
    user = PublicKeyCredentialUserEntity(
        id=USER_ID, name=USER_NAME, display_name=USER_DISPLAY
    )
    key_params = [
        PublicKeyCredentialParameters(
            type=PublicKeyCredentialType.PUBLIC_KEY,
            alg=-7,  # ES256
        )
    ]

    try:
        attestation = ctap2.make_credential(
            client_data_hash = CLIENT_DATA_HASH_MAKE,
            rp               = rp,
            user             = user,
            key_params       = key_params,
        )
    except Exception as e:
        print(f"{FAIL} MakeCredential exception: {e}")
        return None

    auth_data = attestation.auth_data

    print(f"  fmt:          {attestation.fmt}")
    print(f"  rpIdHash:     {auth_data.rp_id_hash.hex()}")
    print(f"  flags:        0x{auth_data.flags:02X} "
          f"(UP={'1' if auth_data.is_user_present() else '0'} "
          f"UV={'1' if auth_data.is_user_verified() else '0'} "
          f"AT={'1' if auth_data.has_attested_credential_data() else '0'})")

    if not auth_data.has_attested_credential_data():
        print(f"{FAIL} No attested credential data in response")
        return None

    cred_id = auth_data.credential_data.credential_id
    pub_key = auth_data.credential_data.public_key
    print(f"  credentialId: {cred_id.hex()}")
    print(f"  pubKey alg:   {pub_key.ALG}")

    # Verify rpIdHash matches our RP ID
    expected_rp_hash = hashlib.sha256(RP_ID.encode()).digest()
    if auth_data.rp_id_hash != expected_rp_hash:
        print(f"{FAIL} rpIdHash mismatch!")
        print(f"       expected: {expected_rp_hash.hex()}")
        print(f"       got:      {auth_data.rp_id_hash.hex()}")
        return None

    print(f"{OK} MakeCredential succeeded — credential created")
    return cred_id, pub_key


# ─── Step 3: GetAssertion ────────────────────────────────────────────────────
def test_get_assertion(ctap2, cred_id, pub_key_cose):
    print(f"\n{INFO} GetAssertion (authenticate with created credential)...")

    from fido2.webauthn import PublicKeyCredentialDescriptor
    allow_list = [
        PublicKeyCredentialDescriptor(
            type=PublicKeyCredentialType.PUBLIC_KEY,
            id=cred_id,
        )
    ]

    try:
        assertion_resp = ctap2.get_assertion(
            rp_id            = RP_ID,
            client_data_hash = CLIENT_DATA_HASH_GET,
            allow_list       = allow_list,
        )
    except Exception as e:
        print(f"{FAIL} GetAssertion exception: {e}")
        return False

    assertion = assertion_resp.get_response(0)
    auth_data = assertion.auth_data
    signature = assertion.signature

    print(f"  rpIdHash:  {auth_data.rp_id_hash.hex()}")
    print(f"  flags:     0x{auth_data.flags:02X}")
    print(f"  signCount: {auth_data.counter}")
    print(f"  sig ({len(signature)} bytes): {signature.hex()}")

    # Verify rpIdHash
    expected_rp_hash = hashlib.sha256(RP_ID.encode()).digest()
    if auth_data.rp_id_hash != expected_rp_hash:
        print(f"{FAIL} rpIdHash mismatch in assertion")
        return False

    if auth_data.counter == 0:
        print(f"{YELLOW}[WARN]{RESET} signCount is 0 — should be ≥ 1 after first use")

    # Verify signature using the public key from MakeCredential
    try:
        # Build the verification data: authData || clientDataHash
        verify_data = bytes(auth_data) + CLIENT_DATA_HASH_GET

        # Extract EC public key from COSE map
        x = bytes(pub_key_cose[-2])   # key -2 = x
        y = bytes(pub_key_cose[-3])   # key -3 = y
        ec_pub = EllipticCurvePublicNumbers(
            x=int.from_bytes(x, 'big'),
            y=int.from_bytes(y, 'big'),
            curve=SECP256R1()
        ).public_key(default_backend())

        # The signature is over SHA256(authData || clientDataHash) 
        # but ECDSA.verify() with SHA256 does the hash internally
        ec_pub.verify(signature, verify_data, ECDSA(SHA256()))
        print(f"{OK} Signature verified cryptographically!")
        return True

    except Exception as e:
        print(f"{FAIL} Signature verification failed: {e}")
        print("       (This may be a test-script issue, not a device issue.)")
        print("       Check Serial Monitor to confirm GetAssertion ran correctly.")
        return False


# ─── Main ────────────────────────────────────────────────────────────────────
def main():
    print("=" * 55)
    print("  FIDO2 Macropad — Phase 2 Test")
    print("  MakeCredential + GetAssertion")
    print("=" * 55)

    dev   = find_device()
    ctap2 = Ctap2(dev)

    # GetInfo — should still work from Phase 1
    print(f"\n{INFO} GetInfo check...")
    try:
        info = ctap2.get_info()
        print(f"  versions: {info.versions}")
        print(f"{OK} GetInfo OK")
    except Exception as e:
        print(f"{FAIL} GetInfo failed: {e}")
        sys.exit(1)

    # MakeCredential
    result = test_make_credential(ctap2)
    if result is None:
        print(f"\n{RED}Phase 2 FAILED at MakeCredential.{RESET}")
        sys.exit(1)

    cred_id, pub_key = result

    # GetAssertion
    ok = test_get_assertion(ctap2, cred_id, pub_key)

    print("\n" + "=" * 55)
    if ok:
        print(f"{GREEN}ALL TESTS PASSED — ESP32-S3 is a working CTAP2 authenticator!{RESET}")
        print()
        print("Next steps:")
        print("  1. Try https://webauthn.io in Chrome/Edge for a browser test")
        print("  2. Phase 3: Migrate crypto to ATECC608A for hardware key storage")
    else:
        print(f"{RED}GetAssertion test FAILED — check Serial Monitor output.{RESET}")
    print("=" * 55)


if __name__ == "__main__":
    main()
