/* At current stage it is just PoC for TX'ing on demo STM8S003 + 433Mhz module connected to PC4)
   You can receive it with:
      rtl_433 -X 'n=name,m=OOK_MC_ZEROBIT,s=676,l=0,r=1776,invert,reflect,preamble={32}0x55555555,bits>=72'


    TODO: Memory content is kept in Halt or Active halt mode. Keep counter.
*/
#include "stm8s.h"
#include <stdint.h>
#define F_CPU_K 16000UL
#define T_COUNT(x) (( F_CPU_K * x / 1000UL )-5)/5
#define PREAMBLE_BYTES 4

/* XXTEA related */
#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

volatile uint8_t bit_state = 0;
/*
    Bit is 1 - set to 2
    Bit is 0 - set to 4
    Wait for 0
*/

static inline void _delay_cycl( unsigned short __ticks )
{
    __asm__("nop\n nop\n");
    do {
        __ticks--;
    } while ( __ticks );
    __asm__("nop\n");
}

static inline void _delay_us( unsigned short __us) {
    _delay_cycl( (unsigned short)( T_COUNT(__us) ));
}

static inline void _delay_ms( unsigned short __ms )
{
    while ( __ms-- )
        _delay_us( 1000 );
}

static uint8_t calc_crc8(uint8_t* data_pointer, uint8_t number_of_bytes) {
    uint8_t temp1, bit_counter, feedback_bit, crc8_result = 0;

    while (number_of_bytes--) {
        temp1 = *data_pointer++;
        for (bit_counter = 8; bit_counter; bit_counter--) {
            feedback_bit = (crc8_result & 0x01);
            crc8_result >>= 1;
            if (feedback_bit ^ (temp1 & 0x01)) {
                crc8_result ^= 0x8c;
            }
            temp1 >>= 1;
        }
    }
    return crc8_result;
}

/* n - positive for encoding, negative for decoding */
void xxtea(uint32_t *v, int n, uint32_t const key[4]) {
    uint32_t y, z, sum;
    unsigned p, rounds, e;
    if (n > 1) {
        rounds = 6 + 52 / n;
        sum = 0;
        z = v[n - 1];
        do {
            sum += DELTA;
            e = (sum >> 2) & 3;
            for (p = 0; p < n - 1; p++) {
                y = v[p + 1];
                z = v[p] += MX;
            }
            y = v[0];
            z = v[n - 1] += MX;
        } while (--rounds);
    }
    /* Decryption is optional, it is TX only device */
    /* else if (n < -1) {
        n = -n;
        rounds = 6 + 52 / n;
        sum = rounds * DELTA;
        y = v[0];
        do {
            e = (sum >> 2) & 3;
            for (p = n - 1; p > 0; p--) {
                z = v[p - 1];
                y = v[p] -= MX;
            }
            z = v[n - 1];
            y = v[0] -= MX;
            sum -= DELTA;
        } while (--rounds);
    }
    */
}


static void send_byte(uint8_t byte) {
    int i;
    for (i = 0; i < 8; i++) {
        if (byte & (1 << i)) {
            bit_state = 2;
        } else {
            bit_state = 4;
        }
        while (bit_state != 0);
    }

}

static void rf_send(uint8_t* data_pointer, uint8_t number_of_bytes) {
    uint8_t tmp = 0;
    uint8_t crcnow = calc_crc8(data_pointer, number_of_bytes);

    for (tmp = 0; tmp < PREAMBLE_BYTES; tmp++)
        send_byte(0x55);

    for (tmp = 0; tmp < number_of_bytes; tmp++)
        send_byte(data_pointer[tmp]);
    send_byte(crcnow);

    _delay_ms(10000);
}

int main() {
    int d = 0;
    uint8_t crcnow = 0;
    /* TX data should be N*4 bytes */
    uint8_t tx[8] = { 0x01, 0x02, 0x03, 0x04, 0xa1, 0xa2, 0xa3, 0xa4 };
    CLK_CKDIVR = 0;
    CLK_PCKENR1 = 0xFF; // Enable peripherals


    // Timer setup
    TIM2_PSCR = 5; // (2^4)=16 prescaler (was 6)
    TIM2_ARRH = 0x1;
    TIM2_ARRL = 0x90;

    TIM2_IER |= TIM_IER_UIE; // CC1IE
    TIM2_CR1 |= TIM_CR1_CEN;

    // Radio on PC4
    PC_DDR = 0x10;
    PC_CR1 = 0x10;
    PC_ODR &= ~0x10;

    PB_DDR = 0x20;
    PB_CR1 = 0x20;

    PB_ODR |= 0x20;

    AWU_TBR = 12+2; // 1 sec
    AWU_APR = 62;
    AWU_CSR1 = 0x10;

    enableInterrupts();
    do {
        // PB_ODR ^= 0x20; // For test only
        rf_send((uint8_t*)&tx, 8);
        // keep halting on AWU several times to sleep longer?
        halt();
    } while (1);
}


void TIM2_CC_Handler(void) __interrupt(13) {
    TIM2_SR1 &= ~0x1; //reset interrupt

    if (bit_state == 0) {
        PC_ODR &= ~0x10; //0
        PB_ODR |= 0x20;
        return;
    }
    switch (bit_state) {
    // OFF bit (10)
    case 4:
        PC_ODR |= 0x10; // 1
        PB_ODR &= ~0x20;
        bit_state--;
        break;
    case 3:
        PC_ODR &= ~0x10; //0
        PB_ODR |= 0x20;
        bit_state = 0;
        break;

    // ON bit(01)
    case 2:
        PC_ODR &= ~0x10; //0
        PB_ODR |= 0x20;
        bit_state--;
        break;
    case 1:
        PC_ODR |= 0x10; // 1
        PB_ODR &= ~0x20;
        bit_state = 0;
        break;

    default:
        bit_state = 0;
        break;
    }
}

void AWU_Handler(void) __interrupt(1) {
    AWU_CSR1 &= ~(1<<5); // clear AWU_CSR1_AWUF AWUF
}