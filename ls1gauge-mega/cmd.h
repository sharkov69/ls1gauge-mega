#define TX 5
#define RX 3
#ifdef DEBUGVPW
void handleError(J1850_Operations op, J1850_ERRORS err);
#endif
J1850VPW vpw;

/*
SEND
byte 0 - priority
0x68 01101000
0x6C 01101100
Bits 7,6 and 5 are priority 0=High, 7=Low
Bit 4 is header style (0=3 byte header-GM)
Bit 3 is In Frame Response (1=Not allowed-GM)
Bit 2 is addressing mode (0=Functional)
Bit 1,0 is message type: (depending on bit 2 and 3 see below)

Bit 3 2 1 0
-----------
Functional
1 0 0 0 Function
Physical
1 1 0 0 Node to Node

byte2 - destination
0x10 - PCM (physical)
0x6A - Request Legislated Diagnostics (functional)

byte3 - source
0xF1 - Off-board scan tool

byte4 - PCM mode requests
0x01 - Request Current Powertrain Diagnostic Data (functional)
0x22 - Request Diagnostic Data by PID (physical) 

//READ
byte1 0x48 - Report Control of On-Board System, Test, or Component 
byte2 0x6B -  Report Legislated Dianostics 
*/

static uint8_t cmd_trans[7] = {0x6C, 0x10, 0xF1, 0x22, 0x19, 0x40, 0x01}; 
static uint8_t cmd_water[5] = {0x68, 0x6A, 0xF1, 0x01, 0x05};
static uint8_t cmd_air[5]   = {0x68, 0x6A, 0xF1, 0x01, 0x0F};
static uint8_t cmd_rpm[5]   = {0x68, 0x6A, 0xF1, 0x01, 0x0C};
static uint8_t cmd_speed[5] = {0x68, 0x6A, 0xF1, 0x01, 0x0D};
static uint8_t cmd_tps[5]   = {0x68, 0x6A, 0xF1, 0x01, 0x11};
//static uint8_t cmd_o2[5]    = {0x68, 0x6A, 0xF1, 0x01, 0x14};
//static uint8_t cmd_o22[5]   = {0x68, 0x6A, 0xF1, 0x01, 0x18};
static uint8_t cmd_o2[7] = {0x6C, 0x10, 0xF1, 0x22, 0x11, 0x4B, 0x01}; 
static uint8_t cmd_o22[7] = {0x6C, 0x10, 0xF1, 0x22, 0x11, 0x4E, 0x01}; 

//static uint8_t cmd_spark[5] = {0x68, 0x6A, 0xF1, 0x01, 0x0E};
static uint8_t cmd_spark[7] = {0x6C, 0x10, 0xF1, 0x22, 0x11, 0x5C, 0x01}; //oil press
static uint8_t cmd_batt[7]  = {0x6C, 0x10, 0xF1, 0x22, 0x11, 0x41, 0x01};
static uint8_t cmd_map[5]   = {0x68, 0x6A, 0xF1, 0x01, 0x0B};

struct Cmd {
  const uint8_t* data;
  uint8_t dl;
  const uint16_t wait;
  uint16_t lastCzas;
  volatile int16_t wartosc;
  int16_t lastValue;
  Cmd(const uint8_t* d, uint8_t l)
    : data(d), dl(l), wait(500), lastCzas(0xFFFF), wartosc(-32768), lastValue(-32768) {}
  Cmd(const uint8_t* d, uint8_t l, uint16_t w)
    : data(d), dl(l), wait(w), lastCzas(0xFFFF), wartosc(-32768), lastValue(-32768) {}
};
Cmd cmd[] = {
  { cmd_air,    sizeof(cmd_air),   5000 },
  { cmd_map,    sizeof(cmd_map)         },
  { cmd_tps,    sizeof(cmd_tps),    250 },
  { cmd_batt,   sizeof(cmd_batt),  5000 },
  { cmd_o22,    sizeof(cmd_o22)         },
  { cmd_speed,  sizeof(cmd_speed)       },
  { cmd_trans,  sizeof(cmd_trans), 5000 },
  { cmd_spark,  sizeof(cmd_spark), 2500 },
  { cmd_rpm,    sizeof(cmd_rpm),    250 },
  { cmd_water,  sizeof(cmd_water), 5000 },
  { cmd_o2,     sizeof(cmd_o2)          },
  { nullptr,    0, }  ,
  { nullptr,    0, }  ,
  { nullptr,    0, }  ,
  { nullptr,    0, }  ,
  { nullptr,    0, }  ,
  { nullptr,    0, }  
};

