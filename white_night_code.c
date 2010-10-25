/*
 * white_night_code
 * 
 * Copyright 2011 BuildBrighton (Mike Pountney, Matthew Edwards)
 * For details, see http://github.com/mikepea/white-night-code
 *
 * Interrupt code based on NECIRrcv by Joe Knapp, IRremote by Ken Shirriff
 *    http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

// Apple codes
#define APPLE_PLAY 0x77E1203A

// for matt's design
//#define redMask  0b00000010
//#define grnMask  0b00010000
//#define bluMask  0b00000100
//#define irInMask     0b00001000
//#define irOutMask    0b00000001
//#define irInPortBPin  4;

// for trippy RGB wave
#define bogusMask 0b00100000
#define redMask   0b00000010
#define grnMask   0b00000100
#define bluMask   0b00010000
#define rgbMask   0b00010110
#define irInMask     0b00001000
#define irOutMask    0b00000001
#define irInPortBPin  4

// IR detector output is active low
#define MARK  0
#define SPACE 1

#define NEC_BITS 32

#define TOPBIT 0x80000000

#define TOLERANCE 25  // percent tolerance in measurements
#define LTOL (1.0 - TOLERANCE/100.) 
#define UTOL (1.0 + TOLERANCE/100.) 

#define TICKS_LOW(us) (int) (((us)*LTOL/USECPERTICK))
#define TICKS_HIGH(us) (int) (((us)*UTOL/USECPERTICK + 1))

// Marks tend to be 100us too long, and spaces 100us too short
// when received due to sensor lag.
#define MARK_EXCESS 100

#define MATCH(measured_ticks, desired_us) ((measured_ticks) >= TICKS_LOW(desired_us) && (measured_ticks) <= TICKS_HIGH(desired_us))
#define MATCH_MARK(measured_ticks, desired_us) MATCH(measured_ticks, (desired_us) + MARK_EXCESS)
#define MATCH_SPACE(measured_ticks, desired_us) MATCH((measured_ticks), (desired_us) - MARK_EXCESS)




// receiver state machine states
#define STATE_IDLE     2
#define STATE_MARK     3
#define STATE_SPACE    4
#define STATE_STOP     5

// results definitions
#define ERR 0
#define DECODED 1

// repeat code for NEC
#define REPEAT 0xffffffff

#define NEC_HDR_MARK    9000
#define NEC_HDR_SPACE   4500
#define NEC_BIT_MARK    560
#define NEC_ONE_SPACE   1600
#define NEC_ZERO_SPACE  560
#define NEC_RPT_SPACE   2250


// Values for decode_type
#define NONE 0
#define NEC 1

#define CLKFUDGE 5        // fudge factor for clock interrupt overhead
#define CLK 256           // max value for clock (timer 2)
#define PRESCALE 8        // timer2 clock prescale
#define SYSCLOCK 8000000  // main clock speed
#define CLKSPERUSEC (SYSCLOCK/PRESCALE/1000000)   // timer clocks per microsecond
#define USECPERTICK 50  // microseconds per clock interrupt tick

#define INIT_TIMER_COUNT0 (CLK - USECPERTICK*CLKSPERUSEC + CLKFUDGE)
#define RESET_TIMER0 TCNT0 = INIT_TIMER_COUNT0


#define _GAP 5000 // Minimum gap between transmissions
#define GAP_TICKS (_GAP/USECPERTICK)

#define RAWBUF 76 // Length of raw duration buffer


// information for the interrupt handler
typedef struct {
  uint8_t recvpin;           // pin for IR data from detector
  uint8_t rcvstate;          // state machine
  uint8_t blinkflag;         // TRUE to enable blinking of pin on IR processing
  unsigned int timer;        // state timer, counts 50uS ticks.
  unsigned int rawbuf[RAWBUF]; // raw data
  uint8_t rawlen;            // counter of entries in rawbuf
} irparams_t;

typedef struct {
  int decode_type; // NEC, SONY, RC5, UNKNOWN
  unsigned long value; // Decoded value
  int bits; // Number of bits in decoded value
  volatile unsigned int *rawbuf; // Raw intervals in .5 us ticks
  int rawlen; // Number of records in rawbuf.
} decode_results;

volatile irparams_t irparams;
volatile decode_results my_results;

void delay_ten_us(unsigned long int us) {
  unsigned long int count;
  const unsigned long int DelayCount=8;

  while (us != 0) {
    for (count=0; count <= DelayCount; count++) {PINB |= bogusMask;};
    us--;
  }
}

void delay_x_us(unsigned long int x) {
  unsigned long int count;
  const unsigned long int DelayCount=0;

  while (x != 0) {
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    x--;
  }
}

// Little start up pattern
void startUp1(void) {
    for (int i = 0; i<15; i++) {
        PORTB ^= grnMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= bluMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= bluMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= grnMask;
    }
}

void enable_ir_recving(void) {
  //Timer0 Overflow Interrupt Enable
  TIMSK |= _BV(TOIE0);
}

void disable_ir_recving(void) {
  //Timer0 Overflow Interrupt disable
  TIMSK &= ~(_BV(TOIE0));
}

void mark(int time) {
  // Sends an IR mark for the specified number of microseconds.
  // The mark output is modulated at the PWM frequency.
  //TCCR0A |= _BV(COM0B1); // Enable pin 6 (OC0B) PWM output
  TCCR0A |= _BV(COM0A1); // Enable pin 5 (OC0A) PWM output
  delay_ten_us(time / 10);
}

/* Leave pin off for time (given in microseconds) */
void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  //TCCR0A &= ~(_BV(COM0B1)); // Disable pin 6 (OC0B) PWM output
  TCCR0A &= ~(_BV(COM0A1)); // Disable pin 5 (OC0A) PWM output
  delay_ten_us(time / 10);
}

