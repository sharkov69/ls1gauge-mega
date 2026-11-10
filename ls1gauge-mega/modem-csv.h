#pragma once
#include "csv_upload.h"
#include "csv_upload_sd.h"

// Native HTTP owns the handshake. Qualification must cover CA AND hostname rejection.
enum CsvHttpState : uint8_t { CSV_IDLE, CSV_SETUP, CSV_PREPARE, CSV_DATA,
                             CSV_BODY, CSV_BODY_OK, CSV_ACTION, CSV_READ, CSV_TERM };
CsvHttpState csvHttpState = CSV_IDLE;
CsvUpload csvUploader;
uint8_t csvSetupStep = 0, csvAckCount = 0;
uint8_t csvBurstCount = 0;
uint8_t csvAck[CsvUpload::ACK_SIZE];
uint16_t csvBodySize = 0, csvBodyOffset = 0;
uint32_t csvBodySince = 0, csvRetryAfter = 0;
bool csvSessionConfigured = false, csvFileStarted = false, csvReadHeader = false;
bool csvReadInitialOk = false;
bool csvReleaseAfterTerm = false;
uint16_t logUploadErrors = 0;

bool modemHttpPending() { return csvHttpState != CSV_IDLE; }
bool csvFastService() {
	return modemOwner == OWNER_HTTP &&
	       (csvHttpState == CSV_READ || csvHttpState == CSV_BODY || csvHttpState == CSV_BODY_OK);
}
void cancelLogSend() {
	csvUploader.retry();
	csvHttpState = CSV_IDLE;
	csvSessionConfigured = csvReadHeader = csvReleaseAfterTerm = false;
	csvAckCount = 0;
	csvRetryAfter = millis() + 30000UL;
	++logUploadErrors;
}

// No sscanf width ambiguity or accepted suffix on modem-controlled lengths.
bool csvNumber(const char *&p, uint32_t &value, char delimiter) {
	while (*p == ' ') ++p;
	value = 0;
	if (*p < '0' || *p > '9') return false;
	while (*p >= '0' && *p <= '9') {
		const uint8_t digit = (uint8_t)(*p++ - '0');
		if (value > (UINT32_MAX - digit) / 10) return false;
		value = value * 10 + digit;
	}
	if (*p != delimiter) return false;
	if (delimiter) ++p;
	return true;
}
bool csvAtByte(uint8_t c) {
	if (csvHttpState != CSV_READ || !csvReadHeader || csvAckCount >= sizeof(csvAck))
		return false;
	csvAck[csvAckCount++] = c;
	return true;
}
bool csvAcceptAck() {
	return csvQueueReady && !memcmp(csvUploader.data() + 36, csvQueueId, 16) &&
	       csvReadHeader && csvAckCount == sizeof(csvAck) &&
	       csvUploader.acknowledge(csvTlsQualified && csvSessionConfigured,
	                              200, csvAck, sizeof(csvAck));
}
void csvFinishRead() {
	bool accepted = csvAcceptAck();
	if (!accepted && csvUploader.status() == CsvUpload::REMOTE_AHEAD) {
		csvReleaseAfterTerm = true;
		++logUploadErrors;
		accepted = true; // The exchange completed; quarantine this file until the next queue scan.
	}
	finishAT(accepted);
}
bool csvAtLine(const char *line) {
	if (modemOwner != OWNER_HTTP) return false;
	if (strncmp_P(line, PSTR("+HTTPACTION:"), 12) == 0) {
		if (csvHttpState == CSV_ACTION) {
			const char *p = line + 12;
			uint32_t method, status, length;
			finishAT(csvNumber(p, method, ',') && csvNumber(p, status, ',') &&
			         csvNumber(p, length, 0) && method == 1 && status == 200 &&
			         length == sizeof(csvAck));
		}
		return true;
	}
	if (csvHttpState != CSV_READ) return false;
	if (strncmp_P(line, PSTR("+HTTPREAD:"), 10) == 0) {
		const char *p = line + 10;
		while (*p == ' ') ++p;
		// LE20CB03 prefixes the binary payload length with DATA,.
		if (!csvReadHeader && strncmp_P(p, PSTR("DATA,"), 5) == 0) p += 5;
		uint32_t length;
		if (!csvNumber(p, length, 0))
			finishAT(false);
		else if (csvReadHeader) {
			if (length == 0) csvFinishRead();
			else finishAT(false);
		}
		else if (length == sizeof(csvAck)) csvReadHeader = true;
		else finishAT(false);
		return true;
	}
	if (strcmp_P(line, PSTR("OK")) == 0) {
		if (!csvReadHeader) csvReadInitialOk = true;
		else if (!csvReadInitialOk) csvFinishRead();
		return true;
	}
	return false;
}

