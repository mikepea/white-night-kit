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
