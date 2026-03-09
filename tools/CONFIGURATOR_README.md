# Macropad Configuration System

## Overview

The ESP32-S3 FIDO2 Macropad now supports **dynamic configuration** without firmware reflashing! You can configure:
- ✅ **Macros** - Assign custom key combinations or strings to any key
- ✅ **LED Colors** - Set individual colors for each of the 5 underglow LEDs
- ✅ **LED Brightness** - Adjust overall brightness (0-255)
- ✅ **OS Mode** - Switch between macOS and Windows modes

## Features

### 🔄 Hybrid Approach
- **EEPROM Storage**: All settings saved persistently
- **Hardcoded Defaults**: Fallback macros always available
- **No Reflash Needed**: Change settings on-the-fly via Serial or Web UI

### 📡 Two Configuration Methods

1. **Serial Commands** - Via Serial Monitor or CLI
2. **Web Configurator** - Beautiful web interface (Chrome/Edge)

---

## Method 1: Serial Commands

### Connection
1. Open Arduino Serial Monitor (115200 baud)
2. Or use any serial terminal (PuTTY, screen, etc.)

### Available Commands

#### Set Macro
```
MACRO <key> <type> <modifier> <keycode> [data]
```
- `key`: Key number (1-12)
- `type`: 0=disabled, 1=keycombo, 2=string, 3=special
- `modifier`: Bitmask (Ctrl=1, Shift=2, Alt=4, GUI=8)
- `keycode`: HID key code (see HID key codes below)
- `data`: Optional string data (max 5 chars for string type)

**Examples:**
```
MACRO 1 1 0 4          # Key 1: Ctrl+C (keycombo, no modifier, keycode 4)
MACRO 5 1 1 6          # Key 5: Ctrl+C (Ctrl modifier)
MACRO 9 2 0 0 hello    # Key 9: Type "hello" (string type)
```

#### Set LED Color
```
LED <index> <r> <g> <b>
```
- `index`: LED number (0-4)
- `r`, `g`, `b`: RGB values (0-255)

**Example:**
```
LED 0 255 0 0          # LED 0: Red
LED 1 0 255 0          # LED 1: Green
LED 2 0 0 255          # LED 2: Blue
```

#### Set Brightness
```
BRIGHTNESS <value>
```
- `value`: Brightness (0-255)

**Example:**
```
BRIGHTNESS 100         # Set brightness to 100
```

#### Set OS Mode
```
OS <mode>
```
- `mode`: `mac` or `win`

**Example:**
```
OS mac                 # Switch to macOS mode
OS win                 # Switch to Windows mode
```

#### List Settings
```
LIST
```
Shows all macros, LED colors, OS mode, and brightness.

#### Reset to Defaults
```
RESET
```
Resets all settings to factory defaults.

#### Help
```
HELP
```
Shows command help.

---

## Method 2: Web Configurator

### Setup

1. **Open the configurator:**
   - Open `tools/web_configurator.html` in Chrome or Edge
   - Or host it on a local web server

2. **Connect to macropad:**
   - Click "Connect to Macropad"
   - Select your ESP32-S3 device from the list
   - Connection status will show "✅ Connected"

3. **Configure:**
   - **Macros**: Select key, choose type, set modifier/keycode, click "Save Macro"
   - **LEDs**: Pick color for each LED, click "Set Color"
   - **Brightness**: Adjust slider, click "Set Brightness"
   - **OS Mode**: Click macOS or Windows card

4. **View/Export:**
   - Click "List All Settings" to see current configuration
   - Click "Export Configuration" (coming soon)
   - Click "Reset to Defaults" to restore factory settings

### Browser Requirements
- ✅ Chrome 89+ (recommended)
- ✅ Edge 89+
- ❌ Firefox (no Web Serial API support)
- ❌ Safari (no Web Serial API support)

---

## HID Key Codes Reference

Common key codes for macro configuration:

| Key | Code | Key | Code | Key | Code |
|-----|------|-----|------|-----|------|
| A-Z | 4-29 | 0-9 | 30-39 | F1-F12 | 58-69 |
| Enter | 40 | Esc | 41 | Backspace | 42 |
| Tab | 43 | Space | 44 | Delete | 76 |
| Arrow Up | 82 | Arrow Down | 81 | Arrow Left | 80 |
| Arrow Right | 79 | Home | 74 | End | 77 |
| Page Up | 75 | Page Down | 78 | Insert | 73 |

**Full list:** See `usb_hid.ino` or HID specification.

---

