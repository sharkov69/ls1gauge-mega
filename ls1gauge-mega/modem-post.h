struct AsyncAT {
  const char* cmd = nullptr;
  const char* expected = nullptr;
  uint8_t expectedLen = 0;
  uint8_t matchPos = 0;
  uint16_t timeout = 1000;
  uint16_t start = 0;
  bool sent = false;
  bool busy = false;
  bool done = false;
  bool result = false;
} at;

void startAT(const char* cmd, const char* expected = "OK", uint16_t timeout = 2000) {
  if (at.busy || at.done) return;
  at.cmd = cmd;
  at.expected = expected;
  at.expectedLen = strlen(expected);
  at.timeout = timeout;
  at.matchPos = 0;
  at.sent = false;
  at.start = 0;
  at.busy = true;
  at.done = false;
  at.result = false;
}

void updateAT() {
  //DBG(F("AT> ")); DBGL(at.cmd);
  if (!at.busy || at.done) return;
  if (!at.sent) {
    while (Serial1.available()) Serial1.read(); 
    Serial1.println(at.cmd);
    at.sent = true;
    delay(50);
    at.start = (uint16_t)(millis() & 0xFFFF);
    return;
  }
  while (Serial1.available()) {
    char c = Serial1.read();
    //Serial.print(F("")); Serial.write(c);
    if (c == at.expected[at.matchPos]) {
      at.matchPos++;
      if (at.matchPos == at.expectedLen) {
        at.done = true;
        at.result = true;
        at.busy = false;
        return;
      }
    } else {
      at.matchPos = 0;
    }
  }
  if ((uint16_t)((millis() & 0xFFFF) - at.start) > at.timeout) {
    at.done = true;
    at.result = false;
    at.busy = false;
  }
}

enum SendLogState {
  IDLE,
  INIT,
  SEND_PARAMS,
  SEND_DATA_CMD,
  SEND_DATA_BODY,
  WAIT,
  HTTP_ACTION,
  PART2,
  DONE
};

SendLogState logState = IDLE;
uint8_t retryCount = 0;
DateTime logNow;
uint16_t wait = 0;
uint8_t logPart = 0;

constexpr uint8_t MAX_RETRY = 5;

size_t postSize = 0;
#define LOG_PART LOG_ENTRIES/2
#define POSTBIN_SIZE (12 + 4 + sizeof(logBuffer) / 2 + 2)
uint8_t postBin[POSTBIN_SIZE];

uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t crc = 0xFFFF) {
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc;
}

void buildBinaryPayload(uint8_t part) {
  postSize = 0;
  memcpy(postBin, token, TOKEN_LENGTH);
  postSize += TOKEN_LENGTH;
  uint16_t year = logNow.year();
  postBin[postSize++] = year & 0xFF;
  postBin[postSize++] = year >> 8;
  postBin[postSize++] = logNow.month();
  memcpy(postBin + postSize, &logBuffer[part * LOG_PART ], sizeof(logBuffer) / 2);
  postSize += sizeof(logBuffer) / 2;
  uint16_t crc = crc16_ccitt(postBin, postSize);
  postBin[postSize++] = crc & 0xFF;     // LSB
  postBin[postSize++] = crc >> 8;       // MSB
}

bool handleATstate(const char* cmd,
                   SendLogState onSuccess,
                   SendLogState onFail,
                   const char* errorWord,
                   //const char* state,
                   const char* expected = "OK",
                   uint16_t timeout = 2000) {
  if (!at.busy && !at.done) {
    startAT(cmd, expected, timeout);
  }
  updateAT();
  if (at.done) {
    if (!at.result) {
      DBG(F("Modem: Err "));
      DBGL(errorWord);
      retryCount++;
      if (retryCount > MAX_RETRY) {
        retryCount = 0;
        logState = INIT;
        modemInitStart();
      } else {
        logState = onFail;
      }
    } else {
      logState = onSuccess;
      //DBGL(state);
    }
    at.done = false;
    at.result = false;
    return true;
  }
  return false; 
}

