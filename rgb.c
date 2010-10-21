/*
RGB Wave
Firmware
for use with ATtiny25
AS220
Mitch Altman
09-Mar-09 -- RBG instead of RGB

Distributed under Creative Commons 3.0 -- Attib & Share Alike
*/


/*
For this project, imagine a bunch of little lights (maybe 20 or 40 of them),
on a table, each about the size of a chess piece.
Each is independent of the other.
You arrange them around on the table any way you want.
Each one continually slowly changes colors on its own.
When you wave your hand over them, it creates waves of colors that follow your hand.
*/


/*
I added an IR emitter and IR detector to the Trippy RGB Light to create this project.
Then converted the firmware to work with an ATtiny25 instead of an ATtiny2313 (because
it is smaller and cheaper).
The Trippy RGB Light uses 3 LEDs, one Red, one Green, one Blue, each fading at different
rates so that when the light from the three LEDs add together, you get a lot of changing
colors (you can get any color a computer monitor can get using different brightnesses of
Red, Green, and Blue light -- this is the way computer monitors create color).  Trippy!

For this project instead of three LEDs, I used one RGB LED (which is one package
containing a Red, a Green, and a Blue LED, all in one package).

The project uses one IR emitter and one IR detector.

The IR detector I chose is very sensitive to IR light that is pulsing at about 38KHz,
and it is very insensitive to IR light that is not pulsing near 38KHz.

The IR emitter is pointed up, and frequently outputs IR light pusling at about 38KHz.
The IR detector is also pointed up, and has one output,
which is Low when it sees 38KHz IR light reflecting from above
(such as when you wave your hand over it, thus reflecting the IR light from the IR emitter),
and the output is High when it does not see 38KHz IR light.

Since the output of the IR detector is connected to an input pin, when the output from the
IR detector goes Low, the firmware can detect this, and restart the firmware from the
beginning, and the net effect is that you see a wave of colors move under you hand
(assuming that there are a lot of these devices near each other on a table).
*/


/*
This project provides a good example of how to use Pulse Width Modulation (PWM)
to fade a voltage up or down (in this case, the changing voltage is used to
control the brightness of Red, Green, and Blue LEDs).

We will use one of the two hardware timers that are embedded inside of the ATtiny25,
set up to control PWM for the Red LED and the Blue LED.  Since there are only two
timer compare regsiters on the one hardware timer, we will create PWM for the Green LED
by manually pulsing it in a firmware loop.
(The other hardware timer will be used to pulse an IR emitter at 38KHz.)

With PWM, we vary the amount of time an LED is on versus how long it is off.
We repeatedly turn an LED on and off very quickly (so quickly you can't see it blink).
If the LED is on 50% of the time (and off for 50% of the time),
  it will be half as bright as if it were on all of the time.
If the LED is on 25% of the time (and off for 75% of the time),
  it will be one quarter as bright as if it were on all of the time.
Each time we turn the LED on and off, the amount of time it is on,
  added to the amount of time it is off, always equals the same length of time
  (this length of time is known as the "PWM period").
It doesn't really matter what the PWM period is,
  as long as it is short enough that we can't perceive the the LED flicker.
  We can perceive flickering if an LED blinks slower than about 1/30 of a second,
  or about 0.033 seconds (or 30Hz).
  In this firmware, I keep the PWM period to less than 0.01 seconds (faster than 100Hz)
    so there is no perceieved flicker (though it is interesting to wave the unit back
    and forth while it is running, and you can actually see the PWM pusle widths widen
    and shrink over time).
*/


/*
This project also provides a simple example of how to use interrupts.

An interrupt is a function like any other, except rather than being called from
somewhere in the firmware, it is called by a hardware event.  In the case of this
firmware, the interrupt function is called whenever the IR detector sees IR light.

An interrupt function is conventionally called "Interrupt Service Routine", or "ISR".

The IR detector is connected to PB3 (set up as an input pin).  PB3 can be set up
so that whenever it sees a logic change from Low to High or from High to Low, it
calls an ISR named PCINT0.  Below you will see the following code:
     ISR(PCINT0_vect) {
       Start_Over = 1;
     }
This is the ISR.  All it does is set a variable that the main routine will see, and
when the main routine sees it, it jumps to the beginning to start the firmware over.

To enable the PCINT0 interrupt, we need to do a few things.  First of all, we need
to include the header for interrupt handling.  You will see the following include
statement, below, to handle that:
     include <avr/interrupt.h>
Next we need to set up two control registers in the ATtiny25 to tell it to generate
interrupts for PB3 logic change (the AVR datasheet calls this a "Pin Change
Interrupt").  You will see the following two lines in the main function, below:
  GIMSK = 0b00100000;   // PCIE=1 to enable Pin Change Interrupts
  PCMSK = 0b00001000;   // PCINT3 bit = 1 to enable Pin Change Interrupts for PB3

Finally, we need to enable the interrupts for the ATtiny25.  This line accomplishes
that:
  sei();  // enable microcontroller interrupts

When we are finished, before putting the microcontroller to sleep, we disable
interrupts with the following line of code:
  cli();  // disable microcontroller interrupts
*/


