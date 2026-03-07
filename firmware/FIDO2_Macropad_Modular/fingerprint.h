/*
 * Fingerprint Module - Header
 * Handles Waveshare fingerprint sensor communication
 * Protocol ported from tested code in tests/06_Fingerprint_Test/
 *
 * Sensor: Waveshare UART Fingerprint Sensor (D) - Capacitive
 * Max capacity: 150 fingerprints
 */

#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <stdint.h>
#include <stdbool.h>

// =====================================================
// Waveshare Fingerprint Sensor Protocol Constants
// =====================================================

// Response / ACK codes
#define ACK_SUCCESS           0x00
#define ACK_FAIL              0x01
#define ACK_FULL              0x04
#define ACK_NO_USER           0x05
#define ACK_USER_OCCUPIED     0x06
#define ACK_FINGER_OCCUPIED   0x07
#define ACK_TIMEOUT           0x08

// User privilege levels
#define ACK_ALL_USER          0x00
#define ACK_GUEST_USER        0x01
#define ACK_NORMAL_USER       0x02
#define ACK_MASTER_USER       0x03

#define USER_MAX_CNT          150   // Waveshare Sensor (D) max fingerprint capacity

// Command framing
#define CMD_HEAD              0xF5
#define CMD_TAIL              0xF5

// Command IDs
#define CMD_ADD_1             0x01
#define CMD_ADD_2             0x02
#define CMD_ADD_3             0x03
#define CMD_DEL               0x04
#define CMD_DEL_ALL           0x05
#define CMD_USER_CNT          0x09
#define CMD_MATCH             0x0C
#define CMD_FINGER_DETECTED   0x14
#define CMD_COM_LEV           0x28
#define CMD_LP_MODE           0x2C
#define CMD_TIMEOUT           0x2E

// =====================================================
// Fingerprint verification result codes
// =====================================================

enum FpResult {
  FP_MATCH,         // Fingerprint matched a known user
  FP_NO_MATCH,      // Finger scanned but no match found
  FP_TIMEOUT,       // No finger placed on sensor within timeout
  FP_SENSOR_ERROR   // UART/hardware failure — sensor not responding
};

// =====================================================
// Public API
// =====================================================

void initFingerprint();
FpResult verifyFingerprint();
bool enrollFingerprint(uint8_t id);
bool deleteFingerprint(uint8_t id);
bool clearAllFingerprints();
int  getFingerprintCount();

// Interactive delete menu (rotary encoder driven)
void enterFingerprintDeleteMenu();
void updateFingerprintDeleteMenu();

// Low-level helpers (used internally and by commands module)
uint8_t fpSetCompareLevel(uint8_t level);
uint8_t fpGetCompareLevel();

#endif // FINGERPRINT_H
