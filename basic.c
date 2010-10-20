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

ISR(TIM1_OVF_vect) { 
    PORTB ^= 0b00000010;
}

int main(void)
{

    OCR1A  = 0x00;              //Set OCR1A 
    TCCR1 = (1 << CS12)|(0 << CS11)|(1 << CS10); // WGM1=4, prescale at 1024 
    TIMSK |= (1 << OCIE1A);         //Set bit 6 in TIMSK to enable Timer 1 compare interrupt. 

    while (true) {
        // Set output (PB) pins
        DDRB =  0b00010111;

        // turn on green
        PORTB = 0b00010011;
    }

}