void enableIROut(int khz) {
  // Enables IR output.  The khz value controls the modulation frequency in kilohertz.
  //
  // The IR output will be on pin 5 (OC0A).
  //
  // This routine is designed for 36-40KHz; if you use it for other values, it's up to you
  // to make sure it gives reasonable results.  (Watch out for overflow / underflow / rounding.)
  // TIMER0 is used in phase-correct PWM mode, with OCR0A controlling the frequency and OCR0B
  // controlling the duty cycle.
  // There is no prescaling, so the output frequency is 16MHz / (2 * OCR0A)
  // To turn the output on and off, we leave the PWM running, but connect and disconnect the output pin.
  // A few hours staring at the ATmega documentation and this will all make sense.
  // See my Secrets of Arduino PWM at http://arcfn.com/2009/07/secrets-of-arduino-pwm.html for details.
  
  // Disable the Timer0 Interrupt (which is used for receiving IR)
  //TIMSK &= ~_BV(TOIE0); //Timer0 Overflow Interrupt
  
  // When not sending PWM, we want pin high (common annode)
  //PORTB &= ~(irOutMask);
  //PORTB &= ~(redMask);
  
  // COM2A = 00: disconnect OC2A
  // COM2B = 00: disconnect OC2B; to send signal set to 10: OC2B non-inverted
  // WGM2 = 101: phase-correct PWM with OCRA as top
  // CS2 = 000: no prescaling

  //TCCR0A = _BV(WGM00);
  //TCCR0B = _BV(WGM02) | _BV(CS00);
  TCCR0A = 0b01000010;  // COM0A1:0=01 to toggle OC0A on Compare Match
                        // COM0B1:0=00 to disconnect OC0B
                        // bits 3:2 are unused
                        // WGM01:00=10 for CTC Mode (WGM02=0 in TCCR0B)
  TCCR0B = 0b00000001;  // FOC0A=0 (no force compare)
                        // F0C0B=0 (no force compare)
                        // bits 5:4 are unused
                        // WGM2=0 for CTC Mode (WGM01:00=10 in TCCR0A)
                        // CS02:00=001 for divide by 1 prescaler (this starts Timer0)
  //OCR0A = SYSCLOCK / 2 / khz / 1000;
  OCR0A = 104;  // to output 38,095.2KHz on OC0A (PB0, pin 5)
  //OCR0B = OCR0A / 3; // 33% duty cycle

}

void sendNEC(unsigned long data, int nbits)
{
    PORTB |= rgbMask; // turns off RGB
    PORTB ^= grnMask; // turns on green
    enableIROut(38); // put timer into send mode
    mark(NEC_HDR_MARK);
    space(NEC_HDR_SPACE);
    for (int i = 0; i < nbits; i++) {
        if (data & TOPBIT) {
            mark(NEC_BIT_MARK);
            space(NEC_ONE_SPACE);
        } else {
            mark(NEC_BIT_MARK);
            space(NEC_ZERO_SPACE);
        }
        data <<= 1;
    }
    mark(NEC_BIT_MARK);
    space(0);
    enableIRIn(); // switch back to recv mode
    PORTB |= rgbMask; // turns off RGB
}

