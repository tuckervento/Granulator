/*
 MuxShield.h - Library for using Mayhew Labs' Mux Shield.
 Created by Mark Mayhew, December 20, 2012.
 Released into the public domain.
 */
#ifndef MuxShield_h
#define MuxShield_h

#include "Arduino.h"

#define DIGITAL_IN   0
#define DIGITAL_OUT  1
#define ANALOG_IN    2
#define DIGITAL_IN_PULLUP    3


class MuxShield
{
public:
    MuxShield();
    uint8_t digitalReadMS(uint8_t chan);
    uint16_t analogReadMS(uint8_t chan);
    
private:
    uint8_t _S0,_S1,_S2,_S3,_IO1,_IO2,_IO3;
};

#endif
