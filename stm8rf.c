/*
    TODO: Use fixed key only for power-on beaconing (encrypting and sending nonce + session key)
          Power on beaconing might increase eeprom counter by 1 and generate "RAM-only" session key deriving it from pseudo-seed+eeprom counter

*/
#include "stm8s.h"
#include <stdint.h>
#define F_CPU_K 16000UL
#define T_COUNT(x) (( F_CPU_K * x / 1000UL )-5)/5
#define PREAMBLE_BYTES 4
#define INIT_BYTES 8

/* XXTEA related */
#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))


/* User defined data */

#define USE_EEPROM // Use EEPROM for startup sequence (session_key generation)

/* User defined data EOF */

uint32_t session_key[4];
volatile uint8_t bit_state = 0;
uint32_t tx_counter = 0; // Used as non-ce

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

//static inline void _delay_ms( unsigned short __ms )
static void _delay_ms( unsigned short __ms )
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
static void xxtea(uint32_t *v, int n, uint32_t const key[4]) {
    uint32_t y, z, sum;
    uint32_t p, rounds, e;
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
    uint8_t crcnow;
    uint8_t *tx_counter_prepared = (uint8_t*)&tx_counter;
    uint8_t raw[32];

    for (tmp = 0; tmp < 4; tmp++)
        raw[tmp] = tx_counter_prepared[tmp];
    for (tmp = 0; tmp < number_of_bytes; tmp++)
        raw[tmp + 4] = data_pointer[tmp];

    xxtea((uint32_t*)raw, number_of_bytes / 4 + 1, session_key);
    crcnow = calc_crc8(&raw[0], number_of_bytes);

    /* Time critical! Send all data in sequence, no other operations */
    for (tmp = 0; tmp < PREAMBLE_BYTES; tmp++)
        send_byte(0x55);
    for (tmp = 0; tmp < number_of_bytes+4; tmp++)
        send_byte(raw[tmp]);

    send_byte(crcnow);
}

#ifdef USE_EEPROM
void eeprom_write(uint16_t addr, uint8_t *buf, uint16_t len) {
    /* unlock EEPROM */
    FLASH_DUKR = FLASH_DUKR_KEY1;
    FLASH_DUKR = FLASH_DUKR_KEY2;
    while (!(FLASH_IAPSR & (1 << FLASH_IAPSR_DUL)));
    /* write data from buffer */
    for (uint16_t i = 0; i < len; i++, addr++) {
        _MEM_(addr) = buf[i];
        while (!(FLASH_IAPSR & (1 << FLASH_IAPSR_EOP)));
    }
    /* lock EEPROM */
    FLASH_IAPSR &= ~(1 << FLASH_IAPSR_DUL);
}
#endif

static void poweron(void) {
    uint32_t poweron_seq = 0x1;
    uint8_t crcnow = 0x0;
    uint8_t *buf = (uint8_t*)&poweron_seq;
    uint16_t addr;
    // TODO: read from eeprom factory key
    uint32_t factory_key[4] = { 0x0102aabb, 0x0203aabb, 0x0304aabb, 0x0506aabb};
    uint32_t buffer_key[5];
    uint8_t tmp;

    #ifdef USE_EEPROM_SEQ
    // TODO: Read nonce-sequence Less ugly way, maybe this is wrong
    for (addr = 0; addr < 4; addr++) {
        buf[addr] = _MEM_(EEPROM_START_ADDR + addr);
        buffer_key[addr] = 0x0;
    }
    poweron_seq++;
    eeprom_write(EEPROM_START_ADDR, buf, 4);
    #endif

    /* Encrypt, so receiver can know seq and verify it */
    buffer_key[0] = poweron_seq;
    buffer_key[1] = factory_key[0];
    xxtea(buffer_key, 2, factory_key);

    buf = (uint8_t*) buffer_key;
    crcnow = calc_crc8(buf, 8);

    for (tmp = 0; tmp < 8; tmp++) {
        /* Time critical! Send all data in sequence, no other operations */
        for (addr = 0; addr < PREAMBLE_BYTES; addr++)
            send_byte(0x55);
        
        for (addr = 0; addr < 8; addr++)
            send_byte(buf[addr]);
        
        send_byte(crcnow);
        _delay_ms(1000);
    }

    /* Now create session key */
    buffer_key[0] = poweron_seq;
    for (addr = 0; addr < 4; addr++)
        buffer_key[addr+1] = factory_key[addr];

    xxtea(buffer_key, 5, factory_key);
    for (addr = 0; addr < 4; addr++)
        session_key[addr] = buffer_key[addr];

    buf = (uint8_t*) session_key;

    // DEBUG ONLY!!! Send in raw session key, it is dead insecure
    /*
    for (tmp = 0; tmp < 8; tmp++) {
        for (addr = 0; addr < PREAMBLE_BYTES; addr++)
            send_byte(0x55);
        
        for (tmp2 = 0; tmp2 < 8; tmp2++)
            send_byte(buf[tmp2]);

        _delay_ms(1000);
    }
    */

}

int main() {
    int d = 0;
    /* TX data should be N*4 bytes */
    uint8_t tx[8] = { 0x01, 0x02, 0x03, 0x04, 0xa1, 0xa2, 0xa3, 0xa4 };
    CLK_CKDIVR = 0;
    CLK_PCKENR1 = 0xFF; // Enable peripherals
    CLK_PCKENR2 = 0x4; // 0x4 AWU only, ADC is 0x8

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

    AWU_TBR = 12 + 2; // 1 sec
    AWU_APR = 62;
    AWU_CSR1 = 0x10;

    /* Setup REGAH in CLK_ICKR? For lower power in Active Halt */

    enableInterrupts();
    _delay_ms(10000); // Delay, in case of power glitches, so it wont increase counter for nothing
    poweron();

    do {
        rf_send((uint8_t*)&tx, 8);
        tx_counter++;
        // keep halting on AWU several times to sleep longer?
        _delay_ms(10); // Hack, otherwise pin might hang in pull-up state on halt. TODO: make pull-down circuit!
        for (d = 0; d < 4; d++)
            halt();
    } while (1);
}


// Manchester timing implemented here
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
    AWU_CSR1 &= ~(1 << 5); // clear AWU_CSR1_AWUF AWUF
}
