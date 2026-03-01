/*
 * Waveshare Fingerprint Sensor - Driver Implementation
 * Adapted for ESP32-S3 Feather from Waveshare reference code
 * 
 * Uses hardware Serial1 (UART1) instead of SoftwareSerial
 * UART1 RX = GPIO38, UART1 TX = GPIO39
 * Baud rate: 19200 (Waveshare default)
 */

#include <string.h>
#include "finger.h"

// UART1 pins (per wiring diagram)
#define FINGERPRINT_RX 38  // U1RXD - receives from sensor TX
#define FINGERPRINT_TX 39  // U1TXD - transmits to sensor RX

uint8_t finger_RxBuf[9];
uint8_t finger_TxBuf[9];

uint8_t Finger_SleepFlag = 0;
int user_id;

/***************************************************************************
 * Initialize Serial1 for fingerprint communication
 ***************************************************************************/
void Finger_Serial_Init(void) {
  Serial1.begin(19200, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
}

/***************************************************************************
 * Send a byte to the sensor
 ***************************************************************************/
void TxByte(uint8_t temp) {
  Serial1.write(temp);
}

/***************************************************************************
 * Send a command and wait for response
 * @param Scnt: bytes to send
 * @param Rcnt: expected response bytes
 * @param Nms: timeout in milliseconds
 * @return ACK_SUCCESS or error code
 ***************************************************************************/
uint8_t TxAndRxCmd(uint8_t Scnt, uint8_t Rcnt, uint16_t Nms) {
  uint8_t i, j, CheckSum;
  uint16_t uart_RxCount = 0;
  unsigned long time_before = 0;
  unsigned long time_after = 0;
  uint8_t overflow_Flag = 0;

  TxByte(CMD_HEAD);
  CheckSum = 0;
  for (i = 0; i < Scnt; i++) {
    TxByte(finger_TxBuf[i]);
    CheckSum ^= finger_TxBuf[i];
  }
  TxByte(CheckSum);
  TxByte(CMD_TAIL);

  memset(finger_RxBuf, 0, sizeof(finger_RxBuf));
  Serial1.flush();

  // Receive with timeout
  time_before = millis();
  do {
    overflow_Flag = 0;
    if (Serial1.available()) {
      finger_RxBuf[uart_RxCount++] = Serial1.read();
    }
    time_after = millis();
    if (time_before > time_after) {
      time_before = millis();
      overflow_Flag = 1;
    }
  } while (((uart_RxCount < Rcnt) && (time_after - time_before < Nms)) || (overflow_Flag == 1));

  user_id = finger_RxBuf[3];
  if (uart_RxCount != Rcnt) return ACK_TIMEOUT;
  if (finger_RxBuf[0] != CMD_HEAD) return ACK_FAIL;
  if (finger_RxBuf[Rcnt - 1] != CMD_TAIL) return ACK_FAIL;
  if (finger_RxBuf[1] != (finger_TxBuf[0])) return ACK_FAIL;
  CheckSum = 0;
  for (j = 1; j < uart_RxCount - 1; j++) CheckSum ^= finger_RxBuf[j];
  if (CheckSum != 0) return ACK_FAIL;
  return ACK_SUCCESS;
}

/***************************************************************************
 * Query the number of enrolled fingerprints
 ***************************************************************************/
uint8_t GetUserCount(void) {
  uint8_t m;

  finger_TxBuf[0] = CMD_USER_CNT;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
    return finger_RxBuf[3];
  } else {
    return 0xFF;
  }
}

/***************************************************************************
 * Get Compare Level
 ***************************************************************************/
uint8_t GetcompareLevel(void) {
  uint8_t m;

  finger_TxBuf[0] = CMD_COM_LEV;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 1;
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
    return finger_RxBuf[3];
  } else {
    return 0xFF;
  }
}

/***************************************************************************
 * Set Compare Level (0-9, default 5, higher = stricter)
 ***************************************************************************/
uint8_t SetcompareLevel(uint8_t temp) {
  uint8_t m;

  finger_TxBuf[0] = CMD_COM_LEV;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = temp;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
    return finger_RxBuf[3];
  } else {
    return 0xFF;
  }
}

/***************************************************************************
 * Get fingerprint collection timeout
 ***************************************************************************/
uint8_t GetTimeOut(void) {
  uint8_t m;

  finger_TxBuf[0] = CMD_TIMEOUT;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 1;
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
    return finger_RxBuf[3];
  } else {
    return 0xFF;
  }
}

/***************************************************************************
 * Register/Enroll a new fingerprint (requires 3 scans)
 ***************************************************************************/
uint8_t AddUser(void) {
  uint8_t m;

  m = GetUserCount();
  if (m >= USER_MAX_CNT)
    return ACK_FULL;

  finger_TxBuf[0] = CMD_ADD_1;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = m + 1;
  finger_TxBuf[3] = 3;       // User privilege: master
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 6000);
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
    finger_TxBuf[0] = CMD_ADD_2;
    m = TxAndRxCmd(5, 8, 6000);
    if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
      finger_TxBuf[0] = CMD_ADD_3;
      m = TxAndRxCmd(5, 8, 6000);
      if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
        return ACK_SUCCESS;
      } else return ACK_FAIL;
    } else return ACK_FAIL;
  } else return ACK_FAIL;
}

/***************************************************************************
 * Clear all fingerprints
 ***************************************************************************/
uint8_t ClearAllUser(void) {
  uint8_t m;

  finger_TxBuf[0] = CMD_DEL_ALL;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 500);

  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS) {
    return ACK_SUCCESS;
  } else {
    return ACK_FAIL;
  }
}

/***************************************************************************
 * Check if user is a master user (ID 1-3)
 ***************************************************************************/
uint8_t IsMasterUser(uint8_t UserID) {
  if ((UserID == 1) || (UserID == 2) || (UserID == 3)) return TRUE;
  else return FALSE;
}

/***************************************************************************
 * Verify/Match a fingerprint against stored templates
 ***************************************************************************/
uint8_t VerifyUser(void) {
  uint8_t m;

  finger_TxBuf[0] = CMD_MATCH;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;

  m = TxAndRxCmd(5, 8, 5000);

  if ((m == ACK_SUCCESS) && (IsMasterUser(finger_RxBuf[4]) == TRUE) && finger_RxBuf[3] != 0) {
    return ACK_SUCCESS;
  } else if (finger_RxBuf[4] == ACK_NO_USER) {
    return ACK_NO_USER;
  } else if (finger_RxBuf[4] == ACK_TIMEOUT) {
    return ACK_TIMEOUT;
  }
  return ACK_FAIL;
}
