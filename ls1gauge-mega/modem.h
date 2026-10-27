bool sendAT(String cmd, const char* expected = "OK", uint16_t timeout = 1000) {
  while (Serial1.available()) Serial1.read();
  Serial1.println(cmd);
  int16_t start = (uint16_t)(millis() & 0xFFFF);
  uint8_t matchPos = 0;
  size_t expectedLen = strlen(expected);
  //String response = "";
  delay(50);
  while ((uint16_t)(millis() - start) < timeout) {
    while (Serial1.available()) {
      char c = Serial1.read();
      if (c == expected[matchPos]) {
        matchPos++;
        if (matchPos == expectedLen) return true; 
      } else {
        matchPos = 0;
             //if (c >= 32 && c <= 126) {
      //response += c; }
      //if (c == '\n') {
      //response += ' '; }
    }
  }
  }
  //response.trim();
  //DBGL(response);
  return false; 
}

enum NetCheckState {
  NC_IDLE,
  NC_SEND_CGATT,
  NC_WAIT_CGATT,
  NC_SEND_CREG,
  NC_WAIT_CREG,
  NC_DONE
};

NetCheckState netState = NC_IDLE;
unsigned long netTimer = 0;
String netResponse = "";
bool cgattOK = false;
bool cregOK = false;
bool stopModemWatchdog = false;

static const uint16_t NET_BUF_SIZE     = 192;
static const uint16_t NET_READ_BUDGET  = 20; 

void networkCheck_async() {
  switch (netState) {
    case NC_IDLE:
      netResponse = "";
      cgattOK = false;
      cregOK = false;
      netTimer = millis();
      netState = NC_SEND_CGATT;
      break;

    case NC_SEND_CGATT:
      Serial1.println("AT+CGATT?");
      netResponse = "";
      netTimer = millis();
      netState = NC_WAIT_CGATT;
      break;

    case NC_WAIT_CGATT:
      while (Serial1.available()) {
        char c = Serial1.read();
        netResponse += c;
      }
      if (millis() - netTimer > 2000) {
        cgattOK = netResponse.indexOf("+CGATT: 1") >= 0;
        netResponse = "";
        netTimer = millis();
        netState = NC_SEND_CREG;
      }
      break;

    case NC_SEND_CREG:
      Serial1.println("AT+CREG?");
      netResponse = "";
      netTimer = millis();
      netState = NC_WAIT_CREG;
      break;

    case NC_WAIT_CREG:
      while (Serial1.available()) {
        char c = Serial1.read();
        netResponse += c;
      }
      if (millis() - netTimer > 2000) {
        cregOK = netResponse.indexOf(",1") >= 0 || netResponse.indexOf(",5") >= 0;
        netState = NC_DONE;
      }
      break;

    case NC_DONE:
      break;
  }
}

bool networkCheckIsDone() {
  return netState == NC_DONE;
}

bool isNetworkReady() {
  netState = NC_IDLE;
  return cgattOK && cregOK;
}

uint16_t netLastCheck = 0;
const uint16_t NET_TIMEOUT = 20000;

float convertToDecimal(const char* nmea) {
  float raw = atof(nmea);
  int deg = int(raw / 100);
  float min = raw - deg * 100;
  return deg + min / 60.0;
}

