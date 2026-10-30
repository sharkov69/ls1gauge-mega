#pragma once
// One immutable half-buffer per HTTP request, same binary format as before.
#define LOG_PART (LOG_ENTRIES / 2)
#define POSTBIN_SIZE (TOKEN_LENGTH + 3 + sizeof(LogEntry) * LOG_PART + 2)
uint8_t postBin[POSTBIN_SIZE];
uint16_t postSize = 0, postOffset = 0;
uint8_t logPart = 0, retryCount = 0;
uint32_t postSince = 0;
uint16_t uploadGeneration = 0;
DateTime logNow;
enum SendLogState : uint8_t { IDLE, INIT, SEND_CID, SEND_URL, SEND_CONTENT, SEND_DATA_CMD, SEND_DATA_BODY, WAIT_BODY, HTTP_ACTION, PART2, DONE };
SendLogState logState = IDLE;
uint16_t logUploadErrors = 0, skippedUploads = 0;

uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t value = 0xFFFF) {
  while (len--) {
    value ^= (uint16_t)*data++ << 8;
    for (uint8_t i=0;i<8;++i) value = (value & 0x8000) ? (uint16_t)((value << 1) ^ 0x1021) : (uint16_t)(value << 1);
  }
  return value;
}
void buildBinaryPayload(uint8_t part) {
  postSize = 0;
  memcpy(postBin, token, TOKEN_LENGTH); postSize += TOKEN_LENGTH;
  uint16_t year = logNow.year();
  postBin[postSize++] = (uint8_t)year; postBin[postSize++] = (uint8_t)(year >> 8);
  postBin[postSize++] = logNow.month();
  memcpy(postBin + postSize, &logBuffer[part * LOG_PART], sizeof(LogEntry) * LOG_PART);
  postSize += sizeof(LogEntry) * LOG_PART;
  uint16_t value = crc16_ccitt(postBin, postSize);
  postBin[postSize++] = (uint8_t)value; postBin[postSize++] = (uint8_t)(value >> 8);
}
bool modemHttpPending() { return logState != IDLE; }
void cancelLogSend() { logState = IDLE; ++logUploadErrors; }
void startSendLog() {
  if (logState != IDLE) { ++skippedUploads; return; }
  uploadGeneration = logGeneration; logNow = rtc.now();
  logPart = retryCount = 0;
  buildBinaryPayload(0); // Snapshot before logBuffer[0] can be reused.
  logState = INIT;
}
void httpStep(const char* command, SendLogState next, const char* expected = AT_OK, uint16_t timeout = 2000) {
  if (at.done) {
    bool success = at.result; at.done = false;
    if (success) { retryCount = 0; logState = next; }
    // Retrying an uncertain HTTP transaction can duplicate POST or inject a
    // command into HTTPDATA. Abandon this upload and recover UART ownership.
    else { recoverModem(); }
  } else if (!at.busy) beginAT(command, expected, timeout);
}
void modemHttpTick() {
  switch (logState) {
    case IDLE: modemOwner = OWNER_IDLE; return;
    case INIT: httpStep(PSTR("AT+HTTPINIT"), SEND_CID); break;
    case SEND_CID: httpStep(PSTR("AT+HTTPPARA=\"CID\",1"), SEND_URL); break;
    case SEND_URL: httpStep(LOGGER_URL, SEND_CONTENT); break;
    case SEND_CONTENT: httpStep(PSTR("AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\""), SEND_DATA_CMD); break;
    case SEND_DATA_CMD:
      if (!at.busy && !at.done) snprintf_P(at.command, sizeof(at.command), PSTR("AT+HTTPDATA=%u,5000"), postSize);
      httpStep(nullptr, SEND_DATA_BODY, PSTR("DOWNLOAD"));
      if (logState == SEND_DATA_BODY) { postOffset = 0; postSince = millis(); }
      break;
    case SEND_DATA_BODY: {
      // Do not write more than fits in the UART ring. Never flush synchronously.
      uint8_t budget = MODEM_BUDGET;
      while (budget-- && postOffset < postSize && Serial1.availableForWrite() > 0) Serial1.write(postBin[postOffset++]);
      if (postOffset == postSize) { postSince = millis(); logState = WAIT_BODY; }
      else if ((uint32_t)(millis() - postSince) >= 4500) { recoverModem(); }
      break;
    }
    case WAIT_BODY:
      // Consume the modem's upload acknowledgement before issuing HTTPACTION.
      if (!at.busy && !at.done) {
        at.expected = PSTR("OK"); at.timeout = 1500; at.since = millis();
        at.length = 0; at.overflow = at.matched = at.ok = false;
        at.phase = 2; at.busy = true;
      }
      if (at.done) {
        bool success = at.result; at.done = false;
        if (success) logState = HTTP_ACTION;
        else { recoverModem(); }
      }
      break;
    case HTTP_ACTION: httpStep(PSTR("AT+HTTPACTION=1"), PART2, PSTR("+HTTPACTION: 1,200"), 3000); break;
    case PART2:
      if (logPart == 0) {
        // No extra 1260-byte buffer: reject stale second half instead of mixing two minutes.
        if (uploadGeneration != logGeneration || logIndex > LOG_PART) { ++logUploadErrors; logState = DONE; break; }
        logPart = 1; buildBinaryPayload(1); logState = SEND_CID;
      } else logState = DONE;
      break;
    case DONE:
      httpStep(PSTR("AT+HTTPTERM"), IDLE);
      if (logState == IDLE) modemOwner = OWNER_IDLE;
      break;
  }
}
