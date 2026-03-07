# ESP32-S3 FIDO2 Fingerprint Macropad - Complete Wiring Diagram

**Project:** Passwordless Browser Authentication Macropad  
**MCU:** Adafruit ESP32-S3 Feather (8MB Flash)  
**Date:** 2026-03-04  
**Version:** 2.3 - **SYNCED WITH SCHEMATIC (macropad_v2.net)**

---

## ⚡ **QUICK PIN REFERENCE (CORRECTED)**

| GPIO | Component | Function |
|------|-----------|----------|
| **GPIO3** | I²C SDA | OLED + ATECC608A |
| **GPIO4** | I²C SCL | OLED + ATECC608A |
| **GPIO5** | Matrix Row 1 | Keys 1-4 |
| **GPIO6** | Matrix Row 2 | Keys 5-8 |
| **GPIO9** | Matrix Row 3 | Keys 9-12 |
| **GPIO10** | Encoder CLK | Volume rotation |
| **GPIO11** | Encoder DT | Volume direction |
| **GPIO12** | Encoder SW | Mute button |
| **GPIO13** | Matrix Col 1 | All keys |
| **GPIO14** | Matrix Col 2 | All keys |
| **GPIO15** | Matrix Col 3 | All keys |
| **GPIO16** | Matrix Col 4 | All keys |
| **GPIO17** | LED Data | 5× WS2812B (via shifter) |
| **GPIO18** | Fingerprint RST | Sleep/wake control (output) |
| **GPIO8** | Fingerprint WAKE | Finger detect (input, interrupt) |
| **GPIO38** | UART1 RX | Fingerprint TX (U1RXD) |
| **GPIO39** | UART1 TX | Fingerprint RX (U1TXD) |

**❌ Unavailable:** GPIO7 (reserved for I2C power, not on header), GPIO21/33 (onboard NeoPixel)


### Pin Reference Table

| Arduino Label | GPIO | Notes |
|---------------|------|-------|
| SDA | 3 | I²C Data (STEMMA QT) |
| SCL | 4 | I²C Clock (STEMMA QT) |
| A0 | 18 | Analog / Digital |
| A1 | 17 | Analog / Digital |
| A2 | 16 | Analog / Digital |
| A3 | 15 | Analog / Digital |
| A4 | 14 | Analog / Digital |
| A5 | 8 | Analog / Digital |
| D5 | 5 | Digital |
| D6 | 6 | Digital |
| D9 | 9 | Digital |
| D10 | 10 | Digital |
| D11 | 11 | Digital |
| D12 | 12 | Digital |
| D13 | 13 | Digital |
| **RX** | **38** | UART1 RX (U1RXD) |
| **TX** | **39** | UART1 TX (U1TXD) |
| NEOPIXEL | 33 | Onboard RGB LED |
| NEOPIXEL_POWER | 21 | Onboard NeoPixel enable |

---

## 📋 Table of Contents

