// INCLUDES
#include <msp430.h>
#include <stdlib.h>
#include <math.h>
#include "peripherals.h"

// PROTOTYPES
__interrupt void Timer_A2_ISR(void);
void states();
void DACInit(void);
void DACSetValue(unsigned int dac_code);
void configButtons();
char buttonStates();

enum States{HOME, DC, SQUARE, SAWTOOTH, TRIANGLE};

int currentState = SAWTOOTH;
long unsigned int timeCount = 0;
unsigned int leapCount = 0;
unsigned int ADCPot = 0;
unsigned int waveCount = 4000;


// TIMER INTERRUPT
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {
    if (leapCount < 482) {
        timeCount++;
        leapCount++;
    }
    else {
        leapCount = 0;
    }

    if (currentState == DC) DACSetValue(ADCPot);
    else if (currentState == SAWTOOTH) {
        DACSetValue(waveCount);
    }
}

// MAIN
void main(void) {
    WDTCTL = WDTPW + WDTHOLD;   // Stop WDT

    // timer A2 management
    TA2CTL = TASSEL_1 + ID_0 + MC_1; // 32786 Hz is set
    TA2CCR0 = 10; // sets interrupt to occur every (TA2CCR0 + 1)/32786 seconds
    TA2CCTL0 = CCIE; // enables TA2CCR0 interrupt



    // Configure P8.0 as digital IO output and set it to 1
    // This supplied 3.3 volts across scroll wheel potentiometer
    // See schematic at end or MSP-EXP430F5529 board users guide
    P6SEL &= ~BIT0;
    REFCTL0 &= ~REFMSTR;                      // Reset REFMSTR to hand over control
    // internal reference voltages to
    // ADC12_A control registers
    ADC12CTL0 = ADC12SHT0_9 | ADC12ON;

      ADC12CTL1 = ADC12SHP;                     // Enable sample timer

    ADC12MCTL0 = ADC12SREF_0 + ADC12INCH_0;    // ADC i/p ch A10 = temp sense
    // ACD12SREF_0 = Vref+ = Vcc
    __delay_cycles(100);                      // delay to allow Ref to settle
    ADC12CTL0 |= ADC12ENC;     // Enable conversion

    // setup for LEDs, LCD, Keypad, Buttons
    initLeds();
    configDisplay();
    configKeypad();
    configButtons();
    DACInit();

    Graphics_clearDisplay(&g_sContext);
    Graphics_drawStringCentered(&g_sContext, "Press a Button", AUTO_STRING_LENGTH, 48, 15, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "To Begin", AUTO_STRING_LENGTH, 48, 25, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B1:DC Voltage", AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B2:Square Wave", AUTO_STRING_LENGTH, 48, 55, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B3:Sawtooth Wave", AUTO_STRING_LENGTH, 48, 65, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B4:Triangle Wave", AUTO_STRING_LENGTH, 48, 75, TRANSPARENT_TEXT);
    Graphics_flushBuffer(&g_sContext);

    // enables global interrupts
    _BIS_SR(GIE);

    while(1) {

        //clear the start bit
        ADC12CTL0 &= ~ADC12SC;
        //Sampling and conversion start
        ADC12CTL0 |= ADC12SC;

        while (ADC12CTL1 & ADC12BUSY) {
            states();
        }
        ADCPot = ADC12MEM0;
    }
}

// 1/100 s = 0.01s
// 1/75 s = 0.01333s
// 1/150 s = 0.006666s

// ideal is 1/300 = 0.003333s
// 32768/300 = 109.2266

void states() {

    /*
    switch(buttonStates()) {
    case BIT0:
        currentState = DC;
    break;
    case BIT1:
        currentState = SQUARE;
    break;
    case BIT2:
        currentState = SAWTOOTH;
    break;
    case BIT3:
        currentState = TRIANGLE;
    break;
    }
    */


    switch(currentState) {
    case DC:
    break;
    case SQUARE:
        if (timeCount % 100 < 50) {

        }

    break;
    case SAWTOOTH:

    break;
    case TRIANGLE:

    break;
    default:
    break;
    }
}

