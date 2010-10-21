#include <avr/io.h>             // this contains all the IO port definitions
#include <avr/interrupt.h>      // definitions for interrupts
#include <avr/sleep.h>          // definitions for power-down modes
#include <avr/pgmspace.h>       // definitions or keeping constants in program memory
#include <stdbool.h>       // true and false!

// My Trippy RGB wave:
//   6 PB1 - R (sink)
//   7 PB2 - G (sink)
//   3 PB4 - B (sink)
//   5 PB0 - IR (sink)
//   2 PB3 - IR sensor

// Our board:
//   7 PB2 - R (sink)
//   5 PB0 - G (sink)
//   6 PB1 - B (sink)
//   3 PB4 - IR (source)
//   2 PB3 - IR sensor

// any variable changed by an interrupt *must* be
// defined as volatile
volatile unsigned char bob = 0x00;

ISR(TIMER1_OVF_vect) { 
    PORTB = 0b00010011;
}

int main(void)
{

    TCCR1 = (1 << CS00)|(1 << CS02);
    TIFR  = 1 << TOV1;
    TIMSK |= 1 << TOIE1;
    TCNT1 = 0; // timer counter

    sei();

    while (true) {
        // Set output (PB) pins
        DDRB =  0b00010111;

        // turn on/off pins in PORTB
        PORTB =  0b00010100;
    }

}

void enableIROut(int khz) {

  // Enables IR output.  The khz value controls the modulation frequency in kilohertz.
  // The IR output will be on pin 3 (OC2B).
  // This routine is designed for 36-40KHz; if you use it for other values, it's up to you
  // to make sure it gives reasonable results.  (Watch out for overflow / underflow / rounding.)
  // TIMER2 is used in phase-correct PWM mode, with OCR2A controlling the frequency and OCR2B
  // controlling the duty cycle.
  // There is no prescaling, so the output frequency is 16MHz / (2 * OCR2A)
  // To turn the output on and off, we leave the PWM running, but connect and disconnect the output pin.
  // A few hours staring at the ATmega documentation and this will all make sense.
  // See my Secrets of Arduino PWM at http://arcfn.com/2009/07/secrets-of-arduino-pwm.html for details.

  
  // Disable the Timer2 Interrupt (which is used for receiving IR)
  TIMSK2 &= ~_BV(TOIE2); //Timer2 Overflow Interrupt
  
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW); // When not sending PWM, we want it low
  
  // COM2A = 00: disconnect OC2A
  // COM2B = 00: disconnect OC2B; to send signal set to 10: OC2B non-inverted
  // WGM2 = 101: phase-correct PWM with OCRA as top
  // CS2 = 000: no prescaling
  TCCR2A = _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);

  // The top value for the timer.  The modulation frequency will be SYSCLOCK / 2 / OCR2A.
  OCR2A = SYSCLOCK / 2 / khz / 1000;
  OCR2B = OCR2A / 3; // 33% duty cycle
}

