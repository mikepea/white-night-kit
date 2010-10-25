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

#define CLKFUDGE 5        // fudge factor for clock interrupt overhead
#define CLK 256           // max value for clock (timer 0)
#define PRESCALE 8        // timer0 clock prescale
#define SYSCLOCK 8000000  // main clock speed
#define CLKSPERUSEC (SYSCLOCK/PRESCALE/1000000)   // timer clocks per microsecond
#define USECPERTICK 50  // microseconds per clock interrupt tick

#define INIT_TIMER_COUNT0 (CLK - USECPERTICK*CLKSPERUSEC + CLKFUDGE)
#define RESET_TIMER0 TCNT0 = INIT_TIMER_COUNT0

// IR detector output is active low
#define MARK  0
#define SPACE 1

#define NBITS 32         // bits in IR code

#define TOPBIT 0x80000000

#define TOLERANCE 25  // percent tolerance in measurements
#define LTOL (1.0 - TOLERANCE/100.) 
#define UTOL (1.0 + TOLERANCE/100.) 

#define STARTNOM      9000
#define SPACENOM      4500
#define BITMARKNOM    620
#define ONESPACENOM   1600
#define ZEROSPACENOM  480
#define RPTSPACENOM   2180

#define NEC_HDR_MARK	9000
#define NEC_HDR_SPACE	4500
#define NEC_BIT_MARK	560
#define NEC_ONE_SPACE	1600
#define NEC_ZERO_SPACE	560
#define NEC_RPT_SPACE	2250

#define TICKS_LOW(us) (int) (((us)*LTOL/USECPERTICK))
#define TICKS_HIGH(us) (int) (((us)*UTOL/USECPERTICK + 1))

// pulse parameters (tick counts)
#define STARTMIN (int)((STARTNOM/USECPERTICK)*LTOL) // start MARK
#define STARTMAX (int)((STARTNOM/USECPERTICK)*UTOL) 
#define SPACEMIN (int)((SPACENOM/USECPERTICK)*LTOL) 
#define SPACEMAX (int)((SPACENOM/USECPERTICK)*UTOL) 
#define BITMARKMIN (int)((BITMARKNOM/USECPERTICK)*LTOL-2) // extra tolerance for low counts
#define BITMARKMAX (int)((BITMARKNOM/USECPERTICK)*UTOL+2) 
#define ONESPACEMIN (int)((ONESPACENOM/USECPERTICK)*LTOL) 
#define ONESPACEMAX (int)((ONESPACENOM/USECPERTICK)*UTOL) 
#define ZEROSPACEMIN (int)((ZEROSPACENOM/USECPERTICK)*LTOL-2) 
#define ZEROSPACEMAX (int)((ZEROSPACENOM/USECPERTICK)*UTOL+2) 
#define RPTSPACEMIN (int)((RPTSPACENOM/USECPERTICK)*LTOL) 
#define RPTSPACEMAX (int)((RPTSPACENOM/USECPERTICK)*UTOL) 

// receiver states
#define IDLE     1
#define STARTH   2
#define STARTL   3
#define BIT      4
#define ONE      5
#define ZERO     6
#define STOP     7
#define BITMARK  8
#define RPTMARK  9

// macros
#define nextstate(X) (irparams.rcvstate = X)

// Marks tend to be 100us too long, and spaces 100us too short
// when received due to sensor lag.
#define MARK_EXCESS 100

#define MATCH(measured_ticks, desired_us) ((measured_ticks) >= TICKS_LOW(desired_us) && (measured_ticks) <= TICKS_HIGH(desired_us))
#define MATCH_MARK(measured_ticks, desired_us) MATCH(measured_ticks, (desired_us) + MARK_EXCESS)
#define MATCH_SPACE(measured_ticks, desired_us) MATCH((measured_ticks), (desired_us) - MARK_EXCESS)

// results definitions
#define ERR 0
#define DECODED 1

// repeat code for NEC
#define REPEAT 0xffffffff

// Values for decode_type
#define NONE 0
#define NEC 1



#define _GAP 5000 // Minimum gap between transmissions
#define GAP_TICKS (_GAP/USECPERTICK)