// initialization
void enableIRIn(void) {
  // setup pulse clock timer interrupt
  TCCR0A = 0;  // normal mode

  //Prescale /8 (8M/8 = 1 microseconds per tick)
  // Therefore, the timer interval can range from 1 to 256 microseconds
  // depending on the reset value (255 to 0)
  //cbi(TCCR0B,CS02);
  TCCR0B &= ~(_BV(CS02));
  //sbi(TCCR0B,CS01);
  TCCR0B |= _BV(CS01);
  //cbi(TCCR0B,CS00);
  TCCR0B &= ~(_BV(CS00));

  //Timer0 Overflow Interrupt Enable
  TIMSK |= _BV(TOIE0);

  RESET_TIMER0;

  //sei();  // enable interrupts

  // initialize state machine variables
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen = 0;
  irparams.blinkflag = 1;

  // set pin modes
  //pinMode(irparams.recvpin, INPUT);
}

int decodeNEC(decode_results *results) {
  long data = 0;
  int offset = 1; // Skip first space
  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], NEC_HDR_MARK)) {
    return ERR;
  }
  offset++;
  // Check for repeat
  ///*
  if (irparams.rawlen == 4 &&
    MATCH_SPACE(results->rawbuf[offset], NEC_RPT_SPACE) &&
    MATCH_MARK(results->rawbuf[offset+1], NEC_BIT_MARK)) {
    results->bits = 0;
    results->value = REPEAT;
    results->decode_type = NEC;
    return DECODED;
  }
  //*/
  if (irparams.rawlen < 2 * NEC_BITS + 4) {
    return ERR;
  }
  // Initial space  
  if (!MATCH_SPACE(results->rawbuf[offset], NEC_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  for (int i = 0; i < NEC_BITS; i++) {
    if (!MATCH_MARK(results->rawbuf[offset], NEC_BIT_MARK)) {
      return ERR;
    }
    offset++;
    if (MATCH_SPACE(results->rawbuf[offset], NEC_ONE_SPACE)) {
      data = (data << 1) | 1;
    } 
    else if (MATCH_SPACE(results->rawbuf[offset], NEC_ZERO_SPACE)) {
      data <<= 1;
    } 
    else {
      return ERR;
    }
    offset++;
  }
  // Success
  results->bits = NEC_BITS;
  results->value = data;
  results->decode_type = NEC;
  return DECODED;
}

void resume(void) {
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen = 0;
}

// Decodes the received IR message
// Returns 0 if no data ready, 1 if data ready.
// Results of decoding are stored in results
int decode(decode_results *results) {
    results->rawbuf = irparams.rawbuf;
    results->rawlen = irparams.rawlen;
    if (irparams.rcvstate != STATE_STOP) {
        return ERR;
    }
    if (decodeNEC(results)) {
        return DECODED;
    }
    // Throw away and start over
    resume();
    return ERR;
}


ISR(PCINT0_vect) {
}

// TIMER0 interrupt code to collect raw data.
// Widths of alternating SPACE, MARK are recorded in rawbuf.
// Recorded in ticks of 50 microseconds.
// rawlen counts the number of entries recorded so far.
// First entry is the SPACE between transmissions.
// As soon as a SPACE gets long, ready is set, state switches to IDLE, timing of SPACE continues.
// As soon as first MARK arrives, gap width is recorded, ready is cleared, and new logging starts
ISR(TIMER0_OVF_vect) {

  RESET_TIMER0;
  
  unsigned char irdata = (PINB & irInMask) >> (irInPortBPin - 1);

  irparams.timer++; // One more 50us tick
  if (irparams.rawlen >= RAWBUF) {
    // Buffer overflow
    irparams.rcvstate = STATE_STOP;
  }

  switch(irparams.rcvstate) {
  case STATE_IDLE: // In the middle of a gap
    if (irdata == MARK) {
      if (irparams.timer < GAP_TICKS) {
        // Not big enough to be a gap.
        irparams.timer = 0;
        //PORTB ^= bluMask; 
      } else {
        // gap just ended, record duration and start recording transmission
        irparams.rawlen = 0;
        irparams.rawbuf[irparams.rawlen++] = irparams.timer;
        irparams.timer = 0;
        irparams.rcvstate = STATE_MARK;
        //PORTB ^= grnMask; delay_ten_us(1000); PORTB ^= grnMask; 
      }
    }
    break;

  case STATE_MARK: // timing MARK
    if (irdata == SPACE) {   // MARK ended, record time
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_SPACE;
    }
    break;

  case STATE_SPACE: // timing SPACE
    if (irdata == MARK) { // SPACE just ended, record it
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_MARK;
    } 
    else { // SPACE
      if (irparams.timer > GAP_TICKS) {
        // big SPACE, indicates gap between codes
        // Mark current code as ready for processing
        // Switch to STOP
        // Don't reset timer; keep counting space width
        irparams.rcvstate = STATE_STOP;
      } 
    }
    break;

  case STATE_STOP: // waiting, measuring gap
    if (irdata == MARK) { // reset gap timer
      irparams.timer = 0;
    }
    break;
  }

  if (irparams.blinkflag) {
    if (irdata == MARK) {
        //PORTB ^= bluMask;
        PORTB ^= bluMask; delay_ten_us(1); PORTB ^= bluMask;
    } else {
        //PORTB ^= bluMask; delay_ten_us(1); PORTB ^= bluMask;
    }
  }

}


int main(void) {

    TCCR0A = 0;
    TCCR0B = 0;
    PORTB = 0xff; // write all outputs high & internal resistors on inputs (turns led's off)
    
    // disable the Watch Dog Timer (since we won't be using it, this will save battery power)
    //MCUSR = 0b00000000;   // first step:   WDRF=0 (Watch Dog Reset Flag)
    //WDTCR = 0b00011000;   // second step:  WDCE=1 and WDE=1 (Watch Dog Change Enable and Watch Dog Enable)
    //WDTCR = 0b00000000;   // third step:   WDE=0
    // turn off power to the USI and ADC modules (since we won't be using it, this will save battery power)
    //PRR = 0b00000011;
    // disable all Timer interrupts
    //TIMSK = 0x00;         // setting a bit to 0 disables interrupts
    // set up the input and output pins (the ATtiny25 only has PORTB pins)
    DDRB = 0b00010111;  // setting a bit to 1 makes it an output, setting a bit to 0 makes it an input
                        //   PB5 (unused) is input
                        //   PB4 (Blue LED) is output
                        //   PB3 (IR detect) is input
                        //   PB2 (Green LED) is output
                        //   PB1 (Red LED) is output
                        //   PB0 (IR LED) is output
    PORTB = 0xFF;   // all PORTB output pins High (all LEDs off) 
                    // -- (if we set an input pin High it activates a 
                    // pull-up resistor, which we don't need, but don't care about either)
                    
    // set up PB3 so that a logic change causes an interrupt 
    // (this will happen when the IR detector goes from seeing 
    // IR to not seeing IR, or from not seeing IR to seeing IR)
    //GIMSK = 0b00100000;   // PCIE=1 to enable Pin Change Interrupts
    //PCMSK = 0b00001000;   // PCINT3 bit = 1 to enable Pin Change Interrupts for PB3

    // pretty boot light sequence.
    //startUp1();

    enableIRIn();
    sei();                // enable microcontroller interrupts

    long my_code = 0x530b87ee; // apple macbook remote, send menu command

    while (1==1) {

        //should take approx 1s
        // for (int i=0; i<730; i++) {
        for (int i=0; i<730; i++) {

            if ( decode(&my_results) == DECODED ) {
                // woo!
            }

            if ( my_results.decode_type == NEC ) {
                //if ( ( my_results.value & 0x00ffffff ) == APPLE_PLAY ) {
                long data = my_results.value;
                //if ( my_results.value != 0xffffffff ) {

                    for (int i=0; i<32; i++) {
                        if ( data & 1 ) {
                            PORTB |= rgbMask; // turns off RGB
                            PORTB ^= redMask; // turns on red
                            delay_ten_us(10000);
                            PORTB |= rgbMask; // turns off RGB
                            delay_ten_us(10000);
                        } else {
                            PORTB |= rgbMask; // turns off RGB
                            PORTB ^= bluMask; // turns on red
                            delay_ten_us(10000);
                            PORTB |= bluMask; // turns off RGB
                            delay_ten_us(10000);
                        }
                        data >>= 1;
                    }

                //}
                my_results.decode_type = NONE;
                resume(); // discard results, ready for next code
            }

            delay_ten_us(100);

        }

        // transmit our identity, without interruption
        disable_ir_recving();
        sendNEC(my_code, 32);  // takes ~70ms
        enable_ir_recving();

        //delay_ten_us(100000);

    }
    return 0;

}