/*
A very brief tutorial about binary numbers, bits, bytes, and regsiters:
----------------------------------------------------------------------

You do not need to know any of this to build and use this project.  You don't even
need to know any of this to hack the firmware.  But this may be helpful if you want
to do some more in depth hacking of the firmware.  And besides, the more you learn,
the more cool stuff you will be able to do!  So, check it out...

In decimal numbers (the ones we normally use in our every-day lives), numbers have
"digits".  For example, the number 23 has two digits.  Since decimal numbers are
base-10, the "2" in the number 23 is in the "ten" position, meaning we have two
"tens".  The "3" is in the "one" position, meaning we have three "ones".  Putting
all that together:  we have 2 tens, plus three ones, to come up with 23.

In binary numbers (the ones used by computers), numbers have "bits" instead of "digits".
For example, the number 10111 has 5 bits.  Since binary numbers are base-2, the 1 all of
the way on the left is in the "sixteen" position, meaning we have one "sixteen".  The 0
that is the fourth from the right is the "eight" position, meaning we have no "eights".
The 1 that is the third from the right is in the "four" position, meaning we have one
"four".  The 1 that is the second from the right is in the "two" position, meaning we
have one "two".  The 1 that is right-most is in the "one" position, meaning we have one
"one".  Putting all that together:
we have 1 sixteen, plus 1 four, plus 1 two, plus 1 one, to come up with 23.

Most microcontrollers use 8-bit numbers, and all of the bits must have a value of either
0 or 1.  So, for microcontrollers, the decimal number 23 is represented as 00010111
(the three left-most zeros mean that there are no "thirty-twos", no "sixty-fours", no
"one-hundred-twenty-eights").  The bits in a binary number are labeled from the right
starting with the number 0.  So, the right-most bit is called "bit 0", and the bit to
that is to the left of it is called "bit 1", all of the way to the left-most bit of an
8-bit number which is called "bit 7".

A "Byte" means 8 bits.  A "Word" means 16 bits, or two bytes.  And, while I'm at it, I
may as well let you know that a "Nybble" is 4 bits, and there are two nybbles to a byte.

The ATtiny25 microcontroller is an 8-bit microcontroller.  This means that all quantities
inside of the microcontroller are 8-bit quantities.  The C language can use values that
are larger than 8 bits.  The gcc compiler can use 8-bit values, 16-bit values, and 32-bit
values.  The 8-bit values are "char", the 16-bit values are "int", and the 32-bit
values are "long int".  (Other C compilers may be different.)  We use "unsigned" versions
of these, since we are not using negative values in this firmware.  The C compiler
converts all numbers to combinations of 8-bit numbers, since that is all that the
microcontroller can handle (how it does this is a for another discussion later).

C compilers can interpret numbers as decimal numbers, binary numbers, or hexadecimal
(base-16) numbers.  For the gcc compiler, binary numbers are preceded with "0b",
hexadecimal numbers are preceded with "0x", and decimal numbers are not preceded with
anything.  So, to express the number 23 in binary so that the gcc compiler knows you mean
"twenty-three", rather than "ten-thousand one-hundred and eleven", you write it like
this:  0b00010111
This same number also means that bit 0 is High, bit 1 is High, bit 2 is High, bit 4 is
High, and all other bits are Low (bits 3, 5, 6, and 7).

In hexadecimal, each byte has two nybbles, and each nybble is a power of 16.  For
example, 0x17 means one "sixteen" plus 7 "ones", or the decimal number 23 (which means
that bits 0, 1, 2, and 4 are High, and bits 3, 5, 6, and 7 are Low).

So, for the gcc compiler, all three of these numbers mean exactly the same thing:
   23
   0b00010111
   0x17

Microcontrollers have some special memory locations that are called "registers".
Registers control the hardware in the microcontroller.  For example, the DDRB register
for the ATtiny25 controls which of the pins on the microcontroller are inputs and outputs,
and the TCCR0B register controls some aspects of how the internal hardware Timer 0 will
function.  For the ATtiny25, registers are all 8-bits.

In the datasheet for the AVR microcontrollers, Atmel (who makes them) uses a somewhat
confusing convention for describing bits in an 8-bit register.  For example, TCCR0B is
the Timer/Counter Control Register B for Timer 0 (there is also a Register A, and there
is also a TCCR1 register, but since there is only one register for Timer 1, there is no
A and B).  Within TCCR0B there are 8 bits, defined like this:
   FOC0A   FOC0B   --   --   WGM02   CS02   CS01   CS00
There are three bits that start with "CS0":  CS02, CS01, CS00.  To refer to all three
of these in the same sentence, Atmel will refer to them as "CS02:0", which means CS0
bit 2 through CS0 bit 0.  Pretty messed up, but that's what we need to live with.
(And, by the way, these bits are "Clock Select" bits, which tell the microcontroller
how to select the clock, which is the heart-beat that controls the speed at which the
Timer 0 runs.)
The dashes mean that bit 4 and bit 5 are not used.
I won't define the other bits here.

You will see the above definitions in the firmware and comments, below.  For example,
in the comments in the main program, under the statement where we are setting up the
TCCR0B register for Timer 0, you will see:
     // CS02:00=001 for divide by 1 prescaler (this starts Timer0)
This means that CS0 bit 2 is 0, CS0 bit 1 is 0, and CS0 bit 0 is 1 (or stated another
way, the three CS0 bits are set to 001).

The datasheet describes in great depth all of the registers and functionality of the
ATtiny25 microcontroller.  I downloaded my copy of the datasheet from here:
http://www.atmel.com/dyn/resources/prod_documents/doc2586.pdf
But if that link is dead by the time you read this, just do a search for
     atmel attiny25 datasheet
and you'll come up with the latest version.
*/

