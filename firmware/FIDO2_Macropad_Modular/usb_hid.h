/*
 * USB HID Module - Header
 * Composite USB: Keyboard+Consumer (combined) + FIDO2 HID (CTAPHID)
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include <stdbool.h>
#include <Adafruit_TinyUSB.h>

// ─── USB interface objects ────────────────────────────────────────────────
extern Adafruit_USBD_HID usb_kbd;    // Combined Keyboard + Consumer Control
extern Adafruit_USBD_HID usb_fido;   // FIDO2 interface
extern volatile bool fidoBusy;        // Suppress keyboard while FIDO2 is active

// ─── Keyboard + Consumer functions ───────────────────────────────────────
void initUSB();
void waitUSBReady();
void sendKey(uint8_t key);
void sendKeyCombo(uint8_t modifier, uint8_t key);
void sendString(const char* str);
void sendVolumeUp();
void sendVolumeDown();
void sendVolumeMute();
uint8_t asciiToKeycode(char c);

// ─── FIDO2 CTAPHID packet senders ────────────────────────────────────────

// Single-packet response (payload ≤ 57 bytes)
bool sendCTAPHIDResponse(uint32_t cid, uint8_t cmd,
                          const uint8_t* payload, uint16_t payloadLen);

// Multi-packet response (auto-fragments into init + continuation packets)
bool sendCTAPHIDLarge(uint32_t cid, uint8_t cmd,
                       const uint8_t* payload, uint16_t payloadLen);

// Send CTAPHID error
void sendCTAPError(uint32_t cid, uint8_t errCode);

// Drain stale RX packets (after blocking operations like fingerprint scan).
// Returns true if a CANCEL was found among the stale packets.
bool drainStalePackets();

// ─── FIDO2 RX ring buffer access ─────────────────────────────────────────
#define CTAP_PACKET_SIZE 64
#define RX_QUEUE_DEPTH   8

struct RxPacket { uint8_t data[CTAP_PACKET_SIZE]; uint16_t len; };
extern RxPacket     rx_queue[RX_QUEUE_DEPTH];
extern volatile int rx_head, rx_tail;

#endif // USB_HID_H
