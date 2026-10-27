#include "j1850vpw-wc.h"
#include <stdlib.h>

#define RX_SOF_MIN (163)
#define RX_SOF_MAX (239) //u mnie 220

#define RX_EOD_MIN (164) // minimum end of data time
#define RX_EOD_MAX (239) // maximum end of data time
#define RX_EOF_MIN (239) // minimum end of data time

#define RX_SHORT_MIN (34) // minimum short pulse time
#define RX_SHORT_MAX (96) // maximum short pulse time
#define RX_SHORT_IGNORE (20) // Ignore pulses shorter than this to reduce noise - było 5

#define RX_LONG_MIN (97)  // minimum long pulse time
#define RX_LONG_MAX (163) // maximum long pulse time

#define RX_ARBITRATION_TOL (10) // Tolerance for shorter passive pulses from other modules during arbitration
#define PROPOGATION_DELAY  (10)  //Propogation delay from driving output Passive to checking input follows 

#define TX_SHORT (64) // Short pulse nominal time
#define TX_LONG (128) // Long pulse nominal time
#define TX_SOF (200)  // Start Of Frame nominal time
#define TX_EOD (200)  // End Of Data nominal time
#define TX_EOF (280)  // End Of Frame nominal time

#define IS_BETWEEN(x, min, max) (x >= min && x <= max)

Storage::Storage()
{
    _first = STORAGE_RESET;
    _last = STORAGE_RESET;
    memset(_items, 0, sizeof(_items));
}

void Storage::push(uint8_t *buff, uint8_t size)
{
    _last = (_last + 1) & STORAGE_MASK;

    if (_first == STORAGE_RESET)
    {
        _first = _last;
    }
    else if (_first == _last)
    {
        _first = (_first + 1) & STORAGE_MASK;
    }

    memcpy(_items[_last].content, buff, BS);
    _items[_last].size = size;
}

uint8_t Storage::tryPopItem(uint8_t *pBuff)
{
    StorageItem *ptr;

    if (_first == STORAGE_RESET || _last == STORAGE_RESET)
    {
        return 0;
    }

    ptr = &_items[_first];

    if (_first == _last)
    {
        _first = _last = STORAGE_RESET;
    }
    else
    {
        _first = (_first + 1) & STORAGE_MASK;
    }

    memcpy(pBuff, ptr->content, ptr->size);

    return ptr->size;
}

Pin::Pin() : _pin(255) {}

Pin::Pin(uint8_t pin, uint8_t mode) : _pin(pin) {
    pinMode(pin, mode);
}

Pin::~Pin() {}

void Pin::write(uint8_t val) {
    digitalWrite(_pin, val);
}

uint8_t Pin::read() {
    return digitalRead(_pin);
}

uint8_t crc(uint8_t *msg_buf, int8_t nbytes)
{
    uint8_t crc_reg = 0xff, poly, byte_count, bit_count;
    uint8_t *byte_point;
    uint8_t bit_point;

    for (byte_count = 0, byte_point = msg_buf; byte_count < nbytes; ++byte_count, ++byte_point)
    {
        for (bit_count = 0, bit_point = 0x80; bit_count < 8; ++bit_count, bit_point >>= 1)
        {
            if (bit_point & *byte_point) // case for new bit = 1
            {
                if (crc_reg & 0x80)
                    poly = 1; // define the polynomial
                else
                    poly = 0x1c;
                crc_reg = ((crc_reg << 1) | 1) ^ poly;
            }
            else // case for new bit = 0
            {
                poly = 0;
                if (crc_reg & 0x80)
                    poly = 0x1d;
                crc_reg = (crc_reg << 1) ^ poly;
            }
        }
    }
    return ~crc_reg; // Return CRC
}

#ifdef DEBUGVPW 
J1850_ERRORS J1850VPW::handleErrorsInternal(J1850_Operations op, J1850_ERRORS err)
{
    if (err != J1850_OK)
    {
        onErrorHandler errHandler = __errHandler;
        if (errHandler)
        {
            errHandler(op, err);
        }
    }

    return err;
}
#endif 

