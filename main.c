#include <msp430.h>

// ── Pin mapping ──────────────────────────────────────────────────
#define SER_PIN    BIT1   // P1.1  →  74HC595 SER   (pin 14)
#define SRCLK_PIN  BIT2   // P1.2  →  74HC595 SRCLK (pin 11)
#define RCLK_PIN   BIT3   // P1.3  →  74HC595 RCLK  (pin 12)

// ── ADC tuning ───────────────────────────────────────────────────
// MAX9814 output is DC-biased at VDD/2 ≈ 1.65 V on a 3.3 V supply.
// 10-bit ADC (0–1023) full-scale = 3.3 V  →  midpoint = 512.
#define ADC_MID      512
#define NUM_SAMPLES   30   // samples per display frame (~17 ms @ 1 MHz)

void          shiftOut(unsigned char data);
unsigned char levelToLED(unsigned int peak);

// ────────────────────────────────────────────────────────────────
void main(void)
{
    WDTCTL  = WDTPW | WDTHOLD;    // Disable watchdog
    BCSCTL1 = CALBC1_1MHZ;        // Calibrated 1 MHz DCO
    DCOCTL  = CALDCO_1MHZ;

    // GPIO
    P1DIR  |=  SER_PIN | SRCLK_PIN | RCLK_PIN;
    P1OUT  &= ~(SER_PIN | SRCLK_PIN | RCLK_PIN);

    // ADC10: channel A0 (P1.0), 64-clock sample-and-hold, VCC ref
    ADC10CTL0 = ADC10SHT_3 | ADC10ON;
    ADC10CTL1 = INCH_0;
    ADC10AE0  = BIT0;               // Analog input enable on P1.0

    unsigned int  peak = 0, raw, deviation;
    unsigned char count = 0;

    while (1)
    {
        // Trigger and wait for ADC conversion
        ADC10CTL0 |= ENC | ADC10SC;
        while (ADC10CTL1 & ADC10BUSY);
        raw = ADC10MEM;

        // Peak-hold: track absolute deviation from DC midpoint
        deviation = (raw > ADC_MID) ? (raw - ADC_MID)
                                    : (ADC_MID - raw);
        if (deviation > peak) peak = deviation;

        // Update LED bar every NUM_SAMPLES conversions
        if (++count >= NUM_SAMPLES)
        {
            shiftOut(levelToLED(peak));
            peak  = 0;
            count = 0;
        }

        __delay_cycles(500);    // ~0.5 ms between samples
    }
}

// ────────────────────────────────────────────────────────────────
// Map peak amplitude (0 – 512) to an 8-bit LED bar-graph pattern.
// Bit 0 (QA) = level 1 (quietest), Bit 7 (QH) = level 8 (loudest).
// Adjust thresholds here to tune sensitivity.
// ────────────────────────────────────────────────────────────────
unsigned char levelToLED(unsigned int level)
{
    if (level <  15) return 0x00;  // ░░░░░░░░  silence
    if (level <  55) return 0x01;  // ▪░░░░░░░
    if (level < 100) return 0x03;  // ▪▪░░░░░░
    if (level < 150) return 0x07;  // ▪▪▪░░░░░
    if (level < 200) return 0x0F;  // ▪▪▪▪░░░░
    if (level < 255) return 0x1F;  // ▪▪▪▪▪░░░
    if (level < 320) return 0x3F;  // ▪▪▪▪▪▪░░
    if (level < 400) return 0x7F;  // ▪▪▪▪▪▪▪░
    return             0xFF;       // ▪▪▪▪▪▪▪▪  peak!
}

// ────────────────────────────────────────────────────────────────
// Shift one byte MSB-first into the 74HC595.
// RCLK rising edge latches the shift register into the output reg.
// After shiftOut(), QA = LSB of data, QH = MSB of data.
// ────────────────────────────────────────────────────────────────
void shiftOut(unsigned char data)
{
    unsigned char i;

    P1OUT &= ~RCLK_PIN;             // Hold latch low during shift

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
            P1OUT |=  SER_PIN;      // Data bit = 1
        else
            P1OUT &= ~SER_PIN;      // Data bit = 0

        P1OUT |=  SRCLK_PIN;        // Rising clock edge
        __delay_cycles(4);
        P1OUT &= ~SRCLK_PIN;        // Falling clock edge
        __delay_cycles(4);

        data <<= 1;                 // Next bit
    }

    P1OUT |=  RCLK_PIN;             // Latch: shift reg → output reg
    __delay_cycles(4);
    P1OUT &= ~RCLK_PIN;
}