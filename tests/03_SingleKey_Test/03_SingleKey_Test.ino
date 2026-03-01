/*
 * Quick Key Matrix Single Button Test
 * Tests K1: Row1 (GPIO5) + Col1 (GPIO13)
 * 
 * Wiring: Push button between GPIO5 and GPIO13
 * GPIO5 = Row output (driven LOW)
 * GPIO13 = Column input (pulled HIGH, reads LOW when pressed)
 */

#define ROW1 5
#define COL1 13

bool lastState = false;
int pressCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.println("  Key Matrix - Single Button Test (K1)");
  Serial.println("  Row1=GPIO5, Col1=GPIO13");
  Serial.println("========================================");
  Serial.println();

  // Row pin: output, drive LOW
  pinMode(ROW1, OUTPUT);
  digitalWrite(ROW1, LOW);

  // Column pin: input with pull-up
  pinMode(COL1, INPUT_PULLUP);

  Serial.println("Press the button...");
  Serial.println();
}

void loop() {
  // When button pressed: COL1 connects to ROW1 (LOW)
  bool pressed = (digitalRead(COL1) == LOW);

  if (pressed && !lastState) {
    pressCount++;
    Serial.print("  ✓ K1 PRESSED!  (count: ");
    Serial.print(pressCount);
    Serial.println(")");
  } else if (!pressed && lastState) {
    Serial.println("    K1 released");
  }

  lastState = pressed;
  delay(50);  // debounce
}
