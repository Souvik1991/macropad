/*
 * Rotary Encoder Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * Encoder: EC11 or M274 360° Rotary Encoder (Volume + Mute)
 * 
 * Connections:
 *   CLK -> GPIO 10
 *   DT  -> GPIO 11
 *   SW  -> GPIO 12
 *   +   -> 3.3V
 *   GND -> GND
 *
 * Functions:
 *   Rotate CW  -> Volume Up
 *   Rotate CCW -> Volume Down
 *   Press      -> Mute/Unmute toggle
 */

// Encoder pins (Volume Control)
#define ENC_CLK 10
#define ENC_DT  11
#define ENC_SW  12

// Encoder state
volatile int encoderPosition = 0;
bool encoderPressed = false;
bool isMuted = false;
int volume = 50;  // Start at 50%

// Debouncing
int lastCLKState = HIGH;
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 50;

// Forward declaration for interrupt handler
void IRAM_ATTR readEncoder();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.println("    Rotary Encoder Test (Volume)");
  Serial.println("========================================");
  Serial.println();
  
  // Configure encoder pins
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  
  // Attach interrupt for rotation detection
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), readEncoder, CHANGE);
  
  Serial.println("Encoder initialized on GPIO 10/11/12");
  Serial.println("  Rotate  -> Change volume");
  Serial.println("  Press   -> Mute/Unmute");
  Serial.println();
  printVolume();
}

void loop() {
  static int lastPosition = 0;
  
  // Check rotation
  if (encoderPosition != lastPosition) {
    int diff = encoderPosition - lastPosition;
    lastPosition = encoderPosition;
    
    if (!isMuted) {
      volume += diff * 5;  // 5% per step
      volume = constrain(volume, 0, 100);
    }
    
    printVolume();
  }
  
  // Check button press (mute/unmute)
  if (digitalRead(ENC_SW) == LOW && !encoderPressed) {
    if ((millis() - lastButtonTime) > debounceDelay) {
      encoderPressed = true;
      isMuted = !isMuted;
      lastButtonTime = millis();
      
      if (isMuted) {
        Serial.println(">>> MUTED (press again to unmute)");
      } else {
        Serial.println(">>> UNMUTED");
      }
      printVolume();
    }
  } else if (digitalRead(ENC_SW) == HIGH && encoderPressed) {
    encoderPressed = false;
  }
  
  delay(10);
}

// Interrupt handler for encoder rotation
void IRAM_ATTR readEncoder() {
  int clkState = digitalRead(ENC_CLK);
  int dtState = digitalRead(ENC_DT);
  
  if (clkState != lastCLKState) {
    if (dtState != clkState) {
      encoderPosition++;
    } else {
      encoderPosition--;
    }
  }
  lastCLKState = clkState;
}

void printVolume() {
  Serial.print("Volume: ");
  if (isMuted) {
    Serial.print("[MUTED] ");
  }
  Serial.print(volume);
  Serial.print("% ");
  
  // Draw a simple bar
  int bars = volume / 5;
  Serial.print("[");
  for (int i = 0; i < 20; i++) {
    Serial.print(i < bars ? "#" : "-");
  }
  Serial.println("]");
}