void J1850VPW::onRxChanged()
{
    uint8_t curr = __rxPin.read();
	
	_currState = curr;
    curr = !curr;

    unsigned long  now = micros();
    unsigned long  diff;

    if (now < _lastChange)
    {
        // overflow occured
        diff = now + ((unsigned long)-1L - _lastChange);
    }
    else
    {
        diff = now - _lastChange;
    }

    if (diff < RX_SHORT_IGNORE) //We filter out some noise for very short pulses around the transition
    {
        return;
    }

    _lastChange = now;

    if (diff < RX_SHORT_MIN)
    {
        // too short to be a valid pulse. Data error
        _sofRead = false;
		#ifdef DEBUGVPW 
        handleErrorsInternal(J1850_Read, J1850_ERR_PULSE_TOO_SHORT);
		#endif 
        return;
    }

    if (!_sofRead)
    {
        if (_sofRead = (curr == LOW && IS_BETWEEN(diff, RX_SOF_MIN, RX_SOF_MAX)))
        {
            _byte = 0;
            _bit = 0;
            msg_buf = _buff;
            *msg_buf = 0;
            _IFRDetected = false;
        }
        else
        {
        }
        return;
    }

    *msg_buf <<= 1;
    if (curr == HIGH)
    {
        if (diff > RX_EOF_MIN)
        {
            // data ended - copy package to buffer
            _sofRead = false;

            if (!_IFRDetected)
            {
                onFrameRead();
		    
            }
            return;
        }
        if (!_IFRDetected && IS_BETWEEN(diff, RX_EOD_MIN, RX_EOD_MAX))
        {
            // data ended and IFR detected - set flag to ignore the incoming IFR and flag error
            _IFRDetected = true;
			#ifdef DEBUGVPW 
            handleErrorsInternal(J1850_Read, J1850_ERR_IFR_RX_NOT_SUPPORTED);
			#endif 
            onFrameRead();
            return;
        }
        if (!_IFRDetected && IS_BETWEEN(diff, RX_LONG_MIN, RX_LONG_MAX))
        {
            *msg_buf |= 1;
        }
    }
    else if (!_IFRDetected)
    {
        if (diff <= RX_SHORT_MAX)
        {
            *msg_buf |= 1;
        }
    }
    if(!_IFRDetected)
    {
        _bit++;
        if (_bit == 8)
        {
            _byte++;
            msg_buf++;
            *msg_buf = 0;
            _bit = 0;
		// poczatek
        if (_byte == 1)
        {
            if (_buff[0] != 0x6C && _buff[0] != 0x48)
            {
                _sofRead = false;
                return;
            }
        } //koniec
        }
    }    
    if (_byte == BS)
    {
        _sofRead = false;
        if (!_IFRDetected)
        {
            onFrameRead();
        }
    }
}

J1850VPW::J1850VPW() 
: RX(-1)
, __rxPin(Pin())
, __txPin(Pin())
, _lastChange(micros())
, _sofRead(false)
, _currState(LOW)
, _bit(0)
, _byte(0)
, msg_buf(NULL)
, _storage(Storage())
#ifdef DEBUGVPW 
, __errHandler(NULL)
#endif
{}

J1850VPW* J1850VPW::init(uint8_t rxPin, uint8_t txPin)
{
    init(rxPin);

    __txPin = Pin(txPin, OUTPUT);
    __txPin.write(HIGH);

    return this;
}
J1850VPW* J1850VPWFriend::instance = nullptr;

void J1850VPWFriend::__handleRnChange() {
    instance->onRxChanged();
}
J1850VPW* J1850VPW::init(uint8_t rxPin) 
{
	J1850VPWFriend::instance = this;
    __rxPin = Pin(rxPin, INPUT_PULLUP);

    _currState = __rxPin.read();

    attachInterrupt(digitalPinToInterrupt(rxPin), &J1850VPWFriend::__handleRnChange, CHANGE);

    return this;
}

