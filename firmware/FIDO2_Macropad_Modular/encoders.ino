/*
 * Encoder Module
 * Handles single rotary encoder input (volume + mute)
 */

// ============================================
// STATE VARIABLES
// ============================================

volatile int encoderPosition = 0;
int lastEncoderCLK = HIGH;
bool isMuted = false;

// ============================================
// INITIALIZATION
// ============================================

void initEncoders() {
  debugPrint("Initializing encoder... ");
  
  // Configure encoder pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  // Attach interrupt for rotation detection
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, CHANGE);
  
  debugPrintln("OK");
}

// ============================================
// INTERRUPT HANDLER
// ============================================

void IRAM_ATTR readEncoder() {
  int clkState = digitalRead(ENCODER_CLK);
  int dtState = digitalRead(ENCODER_DT);
  
  if (clkState != lastEncoderCLK) {
    if (dtState != clkState) {
      encoderPosition++;  // Clockwise (Volume Up)
    } else {
      encoderPosition--;  // Counter-clockwise (Volume Down)
    }
  }
  lastEncoderCLK = clkState;
}

// ============================================
// ENCODER UPDATE (called in main loop)
// ============================================

void updateEncoders() {
  static int lastEncoderPos = 0;
  static unsigned long lastButtonTime = 0;
  const unsigned long debounceDelay = 300;
  
  // ========== ROTATION (VOLUME CONTROL) ==========
  
  if (encoderPosition != lastEncoderPos) {
    int diff = encoderPosition - lastEncoderPos;
    
    // Only change volume if not muted
    if (!isMuted) {
      volumeLevel += diff * ENCODER_STEP_SIZE;
      volumeLevel = constrain(volumeLevel, 0, 100);
      
      debugPrint("Volume: ");
      debugPrint(volumeLevel);
      debugPrintln("%");
      
      // Send volume HID commands
      if (diff > 0) {
        sendVolumeUp();
      } else {
        sendVolumeDown();
      }
      
      updateDisplay();
    } else {
      debugPrintln("Volume locked (muted)");
    }
    
    lastEncoderPos = encoderPosition;
  }
  
  // ========== BUTTON PRESS (MUTE/UNMUTE) ==========
  
  if (digitalRead(ENCODER_SW) == LOW) {
    if ((millis() - lastButtonTime) > debounceDelay) {
      // Toggle mute state
      isMuted = !isMuted;
      
      debugPrint("🔊 Audio: ");
      debugPrintln(isMuted ? "MUTED" : "UNMUTED");
      
      // Send mute command
      sendVolumeMute();
      
      // Visual feedback
      if (isMuted) {
        // Red flash for mute
        flashAllLEDs(CRGB::Red, 150);
      } else {
        // Green flash for unmute
        flashAllLEDs(CRGB::Green, 150);
      }
      
      updateDisplay();
      lastButtonTime = millis();
    }
  }
}

// ============================================
// HELPER FUNCTIONS
// ============================================

bool isMutedState() {
  return isMuted;
}

