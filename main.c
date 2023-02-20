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
long unsigned int timeToSeconds(long unsigned int month, long unsigned int day, long unsigned int hour, long unsigned int minute, long unsigned int second, bool leapYear);
void home();
void edit();
void configButtons();
char buttonStates();
void displayTime(long unsigned int seconds);
void displayTemp(float inAvgTempC);

//Stores the amount of temperatures to store
#define POTSIZE 36
// Temperature Sensor Calibration = Reading at 30 degrees C is stored at addr 1A1Ah
// See end of datasheet for TLV table memory mapping
#define CALADC12_15V_30C  *((unsigned int *)0x1A1A)
// Temperature Sensor Calibration = Reading at 85 degrees C is stored at addr 1A1Ch
//See device datasheet for TLV table memory mapping
#define CALADC12_15V_85C  *((unsigned int *)0x1A1C)

#define degC_per_bit (float)((float)(85.0 - 30.0))/((float)(CALADC12_15V_85C-CALADC12_15V_30C))

//Stores operating states of the program
enum States{HOME, EDIT};
//Stores editing states of the program
enum EditStates{MONTH, DAY, HOUR, MINUTE, SECOND};
//Stores the current program state
int currentState = HOME;
//Stores the current editing state
int editState = MONTH;
//Stores the amount of seconds corresponding to the amount of time passed
long unsigned int timeCount;
//Boolean for whether to update each second
bool update = false;
//Boolean for whether to update the temperature reading
bool poll = false;
//Used to store last 36 temperature readings
float avgTempC[POTSIZE];
//Stores the index used to add a new value to avgTempC
unsigned int adcIndex = 0;
//Stores the last read adc values for the temperature and potentiometer sensor
unsigned int ADCTemp, ADCPot;
//Used to store potentiometer value when switching editing states
unsigned int editADCVal = 2048;


