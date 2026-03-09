/*
 * Encoder Module - Implementation
 * Handles single rotary encoder input (volume + mute)
 */

#include "config.h"
#include "debug.h"
#include "display.h"
#include "encoders.h"
#include "leds.h"
#include "usb_hid.h"

volatile int encoderPosition = 0;
int lastEncoderCLK = HIGH;
bool isMuted = false;

void readEncoder();

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

void updateEncoders() {
  static int lastEncoderPos = 0;
  static bool encoderPressed = false;
  static unsigned long lastButtonTime = 0;
  const unsigned long debounceDelay = 200;
  
  // ========== ROTATION (VOLUME CONTROL) ==========
  
  if (encoderPosition != lastEncoderPos) {
    int diff = encoderPosition - lastEncoderPos;
    lastEncoderPos = encoderPosition;
    
    // Only change volume if not muted
    if (!isMuted) {
      float stepSize = (currentOS == OS_MAC) ? ENCODER_STEP_SIZE_MAC : (float)ENCODER_STEP_SIZE_WIN;
      volumeLevel += diff * stepSize;
      volumeLevel = constrain(volumeLevel, 0.0f, 100.0f);
      
      debugPrint("Volume: ");
      debugPrint((int)(volumeLevel + 0.5f));
      debugPrintln("%");
      
      // Send volume HID commands per step (matching tested code)
      if (diff > 0) {
        for (int i = 0; i < diff; i++) {
          sendVolumeUp();
          delay(10);
        }
      } else {
        for (int i = 0; i < -diff; i++) {
          sendVolumeDown();
          delay(10);
        }
      }
      
      // Trigger volume overlay on OLED
      volumeOverlayUntil = millis() + VOLUME_OVERLAY_MS;
    } else {
      debugPrintln("Volume locked (muted)");
    }
  }
  
  // ========== BUTTON PRESS (MUTE/UNMUTE) ==========
  // Edge detection: trigger once on press, reset on release
  
  if (digitalRead(ENCODER_SW) == LOW && !encoderPressed) {
    if ((millis() - lastButtonTime) > debounceDelay) {
      encoderPressed = true;
      lastButtonTime = millis();
      
      // Toggle mute state
      isMuted = !isMuted;
      
      debugPrint("Audio: ");
      debugPrintln(isMuted ? "MUTED" : "UNMUTED");
      
      // Send mute command
      sendVolumeMute();
      
      // Visual feedback (matches OLED overlay duration)
      if (isMuted) {
        setLEDPattern(PATTERN_VOLUME_MUTE);
      } else {
        setLEDPattern(PATTERN_VOLUME_UNMUTE);
      }
      
      // Trigger volume overlay on OLED
      volumeOverlayUntil = millis() + VOLUME_OVERLAY_MS;
    }
  } else if (digitalRead(ENCODER_SW) == HIGH && encoderPressed) {
    encoderPressed = false;
  }
}

bool isMutedState() {
  return isMuted;
}
