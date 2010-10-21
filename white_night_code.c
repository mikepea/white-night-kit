// the below should generate a 38k ir signal.
// I have more somewhere with the ir pulsing separate [so it works summink like sendIr(HIGH|LOW);
// pulse @ 38k for 6 cycles [low] or 12 cycls [high]].
// Just loops through a byte sending out each bit

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

// for matt's design
//#define redMask  0b00000010;
//#define grnMask  0b00010000;
//#define bluMask  0b00000100;
//#define irIn     0b00000000;
//#define irOut    0b00000001;

// for trippy RGB wave
#define bogusMask 0b00100000;
#define redMask  0b00000010;
#define grnMask  0b00000100;
#define bluMask  0b00010000;
#define irIn     0b00000000;
#define irOut    0b00000001;

// The length of tocks to wait to get a 38KHz signal
//#define IR_PULSE_DELAY  26
// for debugging, extend delay to be visible on RGB led:
#define IR_PULSE_DELAY  10000

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
        PORTB ^= redMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= redMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= grnMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= grnMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= bluMask;
        delay_ten_us(1600-(i*100));
        PORTB ^= bluMask;
        delay_ten_us(1600-(i*100));
    }
}

void send_38khz_pulse(void) {
    PORTB ^= irOut;
    PORTB ^= redMask;
    delay_x_us(IR_PULSE_DELAY);
    PORTB ^= irOut;
    PORTB ^= redMask;
    delay_x_us(IR_PULSE_DELAY);
}

void send_38khz_space(void) {
    PORTB ^= bogusMask;
    PORTB ^= bluMask;
    delay_x_us(IR_PULSE_DELAY);
    PORTB ^= bogusMask;
    PORTB ^= bluMask;
    delay_x_us(IR_PULSE_DELAY);
}

void send_38khz_green_space(void) {
    PORTB ^= bogusMask;
    PORTB ^= grnMask;
    delay_x_us(IR_PULSE_DELAY);
    PORTB ^= bogusMask;
    PORTB ^= grnMask;
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

void send_nec_ir_code(char id, char code) {
    send_nec_header();
    send_nec_byte(id);
    send_nec_byte(code);
    send_nec_byte(~id);
    send_nec_byte(~code);
}

void send_my_ir_code(char id, char code) {
    send_nec_ir_code(id, code);
}

int main(void) {
    TCCR0A = 0;
    TCCR0B = 0;
    DDRB = 0b00010111;
    PORTB = 0xff; // write all outputs high & internal resistors on inputs (turns led's off)

    startUp1();

    while (1==1) {

        send_my_ir_code(0x8d, 0x23);

        PORTB ^= grnMask;
        delay_ten_us(10000);
        PORTB ^= grnMask;

        delay_ten_us(100000);

    }

}