// MAIN
void main(void) {

    //Initializes timeCount to May 12th, 23:59:59
    timeCount = timeToSeconds(5, 12, 23, 59, 59, false);

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
    configButtons();

    //Stores boolean for testing first readings for temperature and potentiometer adcs
    volatile bool firstReading[] = {true, true};

    while(1) {
        //clear the start bit
        ADC12CTL0 &= ~ADC12SC;
        //Sampling and conversion start
        ADC12CTL0 |= ADC12SC;

        //Checks if the temperature interrupt is raised and if a second has passed
        if (ADC12IFG0 && poll) {
            //Stored adc value
            ADCTemp = ADC12MEM0;
            //Runs if this is first temperature reading
            if (firstReading[0]) {
                volatile int i;
                //Sets the previous 36 readings to first reading
                for (i = 0; i < POTSIZE; i++) {
                    avgTempC[i] = (float)((long)ADCTemp - CALADC12_15V_30C) * degC_per_bit + 30.0;
                }
                //Sets first reading to false
                firstReading[0] = false;
            }
            poll = false;
        }
        //checks if potentiometer interrupt is raised
        if (ADC12IFG1) {
            ADCPot = ADC12MEM1;
            if (firstReading[1]) {
                editADCVal = ADCPot;
                firstReading[1] = false;
            }
        }
        //Switches between current states
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
    //Increments timeCount if not in editing mode
    if (currentState != EDIT) {
        timeCount++;
    }
    //Sets update to true to update screen
    update = true;
    //Sets poll to true to update temperature reading
    poll = true;
}

//Home state
void home() {
    if (update) {
        update = false;

        //Calculates the temperature in degrees C from most recent adc value
        float temperatureDegC = (float)((long)ADCTemp - CALADC12_15V_30C) * degC_per_bit + 30.0;

        //Replaces the oldest reading with the newest
        avgTempC[adcIndex] = temperatureDegC;
        //Increments index for new temperature readings
        adcIndex++;
        //Sets index to 0 if over the maximum size
        if (adcIndex >= POTSIZE) {
            adcIndex = 0;
        }

        volatile int i;
        volatile float avgC = 0;
        //Sums all the previous temperature readings
        for (i = 0; i < POTSIZE; i++) {
            avgC += avgTempC[i];
        }
        //Averages the temperature radings
        avgC = avgC / (float)POTSIZE;

        //Writes the date or temperature to the display
        Graphics_clearDisplay(&g_sContext);
        displayTime(timeCount);
        displayTemp(avgC);
        Graphics_flushBuffer(&g_sContext);
    }
}

//Edit state
void edit() {
    //Runs if 1 second has passed
    if (update) {
        update = false;
        //Writes date in editing mode to display
        Graphics_clearDisplay(&g_sContext); // Clear the display
        displayTime(timeCount);
        Graphics_flushBuffer(&g_sContext);
    }
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

//Returns bit for printing display elements
char dataElement() {
    //Prints date and time if in edit mode
    if (currentState == EDIT) {
        return BIT1 | BIT2;
    }
    //Used for debugging
    /*else if (currentState == HOME) {
        return BIT1 | BIT2 | BIT3 | BIT4;
    }*/
    //Prints date
    else if (timeCount % 12 <= 2) {
        return BIT1;
    }
    //Prints time
    else if (timeCount % 12 >= 3 && timeCount % 12 <= 5) {
        return BIT2;
    }
    //Prints temperature in C
    else if (timeCount % 12 >= 6 && timeCount % 12 <= 8) {
        return BIT3;
    }
    //Prints temperature in F
    else if (timeCount % 12 >= 9 && timeCount % 12 <= 11) {
        return BIT4;
    }
    return 0x00;
}

//Converts date and time into seconds
long unsigned int timeToSeconds(long unsigned int month, long unsigned int day, long unsigned int hour, long unsigned int minute, long unsigned int second, bool leapYear) {
    //Subtracts 1 since months start at 0
    month--;
    //Adds time from seconds, minutes, hours, and days
    long unsigned int newTimeime = second + minute * 60 + hour * 3600 + day * 86400;
    volatile int i;
    //Adds time based on the amount of days in the month up to the selected month
    for (i = 0; i < 12; i++) {
        if (i >= month) {
            i = 12;
        }
        else if (i == 0 || 2 || 4 || 6 || 7 || 9 || 11) {
            newTimeime += 30 * 86400;
        }
        else if (i == 3 || 5 || 8 || 10) {
            newTimeime += 29 * 86400;
        }
        else if (i == 1 && leapYear) {
            newTimeime += 28 * 86400;
        }
        else if (i == 1 && !leapYear) {
            newTimeime += 27 * 86400;
        }
        else {
            i = 12;
        }
    }
    return newTimeime;
}

//Displays the date and time
void displayTime(long unsigned int time) {
    //Calculates the time in seconds, minutes, hours, and days
    long unsigned int seconds  = time % 60;
    time -= seconds;
    long unsigned int minutes = (time % 3600) / 60;
    time -= minutes * 60;
    long unsigned int hours = (time % 86400) / 3600;
    time -= hours * 3600;
    long unsigned int days = time / 86400;
    long unsigned int months = 0;

    //Stores whether or not current year is a leap year
    bool leapYear = false;

    //Removes days for each month and adds it to month count until inputted month is reached
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
        else {
            i = 12;
        }
    }

    //Stores adjusted value if in edit mode
    int editorAdjust = 0;
    //Stores half of the upper bounds value based on the editing state
    int bounds = 0;
    //Stores int value of potentiometer based on the current reading minus the previous state adc reading
    //This keeps the value of the adjustment 0 when switching between editing states
    float conversionValue = ((float)ADCPot - (float)editADCVal) / 2048.0;
    //Sets bounds if in editing state
    if (currentState == EDIT) {
        //Sets bounds for month
        if (editState == MONTH) {
            editorAdjust = round(conversionValue * 6.0);
            editorAdjust += (int)months;
            bounds = 12;
        }
        //Sets bounds for day depending on the current month
        else if (editState == DAY) {
            if (months == 0 || 2 || 4 || 6 || 7 || 9 || 11) bounds = 30;
            else if (months == 3 || 5 || 8 || 10) bounds = 29;
            else if (months == 1 && leapYear) bounds = 28;
            else if (months == 1 && !leapYear) bounds = 27;
            editorAdjust = round(conversionValue * (float)bounds / 2.0);
            editorAdjust += (int)days;
        }
        //Sets bounds for hour
        else if (editState == HOUR) {
            editorAdjust = round(conversionValue * 12.0);
            editorAdjust += (int)hours;
            bounds = 24;
        }
        //Sets bounds for minutes and seconds
        else if (editState == MINUTE || SECOND) {
            editorAdjust = round(conversionValue * 30.0);
            if (editState == MINUTE) editorAdjust += (int)minutes;
            else editorAdjust += (int)seconds;
            bounds = 60;
        }
    }

    //If in edit state, set adjust value within bounds
    if (currentState == EDIT) {
        while (editorAdjust >= bounds) {
            editorAdjust -= bounds;
        }
        while (editorAdjust < 0) {
            editorAdjust += bounds;
        }

        //Changes current value to adjust value based on editing state
        if (editState == MONTH) months = (long unsigned int)editorAdjust;
        else if (editState == DAY) days = (long unsigned int)editorAdjust;
        else if (editState == HOUR) hours = (long unsigned int)editorAdjust;
        else if (editState == MINUTE) minutes = (long unsigned int)editorAdjust;
        else seconds = (long unsigned int)editorAdjust;
    }

    //Gets button input
    char buttons = buttonStates();

    //Test if the left button is pressed and not in editing state
    if (buttons & BIT0 && currentState != EDIT) {
        //Sets edit state to month, sets previous pot adc reading to current reading, and sets current state to edit
        editState = MONTH;
        editADCVal = ADCPot;
        currentState = EDIT;
    }
    //Tests if left button is pressed or current state is edit
    else if (buttons & BIT0 || (buttons & BIT3 && currentState == EDIT)) {
        //Appends the adjusted date and time into the real amount of time
        timeCount = timeToSeconds(months+1, days, hours, minutes, seconds, leapYear);
        //Advances the editing state to the next one
        if (editState == SECOND) {
            editState = MONTH;
        }
        else {
            editState++;
        }
        //Sets previous pot adc reading to current reading
        editADCVal = ADCPot;
        //Goes back to home state if the right button is pressed
        if (buttons & BIT3) {
            currentState = HOME;
        }
    }

    //Stores the date in the MMM DD format
    char month[] = {'J', 'a', 'n'};
    char day[] = {floor(days / 10) + 48, (days % 10) + 48};
    //Stores the time in the HH:MM:SS format
    char hour[] = {floor(hours / 10) + 48, (hours % 10) + 48};
    char minute[] = {floor(minutes / 10) + 48, (minutes % 10) + 48};
    char second[] = {floor(seconds / 10) + 48, (seconds % 10) + 48};

    //Sets the month character array based on the current month
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

    //Creates the date and time arrays to print to the screen
    char date[] = {month[0], month[1], month[2], ' ', day[0], day[1], 0x00};
    char clock[] = {hour[0], hour[1], ':', minute[0], minute[1], ':', second[0], second[1], 0x00};

    //Adds an underline under the current editing state if current state is in edit mode
    if (currentState == EDIT) {
        if (editState == HOUR) {
            Graphics_drawStringCentered(&g_sContext, "__      ", AUTO_STRING_LENGTH, 48, 52, TRANSPARENT_TEXT);
        }
        else if (editState == MINUTE) {
            Graphics_drawStringCentered(&g_sContext, "   __   ", AUTO_STRING_LENGTH, 48, 52, TRANSPARENT_TEXT);
        }
        else if (editState == SECOND) {
            Graphics_drawStringCentered(&g_sContext, "      __", AUTO_STRING_LENGTH, 48, 52, TRANSPARENT_TEXT);
        }
        else if (editState == MONTH) {
            Graphics_drawStringCentered(&g_sContext, "___   ", AUTO_STRING_LENGTH, 48, 42, TRANSPARENT_TEXT);
        }
        else if (editState == DAY) {
            Graphics_drawStringCentered(&g_sContext, "    __", AUTO_STRING_LENGTH, 48, 42, TRANSPARENT_TEXT);
        }
    }

    //Changes y values to center time if in editing state and time and temperature if in home state
    int dataY = 25;
    if (currentState == EDIT) {
        dataY = 40;
    }

    //Prints the date
    if (dataElement() & BIT1) {
        Graphics_drawStringCentered(&g_sContext, date, AUTO_STRING_LENGTH, 48, dataY, TRANSPARENT_TEXT);
    }
    //Prints the time
    if ((dataElement() & BIT2) == BIT2) {
        Graphics_drawStringCentered(&g_sContext, clock, AUTO_STRING_LENGTH, 48, dataY + 10, TRANSPARENT_TEXT);
    }

}

//Displays temperature
void displayTemp(float inAvgTempC) {
    //Converts inputted degrees in C to F
    float CToF = inAvgTempC * 9.0/5.0 + 32.0;

    //Stores degree in C array in DDD.D C format
    char tempC[] = {floor(inAvgTempC / 100.0) + 48,
                    ((unsigned int)floor(inAvgTempC / 10.0) % 10) + 48,
                    floor((unsigned int)inAvgTempC % 10) + 48,
                    '.',
                    ((int)floor(inAvgTempC * 10.0) % 10) + 48,
                    ' ', 'C', 0x00};

    //Stores degree in F array in DDD.D F format
    char tempF[] = {floor(CToF / 100.0) + 48,
                    ((unsigned int)floor(CToF / 10.0) % 10) + 48,
                    ((unsigned int)CToF % 10) + 48, '.',
                    (unsigned int)floor(CToF * 10.0) % 10 + 48,
                    ' ', 'F', 0x00};

    //Prints the temperature in C
    if ((dataElement() & BIT3) == BIT3) {
        Graphics_drawStringCentered(&g_sContext, tempC, AUTO_STRING_LENGTH, 48, 45, TRANSPARENT_TEXT);
    }
    //Prints the temperature in F
    if ((dataElement() & BIT4) == BIT4) {
        Graphics_drawStringCentered(&g_sContext, tempF, AUTO_STRING_LENGTH, 48, 55, TRANSPARENT_TEXT);
    }
}


