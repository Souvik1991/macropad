# ESP32-S3 FIDO2 Fingerprint Macropad - Complete Wiring Diagram

**Project:** Passwordless Browser Authentication Macropad  
**MCU:** Adafruit ESP32-S3 Feather (8MB Flash)  
**Date:** 2025-11-14  
**Version:** 2.0 - **FINAL CORRECTED PIN ASSIGNMENTS**

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
| **GPIO38** | UART1 RX | Fingerprint TX (U1RXD) |
| **GPIO39** | UART1 TX | Fingerprint RX (U1TXD) |

**❌ Unavailable:** GPIO7 (doesn't exist), GPIO21/33 (onboard NeoPixel)

---

## 📋 Table of Contents

1. [System Overview](#system-overview)
2. [Power Distribution](#power-distribution)
3. [Component Connections](#component-connections)
4. [Key Matrix Wiring](#key-matrix-wiring)
5. [RGB LED Strip with Level Shifter](#rgb-led-strip-with-level-shifter)
6. [Complete Pin Assignment Table](#complete-pin-assignment-table)
7. [Physical Layout](#physical-layout)
8. [Breadboard Prototype Diagram](#breadboard-prototype-diagram)
9. [Testing Checklist](#testing-checklist)

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
│   │  3.3V Regulator (AMS1117 - 500mA)   │      │              │
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
              ~50-120mA          ~20mA         <1mA         ~720mA max
                                                           (60mA × 12)
```

### Power Budget

| Component | Voltage | Current (Typical) | Current (Max) |
|-----------|---------|-------------------|---------------|
| ESP32-S3 Feather | 3.3V | 80-150mA | 240mA |
| OLED Display | 3.3V | 15mA | 20mA |
| Fingerprint Sensor | 3.3V | 50mA idle | 120mA scanning |
| ATECC608A | 3.3V | <1mA | <1mA |
| 1× Rotary Encoder  | 3.3V | <1mA | <1mA |
| **WS2812B LEDs (12×)** | **5V** | **180mA (25%)** | **720mA (100%)** |
| **Total from USB 5V** | — | **~400mA** | **~1100mA** |

**Note:** 
- 3.3V components draw from onboard regulator (max 500mA) ✅
- WS2812B draws directly from 5V USB (bypasses regulator) ✅
- Typical usage: LEDs at 25% brightness = ~400mA total
- USB 2.0 provides up to 500mA (standard) or 900mA (high-power)

---

## 🔌 Component Connections

### 1. Waveshare Round Capacitive Fingerprint Sensor (UART)

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
│ Pin 5: RST      │ Blue         │ Not connected*        │
│ Pin 6: WAKE     │ White        │ Not connected*        │
└─────────────────┴──────────────┴───────────────────────┘

* RST/WAKE are optional. For FIDO2 sleep mode integration:
  - RST:  Connect to GPIO → LOW=sleep, HIGH=active
  - WAKE: Connect to GPIO → goes HIGH when finger touches sensor

Baud Rate: 19200 (Waveshare default)
Protocol: Custom packet (0xF5 header/tail, XOR checksum)
IMPORTANT: TX → RX, RX → TX (crossover connection)
```

---

### 2. OLED Display 0.96" 128×64 (I²C)

```
┌───────────────────────────────────────────────────────┐
│  SSD1306 OLED Display - 0.96 inch                     │
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
            ├─────► Anode │├──── Cathode (1N4148)
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
  GPIO5 ──→ 1N4148 ──→│├──→ [Switch] ──→ GPIO13

IMPORTANT: 
- Diode cathode (line/band) goes to switch
- Diode anode connects to row pin
- Switch other terminal connects to column pin
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
│  - Black band = Cathode → Switch      │
│  - Other end = Anode → Row pin        │
│                                       │
│  Prevents current backflow that       │
│  causes "ghosting" (false keypresses) │
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
- Use level shifter to convert 3.3V → 5V logic
- Or use a single MOSFET as level shifter (simpler)
```

### Option 1: MOSFET Level Shifter (Recommended - Simple & Cheap)

```
Circuit Diagram:

         3.3V (from ESP32-S3)      5V (from USB VBUS)
           │                           │
           │                           │
          [4.7kΩ]                     ┌┴┐ 10kΩ Pull-up
           │                          │ │ (to 5V rail)
           │                          └┬┘
           │                           │
ESP32-S3   │                           │      To WS2812B
GPIO17 ────┴────┐                      │        Data In
                │                      │           │
                │     ┌────────┐       │           │
                └─────┤G     D ├───────┴───────────┘
                      │        │
                      │ 2N7000 │  ← N-Channel MOSFET
                      │  or    │     (or BSS138)
                      │ BSS138 │
                      │        │
                      └───┬────┘
                          │S
                          │
                         GND

Components Needed:
┌────────────────────┬──────────┬────────────────┐
│ Component          │ Quantity │ Value/Type     │
├────────────────────┼──────────┼────────────────┤
│ N-Channel MOSFET   │ 1        │ 2N7000/BSS138  │
│ Resistor (Pull-up) │ 1        │ 10kΩ           │
│ Resistor (Gate)    │ 1        │ 4.7kΩ (opt)    │
└────────────────────┴──────────┴────────────────┘

Wiring Table:
┌─────────────────────┬───────────────────────────┐
│ Connection          │ Wire To                   │
├─────────────────────┼───────────────────────────┤
│ ESP32-S3 GPIO17     │ MOSFET Gate (G)           │
│ MOSFET Drain (D)    │ WS2812B Data In + 10kΩ→5V │
│ MOSFET Source (S)   │ GND                       │
│ 10kΩ Resistor       │ Between Drain and 5V      │
└─────────────────────┴───────────────────────────┘

How it works:
- When GPIO17 = HIGH (3.3V): MOSFET ON → Drain pulled LOW → WS2812B sees LOW
- When GPIO17 = LOW (0V):    MOSFET OFF → Pull-up pulls Drain HIGH (5V) → WS2812B sees HIGH
- Signal is inverted, but firmware can compensate
```

### Option 2: Dedicated Level Shifter IC (More Reliable)

```
Circuit Diagram:

┌────────────────────────────────────────────────────┐
│       74AHCT125 or 74HCT245 Level Shifter          │
│                                                    │
│   VCC ──── 5V (from USB VBUS)                     │
│   GND ──── GND                                     │
│                                                    │
│   LV Side (3.3V)          HV Side (5V)            │
│        │                       │                   │
│   ESP32-S3 GPIO33 ──→ [IC] ──→ WS2812B Data In   │
│                                                    │
└────────────────────────────────────────────────────┘

Recommended ICs:
- 74AHCT125: Quad buffer (can do 4 signals)
- 74HCT245: Octal bus transceiver
- SN74LVC1G17: Single buffer (cheapest)

Wiring for 74AHCT125:
┌─────────────────┬──────────────┬─────────────────┐
│ 74AHCT125 Pin   │ Connect To   │ Function        │
├─────────────────┼──────────────┼─────────────────┤
│ VCC (Pin 14)    │ 5V (USB)     │ Power           │
│ GND (Pin 7)     │ GND          │ Ground          │
│ 1A (Pin 1)      │ GPIO17       │ Input (3.3V)    │
│ 1Y (Pin 2)      │ WS2812B DIN  │ Output (5V)     │
│ 1OE (Pin 3)     │ GND          │ Enable (active) │
└─────────────────┴──────────────┴─────────────────┘
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
  ESP32-S3 GPIO17 (3.3V) 
       │
       ▼
  Level Shifter (MOSFET or IC)
       │
       ▼
  WS2812B DIN (5V logic)
       │
       ▼
  LED 1 → LED 2 → LED 3 → LED 4 → LED 5
```

### Recommended Level Shifter Breadboard Layout

```
Breadboard Layout (MOSFET Method):

Row 1:  5V ──────[10kΩ]────── to Row 5 (Pull-up)
Row 2:  (empty)
Row 3:  GPIO17 ────────────── to MOSFET Gate
Row 4:  (empty)
Row 5:  MOSFET Drain ──────── to WS2812B DIN + 10kΩ from Row 1
Row 6:  (empty)
Row 7:  MOSFET Source ─────── to GND
Row 8:  GND Rail

2N7000 Pinout (TO-92 package):
Looking at flat side:
┌───────────┐
│ ○   ○   ○ │
│ │   │   │ │
│ S   G   D │
└───────────┘
```

---

## 📊 Complete Pin Assignment Table

> **⚠️ NOTE:** This table below contains OLD/OUTDATED pin assignments for reference.  
> **✅ USE THE "QUICK PIN REFERENCE" AT THE TOP** for the correct, final pin assignments.

### ESP32-S3 Feather GPIO Allocation (LEGACY - DO NOT USE)

| ESP32-S3 Pin | Function | Component | Direction | Notes |
|--------------|----------|-----------|-----------|-------|
| **USB D+/D-** | USB OTG | Native USB | Bidir | HID Keyboard + FIDO2 CTAP2 |
| **3V** | Power Out | All 3.3V components | Output | 500mA max from regulator |
| **GND** | Ground | Common ground | — | All components share |
| **VBUS** | 5V In | USB power | Input | Powers Feather + LEDs |
| **BAT** | Battery | Optional LiPo | Input | For portable use |
| **GPIO1** | Key Matrix | Row 0 (Keys 1-4) | Output | Active low scanning |
| **GPIO2** | Key Matrix | Row 1 (Keys 5-8) | Output | Active low scanning |
| **GPIO3** | Key Matrix | Row 2 (Keys 9-12) | Output | Active low scanning |
| **GPIO5** | Encoder | Rotary 1 CLK (Vol) | Input | Internal pull-up |
| **GPIO6** | Encoder | Rotary 1 DT (Vol) | Input | Internal pull-up |
| **GPIO7** | Key Matrix | Col 0 (Keys 1,5,9) | Input | Internal pull-up |
| **GPIO8** | Key Matrix | Col 1 (Keys 2,6,10) | Input | Internal pull-up |
| **GPIO9** | Encoder | Rotary 1 SW (Vol) | Input | Internal pull-up |
| **GPIO10** | Key Matrix | Col 2 (Keys 3,7,11) | Input | Internal pull-up |
| **GPIO11** | Encoder | Rotary 2 CLK (Scroll) | Input | Internal pull-up |
| **GPIO12** | Encoder | Rotary 2 DT (Scroll) | Input | Internal pull-up |
| **GPIO13** | Encoder | Rotary 2 SW (Scroll) | Input | Internal pull-up |
| **GPIO14** | Key Matrix | Col 3 (Keys 4,8,12) | Input | Internal pull-up |
| **GPIO17** | UART TX | Fingerprint RX | Output | 9600 baud default |
| **GPIO18** | UART RX | Fingerprint TX | Input | 9600 baud default |
| **GPIO33** | RMT | WS2812B LEDs (via shifter) | Output | 800kHz PWM |
| **SDA** | I²C Data | OLED + ATECC608A | Bidir | Shared bus, 100kHz |
| **SCL** | I²C Clock | OLED + ATECC608A | Output | Shared bus, 100kHz |
| **STEMMA QT** | I²C | Alternative OLED connection | Bidir | Same as SDA/SCL |

**GPIO Usage Summary:**
- **Used:** 15 GPIO pins + I²C (GPIO3/4) + USB = 17 functions
- **Available:** 21+ GPIO pins on ESP32-S3 Feather
- **Remaining:** 6+ spare GPIO for expansion ✅

**Reserved/Unavailable Pins:**
- ❌ **GPIO7** - Not available on ESP32-S3 Feather
- ❌ **GPIO21** - Onboard NeoPixel (reserved by board)
- ❌ **GPIO33** - Onboard NeoPixel (reserved by board)

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
│             │                   └─ Double-tap to switch OS       │
│             └───── 3×4 Key Matrix (GPIO 5/6/9 rows)             │
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
│  - Level Shifter (SN74AHCT1G125)                                │
│  - 1N4148 Diodes (12×)                                          │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘

Dimensions (Approximate):
- Overall: 120mm × 100mm × 30mm (W×D×H)
- Key spacing: 19mm (standard Cherry MX spacing)
- OLED: 1.3" diagonal (128×64 pixels)
- Fingerprint: 25mm diameter (round capacitive)
- Encoder: 12mm diameter knob
- LED strip: 5× WS2812B-2020 (mini LEDs)
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
│  │  [OLED] [Fingerprint] [ATECC608A] [2×Encoders]         │   │
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
│  │  - MOSFET or IC                                         │   │
│  │  - Pull-up resistor                                     │   │
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
  □ Upload Adafruit SSD1306 test sketch
  □ Display shows test pattern
  □ Can display text clearly ✅

□ Step 5: ATECC608A Secure Element
  □ Connect ATECC608A to I²C bus
  □ Upload CryptoAuthLib test sketch
  □ Can detect chip at address 0x60
  □ Can read serial number ✅

□ Step 6: Fingerprint Sensor
  □ Connect to UART (TX↔RX, RX↔TX)
  □ Power with 3.3V (check current)
  □ Upload fingerprint test sketch
  □ Blue LED lights up when touched
  □ Can enroll test fingerprint
  □ Can verify enrolled fingerprint ✅

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
  □ Build level shifter circuit
  □ Connect LED strip to 5V (NOT 3.3V!)
  □ Connect data line through level shifter
  □ Upload FastLED or NeoPixel test
  □ All 12 LEDs light up
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
  □ Rotary encoders control volume
  □ RGB LEDs show status correctly ✅

□ Step 13: Final System Test (on macOS)
  □ Visit GitHub.com → create passkey
  □ Logout → login with fingerprint
  □ Use macro keys for productivity
  □ Adjust volume with encoder
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
| **LEDs wrong colors** | No level shifter | Build MOSFET level shifter |
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
| **Display** | SSD1306 OLED 0.96" | 1 | I²C interface |
| **Input** | Cherry MX switches | 12 | Or compatible |
| **Input** | EC11 Rotary encoders | 2 | With push button |
| **LEDs** | WS2812B 5050 RGB | 12 | Addressable |
| **Diodes** | 1N4148 (SOD-123) | 12 | For key matrix |
| **Level Shift** | 2N7000 N-MOSFET | 1 | Or BSS138 |
| **Resistors** | 10kΩ (0805 or 1/4W) | 1 | Pull-up for MOSFET |
| **Resistors** | 4.7kΩ (optional) | 2 | I²C pull-ups (if needed) |
| **Keycaps** | DSA or Cherry profile | 12 | 1U size |
| **Knobs** | 6mm D-shaft knobs | 2 | For encoders |
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
| 2× Encoders | ₹200-400 |
| 12× LEDs | ₹150-300 |
| Keycaps | ₹300-500 |
| Electronics (diodes, resistors, etc) | ₹200-300 |
| Cables & Prototyping | ₹300-500 |
| **TOTAL** | **₹6,750-9,500** |

---

## 📝 Notes & Recommendations

### Important Reminders

1. **UART Crossover:** Fingerprint TX → ESP32 RX, Fingerprint RX → ESP32 TX
2. **Diode Orientation:** Band (cathode) toward switch, anode toward row
3. **LED Power:** WS2812B on 5V rail, NOT 3.3V
4. **Level Shifter:** Required for reliable WS2812B operation
5. **I²C Addresses:** OLED (0x3C), ATECC608A (0x60) - no conflicts
6. **Pull-ups:** Enable internal pull-ups for encoders and key columns
7. **Current Limit:** Keep LED brightness at 25% to stay under USB power budget
8. **Testing:** Test each component individually before full integration
9. **FIDO2:** Requires native USB OTG - confirmed working on ESP32-S3 Feather

### Best Practices

- Use colored wires consistently (Red=VCC, Black=GND, etc.)
- Label all connections with masking tape during prototyping
- Test power supply with multimeter before connecting components
- Keep I²C wires short (<15cm) for reliable communication
- Add 100nF decoupling capacitors near power pins of each IC
- Use heat shrink tubing on all exposed connections
- Document any deviations from this wiring diagram

---

**END OF WIRING DIAGRAM**

**Author:** FIDO2 Macropad Project  
**Version:** 1.0  
**Last Updated:** 2025-11-14  
**License:** Open Source Hardware