1. [Adafruit ESP32-S3 Feather Pinout Reference](#adafruit-esp32-s3-feather-pinout-reference)
2. [System Overview](#system-overview)
3. [Power Distribution](#power-distribution)
4. [Component Connections](#component-connections)
5. [Key Matrix Wiring](#key-matrix-wiring)
6. [RGB LED Strip with Level Shifter](#rgb-led-strip-with-level-shifter)
7. [Complete Pin Assignment Table](#complete-pin-assignment-table)
8. [Physical Layout](#physical-layout)
9. [Breadboard Prototype Diagram](#breadboard-prototype-diagram)
10. [Testing Checklist](#testing-checklist)

---

## 🎯 System Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                         SYSTEM ARCHITECTURE                         │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│                    ┌──────── USB-C ────────┐                       │
│                    │   (Power + Data)      │                       │
│                    │   5V + D+/D- (USB OTG)│                       │
│                    └───────────┬───────────┘                       │
│                                │                                    │
│                                ▼                                    │
│              ╔═════════════════════════════════╗                   │
│              ║  Adafruit ESP32-S3 Feather     ║                   │
│              ║  - Main Controller             ║                   │
│              ║  - USB HID Keyboard            ║                   │
│              ║  - FIDO2 CTAP2 Authenticator   ║                   │
│              ║  - 3.3V Regulator (500mA)      ║                   │
│              ╚═════════════════════════════════╝                   │
│                   │    │    │    │    │    │                       │
│        ┌──────────┘    │    │    │    │    └────────────┐         │
│        │               │    │    │    │                  │         │
│        ▼               ▼    ▼    ▼    ▼                  ▼         │
│   ┌────────┐      I²C Bus  UART  GPIO Matrix       5V Power       │
│   │Waveshare│       │  │   │ │    │  │                 │          │
│   │ Round  │       │  │   │ │    │  │                 │          │
│   │Finger  │       │  │   │ │    │  │                 │          │
│   │print   │       │  │   │ │    │  │                 │          │
│   │RST/WAKE│       │  │   │ │    │  │                 │          │
│   └────────┘       ▼  ▼   ▼ ▼    ▼  ▼                 ▼          │
│  (Capacitive)  ┌─────┐ ┌────┐ ┌──────┐      ┌──────────────┐    │
│                │OLED │ │ATECC│ │12 Keys│      │ Level Shifter│    │
│                │1.3" │ │608A │ │1 Enc. │      │ 3.3V → 5V    │    │
│                └─────┘ └────┘ └──────┘      └──────┬───────┘    │
│                0x3C    0x60    + Diodes              │            │
│                                                      ▼            │
│                                              ┌──────────────┐    │
│                                              │ 5× WS2812B   │    │
│                                              │ Underglow @5V│    │
│                                              └──────────────┘    │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

## ⚡ Power Distribution

### Power Supply Architecture

```
                     USB-C Port (from Mac)
                           │
                           ├───── 5V (VBUS) ─────┐
                           │                      │
                           └───── GND ────────────┼─── Common Ground
                                                  │
┌─────────────────────────────────────────────────┼──────────────┐
│                  ESP32-S3 Feather               │              │
│                                                 │              │
│   ┌─────────────────────────────────────┐      │              │
│   │  3.3V Regulator (AP2112K - 500mA)    │      │              │
│   │  5V IN ──────────► 3.3V OUT         │      │              │
│   └─────────────────────────────────────┘      │              │
│                         │                       │              │
│                         └─── 3.3V Power Rail   │              │
│                                   │             │              │
└───────────────────────────────────┼─────────────┼──────────────┘
                                    │             │
                  ┌─────────────────┼─────────────┼──────────────┐
                  │                 │             │              │
                  ▼                 ▼             ▼              ▼
            ┌──────────┐      ┌─────────┐  ┌─────────┐   ┌──────────┐
            │Fingerprint│      │  OLED   │  │ ATECC   │   │WS2812B LEDs│
            │  Sensor  │      │ Display │  │ 608A    │   │   @ 5V     │
            │  @ 3.3V  │      │ @ 3.3V  │  │ @ 3.3V  │   │  (5 LEDs)  │
            └──────────┘      └─────────┘  └─────────┘   └──────────┘
            ~1-5mA sleep         ~20mA         <1mA         ~300mA max
            ~50-120mA active                                (60mA × 5)
            (RST pin controlled)
```

### Power Budget

| Component | Voltage | Current (Typical) | Current (Max) |
|-----------|---------|-------------------|---------------|
| ESP32-S3 Feather | 3.3V | 80-150mA | 240mA |
| OLED Display | 3.3V | 15mA | 20mA |
| Fingerprint Sensor | 3.3V | ~3mA sleep | 120mA scanning |
| ATECC608A | 3.3V | <1mA | <1mA |
| 1× Rotary Encoder  | 3.3V | <1mA | <1mA |
| **WS2812B LEDs (5×)** | **5V** | **75mA (25%)** | **300mA (100%)** |
| **Total from USB 5V** | — | **~245mA** | **~680mA** |

**Note:** 
- 3.3V components draw from onboard regulator (max 500mA) ✅
- WS2812B draws directly from 5V USB (bypasses regulator) ✅
- Typical usage: fingerprint sleeping + LEDs at 25% = ~245mA total ✅ USB 2.0 safe
- During FIDO2 auth (fingerprint active): ~290mA typical (still well within budget)
- USB 2.0 provides up to 500mA; USB 3.x provides up to 900mA

---

## 🔌 Component Connections

### 1. Waveshare Round Capacitive Fingerprint Sensor (UART + RST/WAKE)

```
┌───────────────────────────────────────────────────────┐
│  Waveshare Round Fingerprint Sensor                   │
│  (All-in-One Capacitive with Cortex Processor)        │
├───────────────────────────────────────────────────────┤
│                                                        │
│         ┌────────────┐                                │
│         │     ○      │  ← Capacitive touch surface    │
│         │   (●●●)    │     (360° recognition)         │
│         │     ○      │                                │
│         └─────┬──────┘                                │
│               │                                        │
│         ┌─────┴──────────┐                            │
│         │  6-pin header  │                            │
│         └────────────────┘                            │
│          1  2  3  4  5  6                             │
│          │  │  │  │  │  │                             │
└──────────┼──┼──┼──┼──┼──┼─────────────────────────────┘
           │  │  │  │  │  │
           │  │  │  │  │  └─ WAKE  (white - finger detect output)
           │  │  │  │  └──── RST   (blue - sleep/reset control)
           │  │  │  └─────── RX    (yellow - sensor receives)
           │  │  └────────── TX    (green - sensor sends)
           │  └───────────── GND   (black)
           └──────────────── VCC   (red - 3.3V)

Wiring to ESP32-S3 Feather (UART1 - crossover TX/RX):
┌─────────────────┬──────────────┬───────────────────────┐
│ Sensor Pin      │ Wire Color   │ ESP32-S3 Feather Pin  │
├─────────────────┼──────────────┼───────────────────────┤
│ Pin 1: VCC      │ Red          │ 3V (3.3V)             │
│ Pin 2: GND      │ Black        │ GND                   │
│ Pin 3: TX       │ Green        │ GPIO38 (U1RXD)        │
│ Pin 4: RX       │ Yellow       │ GPIO39 (U1TXD)        │
│ Pin 5: RST      │ Blue         │ GPIO18 (A0) - Output  │
│ Pin 6: WAKE     │ White        │ GPIO8  (A5) - Input   │
└─────────────────┴──────────────┴───────────────────────┘

Baud Rate: 19200 (Waveshare default)
Protocol: Custom packet (0xF5 header/tail, XOR checksum)
IMPORTANT: TX → RX, RX → TX (crossover connection)

Sleep/Wake Power Management:
┌─────────────────────────────────────────────────────────┐
│  The fingerprint sensor stays in SLEEP MODE by default  │
│  and only wakes for registration or FIDO2 auth.         │
│                                                          │
│  RST Pin (GPIO18 → Sensor Pin 5):                       │
│  - ESP32 OUTPUT: controls sensor power state             │
│  - LOW  = sensor in sleep/standby (~1-5mA)              │
│  - HIGH = sensor active and ready (~50-120mA)            │
│  - On boot: set GPIO18 LOW (keep sensor sleeping)        │
│  - Wake sequence: set HIGH → wait 200ms → send command   │
│  - Sleep sequence: send sleep cmd → set LOW              │
│                                                          │
│  WAKE Pin (GPIO8 → Sensor Pin 6):                        │
│  - ESP32 INPUT with INTERRUPT: finger detect signal      │
│  - Sensor pulls HIGH when a finger touches the surface   │
│  - Use as hardware interrupt (RISING edge) to notify     │
│    ESP32 that a finger is present                        │
│  - Allows the ESP32 to react without polling             │
│                                                          │
│  Typical Flow:                                           │
│   1. Sensor sleeps (RST=LOW), ESP32 running normally     │
│   2. User triggers FIDO2 auth (browser passkey prompt)   │
│   3. ESP32 wakes sensor (RST=HIGH), waits 200ms         │
│   4. OLED shows "Place finger on sensor"                 │
│   5. WAKE pin goes HIGH → interrupt fires                │
│   6. ESP32 captures + verifies fingerprint via UART      │
│   7. On success: sign FIDO2 assertion, send to browser   │
│   8. ESP32 puts sensor back to sleep (RST=LOW)           │
│                                                          │
│  Power Savings:                                          │
│   - Active scanning: ~50-120mA (only during auth)        │
│   - Sleep mode: ~1-5mA (idle most of the time)           │
│   - Reduces avg current by ~50-115mA                     │
│                                                          │
│  Firmware Configuration:                                 │
│   - GPIO18: pinMode(18, OUTPUT); digitalWrite(18, LOW);  │
│   - GPIO8:  pinMode(8, INPUT);                           │
│             attachInterrupt(8, onFingerDetect, RISING);  │
└─────────────────────────────────────────────────────────┘
```

---

### 2. OLED Display 1.3" 128×64 (I²C)

```
┌───────────────────────────────────────────────────────┐
│  SH1106 OLED Display - 1.3 inch                        │
│  128×64 pixels - I²C Interface                        │
├───────────────────────────────────────────────────────┤
│                                                        │
│   ╔════════════════════════════════════╗              │
│   ║  ┌─────────────────────────────┐  ║              │
│   ║  │                             │  ║              │
│   ║  │     128 × 64 Display        │  ║              │
│   ║  │      Active Area            │  ║              │
│   ║  │                             │  ║              │
│   ║  └─────────────────────────────┘  ║              │
│   ╚════════════════════════════════════╝              │
│         │   │   │   │                                 │
│        GND VCC SCL SDA                                │
└─────────┴───┴───┴───┴─────────────────────────────────┘

Wiring to ESP32-S3 Feather (I²C):
┌─────────────────┬──────────────┬───────────────────────┐
│ OLED Pin        │ Wire Color   │ ESP32-S3 Feather Pin  │
├─────────────────┼──────────────┼───────────────────────┤
│ GND             │ Black        │ GND                   │
│ VCC             │ Red          │ 3V (3.3V)             │
│ SCL (Clock)     │ Yellow       │ SCL (I²C Clock)       │
│ SDA (Data)      │ Blue         │ SDA (I²C Data)        │
└─────────────────┴──────────────┴───────────────────────┘

I²C Address: 0x3C (standard)

Alternative: STEMMA QT Connection
┌─────────────────────────────────────────────────────┐
│ If OLED has STEMMA QT/Qwiic connector:             │
│                                                      │
│  OLED STEMMA QT ──[JST-SH Cable]── ESP32-S3 STEMMA │
│  (4-pin JST-SH)                     QT Port         │
│                                                      │
│  No wiring needed! Plug and play! ✅               │
└─────────────────────────────────────────────────────┘
```

---

### 3. ATECC608A Secure Element (I²C - Shared Bus)

```
┌───────────────────────────────────────────────────────┐
│  Microchip ATECC608A Crypto Co-Processor              │
│  I²C Interface (Shares bus with OLED)                 │
├───────────────────────────────────────────────────────┤
│                                                        │
│   ┌────────────────────────┐                          │
│   │   ATECC608A-MAHDA      │  ← SOIC-8 Package        │
│   │   [◼◼◼◼◼◼◼◼]           │     or Breakout Board    │
│   └────────────────────────┘                          │
│     1  2  3  4  5  6  7  8                            │
│     │  │  │  │  │  │  │  │                            │
└─────┼──┼──┼──┼──┼──┼──┼──┼────────────────────────────┘
      │  │  │  │  │  │  │  │
      NC NC NC GND SDA SCL NC  VCC

Wiring to ESP32-S3 Feather (I²C - Shared):
┌─────────────────┬──────────────┬───────────────────────┐
│ ATECC608A Pin   │ Wire Color   │ ESP32-S3 Feather Pin  │
├─────────────────┼──────────────┼───────────────────────┤
│ Pin 4: GND      │ Black        │ GND                   │
│ Pin 5: SDA      │ Blue         │ SDA (shared with OLED)│
│ Pin 6: SCL      │ Yellow       │ SCL (shared with OLED)│
│ Pin 8: VCC      │ Red          │ 3V (3.3V)             │
└─────────────────┴──────────────┴───────────────────────┘

I²C Address: 0x60 (default)

I²C Bus Topology:
                  ESP32-S3 Feather
                      │   │
                     SDA SCL
                      │   │
        ┌─────────────┴───┴─────────────┐
        │                               │
    ┌───┴───┐                      ┌────┴────┐
    │ OLED  │                      │ATECC608A│
    │ 0x3C  │                      │  0x60   │
    └───────┘                      └─────────┘

Pull-up Resistors: 4.7kΩ on SDA and SCL (usually built-in on Feather)

IMPORTANT NOTE - Random Number Generation:
  A brand-new unconfigured ATECC608A will return a fixed dummy pattern
  (FFFF0000FFFF0000...) instead of actual random data. This is NORMAL
  and expected behavior when the config zone is UNLOCKED.

  Once the config zone is configured and locked (during FIDO2 setup),
  the chip will generate proper cryptographic 32-byte random numbers.
```

---

### 4. Rotary Encoder (M274 360° or EC11)

```
┌───────────────────────────────────────────────────────┐
│  M274 / EC11 Rotary Encoder (Standard)                │
│  With integrated push button for mute/unmute          │
├───────────────────────────────────────────────────────┤
│                                                        │
│          ┌────────────┐                               │
│          │     ○      │  ← Knob (6mm D-shaft)         │
│          │   ╱───╲    │                               │
│          │  │  ▓  │   │  ← Encoder body               │
│          │   ╲───╱    │                               │
│          └──────┬─────┘                               │
│                 │                                      │
│         ┌───────┴────────┐                            │
│         │  5-pin header  │                            │
│         └────────────────┘                            │
│         A  B  C  D  E                                 │
│         │  │  │  │  │                                 │
└─────────┼──┼──┼──┼──┼─────────────────────────────────┘
          │  │  │  │  │
         CLK DT SW +  GND

Encoder Pinout:
- CLK (A): Channel A output (rotation detection)
- DT  (B): Channel B output (rotation direction)
- SW  (C): Push button switch (active low - for mute/unmute)
- +   (D): Power (3.3V)
- GND (E): Ground

Rotary Encoder (Volume Control + Mute):
┌─────────────────┬──────────────┬───────────────────────┐
│ Encoder Pin     │ Wire Color   │ ESP32-S3 Feather Pin  │
├─────────────────┼──────────────┼───────────────────────┤
│ CLK (Channel A) │ White        │ GPIO10                │
│ DT (Channel B)  │ Gray         │ GPIO11                │
│ SW (Button)     │ Purple       │ GPIO12                │
│ + (VCC)         │ Red          │ 3V (3.3V)             │
│ GND             │ Black        │ GND                   │
└─────────────────┴──────────────┴───────────────────────┘

Functions (Single Encoder):
- Rotate Clockwise: Volume Up (5% per step)
- Rotate Counter-clockwise: Volume Down (5% per step)
- Press Button: Mute/Unmute toggle
  * Visual Feedback: Red flash = Muted, Green flash = Unmuted
  * When muted, volume rotation is locked
  * OLED shows current volume level and mute status

Internal Pull-up Configuration:
- CLK, DT, SW pins: Enable internal pull-ups in firmware
- No external resistors needed
```

---

## 🎹 Key Matrix Wiring (3 Rows × 4 Columns)

### Matrix Theory

```
A key matrix reduces pin count by scanning rows and columns:
- 12 individual keys would need 12 GPIO pins
- 3×4 matrix only needs 3 + 4 = 7 GPIO pins
- Diodes prevent "ghosting" (false key presses)

Scanning Method:
1. Set all rows HIGH
2. Set one row LOW
3. Read all columns
4. Any column that reads LOW = key pressed at that row/column
5. Repeat for next row
```

### Matrix Layout

```
Physical Key Layout:
┌───────────────────────────────────────┐
│                                       │
│    [1]    [2]    [3]    [4]          │
│                                       │
│    [5]    [6]    [7]    [8]          │
│                                       │
│    [9]    [10]   [11]   [12]         │
│                                       │
└───────────────────────────────────────┘

Electrical Matrix (CORRECTED):
              Col 0    Col 1    Col 2    Col 3
              GPIO13   GPIO14   GPIO15   GPIO16
                │        │        │        │
Row 0 GPIO5  ───┼────────┼────────┼────────┼───
                │        │        │        │
               [K1]     [K2]     [K3]     [K4]
                │        │        │        │
Row 1 GPIO6  ───┼────────┼────────┼────────┼───
                │        │        │        │
               [K5]     [K6]     [K7]     [K8]
                │        │        │        │
Row 2 GPIO9  ───┼────────┼────────┼────────┼───
                │        │        │        │
               [K9]     [K10]    [K11]    [K12]
                │        │        │        │
```

### Individual Key Wiring (WITH DIODE)

```
Each key needs a diode to prevent ghosting:

       Row Pin (GPIO5/6/9)
            │
            │
            ├─────► Cathode ││◄──── Anode (1N4148)
                                    │
                                    │
                              ┌─────┴─────┐
                              │           │
                              │  Switch   │
                              │  (Cherry  │
                              │   MX)     │
                              │           │
                              └─────┬─────┘
                                    │
                                    │
                                Column Pin (GPIO13/14/15/16)

Key 1 Example:
  GPIO5 ──← Cathode │├ Anode ←── [Switch] ──→ GPIO13

IMPORTANT: 
- Diode cathode (line/band) connects to row pin
- Diode anode connects to switch
- Switch other terminal connects to column pin
- Current flows: Column(HIGH) → Switch → Anode → Cathode → Row(LOW)
```

### Complete Matrix Wiring Table

| Key # | Row | Col | Row GPIO | Col GPIO | Diode | Switch Type |
|-------|-----|-----|----------|----------|-------|-------------|
| **1** | 0 | 0 | GPIO5 | GPIO13 | 1N4148 | Cherry MX |
| **2** | 0 | 1 | GPIO5 | GPIO14 | 1N4148 | Cherry MX |
| **3** | 0 | 2 | GPIO5 | GPIO15 | 1N4148 | Cherry MX |
| **4** | 0 | 3 | GPIO5 | GPIO16 | 1N4148 | Cherry MX |
| **5** | 1 | 0 | GPIO6 | GPIO13 | 1N4148 | Cherry MX |
| **6** | 1 | 1 | GPIO6 | GPIO14 | 1N4148 | Cherry MX |
| **7** | 1 | 2 | GPIO6 | GPIO15 | 1N4148 | Cherry MX |
| **8** | 1 | 3 | GPIO6 | GPIO16 | 1N4148 | Cherry MX |
| **9** | 2 | 0 | GPIO9 | GPIO13 | 1N4148 | Cherry MX |
| **10** | 2 | 1 | GPIO9 | GPIO14 | 1N4148 | Cherry MX |
| **11** | 2 | 2 | GPIO9 | GPIO15 | 1N4148 | Cherry MX |
| **12** | 2 | 3 | GPIO9 | GPIO16 | 1N4148 | Cherry MX |

### Matrix Connection Diagram

```
Row Pins (Outputs - Active Low):
ESP32-S3 Feather Pin → Function
┌────────────────────────────────┐
│ GPIO5  → Row 0 (Keys 1-4)      │
│ GPIO6  → Row 1 (Keys 5-8)      │
│ GPIO9  → Row 2 (Keys 9-12)     │
└────────────────────────────────┘

Column Pins (Inputs - Pull-up enabled):
ESP32-S3 Feather Pin → Function
┌────────────────────────────────┐
│ GPIO13 → Col 0 (Keys 1,5,9)    │
│ GPIO14 → Col 1 (Keys 2,6,10)   │
│ GPIO15 → Col 2 (Keys 3,7,11)   │
│ GPIO16 → Col 3 (Keys 4,8,12)   │
└────────────────────────────────┘

Diode Placement:
┌───────────────────────────────────────┐
│  Each 1N4148 Diode:                   │
│  - Black band = Cathode → Row pin     │
│  - Other end = Anode → Switch         │
│                                       │
│  For COL2ROW scanning, current flows: │
│  Col(HIGH)→Switch→Anode→Cathode→Row   │
│  Prevents ghosting (false keypresses) │
└───────────────────────────────────────┘
```

---

## 💡 RGB LED Strip with Level Shifter (5V)

### Why Level Shifter is Needed

```
Problem:
- ESP32-S3 outputs 3.3V logic signals
- WS2812B LEDs expect 5V logic signals (VIH min = 3.5V)
- 3.3V may work but is unreliable (signal integrity issues)

Solution:
- Use SN74AHCT1G125 single buffer IC (TTL-compatible input, 5V output)
- Powered at 5V, accepts 3.3V as valid HIGH (VIH min = 2.0V)
- Clean push-pull 5V output, no external pull-up resistors needed
```

### SN74AHCT1G125 Level Shifter (Single Buffer IC)

```
Circuit Diagram:

  ESP32-S3                SN74AHCT1G125               WS2812B
  GPIO17 ────────────────→ A (pin 2)                  LED Strip
  (3.3V logic)            │                           │
                          │  ┌──────────────┐         │
                     OE ──┤1 │              │ 4├──────┘ DIN
                     (GND) │  │ 74AHCT1G125 │  │     (5V logic)
                     A  ──┤2 │  Buffer IC   │  │
                          │  │              │  │
                    GND ──┤3 │              │ 5├── VCC (5V)
                          │  └──────────────┘  │
                          │                    │
                         GND                  5V

SN74AHCT1G125 Pinout (SOT-23-5 SMD):
Top view, dot/pin 1 at top-left:
     ┌──────────┐
 OE ─┤1        5├─ VCC
     │          │
  A ─┤2        4├─ Y
     │          │
GND ─┤3         │
     └──────────┘

Components Needed:
┌──────────────────────┬──────────┬──────────────────────────────────┐
│ Component            │ Quantity │ Value/Type                       │
├──────────────────────┼──────────┼──────────────────────────────────┤
│ Buffer IC            │ 1        │ SN74AHCT1G125DBVR (SOT-23-5)    │
└──────────────────────┴──────────┴──────────────────────────────────┘
No external resistors required.

Wiring Table:
┌──────────────────────────┬────────────────────────────────────────┐
│ SN74AHCT1G125 Pin        │ Connect To                             │
├──────────────────────────┼────────────────────────────────────────┤
│ Pin 1: OE (Output Enable)│ GND (always enabled, active LOW)       │
│ Pin 2: A (Input)         │ ESP32-S3 GPIO17 (3.3V data signal)     │
│ Pin 3: GND               │ GND (common ground)                    │
│ Pin 4: Y (Output)        │ WS2812B LED1-DI (5V data signal)      │
│ Pin 5: VCC               │ 5V (USB VBUS)                          │
└──────────────────────────┴────────────────────────────────────────┘

How it works:
- IC is powered at 5V, making output swing 0–5V (full logic levels)
- AHCT family uses TTL-compatible inputs: VIH min = 2.0V, VIL max = 0.8V
- ESP32 3.3V output is well above 2.0V threshold → clean HIGH detection
- Output is push-pull (sources AND sinks current) → fast, clean edges
- No pull-up resistors needed (unlike MOSFET approach)
- OE tied to GND = output always enabled
- Propagation delay: ~6ns max (far faster than WS2812B timing needs)

Advantages over BSS138 MOSFET approach:
- Simpler circuit: single IC, no external resistors
- Faster edges: push-pull output vs. passive pull-up
- No RC rise time concerns (MOSFET + pull-up has slow rising edges)
- No boot glitch worries (output is high-impedance until VCC is stable)
```

### WS2812B LED Strip Connection

```
┌───────────────────────────────────────────────────┐
│  WS2812B RGB LED Strip (5 LEDs - Underglow)       │
│  Addressable RGB - Daisy Chain Configuration      │
└───────────────────────────────────────────────────┘

Single LED Pinout:
┌─────────────┐
│   WS2812B   │
│   ┌─────┐   │
│   │ ▓▓▓ │   │  ← RGB LED die
│   └─────┘   │
│  GND 5V DIN │
│   │   │  │  │
└───┴───┴──┴──┘

Strip Wiring (5 LEDs in series - Underglow):
                                    
    5V  ────┬────┬────┬────┬──── (to LED 5)
    GND ────┼────┼────┼────┼──── (to LED 5)
    DIN ────┤    │    │    │
           [L1] [L2] [L3] [L4] [L5]
             │DOUT│DOUT│DOUT│DOUT (not used)
             └────┴────┴────┴────┘

Connection to ESP32-S3 System:
┌─────────────────┬────────────────────────────────────┐
│ LED Strip Pin   │ Connect To                         │
├─────────────────┼────────────────────────────────────┤
│ 5V (Red)        │ USB VBUS (5V) - NOT 3V pin!       │
│ GND (Black)     │ GND (Common ground)                │
│ DIN (Green)     │ Level Shifter Output (5V logic)    │
└─────────────────┴────────────────────────────────────┘

Power Calculation:
- Each WS2812B: ~60mA @ full white brightness
- 5 LEDs × 60mA = 300mA max
- Recommended: Limit brightness to 25% in firmware
- 5 LEDs × 15mA = 75mA @ 25% brightness ✅

Data Signal Path:
  ESP32-S3 GPIO17 (3.3V logic)
       │
       ▼
  SN74AHCT1G125 Pin 2 (A input, TTL-compatible)
       │
       ▼
  SN74AHCT1G125 Pin 4 (Y output, 5V logic)
       │
       ▼
  WS2812B DIN → LED 1 → LED 2 → LED 3 → LED 4 → LED 5
```

### Recommended Level Shifter Breadboard Layout

```
Breadboard Layout (SN74AHCT1G125):

Row 1:  GND ──────────────────── to Pin 1 (OE) + Pin 3 (GND)
Row 2:  GPIO17 from ESP32-S3 ─── to Pin 2 (A input)
Row 3:  (empty)
Row 4:  Pin 4 (Y output) ─────── to WS2812B LED1-DIN
Row 5:  5V (from VBUS) ────────── to Pin 5 (VCC)

SN74AHCT1G125 Pinout (SOT-23-5 package):
Top view, dot at pin 1:
     ┌──────────┐
 OE ─┤1        5├─ VCC
     │          │
  A ─┤2        4├─ Y
     │          │
GND ─┤3         │
     └──────────┘

Note: SOT-23-5 is SMD. For breadboard prototyping,
use a SOT-23-5 breakout board or solder wires directly.
Only 5 wires needed — no external resistors.
```

---

## 📊 Complete Pin Assignment Table

### ESP32-S3 Feather GPIO Allocation (ACTIVE)

| ESP32-S3 Pin | Function | Component | Direction | Notes |
|--------------|----------|-----------|-----------|-------|
| **USB D+/D-** | USB OTG | Native USB | Bidir | HID Keyboard + FIDO2 CTAP2 |
| **3V** | Power Out | All 3.3V components | Output | 500mA max from regulator |
| **GND** | Ground | Common ground | — | All components share |
| **VBUS** | 5V In | USB power | Input | Powers Feather + LEDs |
| **GPIO3 (SDA)** | I²C Data | OLED + ATECC608A | Bidir | Shared bus, 100kHz |
| **GPIO4 (SCL)** | I²C Clock | OLED + ATECC608A | Output | Shared bus, 100kHz |
| **GPIO5** | Key Matrix | Row 0 (Keys 1-4) | Output | Active low scanning |
| **GPIO6** | Key Matrix | Row 1 (Keys 5-8) | Output | Active low scanning |
| **GPIO8 (A5)** | Fingerprint | WAKE (finger detect) | Input | Interrupt on RISING |
| **GPIO9** | Key Matrix | Row 2 (Keys 9-12) | Output | Active low scanning |
| **GPIO10** | Encoder | CLK (rotation) | Input | Internal pull-up |
| **GPIO11** | Encoder | DT (direction) | Input | Internal pull-up |
| **GPIO12** | Encoder | SW (mute button) | Input | Internal pull-up |
| **GPIO13** | Key Matrix | Col 0 (Keys 1,5,9) | Input | Internal pull-up |
| **GPIO14 (A4)** | Key Matrix | Col 1 (Keys 2,6,10) | Input | Internal pull-up |
| **GPIO15 (A3)** | Key Matrix | Col 2 (Keys 3,7,11) | Input | Internal pull-up |
| **GPIO16 (A2)** | Key Matrix | Col 3 (Keys 4,8,12) | Input | Internal pull-up |
| **GPIO17 (A1)** | LED Data | WS2812B (via shifter) | Output | 800kHz RMT |
| **GPIO18 (A0)** | Fingerprint | RST (sleep/wake) | Output | LOW=sleep, HIGH=active |
| **GPIO38 (RX)** | UART1 RX | Fingerprint TX | Input | 19200 baud |
| **GPIO39 (TX)** | UART1 TX | Fingerprint RX | Output | 19200 baud |

**GPIO Usage Summary:**
- **Used:** 17 GPIO pins + USB = 18 functions
- **Available:** SPI pins (GPIO35/36/37), BAT, plus spare GPIOs
- **Remaining:** 4+ spare GPIO for expansion ✅

**Reserved/Unavailable Pins:**
- ❌ **GPIO7** - Reserved for I2C power (STEMMA QT), not on header
- ❌ **GPIO21** - Onboard NeoPixel power (reserved by board)
- ❌ **GPIO33** - Onboard NeoPixel data (reserved by board)

---

## 🏗️ Physical Layout

### Top View - Component Placement

**Based on YOUR design layout:**

```
┌──────────────────────────────────────────────────────────────────┐
│                      TOP VIEW OF MACROPAD                         │
│                  (Matches your provided layout)                   │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌───────┐                         ╔═══════════════════════╗     │
│  │   ○   │ Rotary Encoder         ║                       ║     │
│  │  ( )  │ Volume + Mute          ║   OLED Display        ║     │
│  │   │   │ (GPIO 10/11/12)        ║   128×64 pixels       ║     │
│  └───────┘                         ║   [Win] 🔊 75%        ║     │
│                                    ║   ───────────────     ║     │
│                                    ║   READY FOR ACTION    ║     │
│            ┌───┐ ┌───┐ ┌───┐ ┌───┐║   R1:Shot Mic...      ║     │
│            │ 1 │ │ 2 │ │ 3 │ │ 4 │╚═══════════════════════╝     │
│            │Sht│ │Mic│ │Vid│ │Lck│                              │
│            └───┘ └───┘ └───┘ └───┘                              │
│                                                                   │
│            ┌───┐ ┌───┐ ┌───┐ ┌───┐                              │
│  ┌─────┐  │ 5 │ │ 6 │ │ 7 │ │ 8 │                              │
│  │ ╱◠╲ │  │Cpy│ │Cut│ │Pst│ │Del│                              │
│  │(●●●)│  └───┘ └───┘ └───┘ └───┘                              │
│  │ ╲◡╱ │                                                         │
│  └─────┘  ┌───┐ ┌───┐ ┌───┐ ┌───┐                              │
│ Fingerprint│ 9 │ │10 │ │11 │ │12 │                              │
│  Sensor   │Cur│ │Ins│ │Trm│ │Git│                              │
│  (Round)  └───┘ └───┘ └───┘ └───┘                              │
│ GPIO38/39   ▲                   ▲                                │
│ GPIO18/8    │                   └─ Double-tap to switch OS       │
│ (RST/WAKE)  └───── 3×4 Key Matrix (GPIO 5/6/9 rows)             │
│                                   (GPIO 13/14/15/16 cols)        │
│                                                                   │
│  [● ● ● ● ●] ← 5× WS2812B Underglow LEDs (GPIO17)              │
│                                                                   │
│                          ┌──────────────┐                        │
│                          │  ESP32-S3    │                        │
│                          │  Feather     │                        │
│                          │  [USB-C ▼]   │                        │
│                          └──────────────┘                        │
│                                                                   │
│  Hidden underneath:                                              │
│  - ATECC608A (I²C @ 0x60, GPIO3/4)                              │
│  - Level Shifter (SN74AHCT1G125 buffer IC)                      │
│  - 1N4148 Diodes (12×)                                          │
│  - 220uF bulk caps (3.3V + 5V rails)                            │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘

Dimensions (Approximate):
- Overall: 120mm × 100mm × 30mm (W×D×H)
- Key spacing: 19mm (standard Cherry MX spacing)
- OLED: 1.3" diagonal (128×64 pixels)
- Fingerprint: 25mm diameter (round capacitive)
- Encoder: 12mm diameter knob
- LED strip: 5× WS2812B (5050 or 2020; 5050 easier to solder)
```

### Side View - Layer Stack

```
     ┌──────────────────────────────────────┐
     │  Keycaps & Knobs (top layer)         │  ← 10mm
     ├──────────────────────────────────────┤
     │  Switches & PCB/Protoboard           │  ← 5mm
     ├──────────────────────────────────────┤
     │  ESP32-S3 Feather + Components       │  ← 10mm
     ├──────────────────────────────────────┤
     │  Base plate / Bottom case            │  ← 3mm
     ├──────────────────────────────────────┤
     │  Rubber feet                         │  ← 2mm
     └──────────────────────────────────────┘
     
     Total height: ~30mm
```

---

## 🧪 Breadboard Prototype Diagram

### Recommended Prototyping Setup

```
┌─────────────────────────────────────────────────────────────────┐
│                    BREADBOARD PROTOTYPE LAYOUT                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Power Rails (Red = 3.3V, Blue = GND):                         │
│  ═══════════════════════════════════════════════════════════   │
│  + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +   │
│  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   │
│                                                                  │
│  Main Breadboard Area:                                          │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                                                          │   │
│  │  [ESP32-S3 Feather]                                     │   │
│  │  (Straddling center gap)                                │   │
│  │                                                          │   │
│  │  Left Side Pins:        Right Side Pins:                │   │
│  │  3V, GND, A0...         ...VBUS, GND, BAT, D14...D1     │   │
│  │                                                          │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │                                                          │   │
│  │  [OLED] [Fingerprint] [ATECC608A] [Encoder]            │   │
│  │  (Connected via jumper wires to Feather)                │   │
│  │                                                          │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │                                                          │   │
│  │  [Key Matrix Area]                                      │   │
│  │  - 12× 1N4148 diodes                                    │   │
│  │  - 12× Switches (or buttons for testing)                │   │
│  │  - Wire matrix connections                              │   │
│  │                                                          │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │                                                          │   │
│  │  [Level Shifter Circuit]                                │   │
│  │  - SN74AHCT1G125 (SOT-23-5 on breakout board)          │   │
│  │  - No external resistors needed                         │   │
│  │                                                          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                  │
│  Separate 5V Power Rail (for WS2812B):                         │
│  ═══════════════════════════════════════════════════════════   │
│  + + + + + + + (5V from USB VBUS) + + + + + + + + + + + + +   │
│  - - - - - - - (Common GND) - - - - - - - - - - - - - - - -   │
│                                                                  │
│  [WS2812B LED Strip] ← Connected to level shifter output       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

Component Placement Tips:
1. Place ESP32-S3 Feather straddling the center gap
2. Connect 3V pin to positive power rail (red)
3. Connect GND to ground rail (blue)
4. Place OLED near Feather (short I²C wires)
5. Use separate area for key matrix (lots of wires)
6. Keep level shifter close to LED strip
7. Use wire colors consistently:
   - Red = 3.3V or 5V power
   - Black = GND
   - Yellow = SCL (I²C clock)
   - Blue = SDA (I²C data)
   - Green = TX/RX
   - White/Gray = Encoder signals
```

---

## ✅ Testing Checklist

### Phase 1: Power & Basic Connectivity

```
□ Step 1: Visual Inspection
  □ Check all solder joints (if applicable)
  □ Verify no shorts between VCC and GND
  □ Confirm diode polarity (band = cathode)
  □ Check all wire connections match diagram

□ Step 2: Power On (No Components)
  □ Connect USB-C to Mac
  □ Feather power LED lights up
  □ Measure 3.3V on 3V pin (multimeter)
  □ Measure 5V on VBUS pin
  □ No smoke, no heat, no weird smells ✅

□ Step 3: USB Enumeration
  □ Open Arduino IDE or ESP-IDF
  □ Feather appears as COM/Serial port
  □ Can upload "Blink" test sketch
  □ Onboard NeoPixel blinks ✅
```

### Phase 2: Component Testing (One at a Time)

```
□ Step 4: OLED Display
  □ Connect OLED (I²C or STEMMA QT)
  □ Upload Adafruit SH1106 test sketch
  □ Display shows test pattern
  □ Can display text clearly ✅

□ Step 5: ATECC608A Secure Element
  □ Connect ATECC608A to I²C bus
  □ Upload CryptoAuthLib test sketch
  □ Can detect chip at address 0x60
  □ Can read serial number ✅

□ Step 6: Fingerprint Sensor
  □ Connect to UART (TX↔RX, RX↔TX)
  □ Connect RST (GPIO18) and WAKE (GPIO8)
  □ Power with 3.3V (check current)
  □ Upload fingerprint test sketch
  □ Verify sleep mode: set GPIO18 LOW, measure ~1-5mA
  □ Verify wake: set GPIO18 HIGH, wait 200ms, sensor responds
  □ Verify WAKE interrupt: touch sensor → GPIO8 goes HIGH
  □ Blue LED lights up when touched
  □ Can enroll test fingerprint
  □ Can verify enrolled fingerprint
  □ Sensor returns to sleep after auth ✅

□ Step 7: Rotary Encoder (Volume)
  □ Connect Encoder (CLK→GPIO10, DT→GPIO11, SW→GPIO12)
  □ Upload encoder test sketch
  □ Rotate clockwise → volume up
  □ Rotate counter-clockwise → volume down
  □ Press button → mute/unmute toggle ✅

□ Step 8: Key Matrix
  □ Connect row and column pins
  □ Install diodes on all keys
  □ Upload matrix scanning sketch
  □ Press each key 1-12 individually
  □ Each key registers correctly
  □ Test multiple simultaneous keys (no ghosting) ✅

□ Step 9: RGB LED Strip (WITH Level Shifter)
  □ Wire SN74AHCT1G125: OE+GND→GND, A→GPIO17, Y→LED1-DIN, VCC→5V
  □ Connect LED strip to 5V (NOT 3.3V!)
  □ Connect data line through level shifter (pin 4 Y output)
  □ Upload FastLED or NeoPixel test
  □ All 5 LEDs light up
  □ Can change colors individually
  □ Limit brightness to 25% max ✅
```

### Phase 3: Integration Testing

```
□ Step 10: All Components Together
  □ Connect all components simultaneously
  □ Power on - no shorts, reasonable current
  □ All components respond correctly
  □ No interference between devices ✅

□ Step 11: FIDO2 Functionality (Final Test)
  □ Upload FIDO2 CTAP2 firmware
  □ macOS recognizes as FIDO2 device
  □ Visit webauthn.io test site
  □ Register new passkey (with fingerprint)
  □ Login with passkey (fingerprint verify)
  □ OLED shows correct status messages ✅

□ Step 12: Macro Keyboard Functions
  □ Upload macro key assignments
  □ Press keys → correct shortcuts sent
  □ Rotary encoder controls volume
  □ RGB LEDs show status correctly ✅

□ Step 13: Final System Test (on macOS)
  □ Visit GitHub.com → create passkey
  □ Logout → login with fingerprint
  □ Use macro keys for productivity
  □ Adjust volume with rotary encoder
  □ Verify OLED shows all info correctly
  □ 🎉 SUCCESS! Fully functional! 🎉
```

---

## 🔧 Troubleshooting Guide

### Common Issues & Solutions

| Problem | Possible Cause | Solution |
|---------|---------------|----------|
| **Feather not detected** | USB cable data lines broken | Try different USB-C cable |
| **OLED blank** | Wrong I²C address | Try 0x3C or 0x3D |
| **OLED garbled** | I²C speed too high | Set I²C to 100kHz (not 400kHz) |
| **Fingerprint no response** | TX/RX swapped | Verify TX→RX, RX→TX crossover |
| **Fingerprint red LED** | Low voltage | Check 3.3V supply, add capacitor |
| **Some keys don't work** | Diode backwards | Check diode orientation (band) |
| **Ghost key presses** | Missing diode | Install 1N4148 on every key |
| **Encoder erratic** | No pull-ups | Enable internal pull-ups in code |
| **LEDs don't light** | Wrong voltage | Verify 5V power, not 3.3V |
| **LEDs wrong colors** | No level shifter | Wire SN74AHCT1G125 buffer IC |
| **LEDs flicker** | Insufficient power | Limit brightness to 25% |
| **FIDO2 not detected** | Wrong USB mode | Ensure native USB OTG enabled |
| **Passkey fails** | Fingerprint timeout | Increase verification timeout |

---

## 📦 Complete Parts List with Quantities

### Electronics Components

| Category | Part | Quantity | Notes |
|----------|------|----------|-------|
| **Main Board** | ESP32-S3 Feather | 1 | From Robu.in |
| **Security** | ATECC608A breakout | 1 | I²C crypto chip |
| **Biometric** | Waveshare Round Fingerprint | 1 | Capacitive, 360° |
| **Display** | SH1106 OLED 1.3" | 1 | I²C interface |
| **Input** | Cherry MX switches | 12 | Or compatible |
| **Input** | EC11 Rotary encoder module | 1 | With push button |
| **LEDs** | WS2812B-5050 or 2020 RGB | 5 | Addressable (5050 easier to solder) |
| **Diodes** | 1N4148 (DO-35) | 12 | For key matrix |
| **Level Shift** | SN74AHCT1G125DBVR | 1 | SOT-23-5 buffer IC (TI), LCSC: C7484 |
| **Capacitors** | 220uF/16V Electrolytic | 2 | Bulk decoupling (3.3V + 5V rails) |
| **Resistors** | 4.7kΩ (optional) | 2 | I²C pull-ups (if needed) |
| **Keycaps** | DSA or Cherry profile | 12 | 1U size |
| **Knobs** | 6mm D-shaft knob | 1 | For encoder |
| **Cables** | JST-SH STEMMA QT | 1-2 | For OLED connection |
| **Wires** | Jumper wires M-F | 40 pack | For prototyping |
| **Prototyping** | Full-size breadboard | 1-2 | For testing |
| **Power** | USB-C cable | 1 | Data-capable |
| **Optional** | LiPo battery 400mAh | 1 | For portability |

### Estimated Total Cost (India)

| Component Group | Cost Range |
|----------------|------------|
| ESP32-S3 Feather | ₹2,000-2,500 |
| Fingerprint Sensor | ₹2,500-3,000 |
| OLED Display | ₹200-300 |
| ATECC608A | ₹300-500 |
| 12× Switches | ₹600-1,200 |
| 1× Encoder module | ₹100-200 |
| 5× WS2812B-5050 or 2020 LEDs | ₹75-150 |
| Keycaps | ₹300-500 |
| Electronics (diodes, resistors, caps, etc) | ₹200-300 |
| Cables & Prototyping | ₹300-500 |
| **TOTAL** | **₹6,275-8,650** |

---

## 📝 Notes & Recommendations

### Important Reminders

1. **UART Crossover:** Fingerprint TX → ESP32 RX, Fingerprint RX → ESP32 TX
2. **Diode Orientation:** Band (cathode) toward row pin, anode toward switch
3. **LED Power:** WS2812B on 5V rail, NOT 3.3V
4. **Level Shifter:** SN74AHCT1G125 required for reliable WS2812B operation
5. **I²C Addresses:** OLED (0x3C), ATECC608A (0x60) - no conflicts
6. **Pull-ups:** Enable internal pull-ups for encoders and key columns
7. **Current Limit:** Keep LED brightness at 25% to stay under USB power budget
8. **Testing:** Test each component individually before full integration
9. **FIDO2:** Requires native USB OTG - confirmed working on ESP32-S3 Feather
10. **Fingerprint Sleep:** Sensor defaults to sleep (RST=LOW). Wake only for auth/enroll (RST=HIGH). Use WAKE interrupt (GPIO8) for finger detection.

### Best Practices

- Use colored wires consistently (Red=VCC, Black=GND, etc.)
- Label all connections with masking tape during prototyping
- Test power supply with multimeter before connecting components
- Keep I²C wires short (<15cm) for reliable communication
- Add 100nF decoupling capacitors near power pins of each IC
- Use heat shrink tubing on all exposed connections
- Document any deviations from this wiring diagram

---

## ⚠️ Hardware Circuit Design Notes

### Level Shifter Design (SN74AHCT1G125 Buffer IC)

The level shifter uses a TI SN74AHCT1G125DBVR single buffer gate (SOT-23-5).
No external resistors are required.

1. **Input compatibility:** The AHCT family uses TTL-compatible input thresholds when
   powered at 5V: VIH min = 2.0V, VIL max = 0.8V. The ESP32-S3 outputs 3.3V for HIGH,
   providing 1.3V margin above the minimum HIGH threshold.

2. **Output drive:** The IC provides push-pull output capable of ±8mA at 5V. This
   produces fast, clean edges without the RC rise time limitations of a MOSFET + pull-up
   approach. Propagation delay is ~6ns max, well within WS2812B timing requirements.

3. **Power-on behavior:** With OE tied to GND, the output follows the input as soon as
   VCC is stable. During ESP32 boot (GPIO17 floating/low), the IC output stays LOW,
   which is a valid idle state for WS2812B (no random LED flashing).

### Power Distribution Notes

1. **3.3V regulator budget:** ESP32-S3 (~150mA) + OLED (~20mA) + Fingerprint (~120mA peak active, ~3mA sleep)
   + ATECC608A (<1mA) = ~291mA peak from the 500mA onboard regulator (AP2112K). Safe margin. ✅
   With fingerprint in sleep mode (default): ~174mA typical, leaving ample headroom.

2. **5V USB budget:** ESP32 board overhead (~170mA typical with fingerprint sleeping, ~300mA worst case) +
   5× WS2812B (300mA max / 75mA at 25%) = ~245–600mA total. At 25% LED brightness
   with fingerprint sleeping, this stays well within USB 2.0's 500mA allocation. ✅
   Full LED brightness exceeds USB 2.0 spec — **always limit to ≤50% brightness in firmware**.

3. **Bulk capacitors (C1, C2):** The two 220uF/16V electrolytic capacitors provide bulk
   decoupling on 3.3V and 5V rails. These smooth out current spikes from the fingerprint
   sensor (pulsed IR LED) and WS2812B (simultaneous switching). Observe correct polarity
   (positive terminal to VCC, negative to GND).

### I²C Bus Considerations

1. **Multiple pull-ups:** The Adafruit ESP32-S3 Feather has built-in I²C pull-ups on the
   STEMMA QT port. Many OLED modules also include onboard pull-ups. Having parallel pull-ups
   (Feather + OLED) lowers the effective resistance, which increases current but improves
   signal integrity at 100kHz. This is acceptable but if you add more I²C devices,
   verify the total pull-up resistance doesn't drop below ~1kΩ (excessive bus current).

2. **Bus capacitance:** With OLED + ATECC608A on the same I²C bus, keep wires short (<10cm)
   to minimize capacitance. Long wires may require reducing I²C speed from 400kHz to 100kHz.

### WS2812B Data Integrity

1. **Per-LED decoupling:** The WS2812B datasheet recommends a 100nF ceramic capacitor on
   each LED's VDD pin for power supply filtering. These are not included in the current
   schematic. If LEDs show color glitches or flicker, add 100nF caps as close to each
   LED's VDD/GND pins as possible.

2. **First LED distance:** Keep the wire from the level shifter output to LED1's DIN as
   short as possible. Long data wires are susceptible to noise and signal degradation.

### Key Matrix Scanning

1. **Internal pull-ups:** Column pins (GPIO13/14/15/16) must have internal pull-ups enabled
   in firmware (`INPUT_PULLUP`). Without pull-ups, unpressed keys will float and cause
   false readings.

2. **Scan rate:** For responsive keypress detection, scan the matrix at ≥1kHz (every 1ms).
   Add debouncing (5–10ms) in firmware to filter mechanical switch bounce.

---

**END OF WIRING DIAGRAM**

**Author:** FIDO2 Macropad Project  
**Version:** 2.3  
**Last Updated:** 2026-03-05  
**License:** Open Source Hardware