// NB! Performance critical!!! Do not split
void J1850VPW::send(uint8_t *pData, uint8_t nbytes, int16_t timeoutMs /*= -1*/)
{
	#ifdef DEBUGVPW 
    J1850_ERRORS result = J1850_OK;
	#endif 
    static uint8_t buff[BS];
    memcpy(buff, pData, nbytes);
    buff[nbytes] = crc(buff, nbytes);
    nbytes++;
    
    uint8_t *msg_buf = buff;
    unsigned long now;

    // wait for idle
    if (timeoutMs >= 0)
    {
        now = micros();
        unsigned long  start = now;
        timeoutMs *= 1000; // convert to microseconds
        while (micros() - now < TX_EOF)
        {
            if (__rxPin.read() == LOW)
            {
                now = micros();
            }

            if (micros() - start > timeoutMs)
            {
				#ifdef DEBUGVPW 
                result = J1850_ERR_BUS_IS_BUSY;
				#endif 
                goto stop;
            }
        }
    }

    // SOF
    detachInterrupt(digitalPinToInterrupt(__rxPin._pin));
    __txPin.write(LOW);
    now = micros();
    while (micros() - now < TX_SOF)
        ;

    // send data
    do
    {
        uint8_t temp_byte = *msg_buf; // store byte temporary
        uint8_t nbits = 8;
        unsigned long  delay;
        while (nbits--) // send 8 bits
        {
            if (nbits & 1) // start allways with passive symbol
            {
                delay = (temp_byte & 0x80) ? TX_LONG : TX_SHORT; // send correct pulse lenght
                __txPin.write(HIGH);                          // set bus active
                now = micros();
                delayMicroseconds(PROPOGATION_DELAY);            // Allow time for RX to follow TX for fast processors
                while (micros() - now < delay)
                {
                    if (__rxPin.read() == LOW)
                    {
                        if (delay - (micros() - now)  < RX_ARBITRATION_TOL) //Arbitration not lost
                        {
                            now = micros() - delay - 1; //resync to faster module
                        } else //We lost arbitration so drop out
                        {
							#ifdef DEBUGVPW 
                            result = J1850_ERR_ARBITRATION_LOST;
							#endif 
                            goto stop;
                        }
                    }
                }
            }
            else // send active symbol
            {
                delay = (temp_byte & 0x80) ? TX_SHORT : TX_LONG; // send correct pulse lenght
                __txPin.write(LOW);                           // set bus LOW
                now = micros();
                while (micros() - now < delay)
                    ;
            }

            temp_byte <<= 1; // next bit
        }                    // end nbits while loop
        ++msg_buf;           // next byte from buffer
    } while (--nbytes);      // end nbytes do loop

    // EOF
    __txPin.write(HIGH);
    now = micros();
    delayMicroseconds(PROPOGATION_DELAY);            // Allow time for RX to follow TX for fast processors
    while (micros() - now < TX_SOF)
    {
        if (__rxPin.read() == LOW)
        {
			#ifdef DEBUGVPW 
            result = J1850_ERR_ARBITRATION_LOST;
			#endif 
            goto stop;
        }
    }

stop:
    attachInterrupt(digitalPinToInterrupt(__rxPin._pin), &J1850VPWFriend::__handleRnChange, CHANGE);
	#ifdef DEBUGVPW 
    handleErrorsInternal(J1850_Write, result);
	#endif
}
#ifdef DEBUGVPW 
J1850VPW* J1850VPW::onError(onErrorHandler errHandler)
{
    __errHandler = errHandler;
    return this;
}
#endif
int8_t J1850VPW::tryGetReceivedFrame(uint8_t *pBuff, bool justValid /*= true*/)
{
    uint8_t size;
    bool crcOK = true;

    while (true)
    {
        size = _storage.tryPopItem(pBuff);
        if (!size)
        {
            return 0;
        }
        if (crc(pBuff, size - 1) != pBuff[size - 1])
        {
            crcOK = false;
			#ifdef DEBUGVPW 
            handleErrorsInternal(J1850_Read, J1850_ERR_CRC);
			#endif
        }
        if (!justValid || crcOK)
        {
            break;
        }
    }

    return size;
}


void J1850VPW::onFrameRead()
{
    if (!IS_BETWEEN(_byte, 2, BS))
    {
        return;
    }

        _storage.push(_buff, _byte);
 
}
