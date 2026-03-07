/*
 * USB HID Module - Implementation
 * Composite USB: Keyboard + FIDO2 HID (CTAPHID)
 *
 * Ported from tested code in tests/07_Multi_HID_Test/07_Multi_HID_Test.ino
 * Two HID interfaces: keyboard (for macros) + FIDO2 (Usage Page 0xF1D0)
 */

#include "config.h"
#include "debug.h"
#include "usb_hid.h"
#include <string.h>

// ─── HID report descriptors ──────────────────────────────────────────────

uint8_t const desc_keyboard_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD()
};

// FIDO2: Usage Page 0xF1D0, 64-byte IN+OUT, no Report ID
uint8_t const desc_fido_report[] = {
  0x06, 0xD0, 0xF1, 0x09, 0x01, 0xA1, 0x01,
  0x09, 0x20, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x40, 0x81, 0x02,
  0x09, 0x21, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x40, 0x91, 0x02,
  0xC0
};

// Consumer Control: Volume/Mute (16-bit usage code, standard TinyUSB descriptor)
uint8_t const desc_consumer_report[] = {
  TUD_HID_REPORT_DESC_CONSUMER()
};

// ─── USB interface objects ────────────────────────────────────────────────

Adafruit_USBD_HID usb_kbd;
Adafruit_USBD_HID usb_fido;
Adafruit_USBD_HID usb_consumer;
volatile bool fidoBusy = false;

// ─── FIDO2 RX ring buffer (ISR-safe SPSC, lock-free) ─────────────────────

RxPacket     rx_queue[RX_QUEUE_DEPTH];
volatile int rx_head = 0, rx_tail = 0;

// ─── FIDO2 RX callback (called from TinyUSB task) ─────────────────────────

static void fido_set_report(uint8_t, hid_report_type_t,
                             uint8_t const* buffer, uint16_t bufsize) {
  if (!bufsize) return;
  int next = (rx_head + 1) % RX_QUEUE_DEPTH;
  if (next == rx_tail) return; // queue full, drop
  uint16_t n = bufsize < CTAP_PACKET_SIZE ? bufsize : CTAP_PACKET_SIZE;
  memcpy(rx_queue[rx_head].data, buffer, n);
  rx_queue[rx_head].len = n;
  rx_head = next;
}

// ─── USB Initialization (Composite: Keyboard + FIDO2) ─────────────────────

void initUSB() {
  debugPrint("Initializing USB Composite HID... ");

  TinyUSBDevice.setManufacturerDescriptor("DIY Security");
  TinyUSBDevice.setProductDescriptor("FIDO2 Macropad");

  // Consumer Control interface (volume/mute)
  usb_consumer.setPollInterval(2);
  usb_consumer.setReportDescriptor(desc_consumer_report, sizeof(desc_consumer_report));
  usb_consumer.setStringDescriptor("FIDO2 Macropad Media");
  usb_consumer.begin();

  // Keyboard interface
  usb_kbd.setPollInterval(2);
  usb_kbd.setReportDescriptor(desc_keyboard_report, sizeof(desc_keyboard_report));
  usb_kbd.setStringDescriptor("FIDO2 Macropad Keyboard");
  usb_kbd.begin();

  // FIDO2 interface
  usb_fido.setPollInterval(5);
  usb_fido.setReportDescriptor(desc_fido_report, sizeof(desc_fido_report));
  usb_fido.setStringDescriptor("FIDO2 Authenticator");
  usb_fido.setReportCallback(NULL, fido_set_report);
  usb_fido.enableOutEndpoint(true);
  usb_fido.begin();

  // Wait for USB to be ready
  uint32_t t0 = millis();
  while (!TinyUSBDevice.mounted()) {
    delay(10);
    if (millis() - t0 > 8000) {
      debugPrintln("TIMEOUT");
      return;
    }
  }

  debugPrintln("OK");
  debugPrint("  Keyboard: ");
  debugPrintln(usb_kbd.ready() ? "READY" : "---");
  debugPrint("  FIDO2:    ");
  debugPrintln(usb_fido.ready() ? "READY" : "---");
}

// ─── CTAPHID packet senders (ported from test) ───────────────────────────

// Send a single HID report with retry — waits for endpoint to be ready.
static bool sendReportRetry(const uint8_t* pkt, int maxRetries = 50) {
  for (int attempt = 0; attempt < maxRetries; attempt++) {
    if (usb_fido.sendReport(0, pkt, CTAP_PACKET_SIZE)) return true;
    delay(5);
  }
  debugPrintln("[FIDO2] sendReport FAILED after retries");
  return false;
}

bool sendCTAPHIDResponse(uint32_t cid, uint8_t cmd,
                          const uint8_t* payload, uint16_t payloadLen) {
  uint8_t pkt[CTAP_PACKET_SIZE] = {0};
  pkt[0] = (cid>>24)&0xFF; pkt[1] = (cid>>16)&0xFF;
  pkt[2] = (cid>> 8)&0xFF; pkt[3] =  cid     &0xFF;
  pkt[4] = cmd;
  pkt[5] = (payloadLen>>8)&0xFF; pkt[6] = payloadLen&0xFF;
  if (payload && payloadLen > 0) {
    uint16_t n = payloadLen < 57 ? payloadLen : 57;
    memcpy(pkt + 7, payload, n);
  }
  return usb_fido.sendReport(0, pkt, CTAP_PACKET_SIZE);
}