/*
This project has one RGB LED.
The light from the LED should be diffused so that the light from
it mixes together (a short section of white drinking straw works well).
The firmware goes through a sequence, mixing various amounts of brightness
of R, G, and B to create various different colors, and blinking and fading
these colors to create a trippy effect.
You can easily change this firmware to create your own sequences of colors!
*/


/*
Parts list for this RGB Light project:
1   ATtiny25
1   CR2032 coin-cell battery
1   CR2032 battery holder
1   small slide switch (for on-off)
1   RGB LED (common anode)
1   white drinking straw
1   IR333-A IR emitter
1   TSOP32138 IR detector
2   47 ohm resistors
1   1k ohm resistor
1   0.1 uF capacitor
1   100 uF capacitor, 6v
1   6-pin header (if you want to re-program the ATtiny25)
*/


/*
The hardware for this project is very simple:
    ATtiny25 has 8 pins:
       pin 1  RST - connects to programming port pin 5
       pin 2  PB3 - connects to the output of the IR detector (pin 1 of the detector)
       pin 3  OC1B - Blue lead of RGB LED (cathode -- through a 47 ohm current limiting resistor)
       pin 4  ground (connects to ground of the IR detector)
       pin 5  OC0A -  IR emitter (cathode -- through a 1k ohm current limiting resistor) (also connects to programming port pin 4)
       pin 6  OC1A - Red lead of RGB LED (cathode -- through a 47 ohm current limiting resistor) (also connects to programming port pin 1)
       pin 7  PB2 - Green lead of RGB LED (cathode) (also connects to programming port pin 3)
       pin 8  +3v (connects to common anode lead of RGB LED, and anode of IR emitter, and power pin of IR detector)

    The 6-pin programming header (used only if you want to re-program the ATtiny25)
       also needs to have its pin 2 connected to +3v,
       and its pin 6 connected to ground.

    This firmware requires that the clock frequency of the ATtiny
       is the default that it is shipped with:  8.0MHz
*/


/*
For this project, I hacked a previous project of mine, called Trippy RGB Light.
The Trippy RGB Light was hacked from Ladyada's MiniPOV3 kit.

Ladyada has a great website about the MiniPOV kit, from which this project was hacked:
http://ladyada.net/make/minipov3/index.html
Her website also has a user forum, where people can ask and answer questions by people building
various projects, including this one:
http://www.ladyada.net/forums/
(Click on the link for MiniPOV).
*/


#include <avr/io.h>             // this contains all the IO port definitions
#include <avr/interrupt.h>      // definitions for interrupts
#include <avr/sleep.h>          // definitions for power-down modes
#include <avr/pgmspace.h>       // definitions or keeping constants in program memory

// this global variable is normally 0, but when the IR detector sees IR,
//   its output brings PB3 Low.  That causes an interrupt, which sets Start_Over = 1.
int Start_Over = 0;