void getGPSInfo() {
  Serial1.println("AT+CGPSINFO");

  unsigned long start = millis();
  char gpsLine[75];
  uint8_t gpsLen = 0;
  uint8_t gpsCommas = 0;
  while (millis() - start < 1000) {
  if (Serial1.available()) {
    char c = Serial1.read();
    if (gpsLen < sizeof(gpsLine) - 1) {
      gpsLine[gpsLen++] = c;
      if (c == ',') gpsCommas++;
      if (gpsCommas >= 8) break;
    }
  }
  }

  gpsLine[gpsLen] = '\0';
  //DBGL(gpsLine);
  char* infoPtr = strstr(gpsLine, "+CGPSINFO:");
  if (infoPtr) {
    infoPtr += strlen("+CGPSINFO:");
    while (*infoPtr == ' ') infoPtr++;  

  char* fields[9];
  uint8_t count = 0;
  fields[0] = infoPtr;

  for (int i = 0; i < gpsLen && count < 8; i++) {
    if (gpsLine[i] == ',') {
      gpsLine[i] = '\0';
      fields[++count] = &gpsLine[i + 1];
    }
  }

  float lat = convertToDecimal(fields[0]);  // np. "5209.512719"
  if (fields[1][0] == 'S') lat = -lat;
  float lon = convertToDecimal(fields[2]);
  if (fields[3][0] == 'W') lon = -lon;
  //float alt = atof(fields[6]);
  float spd = atof(fields[7]) * 1.852;

  /*int day = (fields[4][0] - '0') * 10 + (fields[4][1] - '0');
  int month = (fields[4][2] - '0') * 10 + (fields[4][3] - '0');
  int year = 2000 + (fields[4][4] - '0') * 10 + (fields[4][5] - '0');

  int hour = (fields[5][0] - '0') * 10 + (fields[5][1] - '0');
  int minute = (fields[5][2] - '0') * 10 + (fields[5][3] - '0');
  int second = (fields[5][4] - '0') * 10 + (fields[5][5] - '0');

  // Prosty Zeller's Congruence do wyliczenia dnia tygodnia (0=niedziela)
  int y = (month < 3) ? year - 1 : year;
  int m = (month < 3) ? month + 12 : month;
  int K = y % 100;
  int J = y / 100;
  int f = (day + 13 * (m + 1) / 5 + K + K/4 + J/4 + 5*J) % 7;
  int weekday = (f + 6) % 7;  // teraz 0=niedziela, 1=poniedziałek, ..., 6=sobota

  // Sprawdź czas letni w Polsce
  bool lato = czyLetni(day, month, weekday);
  int offset = lato ? 2 : 1;

  hour += offset;
  if (hour >= 24) hour -= 24;

  // Sformatuj wszystko
  char dataBuf[11];
  char czasBuf[9];
  sprintf(dataBuf, "%04d-%02d-%02d", year, month, day);
  sprintf(czasBuf, "%02d:%02d:%02d", hour, minute, second);

  DBG("Data: "); DBG(dataBuf);
  DBG(", Czas lokalny (PL): "); DBGL(czasBuf); */
  //DBG(F("Wspolrzedne: ")); DBG2(lat, 6);
  //DBG(" "); DBGL2(lon, 6);
  encodeCoord(lat, cmd[LAT].wartosc, cmd[LAT].lastValue);
  encodeCoord(lon, cmd[LON].wartosc, cmd[LON].lastValue);
  //DBG("Wysokość: "); DBGL2(alt, 1);
  //DBG(F("Predkosc: ")); DBGL2(spd,1);
}
}

bool blinkState = false;
bool modemOnline = false;

enum StepMode : uint8_t { STEP_SEND=0, STEP_CHECK_SKIP_NEXT, STEP_NO_CHECK };

struct Step {
  const char* cmd;
  const char* expect;   
  uint16_t timeout_ms;    
  StepMode mode;
  Step(const char* c, const char* e = "OK", StepMode m = STEP_SEND, uint16_t t = 1000)
    : cmd(c), expect(e), timeout_ms(t), mode(m) {}
};

static const Step STEPS[] = {
  { "AT"},
  { "AT+CPIN?", "READY"},
  { "AT+CNMP=13"},
  { "AT+CGDCONT=1,\"IP\",\"internet\""},
  { "AT+CREG?", "+CREG: 0,1"},
  { "ATE0" },
  { "AT+NETOPEN?", "+NETOPEN: 1", STEP_CHECK_SKIP_NEXT}, 
  { "AT+NETOPEN"}, 
  { "AT+IPADDR" }, 
  { "AT+HTTPTERM", "ERROR", STEP_NO_CHECK},
  { "AT+HTTPINIT" },
  { "AT+HTTPPARA=\"CID\",1" },
  { "AT+HTTPPARA=\"URL\",\"http://ls1.cyra.com.pl/logger.php\"" },
  { "AT+HTTPACTION=0", "+HTTPACTION: 0,403"}, // czekamy na raport
  { "AT+HTTPTERM" },
  { "AT+CGPS?", "+CGPS: 1,1", STEP_CHECK_SKIP_NEXT },
  { "AT+CGPS=1" },
};

