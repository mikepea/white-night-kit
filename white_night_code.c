// the below should generate a 38k ir signal.
// I have more somewhere with the ir pulsing separate [so it works summink like sendIr(HIGH|LOW);
// pulse @ 38k for 6 cycles [low] or 12 cycls [high]].
// Just loops through a byte sending out each bit

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

// for matt's design
//#define redMask  0b00000010
//#define grnMask  0b00010000
//#define bluMask  0b00000100
//#define irInMask     0b00001000
//#define irOutMask    0b00000001
//#define irInPortBPin  4;

// for trippy RGB wave
#define bogusMask 0b00100000
#define redMask  0b00000010
#define grnMask  0b00000100
#define bluMask  0b00010000
#define irInMask     0b00001000
#define irOutMask    0b00000001
#define irInPortBPin  4

// The length of tocks to wait to get a 38KHz signal
#define IR_PULSE_DELAY  26
// for debugging, extend delay to be visible on RGB led:
//#define IR_PULSE_DELAY  10000

// number of 38Khz pulses in a given period
#define NEC_NUM_PULSES_4500US  176
#define NEC_NUM_PULSES_560US   22
//#define NEC_NUM_PULSES_4500US  17600
//#define NEC_NUM_PULSES_560US   2200

// IR detector output is active low
#define MARK  0
#define SPACE 1

#define TOPBIT 0x80000000

// pulse parameters in 10s of usec
#define NEC_HDR_MARK    900
#define NEC_HDR_SPACE   450
#define NEC_BIT_MARK    56
#define NEC_ONE_SPACE   160
#define NEC_ZERO_SPACE  56
#define NEC_RPT_SPACE   225

void delay_ten_us(unsigned long int us) {
  unsigned long int count;
  const unsigned long int DelayCount=6;

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

void send_38khz_pulse(void) {
    PORTB ^= irOutMask;
#ifdef DEBUG
    PORTB ^= redMask;
#endif
    delay_x_us(IR_PULSE_DELAY);
    PORTB ^= irOutMask;
#ifdef DEBUG
    PORTB ^= redMask;
#endif
    delay_x_us(IR_PULSE_DELAY);
}

void send_38khz_space(void) {
    PORTB ^= bogusMask;
#ifdef DEBUG
    PORTB ^= bluMask;
#endif
    delay_x_us(IR_PULSE_DELAY);
    PORTB ^= bogusMask;
#ifdef DEBUG
    PORTB ^= bluMask;
#endif
    delay_x_us(IR_PULSE_DELAY);
}

void send_38khz_green_space(void) {
    PORTB ^= bogusMask;
#ifdef DEBUG
    PORTB ^= grnMask;
#endif
    delay_x_us(IR_PULSE_DELAY);
    PORTB ^= bogusMask;
#ifdef DEBUG
    PORTB ^= grnMask;
#endif
    delay_x_us(IR_PULSE_DELAY);
}

void send_nec_header(void) {

    for (int i = 0; i < NEC_NUM_PULSES_4500US * 2; i++) {
        send_38khz_pulse();
    }

    for (int i = 0; i < NEC_NUM_PULSES_4500US; i++) {
        send_38khz_space();
    }

}

void send_nec_one(void) {

    for (int i = 0; i < NEC_NUM_PULSES_560US * 1; i++) {
        send_38khz_pulse();
    }

    for (int i = 0; i < NEC_NUM_PULSES_560US * 3; i++) {
        send_38khz_space();
    }

}

void send_nec_zero(void) {

    for (int i = 0; i < NEC_NUM_PULSES_560US * 1; i++) {
        send_38khz_pulse();
    }

    for (int i = 0; i < NEC_NUM_PULSES_560US * 1; i++) {
        send_38khz_green_space();
    }

}

void send_nec_byte(char data) {
    // NEC protocol sends LSB first.
    // 1 = on for 560us, off for 3x560us
    // 0 = on for 560us, off for 1x560us
    for (int pos = 0; pos < 8; pos++) {
        if ((data & 1) == 1 ) {
            send_nec_one();
        } else if ( (data & 1) == 0 ) {
            send_nec_zero();
        }
        data >>= 1;
    }
    //nec_mark();
    //space(0);
}

void send_nec_ir_code(char address, char command) {
    send_nec_header();
    send_nec_byte(address);
    send_nec_byte(~address);
    send_nec_byte(command);
    send_nec_byte(~command);
}

void send_my_ir_code(char id, char code) {
    send_nec_ir_code(id, code);
}

// receiver state machine states
#define STATE_IDLE     2
#define STATE_MARK     3
#define STATE_SPACE    4
#define STATE_STOP     5

#define INIT_TIMER_COUNT0 (CLK - USECPERTICK*CLKSPERUSEC + CLKFUDGE)
#define RESET_TIMER0 TCNT0 = INIT_TIMER_COUNT0
#define CLKFUDGE 5        // fudge factor for clock interrupt overhead
#define CLK 256           // max value for clock (timer 2)
#define PRESCALE 8        // timer2 clock prescale
#define SYSCLOCK 8000000  // main clock speed
#define CLKSPERUSEC (SYSCLOCK/PRESCALE/1000000)   // timer clocks per microsecond
#define _GAP 5000 // Minimum gap between transmissions
#define GAP_TICKS (_GAP/USECPERTICK)

#define USECPERTICK 50  // microseconds per clock interrupt tick
#define RAWBUF 76 // Length of raw duration buffer

// Marks tend to be 100us too long, and spaces 100us too short
// when received due to sensor lag.
#define MARK_EXCESS 100

// information for the interrupt handler
typedef struct {
  uint8_t recvpin;           // pin for IR data from detector
  uint8_t rcvstate;          // state machine
  uint8_t blinkflag;         // TRUE to enable blinking of pin on IR processing
  unsigned int timer;        // state timer, counts 50uS ticks.
  unsigned int rawbuf[RAWBUF]; // raw data
  uint8_t rawlen;            // counter of entries in rawbuf
} 
irparams_t;

volatile irparams_t irparams;

// initialization
void enableIRIn() {
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
  irparams.blinkflag = 0;

  // set pin modes
  //pinMode(irparams.recvpin, INPUT);
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
      PORTB ^= redMask; delay_ten_us(1); PORTB ^= redMask;
    } 
    else {
      PORTB ^= bluMask; delay_ten_us(1); PORTB ^= bluMask;
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
    startUp1();

    enableIRIn();
    sei();                // enable microcontroller interrupts

    while (1==1) {

        // turn off interrupts on our IR sensor
        //PCMSK = 0b00000000;   // PCINT3 bit = 1 to enable Pin Change Interrupts for PB3

        // turn off RGB LED
        //PORTB &= ~( redMask + bluMask + grnMask );
        //PORTB &= 0b11101000;

        PORTB ^= bluMask;
        delay_ten_us(10000);
        PORTB ^= bluMask;

        // transmit our identity
        send_my_ir_code(0x8d, 0x23);
 
        // turn on interrupts on our IR sensor
        //PCMSK = 0b00001000;   // PCINT3 bit = 1 to enable Pin Change Interrupts for PB3

        // send patterns, wait for a second.

#ifdef DEBUG
        // debugging
        PORTB ^= grnMask;
        delay_ten_us(10000);
        PORTB ^= grnMask;
#endif

        delay_ten_us(100000);

    }
    return 0;

}

