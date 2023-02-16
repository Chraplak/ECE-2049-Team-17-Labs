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
void states();
void home();
void edit();
void displayTime(long unsigned int seconds);
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
enum EditStates{HOURS, MINUTES, SECONDS};
int currentState = EDIT;
int editState = HOURS;
long unsigned int timeCount = (31 + 28 + 31 + 30 + 13) * 86400 - 60;
bool update = false;
float acdC[ADCSIZE];
unsigned int adcIndex = 0;
unsigned int ADCTemp, ADCPot;


// MAIN
void main(void) {
    WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

    // timer A2 management
        TA2CTL = TASSEL_1 + ID_0 + MC_1; // 32786 Hz is set
        TA2CCR0 = 32785; // sets interrupt to occur every (TA2CCR0 + 1)/32786 seconds
        TA2CCTL0 = CCIE; // enables TA2CCR0 interrupt

        // enables global interrupts
        _BIS_SR(GIE);

    // Configure P8.0 as digital IO output and set it to 1
    // This supplied 3.3 volts across scroll wheel potentiometer
    // See schematic at end or MSP-EXP430F5529 board users guide
    P6SEL &= ~BIT0;
    REFCTL0 &= ~REFMSTR;                      // Reset REFMSTR to hand over control
    // internal reference voltages to
    // ADC12_A control registers
    ADC12CTL0 = ADC12SHT0_9 | ADC12ON | ADC12MSC | ADC12REFON;

    ADC12CTL1 =  ADC12SHP + ADC12CONSEQ_1;                     // Enable sample timer

    ADC12MCTL0 = ADC12SREF_1 + ADC12INCH_10;    // ADC i/p ch A10 = temp sense
    // Use ADC12MEM0 register for conversion results
    ADC12MCTL1 = ADC12SREF_0 + ADC12INCH_0 + ADC12EOS;   // ADC12INCH5 = Scroll wheel = A0
    // ACD12SREF_0 = Vref+ = Vcc
    __delay_cycles(100);                      // delay to allow Ref to settle
    ADC12CTL0 |= ADC12ENC | ADC12SC;     // Enable conversion

    // setup for LEDs, LCD, Keypad, Buttons
    initLeds();
    configDisplay();
    configKeypad();

    volatile bool firstReading = true;

    while(1) {
        ADC12CTL0 &= ~ADC12SC;  // clear the start bit
        ADC12CTL0 |= ADC12SC;               // Sampling and conversion start
        // Single conversion (single channel)
        // Poll busy bit waiting for conversion to complete
        while (ADC12CTL1 & ADC12BUSY) states();
        ADCTemp = ADC12MEM0;
        if (firstReading) {
            volatile int i;
            for (i = 0; i < ADCSIZE; i++) {
                acdC[i] = (float)((long)ADCTemp - CALADC12_15V_30C) * degC_per_bit + 30.0;
            }
        }
        ADCPot = ADC12MEM1;               // Read results if conversion done
        states();
    }
}

void states() {
    switch (currentState) {
    case HOME:
        home();
    break;
    case EDIT:
        edit();
    break;
    }
}

// TIMER INTERRUPT
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer_A2_ISR(void) {
    if (currentState != EDIT) {
        timeCount++;
    }
    update = true;
}

void home() {
    if (update) {
        update = false;

        float temperatureDegC = (float)((long)ADCTemp - CALADC12_15V_30C) * degC_per_bit + 30.0;

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

        Graphics_clearDisplay(&g_sContext); // Clear the display
        displayTime(timeCount);
        displayTemp(avgC);
        Graphics_flushBuffer(&g_sContext);
    }
}

void edit() {
    if (update) {
        update = false;
        Graphics_clearDisplay(&g_sContext); // Clear the display
        displayTime(timeCount);
        Graphics_flushBuffer(&g_sContext);
    }
}

unsigned int dataElement() {
    if (currentState == EDIT) {
        return 2;
    }
    else if (timeCount % 12 <= 2) {
        return 1;
    }
    else if (timeCount % 12 >= 3 && timeCount % 12 <= 5) {
        return 2;
    }
    else if (timeCount % 12 >= 6 && timeCount % 12 <= 8) {
        return 3;
    }
    else if (timeCount % 12 >= 9 && timeCount % 12 <= 11) {
        return 4;
    }
    return 0;
}

