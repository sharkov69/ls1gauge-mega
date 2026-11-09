#pragma once
// Compile support in; runtime DEBUG ON/OFF commands persist the state in EEPROM.
// Set to 0 only to remove diagnostic code entirely (USB commands unavailable).
#ifndef DEBUG_MODEM
#define DEBUG_MODEM 1
#endif
#define EEPROM_MODEM_DEBUG_ADDR 18
