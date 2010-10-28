/*
 * white_night_code
 *
 * Copyright 2011 BuildBrighton (Mike Pountney, Matthew Edwards)
 * For details, see http://github.com/mikepea/white-night-code
 *
 * Interrupt code based on NECIRrcv by Joe Knapp, IRremote by Ken Shirriff
 *    http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1210243556
 */

#define MY_ID   0x01000000
#define ID_MASK 0xff000000


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

// Apple codes
#define APPLE_PLAY              0x5c0487ee
#define APPLE_VOLUME_UP         0x5c0b87ee
#define APPLE_VOLUME_DOWN       0x5c0d87ee
#define APPLE_NEXT_TRACK        0x5c0787ee
#define APPLE_PREV_TRACK        0x5c0887ee
#define APPLE_MENU              0x5c0287ee

// delay for flashing out the recd code
#define IR_DATA_PRINT_DELAY 50000


// for matt's design
#define bogusMask    0b00100000
#define redMask      0b00000100
#define grnMask      0b00000010
#define bluMask      0b00000001
#define rgbMask      0b00000111
#define irInMask     0b00001000
#define irOutMask    0b00010000
// 4 = PB3
#define irInPortBPin  4

#define RED 0
#define GREEN 1
#define BLUE 2

// how many times to send our IR code in each 1s loop.
#define NUM_SENDS 1

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

#define NEC_HDR_MARK    9000
#define NEC_HDR_SPACE   4500
#define NEC_BIT_MARK    560
#define NEC_ONE_SPACE   1600
#define NEC_ZERO_SPACE  560
#define NEC_RPT_SPACE   2250

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

void delay_ten_us(unsigned int us) {
  unsigned int count;
  const unsigned int DelayCount=8;

  while (us != 0) {
    for (count=0; count <= DelayCount; count++) {
            PINB |= bogusMask;
    }
    us--;
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
  GTCCR |= _BV(COM1B0);  // turn on OC1B PWM output
  delay_ten_us(time / 10);
}

/* Leave pin off for time (given in microseconds) */
void space(int time) {
  // Sends an IR space for the specified number of microseconds.
  // A space is no output, so the PWM output is disabled.
  GTCCR &= ~(_BV(COM1B0));  // turn on OC1B PWM output
  delay_ten_us(time / 10);
}


void enableIROut(int khz) {

  TCCR1 = _BV(CS10);  // turn on clock, prescale = 1
  GTCCR = _BV(PWM1B) | _BV(COM1B0);  // toggle OC1B on compare match; PWM mode on OCR1C/B.
  OCR1C = 210;
  OCR1B = 70;

}

void sendNEC(unsigned long data, int nbits)
{
    PORTB |= rgbMask; // turns off RGB
    PORTB ^= grnMask; // turns on green
    enableIROut(38); // put timer into send mode
    mark(NEC_HDR_MARK);
    space(NEC_HDR_SPACE);
    for (int i = 0; i < nbits; i++) {
        if (data & 1) {
            mark(NEC_BIT_MARK);
            space(NEC_ONE_SPACE);
        } else {
            mark(NEC_BIT_MARK);
            space(NEC_ZERO_SPACE);
        }
        data >>= 1;
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
        } else {
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

}

/*
void flash_ircode(long data) {

    for (int i=0; i<32; i++) {
        if ( data & 1 ) {
            PORTB |= rgbMask; // turns off RGB
            PORTB ^= redMask; // turns on red
            delay_ten_us(IR_DATA_PRINT_DELAY);
            PORTB |= rgbMask; // turns off RGB
            delay_ten_us(IR_DATA_PRINT_DELAY);
        } else {
            PORTB |= rgbMask; // turns off RGB
            PORTB ^= bluMask; // turns on red
            delay_ten_us(IR_DATA_PRINT_DELAY);
            PORTB |= bluMask; // turns off RGB
            delay_ten_us(IR_DATA_PRINT_DELAY);
        }
        data >>= 1;
    }

}
*/

int main(void) {

    // zero our timer controls, for now
    TCCR0A = 0;
    TCCR0B = 0;
    TCCR1 = 0;
    GTCCR = 0;

    DDRB =  (rgbMask) | ( irOutMask );

    // all PORTB output pins High (all LEDs off), except for the
    // IR LED, which is SOURCE not SINK
    PORTB = ( 0xFF & ~irOutMask );
                    // -- (if we set an input pin High it activates a
                    // pull-up resistor, which we don't need, but don't care about either)

    // pretty boot light sequence.
    //startUp1();

    enableIRIn();
    sei();                // enable microcontroller interrupts


    char colour[] = { 0, 0, 0 };
    long my_code      = MY_ID;   // ID, plus blank colour

    while (1==1) {

        disable_ir_recving();
        for (int i=0; i<NUM_SENDS; i++) {
           // transmit our identity, without interruption
           sendNEC(my_code, 32);  // takes ~68ms
           //delay_ten_us(3280); // delay for 32ms
        }
        enable_ir_recving();

        // loop a number of times, to have ~1s of recving/game logic
        for (int i=0; i<730; i++) {

            for (int j=0; j<MAXBUF; j++) {
                if (irparams.irbuf[j]) {


                    //flash_ircode(irparams.irbuf[j]);
                    if ( irparams.irbuf[j] == APPLE_VOLUME_UP ) {
                        colour[RED] ^= 1;
                    } else if ( ( irparams.irbuf[j] & ~ID_MASK ) == ( APPLE_NEXT_TRACK & ~ID_MASK ) ) {
                        colour[GREEN] ^= 1;
                    } else if ( irparams.irbuf[j] == APPLE_VOLUME_DOWN ) {
                        colour[BLUE] ^= 1;
                    } else if ( irparams.irbuf[j] == APPLE_PLAY ) {
                        colour[RED] = 1;
                        colour[GREEN] = 1;
                        colour[BLUE] = 1;
                    } else if ( irparams.irbuf[j] == APPLE_MENU ) {
                        colour[RED] = 0;
                        colour[GREEN] = 0;
                        colour[BLUE] = 0;
                    } else {
                        colour[RED] = ( irparams.irbuf[j] & ( 1 << RED ) ) >> RED;
                        colour[GREEN] = ( irparams.irbuf[j] & ( 1 << GREEN ) ) >> GREEN;
                        colour[BLUE] = ( irparams.irbuf[j] & ( 1 << BLUE ) ) >> BLUE;
                    }
                    irparams.irbuf[j] = 0;

                }
            }

            if ( colour[RED] ) {
                PORTB &= ~redMask;
            } else {
                PORTB |= redMask;
            }
            if ( colour[GREEN] ) {
                PORTB &= ~grnMask;
            } else {
                PORTB |= grnMask;
            }
            if ( colour[BLUE] ) {
                PORTB &= ~bluMask;
            } else {
                PORTB |= bluMask;
            }

            delay_ten_us(100);

        }


        my_code = MY_ID | colour[RED] << RED | colour[GREEN] << GREEN | colour[BLUE] << BLUE;


    }
    return 0;

}