void DACInit(void) {
    // Configure LDAC and CS for digital IO outputs
    DAC_PORT_LDAC_SEL &= ~DAC_PIN_LDAC;
    DAC_PORT_LDAC_DIR |=  DAC_PIN_LDAC;
    DAC_PORT_LDAC_OUT |= DAC_PIN_LDAC; // Deassert LDAC

    DAC_PORT_CS_SEL   &= ~DAC_PIN_CS;
    DAC_PORT_CS_DIR   |=  DAC_PIN_CS;
    DAC_PORT_CS_OUT   |=  DAC_PIN_CS;  // Deassert CS
}

void DACSetValue(unsigned int dac_code) {
    // Start the SPI transmission by asserting CS (active low)
    // This assumes DACInit() already called
    DAC_PORT_CS_OUT &= ~DAC_PIN_CS;

    // Write in DAC configuration bits. From DAC data sheet
    // 3h=0011 to highest nibble.
    // 0=DACA, 0=buffered, 1=Gain=1, 1=Out Enbl
    dac_code |= 0x3000;   // Add control bits to DAC word

    uint8_t lo_byte = (unsigned char)(dac_code & 0x00FF);
    uint8_t hi_byte = (unsigned char)((dac_code & 0xFF00) >> 8);

    // First, send the high byte
    DAC_SPI_REG_TXBUF = hi_byte;

    // Wait for the SPI peripheral to finish transmitting
    while(!(DAC_SPI_REG_IFG & UCTXIFG)) {
        _no_operation();
    }

    // Then send the low byte
    DAC_SPI_REG_TXBUF = lo_byte;

    // Wait for the SPI peripheral to finish transmitting
    while(!(DAC_SPI_REG_IFG & UCTXIFG)) {
        _no_operation();
    }

    // We are done transmitting, so de-assert CS (set = 1)
    DAC_PORT_CS_OUT |=  DAC_PIN_CS;

    // This DAC is designed such that the code we send does not
    // take effect on the output until we toggle the LDAC pin.
    // This is because the DAC has multiple outputs. This design
    // enables a user to send voltage codes to each output and
    // have them all take effect at the same time.
    DAC_PORT_LDAC_OUT &= ~DAC_PIN_LDAC;  // Assert LDAC
    __delay_cycles(10);                 // small delay
    DAC_PORT_LDAC_OUT |=  DAC_PIN_LDAC;  // De-assert LDAC
}

// BUTTON CONFIGURATION HELPER
void configButtons() {
    //Sets P2.2, P3.6, P7.0, and P7.4 to IO
    P2SEL &= ~BIT2;
    P3SEL &= ~BIT6;
    P7SEL &= ~(BIT0 | BIT4);

    //Sets P2.2, P3.6, P7.0, and P7.4 to input
    P2DIR &= ~BIT2;
    P3DIR &= ~BIT6;
    P7DIR &= ~(BIT0 | BIT4);

    //Sets pins to use pull up/down
    P2REN |= BIT2;
    P3REN |= BIT6;
    P7REN |= (BIT0 | BIT4);

    //Sets pins to pull up
    P2OUT |= BIT2;
    P3OUT |= BIT6;
    P7OUT |= (BIT0 | BIT4);
}

// BUTTON PRESS CHECKING HANDLER
char buttonStates() {
    char returnState = 0x00;
    if ((P2IN & BIT2) == 0) {
        returnState |= BIT2;
    }
    if ((P3IN & BIT6) == 0) {
        returnState |= BIT1;
    }
    if ((P7IN & BIT0) == 0) {
        returnState |= BIT0;
    }
    if ((P7IN & BIT4) == 0) {
        returnState |= BIT3;
    }
    return returnState;
}


