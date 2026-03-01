/*
 * Waveshare Fingerprint Sensor Test Sketch
 * Board: Adafruit ESP32-S3 Feather
 * Sensor: Waveshare UART Fingerprint Sensor (C) - Capacitive
 * 
 * Connections (UART1 - crossover TX/RX):
 * Fingerprint VCC    -> ESP32 3.3V
 * Fingerprint GND    -> ESP32 GND
 * Fingerprint TX     -> ESP32 GPIO 38 (U1RXD - UART1 RX)
 * Fingerprint RX     -> ESP32 GPIO 39 (U1TXD - UART1 TX)
 * Fingerprint WAKEUP -> Not connected
 * 
 * Baud Rate: 19200 (Waveshare default)
 * 
 * Based on Waveshare official sample code, adapted for ESP32-S3
 */

#include "finger.h"

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("========================================");
  Serial.println("  Waveshare Fingerprint Sensor Test");
  Serial.println("========================================");
  Serial.println();

  // Initialize fingerprint sensor UART
  Serial.println("[Step 1] Initializing UART1 (GPIO38 RX, GPIO39 TX @ 19200)...");
  Finger_Serial_Init();
  delay(1000);

  // Try to communicate with sensor
  Serial.println("[Step 2] Connecting to fingerprint sensor...");
  Serial.println("  Attempting to set compare level to 5...");

  int retries = 0;
  while (SetcompareLevel(5) != 5) {
    retries++;
    Serial.print("  Retry ");
    Serial.print(retries);
    Serial.println("... waiting for sensor");
    delay(500);
    if (retries >= 10) {
      Serial.println();
      Serial.println("  ✗ FAILED to communicate with sensor!");
      Serial.println();
      Serial.println("  Troubleshooting:");
      Serial.println("  1. Check wiring: Sensor TX -> GPIO38, Sensor RX -> GPIO39");
      Serial.println("  2. Check power: VCC -> 3.3V, GND -> GND");
      Serial.println("  3. Make sure TX/RX are crossed (not straight)");
      Serial.println("  4. Try power cycling the sensor");
      Serial.println();
      Serial.println("========================================");
      Serial.println("  TEST FAILED - Cannot proceed");
      Serial.println("========================================");
      while (1) { delay(1000); }
    }
  }
  Serial.println("  ✓ Sensor connected successfully!");
  Serial.println();

  // Print sensor info
  Serial.println("[Step 3] Sensor Information:");
  uint8_t count = GetUserCount();
  Serial.print("  Compare Level:     5 (0-9, higher=stricter)");
  Serial.println();
  Serial.print("  Enrolled Fingers:  ");
  if (count == 0xFF) {
    Serial.println("Error reading");
  } else {
    Serial.println(count);
  }
  Serial.print("  Max Capacity:      ");
  Serial.println(USER_MAX_CNT);
  Serial.println();

  // Print menu
  Serial.println("========================================");
  Serial.println("  Commands (type in Serial Monitor):");
  Serial.println("  CMD1 - Get fingerprint count");
  Serial.println("  CMD2 - Enroll new fingerprint");
  Serial.println("  CMD3 - Verify fingerprint (match)");
  Serial.println("  CMD4 - Delete all fingerprints");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Ready! Enter a command...");
}

void loop() {
  // Parse serial commands from user
  handleSerialCommands();
  delay(10);
}

void handleSerialCommands() {
  static uint8_t step = 0;
  uint8_t temp;

  if (Serial.available()) {
    temp = Serial.read();

    switch (step) {
      case 0: if (temp == 'C') step++; else step = 0; break;
      case 1: if (temp == 'M') step++; else step = 0; break;
      case 2: if (temp == 'D') step++; else step = 0; break;
      case 3:
        switch (temp) {
          case '1':
            cmdGetCount();
            break;
          case '2':
            cmdEnroll();
            break;
          case '3':
            cmdVerify();
            break;
          case '4':
            cmdClearAll();
            break;
          default:
            Serial.println("Unknown command. Use CMD1-CMD4");
            break;
        }
        step = 0;
        break;
      default:
        step = 0;
        break;
    }
  }
}

