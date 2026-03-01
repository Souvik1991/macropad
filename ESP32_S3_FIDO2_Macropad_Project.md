# ESP32-S3 Based FIDO2 Fingerprint Macropad (with Secure Element ATECC608A)

**Date:** 2025-11-14 (Updated)

---

## 🔰 Overview

This document describes the design of a **FIDO2-compatible fingerprint macropad** built around an **ESP32-S3 microcontroller** and **Microchip ATECC608A secure element**.  

**Primary Objective:** Use fingerprint sensor for passwordless browser authentication (passkeys) on macOS - eliminating the need to type passwords.

The macropad features:
- **12 programmable macro keys** (3×4 matrix) with Cherry MX switches
- **Fingerprint sensor** for FIDO2 passkey verification
- **OLED display** showing key functions and authentication prompts
- **1 rotary encoder** for volume control and mute
- **5 RGB underglow LEDs** for visual feedback
- **Dynamic configuration** - No firmware reflash needed to change macros/LEDs/OS mode
- Functions as **USB composite device** (HID Keyboard + FIDO2 CTAP2)

---

## 🧠 System Architecture Summary

| Component | Interface | Function |
|------------|------------|-----------|
| **ESP32-S3** | USB, I²C, UART, GPIO | Main controller: handles FIDO2 CTAP2, HID Keyboard, fingerprint logic, and UI |
| **ATECC608A Secure Element** | I²C | Securely stores attestation and per-site private keys; performs ECDSA signing |
| **Fingerprint Sensor (R307/AS608)** | UART | Handles biometric user verification for passkey authentication |
| **OLED Display (SSD1306 0.96")** | I²C | Displays key functions, authentication prompts, and status messages |
| **2× Rotary Encoders (EC11)** | GPIO | Volume control + navigation/scrolling |
| **12× Macro Keys** | GPIO (3×4 matrix) | Programmable Cherry MX switches for shortcuts |
| **12× RGB LEDs (WS2812B/SK6812)** | SPI/RMT | Addressable underglow lighting for visual feedback |
| **USB-C Port** | USB 2.0 D+/D- | Native USB for HID Keyboard + FIDO2 CTAP2 |

---

## ⚙️ Operation Flows

### 1. Fingerprint Passkey Authentication (Primary Objective)

**Registration (Create Passkey):**
1. Visit website (GitHub, Google, etc.) → Click "Sign up with passkey"
2. Browser initiates WebAuthn → CTAP2 `makeCredential` command
3. ESP32 receives request over USB HID FIDO2 interface
4. **OLED displays:** "GitHub.com wants to register. Scan finger to confirm."
5. User touches fingerprint sensor → R307 verifies fingerprint locally
6. On match → ESP32 requests ATECC608A to generate new keypair
7. ATECC608A generates private key (stored securely), returns public key
8. ESP32 sends attestation response (signed by ATECC608A) to browser
9. **OLED displays:** "✓ Passkey created for GitHub.com"
10. Registration complete → website stores your public key

**Authentication (Login with Passkey):**
1. Visit login page → Click "Sign in with passkey"
2. Browser sends CTAP2 `getAssertion` command
3. ESP32 receives authentication request
4. **OLED displays:** "GitHub.com login. Scan finger."
5. User touches fingerprint sensor → R307 verifies match
6. On success → ATECC608A signs challenge with stored private key
7. ESP32 sends signed assertion to browser
8. Browser/website verifies signature → **Login success!**
9. **OLED displays:** "✓ Logged in to GitHub.com"
10. **RGB LEDs:** Green pulse animation

**Result:** No password typing needed on your Mac! Just touch fingerprint.

### 2. Macro Key Operations

**Normal Usage:**
- Press any of 12 macro keys → sends programmed shortcut
- **OLED shows current mapping:** 
  ```
  [1:Copy] [2:Paste] [3:Save]
  [4:Undo] [5:Find]  [6:Run]
  [7:App1] [8:App2]  [9:App3]
  [*:Prev] [0:Play]  [#:Next]
  ```
- RGB LEDs indicate active layer (e.g., blue = work, red = gaming)

**Dynamic Configuration:**
- Macros stored in EEPROM - no firmware reflash needed
- Configure via Serial commands or Web UI
- Supports: Key combos, strings, special functions
- Falls back to hardcoded defaults if disabled
- See `tools/CONFIGURATOR_README.md` for details

### 3. Rotary Encoder Controls

**Rotary Encoder:** Volume Control + Mute
- Rotate clockwise → Volume up (5% per step)
- Rotate counter-clockwise → Volume down (5% per step)
- Press button → Mute/unmute toggle
- Visual feedback: Red flash = muted, Green flash = unmuted
- OLED shows current volume level and mute status

---

## 🔐 Security Design

| Layer | Description |
|--------|--------------|
| **Secure Element (ATECC608A)** | Isolated key storage and signing; attestation certificate storage. |
| **ESP32-S3 Firmware** | Manages CTAP2 protocol, fingerprint logic, and USB communication. |
| **Fingerprint Sensor** | Performs on-device biometric match; no template leaves the sensor. |
| **USB-C Data Path** | ESP32-S3 acts as composite HID device for both FIDO2 and keyboard interfaces. |

---

## 🧱 Hardware Connections

### Detailed Pin Assignments

| Peripheral | Interface | ESP32-S3 Pins | Notes |
|-------------|------------|---------------|-------|
| **USB-C D+ / D-** | USB OTG | GPIO19/20 (Built-in USB) | Direct connection for HID + FIDO2 |
| **USB-C CC1 / CC2** | Analog | 5.1kΩ → GND | USB device mode identification |
| **USB-C VBUS** | Power | 5V input | Feed to AMS1117-3.3V regulator |
| **ATECC608A** | I²C | SDA: GPIO8, SCL: GPIO9 | Add 4.7kΩ pull-ups to 3.3V |
| **Fingerprint Sensor (R307)** | UART | TX: GPIO17, RX: GPIO18 | 3.3V logic; 9600 baud default |
| **OLED Display (SSD1306)** | I²C | SDA: GPIO8, SCL: GPIO9 (shared) | I²C address: 0x3C |
| **Rotary Encoder 1 (Volume)** | GPIO | CLK: GPIO1, DT: GPIO2, SW: GPIO3 | Hardware interrupts enabled |
| **Rotary Encoder 2 (Scroll)** | GPIO | CLK: GPIO4, DT: GPIO5, SW: GPIO6 | Hardware interrupts enabled |
| **RGB LED Strip (12× WS2812B)** | RMT | Data: GPIO48 | Use ESP32-S3 RMT peripheral |
| **Key Matrix Rows (3)** | GPIO Output | R0: GPIO10, R1: GPIO11, R2: GPIO12 | Active-low scanning |
| **Key Matrix Columns (4)** | GPIO Input | C0: GPIO13, C1: GPIO14, C2: GPIO15, C3: GPIO16 | Pull-up resistors enabled |
| **Matrix Diodes (12×)** | — | 1N4148 on each key | Prevents ghosting/blocking |

---

## 🔋 USB-C Design Notes

- Use **USB-C receptacle** for USB 2.0 only.  
- Connect **D+ / D-** directly to ESP32-S3 USB pins.  
- Add **5.1kΩ Rd** resistors from CC1 and CC2 to GND to signal device role.  
- Include **ESD protection** and **ferrite bead** on USB data lines.  
- Route D+/D- as **90Ω differential pair** for signal integrity.  
- Use **VBUS sense** to detect host connection if required.

---

## 🧩 Software Architecture

```
macOS / Browser (Safari, Chrome)
  ↓ WebAuthn / CTAP2 Commands over USB HID
  ↓ HID Keyboard Reports (macro keys)
  ↓ HID Consumer Control (volume, media)
  
ESP32-S3 Firmware (Main Controller)
  ├─ USB Stack (Composite Device)
  │   ├─ HID Interface 1: FIDO2 CTAP2 (Usage Page 0xF1D0)
  │   ├─ HID Interface 2: Keyboard (macro keys)
  │   └─ HID Interface 3: Consumer Control (volume/media)
  │
  ├─ FIDO2 Authentication Module
  │   ├─ CTAP2 Handler (makeCredential, getAssertion)
  │   ├─ Fingerprint Verification Gate
  │   └─ ATECC608A Driver (CryptoAuthLib)
  │
  ├─ Macropad Functionality
  │   ├─ Key Matrix Scanner (3×4, 1ms polling)
  │   ├─ Macro Engine (programmable shortcuts)
  │   ├─ Rotary Encoder Handler (volume, scroll)
  │   └─ RGB LED Controller (WS2812B via RMT)
  │
  ├─ User Interface
  │   ├─ OLED Display Driver (SSD1306)
  │   ├─ Status Messages ("Scan finger", "Key 1: Copy")
  │   └─ Visual Feedback (LED patterns)
  │
  └─ Peripheral Drivers
      ├─ Fingerprint Sensor (UART @ 9600 baud)
      ├─ I²C Bus Manager (OLED + ATECC608A)
      └─ GPIO Interrupt Handlers

ATECC608A Secure Element
  ├─ Secure Key Storage (up to 16 slots)
  ├─ ECDSA P-256 Signing Operations
  └─ Attestation Certificate (pre-provisioned)
```

---

## 🧰 Bill of Materials (BOM)

| Component | Qty | Part Number / Model | Source | Notes |
|-----------|-----|---------------------|--------|-------|
| **ESP32-S3 Module** | 1 | ESP32-S3-WROOM-1-N8R8 | [Digikey](https://www.digikey.com), [Adafruit #5364](https://www.adafruit.com/product/5364) | 8MB Flash, 8MB PSRAM, native USB |
| **Secure Element** | 1 | ATECC608A-MAHDA-T | [Digikey](https://www.digikey.com/product-detail/en/microchip-technology/ATECC608A-MAHDA-T/ATECC608A-MAHDA-TCT-ND) | I²C crypto chip (SOIC-8) |
| **Fingerprint Sensor** | 1 | R307 or AS608 | AliExpress, Amazon | UART fingerprint module |
| **OLED Display** | 1 | SSD1306 0.96" 128×64 I²C | [Adafruit #326](https://www.adafruit.com/product/326), Amazon | Shows key functions + prompts |
| **Rotary Encoders** | 2 | EC11 or PEC11R | [Digikey](https://www.digikey.com), [Adafruit](https://www.adafruit.com) | With push button, 6mm D-shaft |
| **Mechanical Switches** | 12 | Cherry MX / Gateron / Kailh | [Adafruit](https://www.adafruit.com), MechanicalKeyboards.com | 3-pin PCB mount |
| **Keycaps** | 12 | DSA or Cherry profile 1U | [Adafruit](https://www.adafruit.com), Amazon | Blank or custom legends |
| **RGB LEDs** | 12 | WS2812B-5050 or SK6812 MINI | [Digikey](https://www.digikey.com), AliExpress | Addressable RGB for underglow |
| **USB-C Receptacle** | 1 | USB4105-GF-A | [Digikey](https://www.digikey.com/product-detail/en/gct/USB4105-GF-A/2073-USB4105-GF-ACT-ND) | 16-pin mid-mount |
| **Voltage Regulator** | 1 | AMS1117-3.3 (SOT-223) | [Digikey](https://www.digikey.com), Amazon | 5V → 3.3V LDO, 1A |
| **Diodes (Switching)** | 12 | 1N4148 (SOD-123) | [Digikey](https://www.digikey.com) | Key matrix anti-ghosting |
| **Resistors (I²C Pull-up)** | 2 | 4.7kΩ 0805 | [Digikey](https://www.digikey.com) | SDA/SCL lines |
| **Resistors (USB-C CC)** | 2 | 5.1kΩ 0805 | [Digikey](https://www.digikey.com) | CC1/CC2 device identification |
| **Capacitors (Decoupling)** | 5 | 0.1µF, 10µF (0805, 1206) | [Digikey](https://www.digikey.com) | Power filtering |
| **Encoder Knobs** | 2 | Aluminum 6mm D-shaft | Amazon, AliExpress | For rotary encoders |
| **Rubber Feet** | 4 | 3M Bumper SJ5302 | Amazon | Non-slip base pads |
| **PCB** | 1 | Custom 2-layer FR4 | [JLCPCB](https://jlcpcb.com), [PCBWay](https://www.pcbway.com) | ~100×100mm (4×4 inches) |

---

## 📺 OLED Display Features

The SSD1306 0.96" display provides real-time feedback for both passkey authentication and macro operations:

### Display Modes

**1. Idle / Macro Reference Mode** (Default)
```
╔════════════════╗
║  MACROPAD READY ║
║ [1:Copy] [2:Paste] ║
║ [3:Save] [4:Undo]  ║
║ Volume: ▓▓▓▓▓░░ 65% ║
╚════════════════╝
```

**2. FIDO2 Authentication Mode**
```
╔════════════════╗
║ 🔐 PASSKEY AUTH  ║
║ GitHub.com      ║
║ >> SCAN FINGER  ║
║ [Waiting...]    ║
╚════════════════╝
```

**3. Success Confirmation**
```
╔════════════════╗
║ ✓ LOGIN SUCCESS ║
║ Welcome back!   ║
║ GitHub.com      ║
║                 ║
╚════════════════╝
```

**4. Error Handling**
```
╔════════════════╗
║ ✗ AUTH FAILED   ║
║ Finger not      ║
║ recognized      ║
║ Try again       ║
╚════════════════╝
```

### Display Updates
- **Key press:** Shows which key was pressed and its function
- **Encoder turn:** Shows volume level or scroll position
- **FIDO2 request:** Shows requesting website and status
- **Fingerprint scan:** Shows real-time verification status

---

## 📡 Development References

### FIDO2 / Security
| Source | Description |
|---------|-------------|
| [ESP-IDF USB HID examples](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/hid) | ESP32 native USB HID reference |
| [Microchip CryptoAuthLib](https://github.com/MicrochipTech/cryptoauthlib) | ATECC608A driver and API |
| [SoloKeys Open Source FIDO2](https://github.com/solokeys/solo) | Reference for CTAP2 implementation |
| [FIDO2 CTAP2 Specification](https://fidoalliance.org/specifications/) | Official CTAP2 protocol specification |
| [WebAuthn.io Test Site](https://webauthn.io) | Test passkey registration/authentication |

### Macropad Hardware
| Source | Description |
|---------|-------------|
| [Ocreeb-12 GitHub](https://github.com/sb-ocr/ocreeb-12) | 12-key macropad design inspiration |
| [Ocreeb-12 Build Guide](https://www.instructables.com/DIY-Mechanical-Macro-Keypad-Ocreeb) | Assembly instructions and techniques |

---

## ⚙️ Dynamic Configuration System

The macropad supports **dynamic configuration** without firmware reflashing! Configure macros, LED colors, brightness, and OS mode on-the-fly.

### Features
- ✅ **EEPROM Storage** - All settings saved persistently
- ✅ **Hardcoded Defaults** - Fallback macros always available
- ✅ **Two Methods** - Serial commands or Web UI
- ✅ **No Reflash** - Change settings instantly

### Configuration Methods

**1. Serial Commands** (via Serial Monitor @ 115200 baud)
```
MACRO 1 1 0 4          # Key 1: Ctrl+C (keycombo)
LED 0 255 0 0          # LED 0: Red
BRIGHTNESS 100          # Set brightness
OS mac                  # Switch to macOS
LIST                    # Show all settings
RESET                   # Reset to defaults
```

**2. Web Configurator** (`tools/web_configurator.html`)
- Beautiful web interface (Chrome/Edge)
- Visual macro editor
- Color picker for LEDs
- OS mode selector
- Real-time log

### What Can Be Configured
- **Macros** (12 keys): Key combos, strings, or special functions
- **LED Colors** (5 LEDs): Individual RGB colors
- **LED Brightness**: Overall brightness (0-255)
- **OS Mode**: macOS or Windows (affects modifier keys)

### EEPROM Layout
```
Address 0:   OS Mode (1 byte)
Address 1:   Magic byte (0xAB)
Address 2:   LED Brightness (1 byte)
Address 3-17: LED Colors (15 bytes)
Address 20-115: Macros (96 bytes)
Address 116-511: Reserved
```

**Documentation:** See `tools/CONFIGURATOR_README.md` for complete user guide.

---

## 🧭 Implementation Roadmap

### Phase 1: Hardware Assembly (Week 1-2)
1. **Order components** from BOM (PCB, ESP32-S3, ATECC608A, fingerprint sensor, etc.)
2. **Design/order PCB** or use breadboard for prototyping
   - Reference Ocreeb-12 layout for 12-key matrix
   - Add mounting points for fingerprint sensor and OLED
3. **Assemble hardware:**
   - Solder ESP32-S3 module, ATECC608A, voltage regulator, resistors
   - Mount 12 mechanical switches with diodes
   - Install 2 rotary encoders
   - Connect fingerprint sensor (UART) and OLED (I²C)
   - Wire RGB LED strip (12× WS2812B)
4. **Power-on test:** Verify 3.3V rails, USB enumeration

### Phase 2: Firmware Development (Week 3-6)

**Step 1: Basic USB HID Keyboard** (Week 3)
- Set up ESP-IDF development environment
- Implement USB HID keyboard functionality
- Test key matrix scanning (3×4)
- Verify macro key input on macOS

**Step 2: Add Peripherals** (Week 4)
- Integrate OLED display (SSD1306 driver)
- Add rotary encoder handlers (volume control)
- Implement RGB LED control (WS2812B via RMT)
- Test macro key display on OLED

**Step 3: Fingerprint Integration** (Week 5)
- Integrate R307/AS608 fingerprint sensor (UART)
- Implement fingerprint enrollment/verification
- Test fingerprint-gated macro execution

**Step 4: FIDO2 Implementation** (Week 6)
- Integrate ATECC608A (CryptoAuthLib)
- Implement CTAP2 HID transport
- Add USB FIDO2 interface (Usage Page 0xF1D0)
- Connect fingerprint verification to FIDO2 flow

### Phase 3: Testing & Validation (Week 7-8)

**macOS Browser Testing:**
1. Test WebAuthn registration on [webauthn.io](https://webauthn.io)
2. Create passkeys on GitHub, Google, Microsoft accounts
3. Test login flow: touch fingerprint → instant login
4. Verify OLED shows correct website names and prompts

**Macropad Testing:**
1. Program 12 macro keys with common shortcuts
2. Test volume control with rotary encoder
3. Verify OLED displays key functions correctly
4. Test RGB LED feedback patterns

### Phase 4: Enclosure & Polish (Week 9+)
1. Design 3D-printed enclosure (inspired by Ocreeb-12)
2. Add keycap legends (printed or dye-sublimation)
3. Fine-tune firmware (animations, timings)
4. Create user guide and configuration tool

---

## ✅ Design Summary

This ESP32-S3 fingerprint macropad combines **security** and **productivity** in one compact device:

### ✨ Key Features Achieved:

**1. 🔐 Passwordless Authentication (Your Primary Goal)**
- Touch fingerprint sensor → Instant login to websites
- No more typing passwords on your Mac
- Works with GitHub, Google, Microsoft, and any WebAuthn-compatible site
- Secure FIDO2 implementation with hardware-backed keys (ATECC608A)

**2. ⌨️ 12-Key Programmable Macropad**
- Cherry MX mechanical switches (Ocreeb-12 inspired layout)
- Customizable shortcuts for productivity
- RGB underglow for visual feedback
- OLED shows what each key does in real-time

**3. 🔊 Media Control**
- 2 rotary encoders for volume and scrolling
- Physical controls for media playback
- Better than on-screen controls for quick adjustments

**4. 📺 Visual Feedback**
- OLED display shows authentication status
- Real-time key function reference
- Website names during passkey requests
- Success/error messages

### 🎯 Use Case Example:
1. **Working on Mac** → Press macro key to launch apps or paste common text
2. **Need to login to GitHub** → Browser prompts for passkey → Touch fingerprint → Logged in (2 seconds, no typing!)
3. **Adjust volume during meeting** → Turn encoder knob → Volume changes smoothly
4. **Check key functions** → Look at OLED → See "[1:Copy] [2:Paste] [3:Save]..."

### 🆚 Why ESP32-S3 Over KB2040:
- ✅ **Full FIDO2 support** (KB2040 cannot do this)
- ✅ **Hardware crypto** acceleration via ATECC608A
- ✅ **More GPIO pins** for expansion
- ✅ **Native USB** for composite HID device
- ✅ **Supports all macropad features** from Ocreeb-12

**Bottom Line:** This design achieves your goal of using a fingerprint sensor for browser passkey authentication on macOS, while also functioning as a full-featured 12-key macropad with OLED display and rotary encoders.

---

## 🖼️ Reference Diagrams

### System Block Diagram
```
┌──────────────────────────────────────────────────────────┐
│                     ESP32-S3 Controller                   │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │ USB Stack   │  │ FIDO2 Module │  │ Macro Engine    │  │
│  │ (Composite) │  │ + CTAP2      │  │ + Key Scanner   │  │
│  └─────────────┘  └──────────────┘  └─────────────────┘  │
└───────┬──────────────────┬───────────────────┬───────────┘
        │                  │                   │
        │ USB              │ I²C               │ GPIO
        ▼                  ▼                   ▼
   ┌────────┐      ┌──────────────┐    ┌─────────────┐
   │  Mac   │      │  ATECC608A   │    │  12 Keys    │
   │(Safari)│      │ Secure Element│    │ 2 Encoders  │
   └────────┘      └──────────────┘    │ 12 RGB LEDs │
                                       └─────────────┘
        │                  │                   │
        │ UART             │ I²C               │
        ▼                  ▼                   │
   ┌────────┐      ┌──────────────┐           │
   │ R307   │      │  SSD1306     │◄──────────┘
   │Finger  │      │  OLED 0.96"  │  (Status Display)
   │print   │      │  Display     │
   └────────┘      └──────────────┘
```

For detailed PCB schematics and layout, refer to:  
**`ESP32_S3_FIDO2_Macropad_Schematic.pdf`** *(to be created)*

---

**Author:** DIY Security Macropad Project  
**Version:** 2.1  
**Last Updated:** 2025-11-14  
