#pragma once
#include <EEPROM.h>
#ifdef __AVR__
#include <avr/eeprom.h>
#endif
bool modemDebugEnabled = false;
uint8_t modemDebugLength = 0;
bool modemDebugReplyPending = false, modemDebugLoadPending = false, modemDebugSaveError = false;
uint8_t modemDebugSavePhase = 0; // 0 idle, 1 request, 2 verify after hardware completes.
void modemDebugLoad() {
  modemDebugLoadPending = !eeprom_is_ready();
  if (!modemDebugLoadPending) modemDebugEnabled = EEPROM.read(EEPROM_MODEM_DEBUG_ADDR) == 0xa5;
}
bool modemDebugCommand(const char* line) {
  if (strcmp_P(line, PSTR("DEBUG ON")) == 0 || strcmp_P(line, PSTR("DEBUG OFF")) == 0) {
    modemDebugEnabled = line[7] == 'N';
    modemDebugLength = 0;
    modemDebugLoadPending = false;
    modemDebugSavePhase = 1; modemDebugSaveError = false;
  } else if (strcmp_P(line, PSTR("DEBUG STATUS")) != 0) return false;
  modemDebugReplyPending = true;
  return true;
}
void modemDebugControlTick() {
  if (modemDebugLoadPending) modemDebugLoad();
  if (modemDebugSavePhase) {
    if (!eeprom_is_ready()) return;
    const uint8_t target = modemDebugEnabled ? 0xa5 : 0x5a;
    const uint8_t saved = EEPROM.read(EEPROM_MODEM_DEBUG_ADDR);
    if (saved != target && modemDebugSavePhase == 1) {
      // Ready checked above. AVR EEPROM.update launches this one-byte write;
      // it does not wait for completion. No ISR in this project writes EEPROM.
      EEPROM.update(EEPROM_MODEM_DEBUG_ADDR, target);
      modemDebugSavePhase = 2; return;
    }
    modemDebugSaveError = saved != target;
    modemDebugSavePhase = 0;
  }
  if (!modemDebugReplyPending || modemDebugLoadPending) return;
  const char* response = modemDebugSaveError ? PSTR("DEBUG SAVE ERROR\n") :
                         modemDebugEnabled ? PSTR("DEBUG ON SAVED\n") : PSTR("DEBUG OFF SAVED\n");
  const uint8_t length = strlen_P(response);
  if (Serial.availableForWrite() < length) return;
  char reply[18]; memcpy_P(reply, response, length);
  Serial.write((const uint8_t*)reply, length);
  modemDebugReplyPending = false;
}
