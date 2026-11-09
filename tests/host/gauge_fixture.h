#pragma once
#include "Arduino.h"
#define private public
#include "../../libraries/J1850-VPW-WC/j1850vpw-wc.cpp"
#undef private
#ifndef DBG
#define DBG(...) ((void)0)
#define DBGL(...) ((void)0)
#endif
inline long random(long a, long) { return a; }
uint8_t pause = 75;
uint16_t counter = 0;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#include "../../ls1gauge-mega/cmd.h"
#undef min
#undef max