const char *csvSetupCommand(uint8_t step) {
	switch (step) {
	case 0: return PSTR("AT+HTTPTERM");
	case 1: return PSTR("AT+HTTPINIT");
	case 2: return PSTR("AT+HTTPPARA=\"CID\",1");
	case 3: return PSTR("AT+CSSLCFG=\"sslversion\",0,3");
	case 4: return PSTR("AT+CSSLCFG=\"authmode\",0,1");
	case 5: return PSTR("AT+CSSLCFG=\"ignorelocaltime\",0,0");
	case 6: return PSTR("AT+CSSLCFG=\"enableSNI\",0,1");
	case 7: return PSTR("AT+CSSLCFG=\"cacert\",0,\"p59-ca.pem\"");
	case 8: return PSTR("AT+HTTPPARA=\"SSLCFG\",0");
	case 9: return PSTR("AT+HTTPPARA=\"URL\",\"https://camaro.it-tiger.pl/v1/chunks\"");
	case 10: return PSTR("AT+HTTPPARA=\"CONTENT\",\"application/octet-stream\"");
	case 11: return PSTR("AT+HTTPPARA=\"CONNECTTO\",20");
	case 12: return PSTR("AT+HTTPPARA=\"RECVTO\",10");
	default: return PSTR("AT+CSSLCFG=\"negotiatetime\",0,20");
	}
}
void csvHttpStep(const char *command, CsvHttpState next,
                 const char *expected = AT_OK, uint32_t timeout = 2000) {
	if (at.done) {
		const bool success = at.result;
		at.done = false;
		if (success) csvHttpState = next;
		else recoverModem();
	} else if (!at.busy) beginAT(command, expected, timeout);
}
void csvUploadScheduler() {
	if (!csvQueueReady || (csvFileStarted && memcmp(csvUploader.data() + 36, csvQueueId, 16)))
		csvFileStarted = false;
	if (!csvProvisioned || !csvTlsQualified || !csvQueueReady ||
	    !modemOnline || modemOwner != OWNER_IDLE || at.busy || at.done ||
	    modemHttpPending() || modemTimeDue() || (int32_t)(millis() - csvRetryAfter) < 0) return;
	if (!csvFileStarted) {
		if (!csvUploader.begin(csvDeviceSecret, csvQueueId)) return;
		csvFileStarted = true;
	}
	csvSetupStep = 1; // Recovery/init owns cleanup; completed requests already terminated HTTP.
	csvBurstCount = 0;
	csvSessionConfigured = false;
	csvHttpState = CSV_SETUP;
}
void modemHttpTick() {
	debugStage(40 + (uint8_t)csvHttpState);
	if (modemHttpPending() && (!csvQueueReady || memcmp(csvUploader.data() + 36, csvQueueId, 16))) {
		csvFileStarted = false;
		recoverModem();
		return;
	}
	switch (csvHttpState) {
	case CSV_IDLE: modemOwner = OWNER_IDLE; break;
	case CSV_SETUP:
		if (at.done && at.result) {
			at.done = false;
			if (++csvSetupStep == 14) {
				csvSessionConfigured = true;
				csvHttpState = CSV_PREPARE;
			}
		} else csvHttpStep(csvSetupCommand(csvSetupStep), CSV_SETUP, AT_OK,
		                   csvSetupStep == 1 ? 120000UL : 2000UL);
		break;
	case CSV_PREPARE: {
		CsvUploadSdSource source(SD, csvQueueName);
		csvBodySize = csvUploader.prepare(csvTlsQualified && csvSessionConfigured,
		                                 csvQueueLimit(), CsvUploadSdSource::read, &source);
		if (csvBodySize) csvHttpState = CSV_DATA;
		else {
			csvReleaseAfterTerm = csvUploader.status() == CsvUpload::AT_END ||
			                      csvUploader.status() == CsvUpload::REMOTE_AHEAD ||
			                      csvUploader.status() == CsvUpload::READ_ERROR;
			if (!csvReleaseAfterTerm) csvRetryAfter = millis() + 30000UL;
			csvHttpState = CSV_TERM;
		}
		break;
	}
	case CSV_DATA:
		if (!at.busy && !at.done)
			snprintf_P(at.command, sizeof(at.command), PSTR("AT+HTTPDATA=%u,5000"), csvBodySize);
		csvHttpStep(nullptr, CSV_BODY, PSTR("DOWNLOAD"));
		if (csvHttpState == CSV_BODY) {
			csvBodyOffset = 0;
			csvBodySince = millis();
#if DEBUG_MODEM
			modemDebugMuted = true;
			modemDebugLength = 0;
#endif
		}
		break;
	case CSV_BODY: {
		CsvUploadSdSource source(SD, csvQueueName);
		const uint8_t *bytes;
		uint16_t available = csvUploader.bodyBlock(csvBodyOffset, CsvUploadSdSource::read,
		                                          &source, bytes);
		if (!available) {
			recoverModem();
			break;
		}
		uint8_t budget = MODEM_BUDGET;
		while (budget-- && available && Serial1.availableForWrite() > 0) {
			Serial1.write(*bytes++);
			--available;
			++csvBodyOffset;
		}
		if (csvBodyOffset == csvBodySize) {
			// Arm reception before the peer can acknowledge the last byte.
			at.expected = AT_OK;
			at.timeout = 2000;
			at.since = millis();
			at.length = 0;
			at.overflow = at.matched = at.ok = false;
			at.phase = 2;
			at.busy = true;
			csvHttpState = CSV_BODY_OK;
		} else if ((uint32_t)(millis() - csvBodySince) >= 4500) recoverModem();
		break;
	}
	case CSV_BODY_OK:
		csvHttpStep(nullptr, CSV_ACTION);
		break;
	case CSV_ACTION:
		csvHttpStep(PSTR("AT+HTTPACTION=1"), CSV_READ, PSTR("+HTTPACTION:"), 60000);
		if (csvHttpState == CSV_READ) { csvReadHeader = csvReadInitialOk = false; csvAckCount = 0; }
		break;
	case CSV_READ:
		if (at.done && at.result) {
			at.done = false;
			csvHttpState = csvReleaseAfterTerm || ++csvBurstCount >= 16 ? CSV_TERM : CSV_PREPARE;
		} else csvHttpStep(PSTR("AT+HTTPREAD=0,24"), CSV_PREPARE, PSTR("+CSVREADCOMPLETE"), 5000);
		break;
	case CSV_TERM:
		csvHttpStep(PSTR("AT+HTTPTERM"), CSV_IDLE);
		if (csvHttpState == CSV_IDLE) {
			csvSessionConfigured = false;
			modemOwner = OWNER_IDLE;
			if (!csvReleaseAfterTerm && csvUploader.status() == CsvUpload::READY)
				csvRetryAfter = millis() + 1000UL; // Let pending GPS/network work acquire the UART.
			if (csvReleaseAfterTerm) {
				csvQueueRelease();
				csvFileStarted = csvReleaseAfterTerm = false;
			}
#if DEBUG_MODEM
			modemDebugMuted = false;
			modemDebugLength = 0;
#endif
		}
		break;
	}
}
