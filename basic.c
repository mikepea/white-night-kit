#include <avr/io.h>             // this contains all the IO port definitions
#include <avr/interrupt.h>      // definitions for interrupts
#include <avr/sleep.h>          // definitions for power-down modes
#include <avr/pgmspace.h>       // definitions or keeping constants in program memory

int main(void)
{
  // Set Port B pins as all outputs
  DDRB = 0xff;

  // Set all Port B pins as HIGH
  PORTB = 0xff;

  return 1;
}