// CMD1: Get fingerprint count
void cmdGetCount() {
  Serial.println();
  Serial.println(">>> Getting fingerprint count...");
  uint8_t count = GetUserCount();
  if (count == 0xFF) {
    Serial.println("  ✗ Error reading count");
  } else {
    Serial.print("  Enrolled fingerprints: ");
    Serial.println(count);
  }
  Serial.println();
}

// CMD2: Enroll new fingerprint (interactive 3-step)
void cmdEnroll() {
  uint8_t m;
  extern uint8_t finger_TxBuf[];
  extern uint8_t finger_RxBuf[];

  Serial.println();
  Serial.println(">>> Enrolling new fingerprint...");

  // Get current count to determine new user ID
  uint8_t count = GetUserCount();
  if (count >= USER_MAX_CNT) {
    Serial.println("  ✗ Fingerprint storage is full!");
    return;
  }
  uint8_t newID = count + 1;
  Serial.print("  New user will be ID: ");
  Serial.println(newID);
  Serial.println();

  // --- Scan 1 of 3 ---
  Serial.println("  [Scan 1/3] Place your finger on the sensor NOW...");
  finger_TxBuf[0] = CMD_ADD_1;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = newID;
  finger_TxBuf[3] = 3;  // Master user privilege
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 10000);  // 10 second timeout
  if (m != ACK_SUCCESS || finger_RxBuf[4] != ACK_SUCCESS) {
    Serial.print("  ✗ Scan 1 failed! (error: ");
    Serial.print(m);
    Serial.print("/");
    Serial.print(finger_RxBuf[4]);
    Serial.println(")");
    Serial.println("  Tip: Place finger flat on center of sensor.");
    return;
  }
  Serial.println("  ✓ Scan 1 OK! Lift your finger...");
  delay(2000);  // Give user time to lift finger

  // --- Scan 2 of 3 ---
  Serial.println("  [Scan 2/3] Place the SAME finger again...");
  finger_TxBuf[0] = CMD_ADD_2;

  m = TxAndRxCmd(5, 8, 10000);
  if (m != ACK_SUCCESS || finger_RxBuf[4] != ACK_SUCCESS) {
    Serial.print("  ✗ Scan 2 failed! (error: ");
    Serial.print(m);
    Serial.print("/");
    Serial.print(finger_RxBuf[4]);
    Serial.println(")");
    Serial.println("  Tip: Use the same finger, slightly different angle.");
    return;
  }
  Serial.println("  ✓ Scan 2 OK! Lift your finger...");
  delay(2000);

  // --- Scan 3 of 3 ---
  Serial.println("  [Scan 3/3] Place the SAME finger one last time...");
  finger_TxBuf[0] = CMD_ADD_3;

  m = TxAndRxCmd(5, 8, 10000);
  if (m != ACK_SUCCESS || finger_RxBuf[4] != ACK_SUCCESS) {
    Serial.print("  ✗ Scan 3 failed! (error: ");
    Serial.print(m);
    Serial.print("/");
    Serial.print(finger_RxBuf[4]);
    Serial.println(")");
    return;
  }

  Serial.println();
  Serial.println("  ✓ Fingerprint enrolled successfully!");
  Serial.print("  User ID: ");
  Serial.println(newID);
  Serial.print("  Total enrolled: ");
  Serial.println(GetUserCount());
  Serial.println();
}

// CMD3: Verify/Match fingerprint
void cmdVerify() {
  Serial.println();
  Serial.println(">>> Verifying fingerprint...");
  Serial.println("  Place finger on sensor now...");

  switch (VerifyUser()) {
    case ACK_SUCCESS:
      Serial.print("  ✓ Match! User ID: ");
      Serial.println(user_id);
      break;
    case ACK_NO_USER:
      Serial.println("  ✗ No match - fingerprint not in library.");
      break;
    case ACK_TIMEOUT:
      Serial.println("  ✗ Timeout - no finger detected.");
      break;
    default:
      Serial.println("  ✗ Verification failed.");
      break;
  }
  Serial.println();
}

// CMD4: Clear all fingerprints
void cmdClearAll() {
  Serial.println();
  Serial.println(">>> Clearing ALL fingerprints...");

  if (ClearAllUser() == ACK_SUCCESS) {
    Serial.println("  ✓ All fingerprints deleted!");
  } else {
    Serial.println("  ✗ Failed to clear fingerprints.");
  }
  Serial.println();
}
