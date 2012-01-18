/*
  Charliplexing.cpp - Using timer2 with 1ms resolution
  
  Alex Wenger <a.wenger@gmx.de> http://arduinobuch.wordpress.com/
  Matt Mets <mahto@cibomahto.com> http://cibomahto.com/
  
  Timer init code from MsTimer2 - Javier Valencia <javiervalencia80@gmail.com>
  Misc functions from Benjamin Sonnatg <benjamin@sonntag.fr>
  
  History:
    2009-12-30 - V0.0 wrote the first version at 26C3/Berlin
    2010-01-01 - V0.1 adding misc utility functions 
      (Clear, Vertical,  Horizontal) comment are Doxygen complaints now
    2010-05-27 - V0.2 add double-buffer mode

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


#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif
#include <inttypes.h>
#include <avr/interrupt.h>
#include "Charliplexing.h"

#if defined (__AVR_ATmega168__) \
    || defined (__AVR_ATmega328P__) \
    || defined (__AVR_ATmega1280__) \
    || defined (__AVR_ATmega2560__)
// Ok!
#else
#error Sorry, your version of Arduino is not supported! Please let someone know \
 and maybe we can help you.
#endif


volatile unsigned int LedSign::tcnt2;

/* -----------------------------------------------------------------  */
/** Table for the LED multiplexing cycles, containing 24 cycles made out of two bytes
 */
uint8_t leds[2][48];

/// Determines whether the display is in single or double buffer mode
uint8_t displayMode;

/// Flag indicating that the display page should be flipped as soon as the
/// current frame is displayed
boolean videoFlipPage;

/// Pointer to the buffer that is currently being displayed
uint8_t* displayBuffer;

/// Pointer to the buffer that should currently be drawn to
uint8_t* workBuffer;

/// Number of timer counts to display each row for
uint8_t timeOn;

/// Number of timer counts between screen displays
uint8_t timeOff;



/// Define to set analog pin 5 high during interrupts, so that an
/// oscilloscope can be used to measure the processor time taken by it
//#define MEASURE_ISR_TIME
#ifdef MEASURE_ISR_TIME
uint8_t statusPIN = 19;
#endif

boolean onPhase;

typedef struct LEDPosition {
    uint8_t high;
    uint8_t low;
};


/* -----------------------------------------------------------------  */
/** Table for LED Position in leds[] ram table
 */

const LEDPosition ledMap[126] = {
    {13, 5}, {13, 6}, {13, 7}, {13, 8}, {13, 9}, {13,10}, {13,11}, {13,12},
    {13, 4}, { 4,13}, {13, 3}, { 3,13}, {13, 2}, { 2,13},
    {12, 5}, {12, 6}, {12, 7}, {12, 8}, {12, 9}, {12,10}, {12,11}, {12,13},
    {12, 4}, { 4,12}, {12, 3}, { 3,12}, {12, 2}, { 2,12},
    {11, 5}, {11, 6}, {11, 7}, {11, 8}, {11, 9}, {11,10}, {11,12}, {11,13},
    {11, 4}, { 4,11}, {11, 3}, { 3,11}, {11, 2}, { 2,11},
    {10, 5}, {10, 6}, {10, 7}, {10, 8}, {10, 9}, {10,11}, {10,12}, {10,13},
    {10, 4}, { 4,10}, {10, 3}, { 3,10}, {10, 2}, { 2,10},
    { 9, 5}, { 9, 6}, { 9, 7}, { 9, 8}, { 9,10}, { 9,11}, { 9,12}, { 9,13},
    { 9, 4}, { 4, 9}, { 9, 3}, { 3, 9}, { 9, 2}, { 2, 9},
    { 8, 5}, { 8, 6}, { 8, 7}, { 8, 9}, { 8,10}, { 8,11}, { 8,12}, { 8,13},
    { 8, 4}, { 4, 8}, { 8, 3}, { 3, 8}, { 8, 2}, { 2, 8},
    { 7, 5}, { 7, 6}, { 7, 8}, { 7, 9}, { 7,10}, { 7,11}, { 7,12}, { 7,13},
    { 7, 4}, { 4, 7}, { 7, 3}, { 3, 7}, { 7, 2}, { 2, 7},
    { 6, 5}, { 6, 7}, { 6, 8}, { 6, 9}, { 6,10}, { 6,11}, { 6,12}, { 6,13},
    { 6, 4}, { 4, 6}, { 6, 3}, { 3, 6}, { 6, 2}, { 2, 6},
    { 5, 6}, { 5, 7}, { 5, 8}, { 5, 9}, { 5,10}, { 5,11}, { 5,12}, { 5,13},
    { 5, 4}, { 4, 5}, { 5, 3}, { 3, 5}, { 5, 2}, { 2, 5},
};