// This is an interrupt routine that will be called whenever the IR detector sees IR.
//   When the IR detector sees IR, its output pin (connected to PB3) goes Low,
//     which causes a PCINT0 interrupt (set up for a logic change for PB3).
//   The only thing this interrupt routine does is set Start_Over to 1.
//     This will be seen in the main routine, which will start the firmware over from the beginning.
ISR(PCINT0_vect) {
  Start_Over = 1;
}



/*
The C compiler creates code that will transfer all constants into RAM when the microcontroller
resets.  Since this firmware has a table (lightTab) that is too large to transfer into RAM
(and since we don't need it in RAM) the C compiler needs to be told to keep it in program
memory space.  This is accomplished by the macro PROGMEM (this is used, below, in the
definition for the lightTab).  Since the C compiler assumes that constants are in RAM, rather
than in program memory, when accessing the lightTab, we need to use the pgm_read_byte() macro,
and we need to use the lightTab as an address, i.e., precede it with "&".  For example, to
access lightTab[3].red, which is a byte, this is how to do it:
     pgm_read_byte( &lightTab[3].red );
And to access lightTab[3].fadeTime, which is a word, this is how to do it:
     pgm_read_word( &lightTab[3].fadeTime );
*/



/*
The following Light Table consists of any number of rgbElements that will fit into the
2k flash ROM of the ATtiny25 microcontroller.
Each rgbElement consists of:
     fadeTime -- how long to take to fade from previous values of RGB
                   to the ones specified in this rgbElement
                   (0 or between 1,000 and 65,535)
     holdTime -- how long to keep the RGB values once they are faded in
                   (0 or between 1,000 and 65,535)
     Red      -- brightness value for Red   (between 0 to 255)
     Green    -- brightness value for Green (between 0 to 255)
     Blue     -- brightness value for Blue  (between 0 to 255)
Both of the time values, fadeTime and holdTime, are expressed as the number of 400
microseconds -- for example, 2 seconds would be entered as 5000.
To signify the last rgbElement in the lightTab, its fadeTime and hold time must both
be 0 (all other values of this last rgbElement are ignored).

NOTE:  I measured the time, and each fadeTime and holdTime is actually
          550 microseconds
       instead of 400 microseconds, as calculated.

The values for fadeTime and holdTime must either be 0, or between 1,000 and 65,535
(i.e., 0, or between 0.4 sec and 26.2 sec).
*/

/*
  The Light Sequences and the notions of fadeTime and holdTime
  are taken from Pete Griffiths, downloaded from:
  http://www.petesworld.demon.co.uk/homebrew/PIC/rgb/index.htm

  I modified it to fit my purposes.

  The sequence takes about 2 minutes.
  More precisely:
       adding all of the fadeTime values together:  121,000
       adding all of the holdTime values together:  134,000
       adding these together = 259,000.
  Since the time values are each 400 microseconds, 255,000 is 102.0 seconds,
    or, 1.70 minutes, which is 1 minute, 42 seconds.

  The Main function repeats the sequence several times.
*/


