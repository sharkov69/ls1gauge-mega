#pragma once
#include <Arduino.h>
//#define DEBUGVPW

#define STORAGE_SIZE 32
#define STORAGE_MASK STORAGE_SIZE-1
#define STORAGE_RESET 0xFF
#define BS               8

class StorageItem
{
public:
    uint8_t content[BS];
    uint8_t size;
};

class Storage
{
    uint8_t _first;
    uint8_t _last;
    StorageItem _items[STORAGE_SIZE];

public:
    Storage();

    void push(uint8_t *buff, uint8_t size);
    uint8_t tryPopItem(uint8_t *pBuff);
};
#ifdef DEBUGVPW
enum J1850_ERRORS {
    J1850_OK                        = 0x00,
    J1850_ERR_BUS_IS_BUSY           = 0x81,
    J1850_ERR_BUS_ERROR             = 0x82,
    J1850_ERR_PULSE_TOO_SHORT       = 0x85,
    J1850_ERR_ARBITRATION_LOST      = 0x87,
    J1850_ERR_PULSE_TOO_LONG        = 0x88,
    J1850_ERR_IFR_RX_NOT_SUPPORTED  = 0x89,
    J1850_ERR_CRC                   = 0x90
};

enum J1850_Operations {
    J1850_Read,
    J1850_Write
};

typedef void (*onErrorHandler)(J1850_Operations op, J1850_ERRORS err);
#endif 

class Pin {

public:
    uint8_t _pin;
    Pin();
    Pin(uint8_t pin, uint8_t mode);
    ~Pin();

    void write(uint8_t val);
    uint8_t read();

};

class J1850VPWFriend;// Forward declaration

class J1850VPW {
    friend class J1850VPWFriend;
private:

    int8_t RX;

    Pin __rxPin;
    Pin __txPin;

    unsigned long  _lastChange;
    volatile bool _sofRead;
    volatile uint8_t _currState;
    volatile uint8_t _bit;
    volatile uint8_t _byte;
    volatile bool _IFRDetected;
    uint8_t _buff[BS];
    uint8_t *msg_buf;

    Storage _storage = Storage();
#ifdef DEBUGVPW 
    volatile onErrorHandler __errHandler;
#endif

private:
    void onFrameRead();
#ifdef DEBUGVPW 
    J1850_ERRORS handleErrorsInternal(J1850_Operations op, J1850_ERRORS err);
#endif
    void onRxChanged();

public:
    J1850VPW();

    J1850VPW* init(uint8_t rxPin, uint8_t txPin);
    J1850VPW* init(uint8_t rxPin);

    int8_t tryGetReceivedFrame(uint8_t *pBuff, bool justValid = true);
    void send(uint8_t* pData, uint8_t size, int16_t timeoutMs = -1);
#ifdef DEBUGVPW 
    J1850VPW* onError(onErrorHandler errHandler);
#endif
};

class J1850VPWFriend {
public:
    static void __handleRnChange();
    static J1850VPW* instance;
};