void startSendLog() {
  logState = INIT;
  logNow = rtc.now();
  retryCount = 0;
  stopModemWatchdog = true;
  logPart = 0;
}

void updateSendLog() {
  switch (logState) {
    case IDLE:
      return;
    case INIT:
      buildBinaryPayload(logPart);
  if (logPart == 0) {
    handleATstate("AT+HTTPINIT", SEND_PARAMS, INIT, "Init");
  } else {
    logState = SEND_PARAMS;  // pomiń INIT jeśli już zrobione
  }
      break;
    case SEND_PARAMS:
      sendAT("AT+HTTPPARA=\"CID\",1");
      sendAT("AT+HTTPPARA=\"URL\",\"http://ls1.cyra.com.pl/logger.php\"");
      sendAT("AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\"");
      logState = SEND_DATA_CMD;
      //DBGL(F("SEND_PARAMS"));
      break;
    case SEND_DATA_CMD:
      char cmd[24];
      sprintf(cmd, "AT+HTTPDATA=%u,5000", postSize);
      handleATstate(cmd, SEND_DATA_BODY, SEND_PARAMS, "Data","DOWNLOAD");
      break;
    case SEND_DATA_BODY:
      /*for (size_t i = 0; i < postSize; i++) {
  if (postBin[i] < 0x10) DBG("0");
  DBG(postBin[i], HEX);
  DBG(" ");
}
DBGL();*/
      while (Serial1.available()) Serial1.read();
      Serial1.write((const uint8_t*)postBin, postSize);
      Serial1.flush();
      wait = (uint16_t)(millis() & 0xFFFF);
      logState = WAIT;
      //DBGL(F("SEND_DATA_BODY"));
      break;
    case WAIT:
      if ((uint16_t)(millis() - wait) > 500) {
        logState = HTTP_ACTION;
        //DBGL(F("WAIT"));
        }
      break;
    case HTTP_ACTION:
      handleATstate("AT+HTTPACTION=1", PART2, SEND_PARAMS, "Action", "+HTTPACTION: 1,200", 3000);
      break;
    case PART2:
      logPart++;
      DBGL(F("SQL: Logi zapisane."));
  if (logPart < 2) {
    logState = INIT;  // druga porcja
  } else {
    logState = DONE;  // koniec
  }
      break;
    case DONE:
      sendAT("AT+HTTPTERM");
      logState = IDLE;
      stopModemWatchdog = false;
      //DBGL(F("DONE"));
      break;
  }
}

/*
void syncTimeFromAPI() {
  sendAT("AT+HTTPINIT");
  sendAT("AT+HTTPPARA=\"CID\",\"1\"");
  sendAT("AT+HTTPPARA=\"URL\",\"http://ls1.cyra.com.pl/time.php\"");
  sendAT("AT+HTTPACTION=0",200);
  String buffer;
  buffer = sendATread("AT+HTTPREAD");
  sendAT("AT+HTTPTERM", 200);
int idx = buffer.indexOf(" ");
if (idx > 0) {
  int start = buffer.indexOf("20");
  int end = buffer.indexOf(" OK", start);
  if (start > 0 && end > start) {
    buffer = buffer.substring(start, end);
    buffer.trim();
  }
}

  if (buffer.length() < 19) {
    DBG(F("Too short data: "));
    DBGL(buffer);
    return;
  }

  int y  = buffer.substring(0, 4).toInt();
  int m  = buffer.substring(5, 7).toInt();
  int d  = buffer.substring(8, 10).toInt();
  int h  = buffer.substring(11, 13).toInt();
  int mi = buffer.substring(14, 16).toInt();
  int s  = buffer.substring(17, 19).toInt();

  if (y > 2020 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
    DateTime compiled(DateTime(y, m, d, h, mi, s));
    EEPROM.update(EEPROM_DST_FLAG_ADDR, czyLetni(d, m, compiled.dayOfTheWeek()) ? 1 : 0);
    rtc.adjust(compiled);
    DBG(F("RTC ustawiony: "));
    DBGL(compiled.timestamp());
  } else {
    DBGL(F("Nieprawidłowe dane czasu z API"));
  }

}
*/