enum {
  AIR,      // 0
  MAP,      // 1
  TPS,      // 2
  BATT,     // 3
  O22,      // 4
  SPEED,    // 5
  TRANS,    // 6
  SPARK,    // 7
  RPM,      // 8
  WATER,    // 9
  O2,       // 10
  TEMP_AC,  //11
  HUM,      //12
  TEMP_OUT, //13
  TEMP_IN,  //14
  LAT,      //15
  LON,      //16
  CMD_SIZE  // 17
};

//const uint8_t cmdSize = sizeof(cmd) / sizeof(cmd[0]);
uint16_t czas = 0;
volatile uint16_t lastRxEnd = 0; 


#define O2SREDNIA 10
struct objO2 {
  int16_t o2[O2SREDNIA];
  uint8_t id;
  bool ready = false;
};

#define O2SENSORS 2
objO2 o2[O2SENSORS] = {};

enum { O2_1 = 0, O2_2 = 1 };

int16_t updateO2(objO2 &o2, int16_t v) {
  if (v < 1029 || v > 1911) v = 1475;
  if (!o2.ready) {
    for (uint8_t i = 0; i < O2SREDNIA; i++) o2.o2[i] = v;
    o2.id = 0;
    o2.ready = true;
  } else {
    if (o2.id >= O2SREDNIA) o2.id = 0;
    o2.o2[o2.id] = v;
    o2.id++;
  }

  int16_t sum = 0;
  for (uint8_t i = 0; i < O2SREDNIA; i++) sum += o2.o2[i];
  return sum / O2SREDNIA;
}

void send(){
  static uint16_t lastSend = 0;
  const uint16_t guardRx = 10;
  uint8_t id = 0xFF;
  int32_t max = -32000;

  for (uint8_t i = 0; i < CMD_SIZE; i++) {
    if (cmd[i].dl < 5) continue;
    if (cmd[i].lastCzas == 0xFFFF || (uint16_t)(czas - cmd[i].lastCzas) > 0x8000) {
      cmd[i].lastCzas = czas - (uint16_t)((i * pause) % cmd[i].wait);
    }
    uint16_t diff = czas - cmd[i].lastCzas;
    if (diff >= cmd[i].wait) {
      int32_t s = (int32_t)diff - cmd[i].wait;
      if (s > max) { max = s; id = i; }
    }
  }
  if (id != 0xFF && (uint16_t)(czas - lastSend) >= pause && (uint16_t)(czas - lastRxEnd) >= guardRx) {
    vpw.send((uint8_t*)cmd[id].data, cmd[id].dl, 10);
    counter2++;  
    #ifdef TXRX
    DBG(F("TX: "));
    for (uint8_t j = 0; j < cmd[id].dl; j++) {
      if (cmd[id].data[j] < 0x10) DBG("0");
      DBG(cmd[id].data[j], HEX);
      DBG(" ");
    }
    DBGL();
    #endif
    czas = (uint16_t)(millis() & 0xFFFF);
    cmd[id].lastCzas = czas;
    lastSend = czas;
  }
}

