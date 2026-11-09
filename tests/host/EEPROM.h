#pragma once
#include <stdint.h>
#include <stdexcept>
inline int fakeEepromWriteBudget = -1;
inline bool fakeEepromReady = true;
inline bool eeprom_is_ready(){return fakeEepromReady;}
struct FakeEEPROM {
  uint8_t bytes[4096]; unsigned writes=0, blocked=0;
  FakeEEPROM(){reset();}
  void reset(){memset(bytes,255,sizeof(bytes));writes=blocked=0;fakeEepromReady=true;}
  uint8_t read(int address){if(!fakeEepromReady)++blocked;return bytes[address];}
  void update(int address,uint8_t value){if(!fakeEepromReady)++blocked;if(bytes[address]!=value){if(fakeEepromWriteBudget==0)throw std::runtime_error("power loss");if(fakeEepromWriteBudget>0)--fakeEepromWriteBudget;bytes[address]=value;++writes;fakeEepromReady=false;}}
} inline EEPROM;
