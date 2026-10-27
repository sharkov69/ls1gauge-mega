#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 47  // pin DATA DS18B20 podłączony do D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempIn  = { 0x28, 0x75, 0xF6, 0xBC, 0x00, 0x00, 0x00, 0xD4 }; //bialy
DeviceAddress tempOut = { 0x28, 0xF5, 0xFB, 0x33, 0x00, 0x00, 0x00, 0x91 };

#include <DHT.h>
#define DHTPIN 48          // pin DHT
#define DHTTYPE DHT22     // DHT11 lub DHT22
DHT dht(DHTPIN, DHTTYPE);
uint16_t SENSORS_INTERVAL = 2500;  // co 5 sekund
uint16_t lastDHTread = 0;