void read(){
    static uint8_t buff[BS];
  uint8_t dataSize;
  while (dataSize = vpw.tryGetReceivedFrame(buff)) {
  uint8_t *pData = buff;
  if (dataSize >= 6) {  
    counter++;  
    #ifdef TXRX     
    DBG(F("RX:"));
    for (uint8_t i = 0; i < dataSize; i++) {
      DBG(F(" "));
      if (pData[i] < 0x10) DBG("0");
      DBG(pData[i], HEX);
    }
    DBGL(); 
    #endif 
    uint8_t pid = pData[4];
      switch (pid) {
        case 0x0C: { //rpm
          if (dataSize >= 7) {
              cmd[RPM].wartosc = ((uint16_t)pData[5] << 8 | pData[6]) / 4;
              #ifdef TXRX 
              DBG(F("RPM: ")); DBGL(cmd[RPM].wartosc);
              #endif 
              }
              break;
            }
        case 0x0D: {
              cmd[SPEED].wartosc = pData[5];
              #ifdef TXRX
              DBG(F("Speed: ")); DBG(cmd[SPEED].wartosc); DBGL(F(" km/h"));
              #endif 
          break;
        }
        case 0x05: {
            cmd[WATER].wartosc = pData[5] - 40;
            #ifdef TXRX 
            DBG(F("Water Temp: ")); DBG(cmd[WATER].wartosc); DBGL(F(" *C"));
            #endif 
          break;
        }
        case 0x0F: {
            cmd[AIR].wartosc = pData[5] - 40;
            #ifdef TXRX 
            DBG(F("Air Temp: ")); DBG(cmd[AIR].wartosc); DBGL(F(" *C"));
            #endif 
          break;
        }
        /*case 0x14: {
            int16_t v = pData[5]*5;
            if (v == 0) v = cmd[O2].lastValue;
            v = updateO2(o2[O2_1], v);
            cmd[O2].wartosc = v;
            cmd[O2].lastValue = v;
            #ifdef TXRX 
            DBG(F("O2: ")); DBG(cmd[O2].wartosc); DBGL(F(" mV"));
            #endif 
          break;
        }
        case 0x18: {
            int16_t v = pData[5]*5;
            if (v == 0) v = cmd[O22].lastValue;
            v = updateO2(o2[O2_2], v);
            cmd[O22].wartosc = v;
            cmd[O22].lastValue = v;
            #ifdef TXRX
            DBG(F("O2 2: ")); DBG(cmd[O22].wartosc); DBGL(F(" mV"));
            #endif 
          break;
        }*/
        case 0x11: {
            if (pData[3] == 0x41) {
                cmd[TPS].wartosc = pData[5] * 100 / 255;
                #ifdef TXRX 
                DBG(F("TPS: ")); DBG(cmd[TPS].wartosc); DBGL(F(" %"));
                #endif 

            }  else if (pData[3] == 0x62 && dataSize >= 7) {

              switch (pData[5]) {
            case 0x41: {
              cmd[BATT].wartosc = pData[6];
              #ifdef TXRX 
              DBG(F("Battery: ")); DBG(cmd[BATT].wartosc/10.0, 1); DBGL(F(" V"));
              #endif
              break; 
            } 
            case 0x5C: {
              int16_t v = (int16_t)(((uint16_t)pData[6] * 111) >> 8) - 10;
              if (v < 0) v = 0;

              cmd[SPARK].wartosc = v;
              #ifdef TXRX 
              DBG(F("Oil pressure: ")); DBG(cmd[SPARK].wartosc/10.0, 1); DBGL(F(" bar"));
              #endif
              break; 
            } 
            case 0x4B: {
              int16_t v = 1029 + 3 * pData[6] + (((uint16_t)pData[6] * 117 + 128) >> 8);
              v = updateO2(o2[O2_1], v);
              cmd[O2].wartosc = v;
              #ifdef TXRX 
              DBG(F("O2: ")); DBG(cmd[O2].wartosc/100.0, 2); DBGL(F(" AFR"));
              #endif 
              break;
            }
            case 0x4E: {
              int16_t v = 1029 + 3 * pData[6] + (((uint16_t)pData[6] * 117 + 128) >> 8);
              v = updateO2(o2[O2_2], v);
              cmd[O22].wartosc = v;
              #ifdef TXRX 
              DBG(F("O2 2: ")); DBG(cmd[O22].wartosc/100.0, 2); DBGL(F(" AFR"));
              #endif 
              break;
            }
              }
            }
          break;
        }
        /*case 0x0E: {
            cmd[SPARK].wartosc = (pData[5] / 2) - 64;
            #ifdef TXRX
            DBG(F("Timing: ")); DBG(cmd[SPARK].wartosc); DBGL(F(" *"));
            #endif 
          break;
        }*/
        case 0x0B: {
            cmd[MAP].wartosc = pData[5];
            #ifdef TXRX 
            DBG(F("MAP: ")); DBG(cmd[MAP].wartosc/100.0,1); DBGL(F(" bar"));
            #endif 
          break;
        }
        case 0x19: {
          if (pData[5] == 0x40 && dataSize >= 7) {
                cmd[TRANS].wartosc = pData[6]-40;
                #ifdef TXRX
                DBG(F("Trans Temp: ")); DBG(cmd[TRANS].wartosc); DBGL(F(" *C"));
                #endif 
          }
          break;
        }
        default:
          break;
      }
  lastRxEnd = (uint16_t)(millis() & 0xFFFF);
	}
  }

}

