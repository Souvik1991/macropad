/*
 * USB Volume Control via Rotary Encoder
 * Board: Adafruit ESP32-S3 Feather (native USB)
 * 
 * Encoder Connections:
 *   CLK -> GPIO 10
 *   DT  -> GPIO 11
 *   SW  -> GPIO 12
 *   +   -> 3.3V
 *   GND -> GND
 * 
 * This sketch sends USB HID Consumer Control commands:
 *   Rotate CW  -> Volume Up
 *   Rotate CCW -> Volume Down
 *   Press      -> Mute/Unmute
 * 
 * NOTE: After upload, the ESP32-S3 will appear as a USB
 * media keyboard. Your PC volume will change in real time!
 * 
 * Board Settings in Arduino IDE:
 *   USB Mode: "USB-OTG (TinyUSB)"  or  "USB Mode: USB-OTG"
 *   USB CDC On Boot: "Enabled" (for Serial Monitor)
 */

#include "USB.h"
#include "USBHIDConsumerControl.h"

USBHIDConsumerControl ConsumerControl;

// Encoder pins
#define ENC_CLK 10
#define ENC_DT  11
#define ENC_SW  12

// Encoder state
volatile int encoderDelta = 0;
int lastCLKState = HIGH;

// Button debounce
bool buttonPressed = false;
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 200;

// Forward declaration
void IRAM_ATTR readEncoder();

void setup() {
  Serial.begin(115200);

  // Initialize USB HID
  ConsumerControl.begin();
  USB.begin();

  delay(2000);
  Serial.println("========================================");
  Serial.println("  USB Volume Control Test");
  Serial.println("  Encoder: GPIO10/11/12");
  Serial.println("========================================");
  Serial.println();

  // Configure encoder pins
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  // Attach interrupt
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), readEncoder, CHANGE);

  Serial.println("Ready! Rotate = Volume, Press = Mute");
  Serial.println("Your PC volume will change in real time.");
  Serial.println();
}

void loop() {
  // Handle rotation (volume up/down)
  if (encoderDelta != 0) {
    int delta = encoderDelta;
    encoderDelta = 0;

    if (delta > 0) {
      // Clockwise = Volume Up
      for (int i = 0; i < delta; i++) {
        ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
        ConsumerControl.release();
        delay(10);
      }
      Serial.println("  >> Volume UP");
    } else {
      // Counter-clockwise = Volume Down
      for (int i = 0; i < -delta; i++) {
        ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
        ConsumerControl.release();
        delay(10);
      }
      Serial.println("  >> Volume DOWN");
    }
  }

  // Handle button press (mute)
  if (digitalRead(ENC_SW) == LOW && !buttonPressed) {
    if ((millis() - lastButtonTime) > debounceDelay) {
      buttonPressed = true;
      lastButtonTime = millis();

      ConsumerControl.press(CONSUMER_CONTROL_MUTE);
      ConsumerControl.release();
      Serial.println("  >> MUTE toggle");
    }
  } else if (digitalRead(ENC_SW) == HIGH && buttonPressed) {
    buttonPressed = false;
  }

  delay(10);
}

// Interrupt handler
void IRAM_ATTR readEncoder() {
  int clkState = digitalRead(ENC_CLK);
  int dtState = digitalRead(ENC_DT);

  if (clkState != lastCLKState) {
    if (dtState != clkState) {
      encoderDelta++;
    } else {
      encoderDelta--;
    }
  }
  lastCLKState = clkState;
}
