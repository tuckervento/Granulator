/*
 MuxShield.cpp - Library for using Mayhew Labs' Mux Shield.
 Created by Mark Mayhew, December 29, 2012.
 Released into the public domain.
 */

#include "Arduino.h"
#include "MuxShield.h"

MuxShield::MuxShield()
{
    _S0 = 2;
    _S1 = 4;
    _S2 = 6;
    _S3 = 7;

    _IO1 = A0; 
    _IO2 = A1;
    _IO3 = A2;
        
    pinMode(_S0,OUTPUT);
    pinMode(_S1,OUTPUT);
    pinMode(_S2,OUTPUT);
    pinMode(_S3,OUTPUT);
    pinMode(_IO1,INPUT);
    pinMode(_IO2,INPUT);
    pinMode(_IO3,INPUT);
    analogReadResolution(12);
}

//hard-code to read from row 3
uint8_t MuxShield::digitalReadMS(uint8_t chan)
{
    uint16_t val;

    digitalWrite(_S0, (chan&1));    
    digitalWrite(_S1, (chan&3)>>1); 
    digitalWrite(_S2, (chan&7)>>2); 
    digitalWrite(_S3, (chan&15)>>3); 

    val = analogRead(_IO3);

    return (val > 0x578) ? 1 : 0;
}

//hard-code to read from row 2
uint16_t MuxShield::analogReadMS(uint8_t chan)
{
    digitalWrite(_S0, (chan&1));    
    digitalWrite(_S1, (chan&3)>>1); 
    digitalWrite(_S2, (chan&7)>>2); 
    digitalWrite(_S3, (chan&15)>>3); 

    return analogRead(_IO2);
}