void displayTime(long unsigned int seconds) {
    unsigned int sec  = seconds % 60;
    seconds -= sec;
    unsigned int minutes = (seconds % 3600) / 60;
    seconds -= minutes * 60;
    unsigned int hours = (seconds % 86400) / 3600;
    seconds -= hours * 3600;
    unsigned int days = seconds / 86400;

    unsigned int months = 0;
    bool leapYear = false;

    volatile int i = 0;
    for (i = 0; i < 12; i++) {
        if ((i == 0 || i == 2 || i == 4 || i == 6 || i == 7 || i == 9 || i == 11) && days >= 31) {
            months++;
            days -= 31;
        }
        else if ((i == 3 || i == 5 || i == 8 || i == 10) && days >= 30) {
            months++;
            days -= 30;
        }
        else if (i == 1 && leapYear && days >= 29) {
            months++;
            days -= 29;
        }
        else if (i == 1 && !leapYear && days >= 28) {
            months++;
            days -= 28;
        }
        else i = 12;
    }

    char month[] = {'J', 'a', 'n'};
    char day[] = {floor(days / 10) + 48, (days % 10) + 48};
    char hour[] = {floor(hours / 10) + 48, (hours % 10) + 48};
    char minute[] = {floor(minutes / 10) + 48, (minutes % 10) + 48};
    char second[] = {floor(sec / 10) + 48, (sec % 10) + 48};

    switch (months) {
    case 1:
        month[0] = 'F';month[1] = 'e';month[2] = 'b';
    break;
    case 2:
        month[0] = 'M';month[1] = 'a';month[2] = 'r';
    break;
    case 3:
        month[0] = 'A';month[1] = 'p';month[2] = 'r';
    break;
    case 4:
        month[0] = 'M';month[1] = 'a';month[2] = 'y';
    break;
    case 5:
        month[0] = 'J';month[1] = 'u';month[2] = 'n';
    break;
    case 6:
        month[0] = 'J';month[1] = 'u';month[2] = 'l';
    break;
    case 7:
        month[0] = 'A';month[1] = 'u';month[2] = 'g';
    break;
    case 8:
        month[0] = 'S';month[1] = 'e';month[2] = 'p';
    break;
    case 9:
        month[0] = 'O';month[1] = 'c';month[2] = 't';
    break;
    case 10:
        month[0] = 'N';month[1] = 'o';month[2] = 'v';
    break;
    case 11:
        month[0] = 'D';month[1] = 'e';month[2] = 'c';
    break;
    }

    char date[] = {month[0], month[1], month[2], ' ', day[0], day[1], 0x00};
    char time[] = {hour[0], hour[1], ':', minute[0], minute[1], ':', second[0], second[1], 0x00};

    if (currentState == EDIT) {
        if (editState == HOURS) {
            Graphics_drawStringCentered(&g_sContext, "__      ", AUTO_STRING_LENGTH, 48, 47, TRANSPARENT_TEXT);
        }
        else if (editState == MINUTES) {
            Graphics_drawStringCentered(&g_sContext, "   __   ", AUTO_STRING_LENGTH, 48, 47, TRANSPARENT_TEXT);
        }
        else if (editState == SECONDS) {
            Graphics_drawStringCentered(&g_sContext, "      __", AUTO_STRING_LENGTH, 48, 47, TRANSPARENT_TEXT);
        }
    }

    if (dataElement() == 1) {
        Graphics_drawStringCentered(&g_sContext, date, AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    }
    else if (dataElement() == 2) {
        Graphics_drawStringCentered(&g_sContext, time, AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    }

}

void displayTemp(float inAvgTempC) {
    float CToF = inAvgTempC * 9.0/5.0 + 32.0;

    char tempC[] = {floor(inAvgTempC / 100.0) + 48,
                    ((unsigned int)floor(inAvgTempC / 10.0) % 10) + 48,
                    floor((unsigned int)inAvgTempC % 10) + 48,
                    '.',
                    ((int)floor(inAvgTempC * 10.0) % 10) + 48,
                    ' ', 'C', 0x00};

    char tempF[] = {floor(CToF / 100.0) + 48,
                    ((unsigned int)floor(CToF / 10.0) % 10) + 48,
                    ((unsigned int)CToF % 10) + 48, '.',
                    (unsigned int)floor(CToF * 10.0) % 10 + 48,
                    ' ', 'F', 0x00};

    if (dataElement() == 3) {
        Graphics_drawStringCentered(&g_sContext, tempC, AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    }
    else if (dataElement() == 4) {
        Graphics_drawStringCentered(&g_sContext, tempF, AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    }
}