#ifdef DEBUGVPW
void handleError(J1850_Operations op, J1850_ERRORS err)
{
  if (err == J1850_OK) return;

  //registerError();
  const char* prefix = (op == J1850_Read) ? "READ " : "WRITE ";
  char buf[40];

  switch (err) {
    case J1850_ERR_BUS_IS_BUSY:
      sprintf(buf, "%sJ1850_ERR_BUS_IS_BUSY", prefix);
      break;
    case J1850_ERR_BUS_ERROR:
      sprintf(buf, "%sJ1850_ERR_BUS_ERROR", prefix);
      break;
    case J1850_ERR_PULSE_TOO_SHORT:
      sprintf(buf, "%sJ1850_ERR_PULSE_TOO_SHORT", prefix);
      break;
    case J1850_ERR_ARBITRATION_LOST:
      sprintf(buf, "%sJ1850_ERR_ARBITRATION_LOST", prefix);
      break;
    case J1850_ERR_PULSE_TOO_LONG:
      sprintf(buf, "%sJ1850_ERR_PULSE_TOO_LONG", prefix);
      break;
    case J1850_ERR_IFR_RX_NOT_SUPPORTED:
      sprintf(buf, "%sJ1850_ERR_IFR_RX_NOT_SUPPORTED", prefix);
      break;
    case J1850_ERR_CRC:
      sprintf(buf, "%sJ1850_ERR_CRC", prefix);
      break;
    default:
      sprintf(buf, "%sERR: %02X", prefix, err);
      break;
  }
  DBGL(buf);
}
#endif

/*uint8_t AFR(int16_t o2) {
  if (o2 >= 350 && o2 <= 550) return 147;
  if (o2 < 350) {
    if (o2 <= 100) return 175;
    int16_t d = o2 - 100;
        // Mapowanie 100–350 → 175–147 (spadek 28, zakres 250)
    uint32_t s = ((uint32_t)d * 28UL * 27684UL) >> 24;  // ≈ d * 28 / 250
    return 180 - s;
  }
  if (o2 < 900) {
    int16_t d = o2 - 550;
        // Mapowanie 550–900 → 147–110 (spadek 37, zakres 350)
    uint32_t s = ((uint32_t)d * 37UL * 30678UL) >> 24;  // ≈ d * 37 / 350
    return 147 - s;
  }
  return 110;
}*/

int16_t randomDelta(int16_t lastValue, int16_t minValue, int16_t maxValue, float maxPercentChange) {
    if (lastValue == -32768) {
    return (int16_t)random(minValue, maxValue + 1);
  }
  int16_t maxDelta = (int16_t)((maxValue - minValue) * maxPercentChange);
  int16_t newMin = max(minValue, lastValue - maxDelta);
  int16_t newMax = min(maxValue, lastValue + maxDelta);
  return (int16_t)random(newMin, newMax + 1);
}