#define MAXBUF 8       // IR command code buffer length (circular buffer)

// state machine variables irparams
static volatile struct {
  char rcvstate ;          // IR receiver state
  char bitcounter ;        // bit counter
  char irdata ;            // MARK or SPACE read from IR input pin
  char fptr ;              // irbuf front pointer
  char rptr ;              // irbuf rear pointer
  char irpin ;             // pin for IR data from detector
  char blinkflag ;         // TRUE to enable blinking of pin 13 on IR processing
  unsigned int timer ;     // state timer
  unsigned long irmask ;   // one-bit mask for constructing IR code
  unsigned long ircode ;   // IR code
  unsigned long irbuf[MAXBUF] ;    // circular buffer for IR codes
} irparams ;

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
  irparams.rcvstate = IDLE ;
  irparams.bitcounter = 0 ;
  irparams.ircode = 0 ;
  irparams.fptr = 0 ;
  irparams.rptr = 0 ;
  irparams.blinkflag = 1 ;

  // set pin modes
  //pinMode(irparams.recvpin, INPUT);
}

ISR(PCINT0_vect) {
}


/*
ISR(TIMER0_OVF_vect) {
  RESET_TIMER0;
  if ( irparams.timer > 5000 ) {
      irparams.timer = 0;
      PORTB ^= bluMask; delay_ten_us(1000); PORTB ^= bluMask; 
  } 
  irparams.timer++;
}
*/

// Recorded in ticks of 50 microseconds.