// table of Light Sequences
struct rgbElement {
  int fadeTime;            // how long to fade from previous values of RGB to the ones specified in this rgbElement (0 to 65,535)
  int holdTime;            // how long to keep the RGB values once they are faded in (0 or 1000 to 65,535)
  unsigned char red;       // brightness value for Red LED (0 to 255)
  unsigned char green;     // brightness value for Green LED (0 to 255)
  unsigned char blue;      // brightness value for Blue LED (0 to 255)
} const lightTab[] PROGMEM = {
  {     0,   500, 255,   0,   0 },
  {   500,     0,   0,   0,   0 },
  {     0,   500,   0, 255,   0 },
  {   500,     0,   0,   0,   0 },
  {     0,   500,   0,   0, 255 },
  {   500,     0,   0,   0,   0 },
  {  2500,  2500, 255,   0,   0 },
  {  2500,  2500,   0, 255,   0 },
  {  2500,  2500,   0,   0, 255 },
  {  2500,  2500, 155,  64,   0 },
  {  2500,  2500,  64, 255,  64 },
  {  2500,  2500,   0,  64, 255 },
  {  2500,  2500,  64,   0,  64 },
  {     0,  1500, 155,   0,   0 },
  {     0,  1500,   0, 255,   0 },
  {     0,  1500,   0,   0, 255 },
  {     0,  1500, 140,   0, 240 },
  {     0,  1500, 155, 155,   0 },
  {     0,  1500, 155, 255, 255 },
  {     0,  1500, 128, 128, 128 },
  {     0,  1500,  48,  48,  58 },
  {     0,  1500,   0,   0,   0 },
  {  2500,  2500, 155,   0,   0 },
  {  2500,  2500, 155, 255,   0 },
  {  2500,  2500,   0, 255,   0 },
  {  2500,  2500,   0, 255, 255 },
  {  2500,  2500,   0,   0, 255 },
  {  2500,  2500, 155,   0, 255 },
  {  2500,     0,   0,   0,   0 },
  {  2500,  2500, 155,   0,   0 },
  {  2500,  2500, 155, 255,   0 },
  {  2500,  2500,   0, 255,   0 },
  {  2500,  2500,   0, 255, 255 },
  {  2500,  2500,   0,   0, 255 },
  {  2500,  2500, 155,   0, 255 },
  {  2500,     0,   0,   0,   0 },
  {  2500,  2500, 154,  32,   0 },
  {  2500,  2500, 154, 128,   0 },
  {  2500,  2500, 154, 240,   0 },
  {  2500,  2500, 128, 240,   0 },
  {     0,  2500,   0,   0,   0 },
  {  2500,  2500,   0,  16, 255 },
  {  2500,  2500,   0, 128, 255 },
  {  2500,  2500,   0, 240, 128 },
  {  2500,  2500,  16,  16, 240 },
  {  2500,  2500, 140,  16, 240 },
  {  2500,  2500,  64,   0, 250 },
  {     0,  2500,  10,  10,  10 },
  {     0,  2500,   0,   0,   0 },
  {  2500,  2500, 140,   0, 240 },
  {  2500,  2500,  32,   0, 240 },
  {  2500,  2500, 128,   0, 128 },
  {  2500,  2500, 140,   0,  32 },
  {  2500,     0,   0,   0,  10 },
  {  2500,     0,   0,   0,   0 },
  {  1000,  1000,   0,   0,   0 },
  {  1000,  1000,  32,   0,   0 },
  {  1000,  1000,  64,   0,   0 },
  {     0,  1000,  96,   0,   0 },
  {  1000,     0, 128,   0,   0 },
  {  1000,     0, 160,  32,   0 },
  {  1000,     0, 192,  64,   0 },
  {  1000,     0, 124,  96,   0 },
  {     0,  1000, 155, 128,   0 },
  {  1000,  1000,   0, 160,   0 },
  {     0,  1000,   0, 192,   0 },
  {  1000,  1000,   0, 224,  32 },
  {  1000,     0,   0, 255,  64 },
  {  1000,     0,   0,   0,  96 },
  {  1000,     0,   0,   0, 128 },
  {  1000,     0,   0,   0, 160 },
  {  1000,     0,   0,   0, 192 },
  {  1000,     0,   0,   0, 224 },
  {  1000,  1000,   0,   0, 255 },
  {  1000,     0,   0,   0,   0 },
  {     0,  1000,   0,   0, 255 },
  {  1000,  1000,  32,   0,   0 },
  {  1000,  1000,  96,   0,   0 },
  {  1000,  1000, 160,   0,   0 },
  {  1000,     0, 255,   0,   0 },
  {  1000,  1000,   0,  96,   0 },
  {  1000,  1000,   0, 160,  32 },
  {  1000,  1000,   0, 224,  64 },
  {  1000,  1000,   0, 255,  96 },
  {  1000,  1000,   0,   0, 128 },
  {  1000,  1000,   0,   0, 160 },
  {     0,  1000,   0,  32, 192 },
  {     0,  1000,   0,  64, 224 },
  {     0,  1000,   0,  96, 225 },
  {     0,  1000,   0, 128,   0 },
  {     0,  1000,   0, 160,   0 },
  {     0,  1000,   0, 192,  32 },
  {     0,  1000,   0, 224,  64 },
  {     0,  1000,   0, 255,  96 },
  {     0,  1000,   0,   0, 255 },
  {     0,  1000,   0,   0,   0 },
  {     0,     0,   0,   0,   0 }
};



// This function delays the specified number of 10 microseconds
void delay_ten_us(unsigned long int us) {
  unsigned long int count;
  const unsigned long int DelayCount=6;  // this value was determined by trial and error

  while (us != 0) {
    // Toggling PB5 is done here to force the compiler to do this loop, rather than optimize it away
    //   NOTE:  Below you will see: "0b00100000".
    //   This is an 8-bit binary number with all bits set to 0 except for the the 6th one from the right.
    //   Since bits in binary numbers are labeled starting from 0, the bit in this binary number that is set to 1
    //     is called PB5, which is the one unsed PORTB pin, which is why we can use it here
    //     (to fool the optimizing compiler to not optimize away this delay loop).
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    us--;
  }
}