bool sendCTAPHIDLarge(uint32_t cid, uint8_t cmd,
                       const uint8_t* payload, uint16_t payloadLen) {
  uint8_t  pkt[CTAP_PACKET_SIZE];
  uint16_t sent = 0;
  uint8_t  seq  = 0;

  // Init packet: 7-byte header + up to 57 bytes
  memset(pkt, 0, CTAP_PACKET_SIZE);
  pkt[0]=(cid>>24)&0xFF; pkt[1]=(cid>>16)&0xFF;
  pkt[2]=(cid>> 8)&0xFF; pkt[3]= cid     &0xFF;
  pkt[4]=cmd;
  pkt[5]=(payloadLen>>8)&0xFF; pkt[6]=payloadLen&0xFF;
  uint16_t chunk = payloadLen < 57 ? payloadLen : 57;
  memcpy(pkt + 7, payload, chunk);
  sent = chunk;
  if (!sendReportRetry(pkt)) return false;

  // Continuation packets: 5-byte header + up to 59 bytes
  while (sent < payloadLen) {
    delay(3);
    memset(pkt, 0, CTAP_PACKET_SIZE);
    pkt[0]=(cid>>24)&0xFF; pkt[1]=(cid>>16)&0xFF;
    pkt[2]=(cid>> 8)&0xFF; pkt[3]= cid     &0xFF;
    pkt[4] = seq++;                              // bit7=0 → continuation
    chunk = (payloadLen - sent) < 59 ? (payloadLen - sent) : 59;
    memcpy(pkt + 5, payload + sent, chunk);
    sent += chunk;
    if (!sendReportRetry(pkt)) return false;
  }
  debugPrintf("[FIDO2] TX CID=0x%08X CMD=0x%02X BCNT=%d\n", cid, cmd, payloadLen);
  return true;
}

void sendCTAPError(uint32_t cid, uint8_t errCode) {
  uint8_t p[1] = { errCode };
  sendCTAPHIDResponse(cid, 0xBF, p, 1);  // CTAPHID_ERROR = 0xBF
}

// ─── Drain stale RX packets ──────────────────────────────────────────────

bool drainStalePackets() {
  bool cancelled = false;
  while (rx_tail != rx_head) {
    uint16_t len = rx_queue[rx_tail].len;
    if (len >= 5 && rx_queue[rx_tail].data[4] == 0x91) {
      cancelled = true;
      debugPrintln("[FIDO2] CANCEL found in stale queue");
    }
    rx_tail = (rx_tail + 1) % RX_QUEUE_DEPTH;
  }
  return cancelled;
}

// ─── Keyboard functions ──────────────────────────────────────────────────

void sendKey(uint8_t key) {
  uint8_t keycode[6] = {0};
  keycode[0] = key;

  usb_kbd.keyboardReport(0, 0, keycode);
  delay(10);
  usb_kbd.keyboardRelease(0);
}

void sendKeyCombo(uint8_t modifier, uint8_t key) {
  uint8_t keycode[6] = {0};
  keycode[0] = key;

  usb_kbd.keyboardReport(0, modifier, keycode);
  delay(10);
  usb_kbd.keyboardRelease(0);
}

void sendString(const char* str) {
  for (size_t i = 0; i < strlen(str); i++) {
    uint8_t keycode = asciiToKeycode(str[i]);
    if (keycode) {
      sendKey(keycode);
      delay(10);
    }
  }
}

void sendVolumeUp() {
  uint16_t usage = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
  usb_consumer.sendReport(0, &usage, sizeof(usage));
  delay(10);
  usage = 0;
  usb_consumer.sendReport(0, &usage, sizeof(usage));
  debugPrintln("  -> Volume Up");
}

void sendVolumeDown() {
  uint16_t usage = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
  usb_consumer.sendReport(0, &usage, sizeof(usage));
  delay(10);
  usage = 0;
  usb_consumer.sendReport(0, &usage, sizeof(usage));
  debugPrintln("  -> Volume Down");
}

void sendVolumeMute() {
  uint16_t usage = HID_USAGE_CONSUMER_MUTE;
  usb_consumer.sendReport(0, &usage, sizeof(usage));
  delay(10);
  usage = 0;
  usb_consumer.sendReport(0, &usage, sizeof(usage));
  debugPrintln("  -> Toggle Mute");
}

uint8_t asciiToKeycode(char c) {
  if (c >= 'a' && c <= 'z') {
    return HID_KEY_A + (c - 'a');
  }
  if (c >= 'A' && c <= 'Z') {
    return HID_KEY_A + (c - 'A');
  }
  if (c >= '0' && c <= '9') {
    return (c == '0') ? HID_KEY_0 : HID_KEY_1 + (c - '1');
  }
  if (c == ' ') return HID_KEY_SPACE;
  if (c == '\n') return HID_KEY_ENTER;
  if (c == '\t') return HID_KEY_TAB;

  return 0;
}
