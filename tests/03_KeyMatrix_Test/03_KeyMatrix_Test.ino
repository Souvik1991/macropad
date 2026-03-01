/*
 * Key Matrix Test Sketch (3x4 = 12 keys)
 * Board: Adafruit ESP32-S3 Feather
 * Keys: 12x Cherry MX switches with 1N4148W diodes
 * 
 * Matrix Configuration:
 * 3 Rows (outputs) × 4 Columns (inputs with pull-ups)
 * 
 * Connections (FINAL - avoiding board-reserved pins):
 * Row 1 -> GPIO 5
 * Row 2 -> GPIO 6
 * Row 3 -> GPIO 9  (GPIO7 not available on board)
 * 
 * Col 1 -> GPIO 13 (GPIO21 reserved for onboard NeoPixel)
 * Col 2 -> GPIO 14
 * Col 3 -> GPIO 15
 * Col 4 -> GPIO 16
 * 
 * Key Layout:
 *    Col1  Col2  Col3  Col4
 * R1 [ 1] [ 2] [ 3] [ 4]
 * R2 [ 5] [ 6] [ 7] [ 8]
 * R3 [ 9] [10] [11] [12]
 */

// Row pins (outputs) - Avoiding I2C and non-existent pins
const int rowPins[3] = {5, 6, 9};

// Column pins (inputs with pull-ups) - Avoiding GPIO21 (onboard NeoPixel)
const int colPins[4] = {13, 14, 15, 16};

// Key state tracking
bool keyStates[3][4] = {0};
bool lastKeyStates[3][4] = {0};

// Key labels for display
const char* keyLabels[3][4] = {
  {"Key 1",  "Key 2",  "Key 3",  "Key 4"},
  {"Key 5",  "Key 6",  "Key 7",  "Key 8"},
  {"Key 9",  "Key 10", "Key 11", "Key 12"}
};

// Macro functions for keys (examples)
const char* keyMacros[3][4] = {
  {"Ctrl+C (Copy)",     "Ctrl+V (Paste)",    "Ctrl+S (Save)",     "Ctrl+Z (Undo)"},
  {"Ctrl+Shift+T (Tab)","Alt+Tab (Switch)",  "Win+D (Desktop)",   "Ctrl+F (Find)"},
  {"F5 (Refresh)",      "Ctrl+W (Close)",    "Ctrl+T (New Tab)",  "Ctrl+A (Select All)"}
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Key Matrix Test Starting...");
  Serial.println("3x4 Matrix = 12 Keys");
  Serial.println();
  
  // Configure row pins as outputs
  for (int i = 0; i < 3; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);  // Set high (inactive) by default
  }
  
  // Configure column pins as inputs with pull-ups
  for (int i = 0; i < 4; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
  
  Serial.println("Matrix initialized!");
  Serial.println("Press any key...");
  Serial.println();
  printKeyMap();
}

void loop() {
  scanMatrix();
  delay(10);  // Debounce delay
}

void scanMatrix() {
  // Scan each row
  for (int row = 0; row < 3; row++) {
    // Activate current row (set LOW)
    digitalWrite(rowPins[row], LOW);
    delayMicroseconds(10);  // Small delay for signal to settle
    
    // Read all columns
    for (int col = 0; col < 4; col++) {
      keyStates[row][col] = !digitalRead(colPins[col]);  // Invert (LOW = pressed)
      
      // Detect key press (state changed from released to pressed)
      if (keyStates[row][col] && !lastKeyStates[row][col]) {
        handleKeyPress(row, col);
      }
      
      // Detect key release (state changed from pressed to released)
      if (!keyStates[row][col] && lastKeyStates[row][col]) {
        handleKeyRelease(row, col);
      }
      
      // Update last state
      lastKeyStates[row][col] = keyStates[row][col];
    }
    
    // Deactivate current row (set HIGH)
    digitalWrite(rowPins[row], HIGH);
  }
}

void handleKeyPress(int row, int col) {
  int keyNum = (row * 4) + col + 1;
  Serial.print(">>> KEY PRESSED: ");
  Serial.print(keyLabels[row][col]);
  Serial.print(" (Position: R");
  Serial.print(row + 1);
  Serial.print("C");
  Serial.print(col + 1);
  Serial.println(")");
  Serial.print("    Macro: ");
  Serial.println(keyMacros[row][col]);
  Serial.println();
}

void handleKeyRelease(int row, int col) {
  Serial.print("<<< KEY RELEASED: ");
  Serial.println(keyLabels[row][col]);
  Serial.println();
}

void printKeyMap() {
  Serial.println("┌──────────────────────────────────────────────────┐");
  Serial.println("│              KEY MAPPING REFERENCE               │");
  Serial.println("├──────────────────────────────────────────────────┤");
  for (int row = 0; row < 3; row++) {
    Serial.print("│ Row ");
    Serial.print(row + 1);
    Serial.print(": ");
    for (int col = 0; col < 4; col++) {
      Serial.print("[");
      if ((row * 4) + col + 1 < 10) Serial.print(" ");
      Serial.print((row * 4) + col + 1);
      Serial.print("] ");
    }
    Serial.println("│");
    Serial.print("│       ");
    for (int col = 0; col < 4; col++) {
      Serial.print(keyMacros[row][col]);
      if (col < 3) Serial.print(" | ");
    }
    Serial.println("│");
    if (row < 2) {
      Serial.println("├──────────────────────────────────────────────────┤");
    }
  }
  Serial.println("└──────────────────────────────────────────────────┘");
  Serial.println();
}

