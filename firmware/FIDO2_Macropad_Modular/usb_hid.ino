/*
 * USB HID Module
 * Handles USB keyboard communication
 */

// ============================================
// GLOBAL OBJECTS
// ============================================

Adafruit_USBD_HID usb_hid;

uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD()
};

// ============================================
// INITIALIZATION
// ============================================

void initUSB() {
  debugPrint("Initializing USB HID... ");
  
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
  
  // Wait for USB to be ready
  while(!TinyUSBDevice.mounted()) {
    delay(1);
  }
  
  debugPrintln("OK");
}

// ============================================
// KEYBOARD FUNCTIONS
// ============================================

void sendKey(uint8_t key) {
  uint8_t keycode[6] = {0};
  keycode[0] = key;
  
  usb_hid.keyboardReport(0, 0, keycode);
  delay(10);
  usb_hid.keyboardRelease(0);
}

void sendKeyCombo(uint8_t modifier, uint8_t key) {
  uint8_t keycode[6] = {0};
  keycode[0] = key;
  
  usb_hid.keyboardReport(0, modifier, keycode);
  delay(10);
  usb_hid.keyboardRelease(0);
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
  // HID Consumer Control: Volume Up
  // Note: Requires consumer control descriptor for true volume control
  // For now, we use keyboard shortcuts as fallback
  
  if (currentOS == OS_MAC) {
    // Mac doesn't have keyboard volume shortcuts that work reliably
    // Would need consumer control HID
    debugPrintln("  -> Volume Up (Mac - needs consumer HID)");
  } else {
    // Windows: Some keyboards support this, but not standard
    debugPrintln("  -> Volume Up (Win - needs consumer HID)");
  }
}

void sendVolumeDown() {
  // HID Consumer Control: Volume Down
  if (currentOS == OS_MAC) {
    debugPrintln("  -> Volume Down (Mac - needs consumer HID)");
  } else {
    debugPrintln("  -> Volume Down (Win - needs consumer HID)");
  }
}

void sendVolumeMute() {
  // HID Consumer Control: Mute
  // This is the most universally supported volume control
  
  if (currentOS == OS_MAC) {
    // Mac: Mute key (requires consumer HID)
    debugPrintln("  -> Toggle Mute (Mac)");
  } else {
    // Windows: Mute key (requires consumer HID)
    debugPrintln("  -> Toggle Mute (Windows)");
  }
  
  /*
   * TODO: Implement Consumer Control HID descriptor
   * 
   * HID Consumer Control Report Descriptor needed:
   * - 0xE2 (Mute)
   * - 0xE9 (Volume Up)
   * - 0xEA (Volume Down)
   * 
   * Add to USB descriptor in initUSB()
   */
}

// ============================================
// HELPER FUNCTIONS
// ============================================

uint8_t asciiToKeycode(char c) {
  // Simple ASCII to HID keycode conversion
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
  
  return 0;  // Unsupported character
}