// This function delays (1.56 microseconds * x) + 2 microseconds
//   (determined empirically)
//    e.g.  if x = 1, the delay is (1 * 1.56) + 2 = 5.1 microseconds
//          if x = 255, the delay is (255 * 1.56) + 2 = 399.8 microseconds
void delay_x_us(unsigned long int x) {
  unsigned long int count;
  const unsigned long int DelayCount=0;  // the shortest delay

  while (x != 0) {
    // Toggling PB5 is done here to force the compiler to do this loop, rather than optimize it away
    //   (see NOTE in the comments in the delay_ten_us() function, above)
    for (count=0; count <= DelayCount; count++) {PINB |= 0b00100000;};
    x--;
  }
}



// This function pulses the Green LED on PB2 (pin 7)
// Since Green LED is not on a PWM pin on a hardware timer, we need to pulse it manually.
//   We pulse it High for [Green value], and pulse it Low for [255 - Green value].
//   Since the delay_x_us function delays 1.56x+2 microseconds,
//     the total period is about 400 microseconds, which is 2500Hz (if we repeat it)
//     (and that is way fast enough so that we don't perceive the Green LED flicker).
void pulseGreen(unsigned char greenVal) {
  PORTB &= 0b11111011;  // turn on Green LED at PB2 (pin 7) for 4 * Green value -- (by bringing this pin Low)
  delay_x_us( greenVal );
  PORTB |= 0b00000100;  // turn off Green LED at PB2 for 4 * (255 - Green value) -- (by bringing this pin High)
  delay_x_us( (255 - greenVal) );
}



void pulseIR(void) {
  // We will use Timer 0 to pulse the IR LED at 38KHz
  //
  // We pulse the IR emitter for only 170 microseconds.  We do this to save battery power.
  //   The minimum number of 38KHz pulses that the IR detector needs to detect IR is 6.
  //   170 microseconds gives slightly more than 6 periods of 38KHz, which is enough to
  //   reflect off of your hand and hit the IR detector.
  //
  // start up Timer0 in CTC Mode at about 38KHz to drive the IR emitter on output OC0A:
  //   8-bit Timer0 OC0A (PB0, pin 5) is set up for CTC mode, toggling output on each compare
  //   Fclk = Clock = 8MHz
  //   Prescale = 1
  //   OCR0A = 104
  //   F = Fclk / (2 * Prescale * (1 + OCR0A) ) = 38KHz
  //
  // Please see the ATtiny25 datasheet for descriptions of these registers.
  TCCR0A = 0b01000010;  // COM0A1:0=01 to toggle OC0A on Compare Match
                        // COM0B1:0=00 to disconnect OC0B
                        // bits 3:2 are unused
                        // WGM01:00=10 for CTC Mode (WGM02=0 in TCCR0B)
  TCCR0B = 0b00000001;  // FOC0A=0 (no force compare)
                        // F0C0B=0 (no force compare)
                        // bits 5:4 are unused
                        // WGM2=0 for CTC Mode (WGM01:00=10 in TCCR0A)
                        // CS02:00=001 for divide by 1 prescaler (this starts Timer0)
  OCR0A = 104;  // to output 38,095.2KHz on OC0A (PB0, pin 5)

  // keep the IR going at 38KHz for 170 microseconds (which is slightly more than 6 periods of 38KHz)
  delay_ten_us(17000);   // delay 170 microseconds

  // turn off Timer0 to stop 38KHz pulsing of IR
  TCCR0B = 0b00000000;  // CS02:CS00=000 to stop Timer0 (turn off IR emitter)
  TCCR0A = 0b00000000;  // COM0A1:0=00 to disconnect OC0A from PB0 (pin 5)
}



