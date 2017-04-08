/*
  sbus.cpp

  Copyright (c) 2017, Fabrizio Di Vittorio (fdivitto2013@gmail.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "sbus.h"


// quick IO functions
#define portOfPin(P)        (((P) >= 0 && (P) < 8) ? &PORTD : (((P) > 7 && (P) < 14) ? &PORTB : &PORTC))
#define ddrOfPin(P)         (((P) >= 0 && (P) < 8) ? &DDRD : (((P) > 7 && (P) < 14) ? &DDRB : &DDRC))
#define pinOfPin(P)         (((P) >= 0 && (P) < 8) ? &PIND : (((P) > 7 && (P) < 14) ? &PINB : &PINC))
#define pinIndex(P)         ((uint8_t)(P > 13 ? P - 14 : P & 7))
#define pinMask(P)          ((uint8_t)(1 << pinIndex(P)))
#define pinAsInput(P)       (*(ddrOfPin(P)) &= ~pinMask(P))
#define pinAsInputPullUp(P) (*(ddrOfPin(P)) &= ~pinMask(P); digitalHigh(P))
#define pinAsOutput(P)      (*(ddrOfPin(P)) |= pinMask(P))
#define digitalLow(P)       (*(portOfPin(P)) &= ~pinMask(P))
#define digitalHigh(P)      (*(portOfPin(P)) |= pinMask(P))
#define isHigh(P)           ((*(pinOfPin(P)) & pinMask(P)) > 0)
#define isLow(P)            ((*(pinOfPin(P)) & pinMask(P)) == 0)
#define pinGet(pin, mask)   (((*(pin)) & (mask)) > 0)

static uint8_t * s_pin;
static uint8_t s_pinMask;
static uint8_t s_PCICRMask;

static volatile mode_t s_mode;

// contains which word is currently receiving (0..24, 24 is the last word)
static volatile uint8_t s_receivingWordIndex = 0;

// received frame
static volatile uint8_t s_frame[23];

  
struct chinfo_t {
  uint8_t idx;
  uint8_t shift1;
  uint8_t shift2;
  uint8_t shift3; // 11 = byte 3 ignored
};


static const chinfo_t CHINFO[16] PROGMEM = { {0,  0, 8, 11}, {1,  3, 5, 11}, {2,  6, 2, 10}, {4,  1, 7, 11}, {5,  4, 4, 11},
                                           {6,  7, 1, 9},  {8,  2, 6, 11}, {9,  5, 3, 11}, {11, 0, 8, 11}, {12, 3, 5, 11},               
                                           {13, 6, 2, 10}, {15, 1, 7, 11}, {16, 4, 4, 11}, {17, 7, 1, 9},  {19, 2, 6, 11},  {20, 5, 3, 11} };               

// flag field
#define FLAG_CHANNEL_17  1
#define FLAG_CHANNEL_18  2
#define FLAG_SIGNAL_LOSS 4
#define FLAG_FAILSAFE    8

#define CHANNEL_MIN 173
#define CHANNEL_MAX 1812



inline void enablePinChangeInterrupts()
{
  // clear any outstanding interrupt
  PCIFR |= s_PCICRMask; 
  
  // enable interrupt for the group
  PCICR |= s_PCICRMask;   
}


inline void disablePinChangeInterrupts()
{
  PCICR &= ~s_PCICRMask; 
}


// handle pin change interrupt for D0 to D7 here
ISR(PCINT2_vect) 
{
  // start bit?
  if (pinGet(s_pin, s_pinMask) > 0) {
  
    // reset timer 2 counter
    TCNT2 = 0;

    // start bit received
    uint8_t receivedBitIndex = 1;
    uint8_t receivingWord = 0;
    
    // receive other bits

    // reset OCF2A flag by writing "1"
    TIFR2 |= 1 << OCF2A;

    uint8_t calc_parity = 0xFF;

    while (true) {

      // wait for TCNT2 == OCR2A
      while (!(TIFR2 & (1 << OCF2A)))
        ;

      // reset OCF2A flag by writing "1"
      TIFR2 |= 1 << OCF2A;
      
      // sample current bit  
      if (receivedBitIndex >= 1 && receivedBitIndex <= 8) {
        if (pinGet(s_pin, s_pinMask) > 0)
          receivingWord |= 1 << (receivedBitIndex - 1);
        else
          calc_parity = ~calc_parity;
      }

      // last bit?
      if (receivedBitIndex == 9) {

        // check parity
        uint8_t in_parity = pinGet(s_pin, s_pinMask) > 0;
        if ((calc_parity && !in_parity) || (!calc_parity && in_parity)) {          
          // parity check failed!
          s_receivingWordIndex = 0;
          break;          
        }
    
        receivingWord = ~receivingWord;
    
        if (s_receivingWordIndex == 0) {
    
          //  start byte (must be 0x0F)
          if (receivingWord != 0x0F) {
            // wrong start byte, restart word count
            s_receivingWordIndex = 0;
            break;
          }
    
        } else if (s_receivingWordIndex == 24) {
          
          // last word, restart word count
          if (s_mode == sbusBlocking)
            disablePinChangeInterrupts();            
          s_receivingWordIndex = 0;
          break;
          
        } else {
    
          // channels and flags
          s_frame[s_receivingWordIndex - 1] = receivingWord;
          
        }
    
        // prepare for the next word          
        ++s_receivingWordIndex;

        break;
        
      } else {

        // other bits required, continue loop
        
        ++receivedBitIndex;
    
      }

      // exit loop, a pin change (start bit) is required to restart
    
    }

    // reset pin change interrupt flag
    PCIFR = 0xFF;    

  }
    
}


// used only for blocking operations (mode = sbusBlocking)
bool SBUS::waitFrame(uint32_t timeOut)
{
  if (s_mode == sbusBlocking) {
    enablePinChangeInterrupts();
    // wait until Pin change bit return back to 0
    uint32_t t0 = millis();
    while (PCICR & s_PCICRMask)
      if (millis() - t0 >= timeOut)  // note: this millis() compare is overflow safe due unsigned diffs
        return false; // timeout!
  }

  return true;
}


// channels start from 1 to 18
// returns value 173..1812 (a received by SBUS)
uint16_t SBUS::getChannelRaw(uint8_t channelIndex)
{
  if (channelIndex >= 1 && channelIndex <= 16) {
    
    chinfo_t chinfo;
    memcpy_P(&chinfo, CHINFO + channelIndex - 1, sizeof(chinfo_t));
  
    uint8_t idx = chinfo.idx;
  
    noInterrupts();
    uint8_t b1 = s_frame[idx++];
    uint8_t b2 = s_frame[idx++];
    uint8_t b3 = s_frame[idx];
    interrupts();
  
    return ((b1 >> chinfo.shift1) | (b2 << chinfo.shift2) | (b3 << chinfo.shift3)) & 0x7FF;
    
  } else if (channelIndex == 17 || channelIndex == 18) {

    if (channelIndex == 17)
      return s_frame[22] & FLAG_CHANNEL_17 ? CHANNEL_MAX : CHANNEL_MIN;
    else
      return s_frame[22] & FLAG_CHANNEL_18 ? CHANNEL_MAX : CHANNEL_MIN;
    
  } else
  
    return 0; // error
}


// channels start from 1 to 18
// returns value 988..2012 (cleanflight friendly)
uint16_t SBUS::getChannel(uint8_t channelIndex)
{
  return 5 * getChannelRaw(channelIndex) / 8 + 880;
}


bool SBUS::signalLossActive()
{
  return s_frame[22] & FLAG_SIGNAL_LOSS;
}


bool SBUS::failsafeActive()
{
  return s_frame[22] & FLAG_FAILSAFE;
}


// if mode = sbusNonBlocking, an interrupt is always generated for every frame received. getChannel (or getChannelRaw) is no-blocking
// if mode = sbusBlocking, interrupts are enabled only inside getChannel (or getChannelRaw), which becomes blocking (until arrive of a new frame)
void SBUS::begin(uint8_t pin, mode_t mode)
{
  s_mode = mode;

  s_pin        = pinOfPin(pin);
  s_pinMask    = pinMask(pin);
  s_PCICRMask  = 1 << digitalPinToPCICRbit(pin);
  
  //// setup TIMER 2 CTC

  // select "Clear Timer on Compare (CTC)" mode
  TCCR2A = 1 << WGM21; 
  
  // no prescaling
  TCCR2B = 1 << CS20; 

  // set TOP timer 2 value
  // with no-prescaler 1/16000000=0.00000000625, each bit requires 10us, so reset timer 2 every 10us, so reset timer every 10/0.0625 = 160 ticks
  OCR2A = F_CPU / 1000000 * 10 - 1; // for 16MHz = 159;  

  //// setup pin change interrupt
  
  pinAsInput(pin);
  
  // enable pin
  *digitalPinToPCMSK(pin) |= 1 << digitalPinToPCMSKbit(pin);  

  if (mode == sbusNonBlocking)
    enablePinChangeInterrupts();
}

  