enum InitRunState : uint8_t { INIT_IDLE=0, INIT_WAITING, INIT_DONE, INIT_FAILED };

static uint8_t  stepIdx = 0, tries = 0, matchPos = 0; //,tries2 = 0;
static uint16_t tStart = 0;
static InitRunState initState = INIT_IDLE;

static inline bool timedOut(uint16_t since, uint16_t ms) {
  return (uint16_t)((uint16_t)(millis() & 0xFFFF) - since) >= ms;
}

static bool matchExpected(const char* expect, uint8_t &pos){
  while (Serial1.available()){
    char c = Serial1.read();
    if (c=='\n'||c=='\r') continue;
    if (c == expect[pos]) {
      pos++;
      if (expect[pos]=='\0') return true;
    } else {
      pos = (c == expect[0]) ? 1 : 0; // proste dosuwanie
    }
  }
  return false;
}

void modemInitStart() {
  stepIdx = 0;
  tries = 0;
  matchPos = 0;
  initState = INIT_IDLE;
}

bool modemInitDone() {
  return initState == INIT_DONE;
}

void modemInitTick() {
  if (initState == INIT_DONE || initState == INIT_FAILED) return;

  const Step &st = STEPS[stepIdx];

  switch (initState){
    case INIT_IDLE: {
      while (Serial1.available()) Serial1.read();

      Serial1.println(st.cmd);
      tStart   = (uint16_t)(millis() & 0xFFFF);
      matchPos = 0;
      initState= INIT_WAITING;
      DBG(F("TX> ")); DBGL(st.cmd);
    } break;

    case INIT_WAITING: {
      if (matchExpected(st.expect, matchPos)) {
        DBG(F("RX< ")); DBGL(st.expect);
        tries=0; matchPos=0;
        stepIdx += (st.mode == STEP_CHECK_SKIP_NEXT) ? 2 : 1;
        if (stepIdx >= (sizeof(STEPS)/sizeof(STEPS[0]))) {
          modemOnline = true;
          initState = INIT_DONE;
        } else {
          initState = INIT_IDLE;
        }
        return;
      }

      if (timedOut(tStart, st.timeout_ms)) {
        tries++;
        DBG(F("TIMEOUT ")); DBG(tries); DBGL(F("/5"));
      if (st.mode == STEP_CHECK_SKIP_NEXT || st.mode == STEP_NO_CHECK) {
    stepIdx += 1;
    tries = 0;
    matchPos = 0;
    if (stepIdx >= (sizeof(STEPS)/sizeof(STEPS[0]))) {
      modemOnline = true;
      initState = INIT_DONE;
      netLastCheck = czas;
    } else {
      initState = INIT_IDLE;
    }
  } else {
    if (tries >= 5) {
      
      /*if (stepIdx == 0)
      {
        tries2++;
        if (tries2 == 5)
      {
        tries2 = 0;
        DBGL(F("Modem: offline... cold reset"));
        wdt_reset();
        digitalWrite(12, LOW);
        delay(3000);
        wdt_reset();
        digitalWrite(12, HIGH);
        delay(1000);
        digitalWrite(12, LOW);
        delay(500);  
        digitalWrite(12, HIGH);
        wdt_reset();       
      }
      }*/
      stepIdx = 0;
      tries = 0;
      matchPos = 0;
      initState = INIT_IDLE;
    } else {
      initState = INIT_IDLE; // retry tego samego kroku
    }
  }
      }
    } break;
  }
}