// This function sends one rgbElement of lightTab to the LEDs,
//   given index to the codeElement in lightTab
//     If both fadeTime = 0 and holdTime = 0 that signifies the last rgbElement of the lightTab
//
// There are several variables used in this function:
//   index:     the input argument for this function -- it is the index to lightTab, pointing to the rgbElement from lightTab
//
//   FadeTime:  gotten from rgbElement in lightTab
//   HoldTime:  gotten from rgbElement in lightTab
//   Red:       gotten from rgbElement in lightTab
//   Green:     gotten from rgbElement in lightTab
//   Blue:      gotten from rgbElement in lightTab
//
//   redPrev:   gotten from previous rgbElement in lightTab (set to 0 if index to lightTab = 0)
//   greenPrev: gotten from previous rgbElement in lightTab (set to 0 if index to lightTab = 0)
//   bluePrev:  gotten from previous rgbElement in lightTab (set to 0 if index to lightTab = 0)
//
//   redTime:   when the fadeCounter in the fade loop reaches this value, we will update the Red LED brightness value
//   greenTime: when the fadeCounter in the fade loop reaches this value, we will update the Green LED brightness value
//   blueTime:  when the fadeCounter in the fade loop reaches this value, we will update the Blue LED brightness value
//
//   redTemp:   keep track of current Red LED brightness value as we're fading up or down in the fade loop
//   greenTemp: keep track of current Green LED brightness value as we're fading up or down in the fade loop
//   blueTemp:  keep track of current Blue LED brightness value as we're fading up or down in the fade loop
//
//   redDelta:   the total amount of brightness we need to change to get from where the Red LED was to where we want it
//   greenDelta: the total amount of brightness we need to change to get from where the Green LED was to where we want it
//   blueDelta:  the total amount of brightness we need to change to get from where the Blue LED was to where we want it
//
//   redTimeInc:   in the fade loop we will update the Red LED every time the fadeCounter increments by this amount
//   greenTimeInc: in the fade loop we will update the Green LED every time the fadeCounter increments by this amount
//   blueTimeInc:  in the fade loop we will update the Blue LED every time the fadeCounter increments by this amount
//
//   fadeCounter:  for counting through the steps (each of which is 400us long) in the fade loop
//
//   holdCounter:  for counting through the steps (each of which is 400us long) in the hold loop


void sendrgbElement( int index ) {

    //pulseIR();             // pulse the IR emitter for a little bit (if there's a reflection to the IR detector as a result, then the ISR will set Start_Over = 1)
    if ( Start_Over ) return;  // if the IR detector saw IR, then we should reset and start over (Start_Over gets set to 1 in the interrupt routine if the IR detector saw IR)

}