/* -----------------------------------------------------------------  */
/** Constructor : Initialize the interrupt code. 
 * should be called by setup() in the main Arduino sketch
 */
void LedSign::Init(uint8_t mode)
{
#ifdef MEASURE_ISR_TIME
    pinMode(statusPIN, OUTPUT);
    digitalWrite(statusPIN, LOW);
#endif

	float prescaler = 0.0;

    // Configure the interrupt routine to run at 2kHz
	TIMSK2 &= ~(1<<TOIE2);
	TCCR2A &= ~((1<<WGM21) | (1<<WGM20));
	TCCR2B &= ~(1<<WGM22);
	ASSR &= ~(1<<AS2);
	TIMSK2 &= ~(1<<OCIE2A);

    TCCR2B |= (1<<CS22);
    TCCR2B &= ~((1<<CS21) | (1<<CS20));
    prescaler = 65.0;
	
	tcnt2 = 256 - (int)((float)F_CPU * 0.0005 / prescaler);

    LedSign::SetBrightness(127);
	
	TCNT2 = tcnt2;
	TIMSK2 |= (1<<TOIE2);

    // Record whether we are in single or double buffer mode
    displayMode = mode;

    // Point the display buffer to the first physical buffer
    displayBuffer = leds[0];

    // If we are in single buffered mode, point the work buffer
    // at the same physical buffer as the display buffer.  Otherwise,
    // point it at the second physical buffer.
    if( displayMode == SINGLE_BUFFER ) {
        workBuffer = displayBuffer;
    }
    else {
        workBuffer = leds[1];
    }

    // Clear the buffer and display it
    LedSign::Clear(0);
    LedSign::Flip(false);
}


/* -----------------------------------------------------------------  */
/** Signal that the front and back buffers should be flipped
 * @param blocking if true : wait for flip before returning, if false :
 *                 return immediately.
 */
void LedSign::Flip(bool blocking)
{
    if (displayMode == DOUBLE_BUFFER)
    {
        // Just set the flip flag, the buffer will flip between redraws
        videoFlipPage = true;

        // If we are blocking, sit here until the page flips.
        while (blocking && videoFlipPage) {
            delay(1);
        }
    }
}


/* -----------------------------------------------------------------  */
/** Clear the screen completely
 * @param set if 1 : make all led ON, if not set or 0 : make all led OFF
 */
void LedSign::Clear(int set) {
    for(int x=0;x<14;x++)  
        for(int y=0;y<9;y++) 
            Set(x,y,set);
}


/* -----------------------------------------------------------------  */
/** Clear an horizontal line completely
 * @param y is the y coordinate of the line to clear/light [0-8]
 * @param set if 1 : make all led ON, if not set or 0 : make all led OFF
 */
void LedSign::Horizontal(int y, int set) {
    for(int x=0;x<14;x++)  
        Set(x,y,set);
}


/* -----------------------------------------------------------------  */
/** Clear a vertical line completely
 * @param x is the x coordinate of the line to clear/light [0-13]
 * @param set if 1 : make all led ON, if not set or 0 : make all led OFF
 */
void LedSign::Vertical(int x, int set) {
    for(int y=0;y<9;y++)  
        Set(x,y,set);
}


/* -----------------------------------------------------------------  */
/** Set : switch on and off the leds. All the position #for char in frameString:
 * calculations are done here, so we don't need to do in the
 * interrupt code
 */
void LedSign::Set(uint8_t x, uint8_t y, uint8_t c)
{
    uint8_t pin_high = ledMap[x+y*14].high;
    uint8_t pin_low  = ledMap[x+y*14].low;
    // pin_low is directly the address in the led array (minus 2 because the 
    // first two bytes are used for RS232 communication), but
    // as it is a two byte array we need to check pin_high also.
    // If pin_high is bigger than 8 address has to be increased by one

    uint8_t bufferNum = (pin_low-2)*2 + (pin_high / 8) + ((pin_high > 7)?24:0);
    uint8_t work = _BV(pin_high & 0x07);

    if (c == 1) {
        workBuffer[bufferNum] |= work;   // ON
    } 
    else {
        workBuffer[bufferNum] &= ~work;   // OFF
    }
}

/* Set the overall brightness of the screen
 * @param brightness LED brightness, from 0 (off) to 127 (full on)
 */
void LedSign::SetBrightness(uint8_t brightness)
{
    // An exponential fit seems to approximate a (perceived) linear scale
    float brightnessPercent = ((float)brightness / 127)*((float)brightness / 127);

    // Compute on and off times
    uint8_t interval = 255 - tcnt2;
    int newTimeOn = 255 -brightnessPercent*interval;
    int newTimeOff = 255 - (1 - brightnessPercent)*interval;

    // Then update the registers
    timeOn = newTimeOn;
    timeOff = newTimeOff;
}


