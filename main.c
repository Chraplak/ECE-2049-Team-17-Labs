/************** ECE2049 DEMO CODE ******************/
/**************  13 March 2019   ******************/
/***************************************************/
// INCLUDES
#include <msp430.h>
#include <stdlib.h>
#include <math.h>
#include "peripherals.h"

// PROTOTYPES
__interrupt void Timer_A2_ISR(void);
void home();
void edit();
void displayTime(int count);
void displayTemp(float inAvgTempC);

#define ADCSIZE 36
// Temperature Sensor Calibration = Reading at 30 degrees C is stored at addr 1A1Ah
// See end of datasheet for TLV table memory mapping
#define CALADC12_15V_30C  *((unsigned int *)0x1A1A)
// Temperature Sensor Calibration = Reading at 85 degrees C is stored at addr 1A1Ch
//See device datasheet for TLV table memory mapping
#define CALADC12_15V_85C  *((unsigned int *)0x1A1C)

#define degC_per_bit (float)((float)(85.0 - 30.0))/((float)(CALADC12_15V_85C-CALADC12_15V_30C))

enum States{HOME, EDIT};
int currentState = HOME;
bool update = false;
float acdC[ADCSIZE];
unsigned int adcIndex = 0;
int timeCount = 0;

int dayCount[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
int date[5];
int tempC[5];
int tempF[5];


// MAIN
void main(void) {

    WDTCTL = WDTPW | WDTHOLD;    // Stop watchdog timer. Always need to stop this!!
                                 // You can then configure it properly, if desired

    // timer A2 management
    TA2CTL = TASSEL_1 + ID_0 + MC_1; // 32786 Hz is set
    TA2CCR0 = 32785; // sets interrupt to occur every (TA2CCR0 + 1)/32786 seconds
    TA2CCTL0 = CCIE; // enables TA2CCR0 interrupt

    // enables global interrupts
    _BIS_SR(GIE);


    REFCTL0 &= ~REFMSTR;    // Reset REFMSTR to hand over control of
                            // internal reference voltages to
                            // ADC12_A control registers

    ADC12CTL0 = ADC12SHT0_9 | ADC12REFON | ADC12ON;     // Internal ref = 1.5V

    ADC12CTL1 = ADC12SHP;    // Enable sample timer

    // Using ADC12MEM0 to store reading
    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_10;    // ADC i/p ch A10 = temp sense
                                                // ACD12SREF_1 = internal ref = 1.5v

    __delay_cycles(100);                        // delay to allow Ref to settle

    ADC12CTL0 |= ADC12ENC;                      // Enable conversion

    // setup for LEDs, LCD, Keypad, Buttons
    initLeds();
    configDisplay();
    configKeypad();

    // state machine
    while (1) {
        char key = getKey();
        switch (currentState) {
        case HOME:
            home();
        break;
        case EDIT:
            edit();
        break;
        }
    }
}

// TIMER INTERRUPT
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {
    timeCount++;
    update = true;
}

void home() {
    if (update) {
        update = false;

        ADC12CTL0 &= ~ADC12SC;      // clear the start bit
        ADC12CTL0 |= ADC12SC;       // Sampling and conversion start
                                    // Single conversion (single channel)

        unsigned int adc = ADC12MEM0;

        float temperatureDegC = (float) ( (long)adc - CALADC12_15V_30C) * degC_per_bit + 30.0;

        acdC[adcIndex] = temperatureDegC;
        adcIndex++;
        if (adcIndex >= ADCSIZE) {
            adcIndex = 0;
        }

        volatile int i;
        volatile float avgC = 0;
        for (i = 0; i < ADCSIZE; i++) {
            avgC += acdC[i];
        }
        avgC = avgC / (float)ADCSIZE;

        if (timeCount % 3 == 0) {
            Graphics_clearDisplay(&g_sContext); // Clear the display
            displayTime(timeCount);
            displayTemp(avgC);
            Graphics_flushBuffer(&g_sContext);
        }
    }
}

void edit() {

}
void displayTime (int count) {
    int rawDays = count / 86400; // 60 * 60 * 24 = seconds in a day
    int daysAccum = 0;

    int i = 0;
    for(i=0;i<12;i++) {
        if (rawDays < (dayCount[i]+daysAccum)) {
            date[0] = i + 1;
            date[1] = (rawDays - daysAccum) + 1;
            date[2] = ((count % 86400) - (daysAccum * 86400)) / 3600;
            date[3] = ((count % 86400) - (daysAccum * 86400)) % 3600 / 60;
            date[4] = ((count % 86400) - (daysAccum * 86400)) % 3600 % 60;
            break;
        }
        else daysAccum += dayCount[i];
    }
}


void displayTemp(float inAvgTempC) {
    int correctedC = (int)(inAvgTempC * 10);
    tempC[0] = correctedC / 1000;
    tempC[1] = (correctedC - (tempC[0] * 1000)) / 100;
    tempC[2] = (correctedC - (tempC[1] * 100)) / 10;
    tempC[3] = '.';
    tempC[4] = inAvgTempC - (tempC[2] * 10);
    Graphics_drawStringCentered(&g_sContext, (char)tempC, AUTO_STRING_LENGTH, 48, 25, TRANSPARENT_TEXT);

    int correctedF = (correctedC * 9 / 5) + 32;
    tempF[0] = correctedF / 1000;
    tempF[1] = (correctedF - (tempF[0] * 1000)) / 100;
    tempF[2] = (correctedF - (tempF[1] * 100)) / 10;
    tempF[3] = '.';
    tempF[4] = correctedF - (tempF[2] * 10);
    Graphics_drawStringCentered(&g_sContext, (char)tempF, AUTO_STRING_LENGTH, 48, 25, TRANSPARENT_TEXT);

}



