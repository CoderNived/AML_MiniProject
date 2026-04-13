/*  Sound Meter — MSP430G2553 + 74HC595 + 8 LEDs
 *  Compiler : mspgcc / TI CCS
 *  Crystal  : none (1 MHz DCO)
 */
#include <msp430.h>

/* Shift-register pins (Port 1) */
#define SER_PIN   BIT2   /* P1.2 → 74HC595 pin 14 */
#define SRCLK_PIN BIT3   /* P1.3 → 74HC595 pin 11 */
#define RCLK_PIN  BIT4   /* P1.4 → 74HC595 pin 12 */

#define NUM_SAMPLES 64   /* samples per measurement window */

/* ---- prototypes ---- */
void     shiftOut(unsigned char data);
unsigned readADC(void);
unsigned measurePeakToPeak(void);

/* ==================== MAIN ==================== */
void main(void) {
    WDTCTL = WDTPW | WDTHOLD;      /* stop watchdog */

    /* 1 MHz calibrated DCO */
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL  = CALDCO_1MHZ;

    /* GPIO for shift register */
    P1DIR |= SER_PIN | SRCLK_PIN | RCLK_PIN;
    P1OUT &= ~(SER_PIN | SRCLK_PIN | RCLK_PIN);

    /* ADC10 on P1.0 (channel A0) */
    ADC10CTL1 = INCH_0 | ADC10DIV_3;
    ADC10CTL0 = ADC10SHT_3 | ADC10ON;
    ADC10AE0 |= BIT0;               /* enable analog on P1.0 */

    unsigned level;
    unsigned char pattern;

    while (1) {
        level = measurePeakToPeak();   /* 0 – ~900 typical */

        /* Bar-graph mapping: each step ≈ 100 counts */
        if      (level <  60)  pattern = 0x00;
        else if (level < 140)  pattern = 0x01;
        else if (level < 220)  pattern = 0x03;
        else if (level < 300)  pattern = 0x07;
        else if (level < 400)  pattern = 0x0F;
        else if (level < 520)  pattern = 0x1F;
        else if (level < 660)  pattern = 0x3F;
        else if (level < 820)  pattern = 0x7F;
        else                   pattern = 0xFF;

        shiftOut(pattern);
        __delay_cycles(30000);         /* ~30 ms refresh */
    }
}

/* ==================== FUNCTIONS ==================== */

/* Peak-to-peak amplitude over NUM_SAMPLES samples */
unsigned measurePeakToPeak(void) {
    unsigned hi = 0, lo = 1023, s;
    int i;
    for (i = 0; i < NUM_SAMPLES; i++) {
        s = readADC();
        if (s > hi) hi = s;
        if (s < lo) lo = s;
        __delay_cycles(400);           /* ~150 kHz sample rate */
    }
    return (hi - lo);
}

/* Single ADC10 conversion */
unsigned readADC(void) {
    ADC10CTL0 |= ENC | ADC10SC;
    while (ADC10CTL1 & ADC10BUSY);
    return ADC10MEM;
}

/* Bit-bang shift out — MSB first */
void shiftOut(unsigned char data) {
    int i;
    P1OUT &= ~RCLK_PIN;              /* latch low */
    for (i = 7; i >= 0; i--) {
        P1OUT &= ~SRCLK_PIN;         /* clock low  */
        if (data & (1 << i))
            P1OUT |=  SER_PIN;
        else
            P1OUT &= ~SER_PIN;
        P1OUT |= SRCLK_PIN;          /* clock high — latch bit */
    }
    P1OUT |= RCLK_PIN;               /* pulse latch → output */
    P1OUT &= ~RCLK_PIN;
}