/* -----------------------------------------------------------------  */
/** The Interrupt code goes here !  
 */

#define MIN_ISR_TIME 250

ISR(TIMER2_OVF_vect) {

#ifdef MEASURE_ISR_TIME
    digitalWrite(statusPIN, HIGH);
#endif

    if (!onPhase) {
        if (timeOn < 255) {
            onPhase = true;
        }
    }
    else {
        if (timeOff < 255) {
            onPhase = false;
        }
    }

    if ( onPhase ) {
        if ( timeOn > MIN_ISR_TIME ) {
            TCNT2 = 255 - ((255 - timeOn) + (255 - timeOff));
        }
        else {
            TCNT2 = timeOn;
        }

        // 24 Cycles of Matrix
        static uint8_t i = 0;

        static uint8_t pinDirLow;
        static uint8_t pinDirHigh;
        static uint8_t pinDataLow;
        static uint8_t pinDataHigh;

        pinDataLow = displayBuffer[i*2];
        pinDataHigh = displayBuffer[i*2+1];

        if (i < 6) {
            pinDirLow = _BV(i+2) | displayBuffer[i*2];

            pinDirHigh =            displayBuffer[i*2+1];
        } else if (i < 12) {
            pinDirLow =             displayBuffer[i*2];

            pinDirHigh = _BV(i-6) | displayBuffer[i*2+1];
        } else if (i < 18) {
            pinDirLow = _BV(i+2-12) | displayBuffer[i*2];

            pinDirHigh =            displayBuffer[i*2+1];
        } else {
            pinDirLow =             displayBuffer[i*2];

            pinDirHigh = _BV(i-6-12) | displayBuffer[i*2+1];
        }

#if defined (__AVR_ATmega168__) || defined (__AVR_ATmega328P__)
    PORTD = pinDataLow;
    PORTB = pinDataHigh;
    DDRD = pinDirLow;
    DDRB = pinDirHigh;
#else // defined (__AVR_ATmega1280__) || defined (__AVR_ATmega2560__)
    //port E mappings 
    DDRE = (DDRE & (~0x38)) | ((pinDirLow << 2) & 0x30) | ((pinDirLow >> 2) & 0x8);
    PORTE = (PORTE & (~0x38)) | ((pinDataLow << 2) & 0x30) | ((pinDataLow >> 2) & 0x8);	
    //port G mappings 
    DDRG = (DDRG & (~0x20)) | ((pinDirLow << 1) & 0x20);
    PORTG = (PORTG & (~0x20)) | ((pinDataLow << 1) & 0x20);
    //port H mappings
    DDRH = (DDRH & (~0x78)) | ((pinDirLow >> 3) & 0x18) | ((pinDirHigh << 5) & 0x60);
    PORTH = (PORTH & (~0x78)) | ((pinDataLow >> 3) & 0x18) | ((pinDataHigh << 5) & 0x60);
    //port B mappings
    DDRB = (DDRB & (~0xf0)) | ((pinDirHigh << 2) & 0xf0);
    PORTB = (PORTB & (~0xf0)) | ((pinDataHigh << 2) & 0xf0);	
#endif

        i++;

        if (i > 23) {
            i = 0;

            // If the page should be flipped, do it here.
            if (videoFlipPage && displayMode == DOUBLE_BUFFER)
            {
                // TODO: is this an atomic operation?
                videoFlipPage = false;

                uint8_t* temp = displayBuffer;
                displayBuffer = workBuffer;
                workBuffer = temp;
            }
        }

        // If our on time isn't long, just do the timing in hardware
        if ( timeOn > MIN_ISR_TIME ) {
            volatile int j = (int)(255 - timeOn)*4;
            for (; j > 0; j--) {
//                for (volatile int k = 0; k < 10; k++) {}
            }
            onPhase = false;

            DDRD  = 0x0;
            DDRB  = 0x0;
#ifdef MEASURE_ISR_TIME
    digitalWrite(statusPIN, LOW);
#endif
        }
    }
    else {
#if defined (__AVR_ATmega168__) || defined (__AVR_ATmega328P__) || defined (__AVR_ATmega1280__) || defined (__AVR_ATmega2560__)
        TCNT2 = timeOff;
#endif

        // Turn everything off
        DDRD  = 0x0;
        DDRB  = 0x0;
#ifdef MEASURE_ISR_TIME
    digitalWrite(statusPIN, LOW);
#endif
    }

#ifdef MEASURE_ISR_TIME
//    digitalWrite(statusPIN, LOW);
#endif
}