int main(void) {
  start_over:
  // disable the Watch Dog Timer (since we won't be using it, this will save battery power)
  MCUSR = 0b00000000;   // first step:   WDRF=0 (Watch Dog Reset Flag)
  WDTCR = 0b00011000;   // second step:  WDCE=1 and WDE=1 (Watch Dog Change Enable and Watch Dog Enable)
  WDTCR = 0b00000000;   // third step:   WDE=0
  // turn off power to the USI and ADC modules (since we won't be using it, this will save battery power)
  PRR = 0b00000011;
  // disable all Timer interrupts
  TIMSK = 0x00;         // setting a bit to 0 disables interrupts
  // set up the input and output pins (the ATtiny25 only has PORTB pins)
  DDRB = 0b00010111;    // setting a bit to 1 makes it an output, setting a bit to 0 makes it an input
                        //   PB5 (unused) is input
                        //   PB4 (Blue LED) is output
                        //   PB3 (IR detect) is input
                        //   PB2 (Green LED) is output
                        //   PB1 (Red LED) is output
                        //   PB0 (IR LED) is output
  PORTB = 0xFF;         // all PORTB output pins High (all LEDs off) -- (if we set an input pin High it activates a pull-up resistor, which we don't need, but don't care about either)

  // set up PB3 so that a logic change causes an interrupt (this will happen when the IR detector goes from seeing IR to not seeing IR, or from not seeing IR to seeing IR)
  GIMSK = 0b00100000;   // PCIE=1 to enable Pin Change Interrupts
  PCMSK = 0b00001000;   // PCINT3 bit = 1 to enable Pin Change Interrupts for PB3
  sei();                // enable microcontroller interrupts

  // this global var gets set to 1 in the interrupt service routine if the IR detector sees IR
  Start_Over = 0;

  // We will use Timer 1 to fade the Red and Blue LEDs up and down
  //
  // start up Timer1 in PWM Mode at 122Hz to drive Red LED on output OC1A and Blue LED on output OC1B:
  //   8-bit Timer1 OC1A (PB1, pin 6) and OC1B (PB4, pin 3) are set up as outputs to toggle in PWM mode
  //   Fclk = Clock = 8MHz
  //   Prescale = 256
  //   MAX = 255 (by setting OCR1C=255)
  //   OCR1A =  0 (as an initial value -- this value will increase to increase brightness of Red LED)
  //   OCR1B =  0 (as an initial value -- this value will increase to increase brightness of Blue LED)
  //   F = Fclk / (Prescale * (MAX+1) ) = 122Hz
  // There is nothing too important about driving the Red and Blue LEDs at 122Hz, it is somewhat arbitrary,
  //   but it is fast enough to make it seem that the Red and Blue LEDs are not flickering.
  // Later in the firmware, the OCR1A and OCR1B compare register values will change,
  //   but the period for Timer1 will always remain the same (with F = 122Hz, always) --
  //   with OCR1A = 0, the portion of the period with the Red LED on is a minimum
  //     so the Red LED is very dim,
  //   with OCR1A = 255, the portion of the period with the Red LED on is a maximum
  //     so the Red LED is very bright.
  //   with OCR1B = 0, the portion of the period with the Blue LED on is a minimum
  //     so the Blue LED is very dim,
  //   with OCR1B = 255, the portion of the period with the Blue LED on is a maximum
  //     so the Blue LED is very bright.
  //   the brightness of the Red LED can be any brightness between the min and max
  //     by varying the value of OCR1A between 0 and 255.
  //   the brightness of the Blue LED can be any brightness between the min and max
  //     by varying the value of OCR1B between 0 and 255.
  //
  // Please see the ATtiny25 datasheet for descriptions of these registers.
  GTCCR = 0b01110000;   // TSM=0 (we are not using synchronization mode)
                        // PWM1B=1 for PWM mode for compare register B
                        // COM1B1:0=11 for inverting PWM on OC1B (Blue LED output pin)
                        // FOC1B=0 (no Force Output Compare for compare register B)
                        // FOC1A=0 (no Force Output Compare for compare register A)
                        // PSR1=0 (do not reset the prescaler)
                        // PSR0=0 (do not reset the prescaler)
  TCCR1 = 0b01111001;   // CTC1=0 for PWM mode
                        // PWM1A=1 for PWM mode for compare register A
                        // COM1A1:0=11 for inverting PWM on OC1A (Red LED output pin)
                        // CS13:0=1001 for Prescale=256 (this starts Timer 1)
  OCR1C = 255;   // sets the MAX count for the PWM to 255 (to get PWM frequency of 122Hz)
  OCR1A = 0;  // start with minimum brightness for Red LED on OC1A (PB1, pin 6)
  OCR1B = 0;  // start with minimum brightness for Blue LED on OC1B (PB4, pin 3)

  // Since we are only using hardware timers to drive the Red and Blue LEDs with PWM
  //   we will pulse the Green LED manually with the firmware (using the pulseGreen() function, which is called by the sendrgbElement() function)

  // This loop goes through the lightTab, displaying each rgbElement in the table
  //   the loop knows that the last rgbElement has been displayed
  //   when it sees an rgbElement from lightTab that has both fadeTime=0 and holdTime=0
  // This loop also starts the color sequence over from the beginning of lightTab if Start_Over is set
  //   (which happens when IR was detected by the IR detector)
  int index = 0;  // this points to the next rgbElement in the lightTab (initially pointing to the first rgbElement)
  // send the entire 1.7-minute sequence 18 times so that it lasts about 1/2 hour.
  // Actually, I measured the time, and the sequence lasts 2.33 minutes, so 13 times gives about 1/2 hour.
  for (int count=0; count<180; count++) {
    // send all rgbElements to LEDs (when both fadeTime = 0 and holdTime = 0 it signifies the last rgbElement in lightTab)
    do {
      sendrgbElement(index);        // send one rgbElement to LEDs
      index++;                      // increment to point to next rgbElement in lightTab
      if ( Start_Over ) break;      // if the IR detector saw IR, then we should reset and start over
    } while ( !( (pgm_read_word(&lightTab[index].fadeTime) == 0 ) && (pgm_read_word(&lightTab[index].holdTime) == 0 ) ) );
    if ( Start_Over ) break;      // if the IR detector saw IR, then we should reset and start over
    index = 0;
  }
  if ( Start_Over ) goto start_over;    // if the IR detector saw IR, then we should reset and start over

  // Shut down everything and put the CPU to sleep
  cli();                 // disable microcontroller interrupts
  delay_ten_us(10000);   // wait .1 second
  TCCR0B &= 0b11111000;  // CS02:CS00=000 to stop Timer0 (turn off IR emitter)
  TCCR0A &= 0b00111111;  // COM0A1:0=00 to disconnect OC0A from PB0 (pin 5)
  TCCR1 &= 0b11110000;   // CS13:CS10=0000 to stop Timer1 (turn off Red and Blue LEDs)
  TCCR1 &= 0b11001111;   // COM1A1:0=00 to disconnect OC1A from PB1 (pin 6)
  GTCCR &= 0b11001111;   // COM1B1:0=00 to disconnect OC1B from PB4 (pin 3)
  DDRB = 0x00;           // make PORTB all inputs (saves battery power)
  PORTB = 0xFF;          // enable pull-up resistors on all PORTB input pins (saves battery power)
  MCUCR |= 0b00100000;   // SE=1 (bit 5)
  MCUCR |= 0b00010000;   // SM1:0=10 to enable Power Down Sleep Mode (bits 4, 3)
  sleep_cpu();           // put CPU into Power Down Sleep Mode
}