## Modifier Bitmask

Combine modifiers by adding their values:

| Modifier | Value |
|----------|-------|
| Ctrl | 1 |
| Shift | 2 |
| Alt | 4 |
| GUI (Cmd/Win) | 8 |

**Examples:**
- `0` = No modifier (uses OS-aware: Cmd on Mac, Ctrl on Win)
- `1` = Ctrl
- `2` = Shift
- `3` = Ctrl + Shift (1 + 2)
- `5` = Ctrl + Shift (1 + 4)
- `9` = GUI (Cmd/Win)

---

## Macro Types

### 1. Disabled (0)
- Key does nothing
- Falls back to hardcoded macro

### 2. Key Combo (1)
- Sends keyboard combination
- Example: `MACRO 1 1 0 4` = Ctrl+C

### 3. String (2)
- Types a string (max 5 characters)
- Example: `MACRO 9 2 0 0 hello` = Types "hello"

### 4. Special (3)
- OS-aware special functions
- Maps to hardcoded equivalents:
  - Screenshot (Key 1)
  - Lock Screen (Key 4)
  - Open Cursor (Key 9)
  - Open Terminal (Key 11)
  - Git Pull / OS Switch (Key 12)

---

## EEPROM Layout

```
Address 0:   OS Mode (1 byte)
Address 1:   Magic byte (0xAB)
Address 2:   LED Brightness (1 byte)
Address 3-17: LED Colors (5 LEDs × 3 bytes = 15 bytes)
Address 20-115: Macros (12 keys × 8 bytes = 96 bytes)
Address 116-511: Reserved
```

---

## Troubleshooting

### Serial Commands Not Working
- ✅ Check baud rate: 115200
- ✅ Ensure Serial Monitor sends newline (`\n`)
- ✅ Verify macropad is connected and powered

### Web Configurator Not Connecting
- ✅ Use Chrome or Edge browser
- ✅ Check USB cable (data-capable)
- ✅ Try different USB port
- ✅ Check device manager for COM port

### Settings Not Saving
- ✅ Check EEPROM initialization (should see "First boot detected" on first run)
- ✅ Verify EEPROM commit succeeded (check Serial output)
- ✅ Try RESET command to reinitialize

### Settings/Key Names Lost After Every Flash Upload
**Root cause:** ESP32 stores settings in the NVS (flash) partition. A **full flash erase** during upload wipes this partition. All bytes become 0xFF. The firmware reads the magic byte, gets 0xFF (instead of 0xAB), treats it as "first boot," and overwrites everything with defaults.

**Fix — Arduino IDE:**
1. Go to **Tools → Erase All Flash Before Sketch Upload**
2. Set to **Disabled** (or "Only Sketch" if available)
3. Upload again — your macros, key names, LED colors, and OS mode will be preserved

**Fix — PlatformIO:** Add to `platformio.ini`:
```ini
upload_flags = 
    --before=default_reset
    --after=hard_reset
```
Avoid using `erase` in upload commands.

### Macros Not Executing
- ✅ Check macro type (0 = disabled)
- ✅ Verify keycode is valid HID code
- ✅ Check modifier bitmask
- ✅ Use LIST command to verify saved macro

---

## Examples

### Example 1: Custom Copy Macro
```
MACRO 5 1 0 6    # Key 5: Ctrl+C (OS-aware modifier)
```

### Example 2: Custom String Macro
```
MACRO 1 2 0 0 git pull    # Key 1: Types "git pull"
```

### Example 3: Red LED Theme
```
LED 0 255 0 0    # LED 0: Red
LED 1 255 0 0    # LED 1: Red
LED 2 255 0 0    # LED 2: Red
LED 3 255 0 0    # LED 3: Red
LED 4 255 0 0    # LED 4: Red
BRIGHTNESS 100   # Set brightness
```

### Example 4: Rainbow LEDs
```
LED 0 255 0 0      # Red
LED 1 255 127 0    # Orange
LED 2 255 255 0    # Yellow
LED 3 0 255 0      # Green
LED 4 0 0 255      # Blue
```

---

## Advanced Usage

### Export/Import Configuration
(Coming soon - JSON export/import)

### Multiple Profiles
(Coming soon - Profile switching)

### Macro Chaining
(Coming soon - Execute multiple macros)

---

## Support

For issues or questions:
1. Check Serial Monitor for error messages
2. Use LIST command to verify settings
3. Try RESET to restore defaults
4. Check firmware version compatibility

---

**Happy Configuring! 🎉**