ISR(TIMER0_OVF_vect) {

  RESET_TIMER0;

  irparams.irdata = (PINB & irInMask) >> (irInPortBPin - 1);

  if ( irparams.timer > 21 ) {
          //PORTB ^= bluMask; delay_ten_us(1); PORTB ^= bluMask; 
  }

  // process current state
  switch(irparams.rcvstate) {
    case IDLE:
      if (irparams.irdata == MARK) {  // got some activity
          nextstate(STARTH) ;
          irparams.timer = 0 ;
      }
      break ;
    case STARTH:   // looking for initial start MARK
      // entered on MARK
      if (irparams.irdata == SPACE) {   // MARK ended, check time
        if ((irparams.timer >= STARTMIN) && (irparams.timer <= STARTMAX)) {
          nextstate(STARTL) ;  // time OK, now look for start SPACE
          irparams.timer = 0 ;
          PORTB ^= bluMask; delay_ten_us(1); PORTB ^= bluMask; 
        }
        else {
          nextstate(IDLE) ;  // bad MARK time, go back to IDLE
        }
      }
      else {
        irparams.timer++ ;  // still MARK, increment timer
      }
      break ;
    case STARTL:
      // entered on SPACE
      if (irparams.irdata == MARK) {  // SPACE ended, check time
        if ((irparams.timer >= SPACEMIN) && (irparams.timer <= SPACEMAX)) {
          nextstate(BITMARK) ;  // time OK, check first bit MARK 
          irparams.timer = 0 ;
          irparams.bitcounter = 0 ;  // initialize ircode vars
          irparams.irmask = (unsigned long)0x1 ;
          irparams.ircode = 0 ;
        }
        else if ((irparams.timer >= RPTSPACEMIN) && (irparams.timer <= RPTSPACEMAX)) {  // not a start SPACE, maybe this is a repeat signal
          nextstate(RPTMARK) ;   // yep, it's a repeat signal
          irparams.timer = 0 ;
        }
        else
          nextstate(IDLE) ;  // bad start SPACE time, go back to IDLE
      }
      else {   // still SPACE
        irparams.timer++ ;    // increment time
        if (irparams.timer >= SPACEMAX)  // check against max time for SPACE
          nextstate(IDLE) ;  // max time exceeded, go back to IDLE
      }
      break ;
    case RPTMARK:
      irparams.timer++ ;  // measuring MARK
      if (irparams.irdata == SPACE) {  // MARK ended, check time
        if ((irparams.timer >= BITMARKMIN) && (irparams.timer <= BITMARKMAX))
          nextstate(IDLE) ;  // repeats are ignored here, just go back to IDLE
        else
          nextstate(IDLE) ;  // bad repeat MARK time, go back to IDLE
      }
      break ;
    case BITMARK:
      irparams.timer++ ;   // timing MARK
      if (irparams.irdata == SPACE) {   // MARK ended, check time
        if ((irparams.timer < BITMARKMIN) || (irparams.timer > BITMARKMAX))
          nextstate(IDLE) ;  // bad MARK time, go back to idle
        else {
          irparams.rcvstate = BIT ;  // MARK time OK, go to BIT
          irparams.timer = 0 ;
        }
      }
      break ;
    case BIT:
      irparams.timer++ ; // measuring SPACE
      if (irparams.irdata == MARK) {  // bit SPACE ended, check time
        if ((irparams.timer >= ONESPACEMIN) && (irparams.timer <= ONESPACEMAX)) {
          nextstate(ONE) ;   // SPACE matched ONE timing
          irparams.timer = 0 ;
        }
        else if ((irparams.timer >= ZEROSPACEMIN) && (irparams.timer <= ZEROSPACEMAX)) {
          nextstate(ZERO) ;  // SPACE matched ZERO timimg
          irparams.timer = 0 ;
        }
        else
          nextstate(IDLE) ;  // bad SPACE time, go back to IDLE
      }
      else {  // still SPACE, check against max time
        if (irparams.timer > ONESPACEMAX)
          nextstate(IDLE) ;  // SPACE exceeded max time, go back to IDLE
      }
      break ;
    case ONE:
      irparams.ircode |= irparams.irmask ;  // got a ONE, update ircode
      irparams.irmask <<= 1 ;  // set mask to next bit
      irparams.bitcounter++ ;  // update bitcounter
      if (irparams.bitcounter < NBITS)  // if not done, look for next bit
        nextstate(BITMARK) ;
      else
        nextstate(STOP) ;  // done, got NBITS, go to STOP
      break ;
    case ZERO:
      irparams.irmask <<= 1 ;  // got a ZERO, update mask
      irparams.bitcounter++ ;  // update bitcounter
      if (irparams.bitcounter < NBITS)  // if not done, look for next bit
        nextstate(BITMARK) ;
      else
        nextstate(STOP) ;  // done, got NBITS, go to STOP
      break ;
    case STOP:
      irparams.timer++ ;  //measuring MARK
      if (irparams.irdata == SPACE) {  // got a SPACE, check stop MARK time
        if ((irparams.timer >= BITMARKMIN) && (irparams.timer <= BITMARKMAX)) {
          // time OK -- got an IR code
          irparams.irbuf[irparams.fptr] = irparams.ircode ;   // store code at fptr position
          irparams.fptr = (irparams.fptr + 1) % MAXBUF ; // move fptr to next empty slot
        }
        nextstate(IDLE) ;  // finished with this code, go back to IDLE
      }
      break ;
  }
  // end state processing

  if (irparams.blinkflag) {
    //if (irparams.irdata == MARK) {
    if (irparams.bitcounter > 30 ) {
        //PORTB ^= bluMask; delay_ten_us(1); PORTB ^= bluMask;
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

            /*
            if ( ( irparams.irbuf[0] & 0x00ffffff ) == APPLE_PLAY ) {
                // woo!
            }
            */

            //if ( my_results.decode_type == NEC ) {
            if ( irparams.irbuf[0] ) {
                //if ( ( my_results.value & 0x00ffffff ) == APPLE_PLAY ) {
                long data = irparams.irbuf[0];
                //if ( my_results.value != 0xffffffff ) {

                    for (int i=0; i<32; i++) {
                        if ( data & 1 ) {
                            PORTB |= rgbMask; // turns off RGB
                            PORTB ^= redMask; // turns on red
                            delay_ten_us(50000);
                            PORTB |= rgbMask; // turns off RGB
                            delay_ten_us(50000);
                        } else {
                            PORTB |= rgbMask; // turns off RGB
                            PORTB ^= bluMask; // turns on red
                            delay_ten_us(50000);
                            PORTB |= bluMask; // turns off RGB
                            delay_ten_us(50000);
                        }
                        data >>= 1;
                    }

                //}
                irparams.irbuf[0] = 0;
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

