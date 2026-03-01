# OLED Display Layout - 128×64 pixels

## 📺 **Persistent Status Bar Design**

The status bar is **always visible** at the top (rows 0-10), showing critical information at a glance.

---

## 🎨 **Visual Mockups**

### **Idle Screen**
```
┌──────────────────────────────────┐ 128px wide
│ [Win]    🔊 75%                  │ ← Status bar (row 0-9)
├──────────────────────────────────┤
│ READY FOR ACTION                 │ ← Content starts (row 12)
│ ---------------                  │
│ R1:Shot Mic Vid Lock             │
│ R2:Copy Cut Pst Del              │
│ R3:Cur Insp Trm Git              │
│                                  │
│ Double-tap Key 12                │
│ to switch OS mode                │
└──────────────────────────────────┘ 64px tall
```

### **Muted State**
```
┌──────────────────────────────────┐
│ [Mac]    🔇 MUTE                 │ ← Mute icon + text
├──────────────────────────────────┤
│ READY FOR ACTION                 │
│ ---------------                  │
│ R1:Shot Mic Vid Lock             │
│ R2:Copy Cut Pst Del              │
│ R3:Cur Insp Trm Git              │
│                                  │
│ Double-tap Key 12                │
│ to switch OS mode                │
└──────────────────────────────────┘
```

### **FIDO2 Authentication Screen**
```
┌──────────────────────────────────┐
│ [Win]    🔊 85%                  │ ← Status bar visible
├──────────────────────────────────┤
│          🔒                       │
│    PASSKEY AUTH                  │
│                                  │
│    Please scan                   │
│    your finger                   │
│                                  │
│                                  │
│                                  │
└──────────────────────────────────┘
```

### **Success Screen**
```
┌──────────────────────────────────┐
│ [Mac]    🔊 60%                  │ ← Status bar visible
├──────────────────────────────────┤
│                                  │
│      ✓         AUTH              │
│                 OK!              │
│                                  │
│                                  │
│                                  │
│ Welcome back!                    │
│                                  │
└──────────────────────────────────┘
```

### **Error Screen**
```
┌──────────────────────────────────┐
│ [Win]    🔊 50%                  │ ← Status bar visible
├──────────────────────────────────┤
│                                  │
│      ✗         AUTH              │
│                ERR               │
│                                  │
│                                  │
│                                  │
│ Try again...                     │
│                                  │
└──────────────────────────────────┘
```

### **OS Switch Confirmation**
```
┌──────────────────────────────────┐
│ [Mac]    🔊 75%                  │ ← Status bar updates!
├──────────────────────────────────┤
│                                  │
│                                  │
│        OS MODE:                  │
│                                  │
│         MAC                      │ ← Large text
│                                  │
│                                  │
│                                  │
└──────────────────────────────────┘
```

---

## 🔧 **Technical Details**

### **Layout Dimensions**

```
Total screen: 128×64 pixels

┌─────────────────────────────────┐
│ Row 0-9:   Status Bar (10px)    │
│            - OS: [Win]/[Mac]    │
│            - Volume: 🔊 75%     │
│            - Mute: 🔇 MUTE      │
├─────────────────────────────────┤
│ Row 10:    Separator (1px)      │
├─────────────────────────────────┤
│ Row 11:    Padding (1px)        │
├─────────────────────────────────┤
│ Row 12-63: Main Content (52px)  │
│            - Dynamic content    │
│            - Mode-specific UI   │
└─────────────────────────────────┘
```

### **Status Bar Components**

| Element | Position | Size | Description |
|---------|----------|------|-------------|
| **OS Mode** | (0,0) | ~30px wide | `[Win]` or `[Mac]` |
| **Separator** | — | 10px gap | Space between elements |
| **Speaker Icon** | (48,1) | 10px wide | Filled triangle shape |
| **Volume/Mute** | (58,0) | ~40px wide | Percentage or "MUTE" text |

### **Icon Designs**

**Speaker Icon (Normal):**
```
■■■ ▶
```

**Speaker Icon (Muted):**
```
■■■ ▶ ✗
```

**Lock Icon (FIDO2):**
```
 ┌─┐
 │ │
┌┴─┴┐
│   │
└───┘
```

**Checkmark (Success):**
```
    ╱
  ╱
╱
```

**X Mark (Error):**
```
╲ ╱
 ╳
╱ ╲
```

---

## 📝 **Status Bar Behavior**

| Condition | Display |
|-----------|---------|
| **Normal playback** | `[Win] 🔊 75%` |
| **Muted** | `[Mac] 🔇 MUTE` |
| **Volume 0%** | `[Win] 🔊 0%` |
| **Volume 100%** | `[Mac] 🔊 100%` |
| **OS switched** | Updates immediately: `[Win]` → `[Mac]` |

---

## 🎯 **Key Features**

1. ✅ **Always visible** - Status bar never disappears
2. ✅ **OS at-a-glance** - Always know which OS mode you're in
3. ✅ **Volume feedback** - See current volume level
4. ✅ **Mute indication** - Clear visual when muted
5. ✅ **Space efficient** - Only 10 pixels tall, 52 pixels for content

---

## 🔄 **Update Flow**

```
User action (key press, encoder turn)
    │
    ▼
updateDisplay() called
    │
    ├─► drawStatusBar()      ← Always drawn first
    │   ├─ Show OS mode
    │   ├─ Show volume/mute
    │   └─ Draw separator
    │
    ├─► displayIdleScreen()  ← Or other mode
    │   └─ Show key layout
    │
    └─► display.display()    ← Refresh screen
```

---

## 💡 **Future Enhancements**

Possible additions to status bar (if space permits):

```
┌──────────────────────────────────┐
│ [Win] 🔊75% │ 🔋 USB │ 📶 WiFi  │ ← With connectivity icons
├──────────────────────────────────┤
│ Content...                       │
```

- 🔋 Power indicator (USB/Battery)
- 📶 WiFi/Bluetooth status
- 🔐 FIDO2 ready indicator
- ⏰ Time (optional)

---

## 🎨 **Design Philosophy**

1. **Information hierarchy** - Most important info (OS mode) always visible
2. **Glanceable** - Status readable in <1 second
3. **Consistent** - Same layout across all modes
4. **Minimal** - No clutter, only essential info
5. **Responsive** - Updates immediately on changes

---

## ✅ **Implementation Status**

- ✅ Status bar structure
- ✅ OS mode display
- ✅ Volume percentage
- ✅ Mute icon
- ✅ Separator line
- ✅ All screen modes updated
- ✅ Double-tap OS switch visual feedback
- ⏳ FIDO2 authentication flow (hardware pending)

**Ready to test when hardware arrives!** 🚀

