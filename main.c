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
void displayTime(long unsigned int seconds);
void displayTemp(float inAvgTempC);

enum States{HOME, EDIT};
int currentState = HOME;
long unsigned int timeCount = 0;

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
}

void home() {

}

void edit() {

}

void displayTime(long unsigned int seconds) {
    unsigned int sec  = seconds % 60;
    seconds -= sec;
    unsigned int minutes = seconds % 3600;
    seconds -= minutes * 60;
    unsigned int hours = seconds % 86400;
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

    char month[] = "Jan";
    char day[] = {floor(days / 10) + 30, (days % 10) + 30};
    char hour[] = {floor(hours / 10) + 30, (hours % 10) + 30};
    char minute[] = {floor(minutes / 10) + 30, (minutes % 10) + 30};
    char second[] = {floor(seconds / 10) + 30, (seconds % 10) + 30};

    switch (days) {
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

    char date[] = {month, ' ', day};
    char time[] = {hour, ':', minute, ':', second};

    Graphics_drawStringCentered(&g_sContext, date, AUTO_STRING_LENGTH, 48, 25, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, time, AUTO_STRING_LENGTH, 48, 35, TRANSPARENT_TEXT);
}

void displayTemp(float inAvgTempC) {
    float CToF = 33.8 * inAvgTempC;

    float intermediate;

    char tempC[] = {floor(inAvgTempC / 100.0) + 30, (floor(inAvgTempC / 10.0) % 10) + 30, floor(inAvgTempC % 10) + 30, '.', floor(inAvgTempC * 10.0) % 10 + 30, ' ', 'C'};
    char tempF[] = {floor(CToF / 100.0) + 30, (floor(CToF / 10.0) % 10) + 30, floor(CToF % 10) + 30, '.', floor(CToF * 10.0) % 10 + 30, ' ', 'F'};

    Graphics_drawStringCentered(&g_sContext, tempC, AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    Graphics_drawStringCentered(&g_sContext, tempF, AUTO_STRING_LENGTH, 48, 55, TRANSPARENT_TEXT);
}


