// INCLUDES
#include <msp430.h>
#include <stdlib.h>
#include <math.h>
#include "peripherals.h"

// PROTOTYPES
__interrupt void Timer_A2_ISR(void);
void DACInit(void);
void DACSetValue(unsigned int dac_code);
void configButtons();
char buttonStates();

//Stores the operating states of the board
enum States{HOME, DC, SQUARE, SAWTOOTH, TRIANGLE};
//Stores the current state
int currentState = HOME;
//Stores the ADC value received from the potentiometer
unsigned int ADCPot = 0;
//Boolean storing whether or not the triangle is rising or falling
bool goingUp = true;
//Stores the count amount from the interrupt to control waves
unsigned int counter = 0;
//Stores the max value of the DAC
unsigned int max = 4095;
//Stores the value to set the DAC at
unsigned int DACValue = 0;
//Stores the increment amount for the triangle wave
unsigned int increment = 210;
//Stores values for testing the linearity
float linVal = 0.0;
unsigned int ADC = 0;


// TIMER INTERRUPT
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {

    //Temporary variables to test the linearity functionality
    float linTest;
    unsigned int testDAC;
    linTest = linVal;
    testDAC = DACValue;

    //Switches between the operating states of the board
    switch(currentState) {
    case SQUARE:
        counter++;
        //Resets the count value if over the set value
        if (counter >= 56) {
            counter = 0;
        }

        //Sets the wave to have a 50% duty cycle
        if (counter < 28) {
            DACValue = ADCPot;
        }
        else {
            DACValue = 0;
        }
    break;
    case SAWTOOTH:
        //Sets the DAC value to the counter amount
        DACValue = counter;
        //Increments the counter
        counter+= 53;
        //Resets the counter to 0 when reaching the summit
        if(counter >= max - 53) counter = 0;
    break;
    case TRIANGLE:
        //Sets the DAC value to the counter amount
        DACValue = counter;

        //Incremennts or decrements based on going up boolean
        if(goingUp) counter+= increment;
        else counter-= increment;
        //Switches going up boolean based on the current count value
        if(counter > 4095 - increment) {
            goingUp = false;
        }
        else if(counter < increment) {
            goingUp = true;
        }

    break;
    case DC:
        //Sets the DAC based on the read ADC value
        DACValue = ADCPot;
    break;
    }
}

// MAIN
void main(void) {
    WDTCTL = WDTPW + WDTHOLD;   // Stop WDT

    // timer A2 management
    TA2CTL = TASSEL_2 + ID_0 + MC_1; // 1,048,576 Hz is set
    TA2CCR0 = 183; // sets interrupt to occur every (TA2CCR0 + 1)/1,048,576 seconds
    TA2CCTL0 = CCIE; // enables TA2CCR0 interrupt

    // Configure P8.0 as digital IO output and set it to 1
    // This supplied 3.3 volts across scroll wheel potentiometer
    // See schematic at end or MSP-EXP430F5529 board users guide
    P6SEL &= ~BIT0;
    // Reset REFMSTR to hand over control
    REFCTL0 &= ~REFMSTR;
    // internal reference voltages to
    // ADC12_A control registers
    ADC12CTL0 = ADC12SHT0_9 | ADC12ON | ADC12MSC | ADC12REFON;
    // Enable sample timer and multi-channel readings
    ADC12CTL1 = ADC12SHP + ADC12CONSEQ_1;

    ADC12MCTL0 = ADC12SREF_0 + ADC12INCH_0;    //ADC for potentiometer
    ADC12MCTL1 = ADC12SREF_0 + ADC12INCH_1 + ADC12EOS;  //ADC for A1
    // ACD12SREF_0 = Vref+ = Vcc
    __delay_cycles(100);                      // delay to allow Ref to settle
    ADC12CTL0 |= ADC12ENC | ADC12SC;     // Enable conversion

    // setup for LEDs, LCD, Keypad, Buttons
    initLeds();
    configDisplay();
    configKeypad();
    configButtons();
    DACInit();

    //Prints instructions to screen
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawStringCentered(&g_sContext, "Press a Button", AUTO_STRING_LENGTH, 48, 15, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "To Begin", AUTO_STRING_LENGTH, 48, 25, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B1:DC Voltage", AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B2:Square Wave", AUTO_STRING_LENGTH, 48, 55, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B3:Sawtooth Wave", AUTO_STRING_LENGTH, 48, 65, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, "B4:Triangle Wave", AUTO_STRING_LENGTH, 48, 75, TRANSPARENT_TEXT);
    Graphics_flushBuffer(&g_sContext);

    //Configures A1 to be able to read input voltages
    P6SEL &= ~BIT1;
    P6DIR &= ~BIT1;

    // enables global interrupts
    _BIS_SR(GIE);

    //clear the start bit
    ADC12CTL0 &= ~ADC12SC;
    //Sampling and conversion start
    ADC12CTL0 |= ADC12SC;

    while(1) {
        //Tests if interrupt occurs for potentiometer
        if (ADC12IFG0) {
            ADCPot = ADC12MEM0;
            //clear the start bit
            ADC12CTL0 &= ~ADC12SC;
            //Sampling and conversion start
            ADC12CTL0 |= ADC12SC;
        }
        //Tests if interrupt occurs for A1
        if (ADC12IFG1) {
            ADC = ADC12MEM1;
            //clear the start bit
            ADC12CTL0 &= ~ADC12SC;
            //Sampling and conversion start
            ADC12CTL0 |= ADC12SC;
        }

        //Gets linearity value if on square wave
        if (currentState == SQUARE) {
            linVal = (float)ADC / 4095.0 * 3.3;
        }

        //Gets the buttons pressed
        char buttons = buttonStates();

        //Switches states based on what button is pressed and resets the wave variables
        if (buttons & BIT0) {
            currentState = DC;
            counter = 0;
        }
        else if (buttons & BIT1) {
            currentState = SQUARE;
            counter = 0;
        }
        else if (buttons & BIT2) {
            currentState = SAWTOOTH;
            counter = 0;
        }
        else if (buttons & BIT3) {
            currentState = TRIANGLE;
            counter = 0;
            goingUp = true;
        }

        //Sets the DAC values to the value set in the interrupt
        DACSetValue(DACValue);
    }
}

// DAC INITIALIZING HELPER
void DACInit(void) {
    // Configure LDAC and CS for digital IO outputs
    DAC_PORT_LDAC_SEL &= ~DAC_PIN_LDAC;
    DAC_PORT_LDAC_DIR |=  DAC_PIN_LDAC;
    DAC_PORT_LDAC_OUT |= DAC_PIN_LDAC; // Deassert LDAC

    DAC_PORT_CS_SEL   &= ~DAC_PIN_CS;
    DAC_PORT_CS_DIR   |=  DAC_PIN_CS;
    DAC_PORT_CS_OUT   |=  DAC_PIN_CS;  // Deassert CS
}

// DAC SETTING HELPER
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

// Returns button states with B1-B4 corresponding to BIT0-BIT3
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



