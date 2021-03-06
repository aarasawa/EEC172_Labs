// Names:
// Alexander Arasawa
// Jiawei Zheng

// standard includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

// driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "hw_apps_rcm.h"
#include "prcm.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "gpio.h"
#include "utils.h"
#include "systick.h"
#include "hw_timer.h"
#include "uart.h"
#include "timer.h"
#include "timer_if.h"

// common interface includes
#include "uart_if.h"
#include "pin_mux_config.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

// color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define GREEN 0x07E0
#define CYAN 0x07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

// variable for looping
int i;

// as specified, bit rate should be at least 400 kbps
#define SPI_IF_BIT_RATE 400000

// samples array
int samples[410];

int samples_index = 0;

// just for reference; not used
int f_tone[8] = {697, 770, 852, 941, 1209, 1336, 1477, 1633};

// precalculated coefficients
int coeff[8] = {31548, 31281, 30951, 30556, 29144, 28361, 27409, 26258};

// array to store calculated power of 8 frequencies; debounce
int power_all[8];

extern void (*const g_pfnVectors[])(void);

typedef struct PinSetting
{
        unsigned long port;
        unsigned int pin;
} PinSetting;

static PinSetting pin8 = {.port = GPIOA2_BASE, .pin = 0x2}; // GPIO pin8 for MISO of ADC

static void
BoardInit(void)
{
        MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);

        MAP_IntMasterEnable();
        MAP_IntEnable(FAULT_SYSTICK);

        PRCMCC3200MCUInit();
}

unsigned char g_ucTxBuff[2];

// we want to read two bytes from ADC each time
unsigned char g_ucRxBuff[2];

void readADC()
{
        // clear periodic timer interrupt flag
        Timer_IF_InterruptClear(TIMERA0_BASE);

        if (samples_index != 410)
        {
                // assert that we want to read from ADC
                MAP_GPIOPinWrite(pin8.port, pin8.pin, 0);

                // get readings from ADC into g_ucRxBuff
                MAP_SPITransfer(GSPI_BASE, g_ucTxBuff, g_ucRxBuff, 2, SPI_CS_ENABLE | SPI_CS_DISABLE);

                // reset MISO for another ADC reading
                MAP_GPIOPinWrite(pin8.port, pin8.pin, pin8.pin);

                // use bit shifting to get the correct data, readjust with -390 due to DC bias,
                // and then append to samples array
                samples[samples_index++] = ((*((int *)g_ucRxBuff) & 0x1F) << 5 | (*((int *)g_ucRxBuff) & 0xF800) >> 11) - 390;
        }
}

// Goertzel function
// https://github.com/OmaymaS/DTMF-Detection-Goertzel-Algorithm-/blob/master/Detect_DTMF_July2014.c
long int
goertzel(int sample[], long int coeff, int N)
{
        // initialize variables to be used in the function
        int Q, Q_prev, Q_prev2, i;
        long prod1, prod2, prod3, power;

        Q_prev = 0;  // set delay element1 Q_prev as zero
        Q_prev2 = 0; // set delay element2 Q_prev2 as zero
        power = 0;   // set power as zero

        for (i = 0; i < N; i++) // loop N times and calculate Q, Q_prev, Q_prev2 at each iteration
        {
                Q = (sample[i]) + ((coeff * Q_prev) >> 14) - (Q_prev2); // >>14 used as the coeff was used in Q15 format
                Q_prev2 = Q_prev;                                       // shuffle delay elements
                Q_prev = Q;
        }

        // calculate the three products used to calculate power
        prod1 = ((long)Q_prev * Q_prev);
        prod2 = ((long)Q_prev2 * Q_prev2);
        prod3 = ((long)Q_prev * coeff) >> 14;
        prod3 = (prod3 * Q_prev2);

        power = ((prod1 + prod2 - prod3)) >> 8; // calculate power using the three products and scale the result down

        return power;
}

// our letter struct. makes it easier to store and send
// strings that contain different colored characters
typedef struct Letter
{
        unsigned int x;
        char alpha;
        int color;
} Letter;

// global x and y position on OLED for composing message
int c_x = 0;
int c_y = 10;

// global x and y position on OLED for the receiving message
int r_x = 0;
int r_y = 64;

// for storing the current composing message
// user adds to composing messsage by pressing buttons
Letter c_message[128];
int c_message_index = -1; // initially, invalid

// clear composing message
void c_clear()
{
        drawFastHLine(0, 10, 128, BLACK);
        drawFastHLine(0, 11, 128, BLACK);
        drawFastHLine(0, 12, 128, BLACK);
        drawFastHLine(0, 13, 128, BLACK);
        drawFastHLine(0, 14, 128, BLACK);
        drawFastHLine(0, 15, 128, BLACK);
        drawFastHLine(0, 16, 128, BLACK);
        drawFastHLine(0, 17, 128, BLACK);
        c_message_index = -1;
        c_x = 0;
        c_y = 10;
}

// clear receiving message
void r_clear()
{
        drawFastHLine(0, 64, 128, BLACK);
        drawFastHLine(0, 65, 128, BLACK);
        drawFastHLine(0, 66, 128, BLACK);
        drawFastHLine(0, 67, 128, BLACK);
        drawFastHLine(0, 68, 128, BLACK);
        drawFastHLine(0, 69, 128, BLACK);
        drawFastHLine(0, 70, 128, BLACK);
        drawFastHLine(0, 71, 128, BLACK);
        r_x = 0;
}

// clear last composing message character
void c_char_clear()
{
        drawChar(c_message[c_message_index].x, c_y, c_message[c_message_index].alpha, BLACK, BLACK, 1);
}

// color array to cycle through so characters can be different colors, as requested by specifications
int color_arr[5] = {WHITE, RED, GREEN, BLUE, YELLOW};
int color_arr_index = 0;

void SendMessage()
{
        if (c_message_index != -1)
        {
                int i;

                // start of message, for other board to know there is a incoming message
                while (UARTBusy(UARTA1_BASE))
                        ;
                UtilsDelay(999999 * 3);
                UARTCharPut(UARTA1_BASE, '5');

                for (i = 0; i <= c_message_index; i++)
                {
                        // wait for UART to be available
                        while (UARTBusy(UARTA1_BASE))
                                ;
                        UtilsDelay(999999 * 3);

                        // send color of Letter first
                        if (c_message[i].color == 0)
                        {
                                UARTCharPut(UARTA1_BASE, '0');
                        }
                        if (c_message[i].color == 1)
                        {
                                UARTCharPut(UARTA1_BASE, '1');
                        }
                        if (c_message[i].color == 2)
                        {
                                UARTCharPut(UARTA1_BASE, '2');
                        }
                        if (c_message[i].color == 3)
                        {
                                UARTCharPut(UARTA1_BASE, '3');
                        }
                        if (c_message[i].color == 4)
                        {
                                UARTCharPut(UARTA1_BASE, '4');
                        }

                        while (UARTBusy(UARTA1_BASE))
                                ;
                        UtilsDelay(999999 * 3);

                        // send the Letter afterwards
                        UARTCharPut(UARTA1_BASE, c_message[i].alpha);
                }

                c_message_index = -1;
        }
}

// global variable for aid with time interval since last button pressed
long int timer;

// global variable for storing previous characters
// initially, not set to any of the buttons that map to characters
char prevbutton = '#';

// Post-test function
// https://github.com/OmaymaS/DTMF-Detection-Goertzel-Algorithm-/blob/master/Detect_DTMF_July2014.c
void post_test(void)
{
        // initialize variables to be used in the function
        int i, row, col, max_power;

        char row_col[4][4] = // array with the order of the digits in the DTMF system
            {
                {'1', '2', '3', 'A'},
                {'4', '5', '6', 'B'},
                {'7', '8', '9', 'C'},
                {'*', '0', '#', 'D'}};

        // find the maximum power in the row frequencies and the row number

        max_power = 0; // initialize max_power=0

        for (i = 0; i < 4; i++) // loop 4 times from 0>3 (the indecies of the rows)
        {
                if (power_all[i] > max_power) // if power of the current row frequency > max_power
                {
                        max_power = power_all[i]; // set max_power as the current row frequency
                        row = i;                  // update row number
                }
        }

        // find the maximum power in the column frequencies and the column number

        max_power = 0; // initialize max_power=0

        for (i = 4; i < 8; i++) // loop 4 times from 4>7 (the indecies of the columns)
        {
                if (power_all[i] > max_power) // if power of the current column frequency > max_power
                {
                        max_power = power_all[i]; // set max_power as the current column frequency
                        col = i;                  // update column number
                }
        }

        if (power_all[col] > 75000 && power_all[row] > 75000) // If confident above threshold, register the button that was pressed
        {
                Report("%c\n\r", row_col[row][col - 4]); // display the button pressed to terminal

                // get time interval since last button pressed and restart Timer_A in TIMERA1_BASE
                int time_since_last_press = TimerValueGet(TIMERA1_BASE, TIMER_A) - timer;
                TimerValueSet(TIMERA1_BASE, TIMER_A, 0);
                timer = TimerValueGet(TIMERA1_BASE, TIMER_A);

                switch (row_col[row][col - 4])
                {
                case '0':
                        // space
                        // add Letter ' ' to composing message
                        c_message[++c_message_index].alpha = ' ';
                        c_message[c_message_index].x = c_x;
                        c_message[c_message_index].color = color_arr_index;
                        setCursor(c_x, c_y);
                        Outstr(" ");

                        // update composing message x position for next character
                        c_x += 6;

                        // update previous button pressed
                        prevbutton = '0';
                        break;
                case '1':
                        // update current text color
                        setCursor(0, 0);
                        if (++color_arr_index == 5)
                        {
                                color_arr_index = 0;
                        }

                        // so the user know what is the current text color
                        setTextColor(color_arr[color_arr_index], BLACK);
                        Outstr("-> Current Font Color <-");

                        // update previous button pressed
                        prevbutton = '1';
                        break;
                case '2':
                        // b, c
                        if (prevbutton == '2' && c_message[c_message_index].alpha != 'c' && time_since_last_press < 90000000)
                        {
                                // clear previous character so we can update it to the new one
                                // update the last character in the composing message
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'b')
                                {
                                        Outstr("b");
                                }
                                if (c_message[c_message_index].alpha == 'c')
                                {
                                        Outstr("c");
                                }

                                // update composing message x position for next character
                                c_x += 6;
                        }
                        else
                        {
                                // a
                                c_message[++c_message_index].alpha = 'a';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("a");

                                // update composing message x position for next character
                                c_x += 6;

                                // update previous button pressed
                                prevbutton = '2';
                        }
                        break;
                // for case three to nine, please look at case two. The code is similar
                // so the explanation in case two is sufficient for understanding case three to nine
                case '3':
                        if (prevbutton == '3' && c_message[c_message_index].alpha != 'f' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'e')
                                {
                                        Outstr("e");
                                }
                                if (c_message[c_message_index].alpha == 'f')
                                {
                                        Outstr("f");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 'd';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("d");
                                c_x += 6;
                                prevbutton = '3';
                        }
                        break;
                case '4':
                        if (prevbutton == '4' && c_message[c_message_index].alpha != 'i' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'h')
                                {
                                        Outstr("h");
                                }
                                if (c_message[c_message_index].alpha == 'i')
                                {
                                        Outstr("i");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 'g';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("g");
                                c_x += 6;
                                prevbutton = '4';
                        }
                        break;
                case '5':
                        if (prevbutton == '5' && c_message[c_message_index].alpha != 'l' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'k')
                                {
                                        Outstr("k");
                                }
                                if (c_message[c_message_index].alpha == 'l')
                                {
                                        Outstr("l");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 'j';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("j");
                                c_x += 6;
                                prevbutton = '5';
                        }
                        break;
                case '6':
                        if (prevbutton == '6' && c_message[c_message_index].alpha != 'o' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'n')
                                {
                                        Outstr("n");
                                }
                                if (c_message[c_message_index].alpha == 'o')
                                {
                                        Outstr("o");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 'm';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("m");
                                c_x += 6;
                                prevbutton = '6';
                        }
                        break;
                case '7':
                        if (prevbutton == '7' && c_message[c_message_index].alpha != 's' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'q')
                                {
                                        Outstr("q");
                                }
                                if (c_message[c_message_index].alpha == 'r')
                                {
                                        Outstr("r");
                                }
                                if (c_message[c_message_index].alpha == 's')
                                {
                                        Outstr("s");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 'p';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("p");
                                c_x += 6;
                                prevbutton = '7';
                        }
                        break;
                case '8':
                        if (prevbutton == '8' && c_message[c_message_index].alpha != 'v' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'u')
                                {
                                        Outstr("u");
                                }
                                if (c_message[c_message_index].alpha == 'v')
                                {
                                        Outstr("v");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 't';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("t");
                                c_x += 6;
                                prevbutton = '8';
                        }
                        break;
                case '9':
                        if (prevbutton == '9' && c_message[c_message_index].alpha != 'z' && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if (c_message[c_message_index].alpha == 'x')
                                {
                                        Outstr("x");
                                }
                                if (c_message[c_message_index].alpha == 'y')
                                {
                                        Outstr("y");
                                }
                                if (c_message[c_message_index].alpha == 'z')
                                {
                                        Outstr("z");
                                }
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = 'w';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("w");
                                c_x += 6;
                                prevbutton = '9';
                        }
                        break;
                case '#':
                        // send message
                        SendMessage();

                        // clear composing message on OLED
                        c_clear();

                        // update previous button pressed
                        prevbutton = '#';
                        break;
                case '*': // delete last character
                        if (c_message_index > -1)
                        {
                                c_char_clear();
                                c_x -= 6;
                                setCursor(c_x, c_y);
                                c_message_index--;
                        }
                        prevbutton = '*';
                        break;
                // map other ASCII characters to buttons A, B, C, D in a similar fashion to how buttons 2 - 9 was implemented.
                // using drawChar instead of Outstr() simplified some of the redundant code
                case 'A':
                        if (prevbutton == 'A' && c_message[c_message_index].alpha != 39 && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                drawChar(c_x, c_y, c_message[c_message_index].alpha, color_arr[c_message[c_message_index].color], BLACK, 1);
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = '!';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("!");
                                c_x += 6;
                                prevbutton = 'A';
                        }
                        break;
                case 'B':
                        if (prevbutton == 'B' && c_message[c_message_index].alpha != 47 && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                drawChar(c_x, c_y, c_message[c_message_index].alpha, color_arr[c_message[c_message_index].color], BLACK, 1);
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = '(';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("(");
                                c_x += 6;
                                prevbutton = 'B';
                        }
                        break;
                case 'C':
                        if (prevbutton == 'C' && c_message[c_message_index].alpha != 64 && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                drawChar(c_x, c_y, c_message[c_message_index].alpha, color_arr[c_message[c_message_index].color], BLACK, 1);
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = ':';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr(":");
                                c_x += 6;
                                prevbutton = 'C';
                        }
                        break;
                case 'D':
                        if (prevbutton == 'D' && c_message[c_message_index].alpha != 96 && time_since_last_press < 90000000)
                        {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                drawChar(c_x, c_y, c_message[c_message_index].alpha, color_arr[c_message[c_message_index].color], BLACK, 1);
                                c_x += 6;
                        }
                        else
                        {
                                c_message[++c_message_index].alpha = '[';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("[");
                                c_x += 6;
                                prevbutton = 'D';
                        }
                        break;
                }

                // stop registering buttons for while to prevent one ring-tone from registering as two or more button presses.
                UtilsDelay(999999 * 4);
        }
}

int color;

void UARTMessageInHandler()
{
        // clear UART interrupt
        UARTIntClear(UARTA1_BASE, UART_INT_RX);

        while (UARTCharsAvail(UARTA1_BASE))
        {
                char c = UARTCharGet(UARTA1_BASE);

                // received the color of the Letter
                if (c >= '0' && c <= '4')
                {
                        if (c == '0')
                        {
                                color = WHITE;
                        }
                        if (c == '1')
                        {
                                color = RED;
                        }
                        if (c == '2')
                        {
                                color = GREEN;
                        }
                        if (c == '3')
                        {
                                color = BLUE;
                        }
                        if (c == '4')
                        {
                                color = YELLOW;
                        }
                }
                else if (c == '5')
                {
                        // clear received message spot on OLED for new receiving message
                        r_clear();
                }
                else
                {
                        // else, received the Letters itself.
                        drawChar(r_x, r_y, c, color, BLACK, 1);
                        r_x += 6;
                }
        }
}

int main()
{
        // initialize Board configurations
        BoardInit();

        // muxing UART, SPI lines, and GPIO.
        PinMuxConfig();

        // configure UART
        UARTConfigSetExpClk(UARTA1_BASE, 80000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
        UARTConfigSetExpClk(UARTA0_BASE, 80000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
        UARTEnable(UARTA1_BASE);
        UARTEnable(UARTA0_BASE);
        UARTDMADisable(UARTA1_BASE, (UART_DMA_RX | UART_DMA_TX));
        UARTFIFODisable(UARTA1_BASE);

        // initialize and clear the terminal
        InitTerm();

        ClearTerm();

        // enable the SPI module clock
        MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);

        // reset SPI
        MAP_SPIReset(GSPI_BASE);

        // reset the peripheral
        MAP_PRCMPeripheralReset(PRCM_GSPI);

        Message("\t****************************************************\n\r");
        Message("\tLab4\n\r");
        Message("\t****************************************************\n\r");
        Message("\n\n\n\r");

        // configure spi
        MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                               SPI_IF_BIT_RATE, SPI_MODE_MASTER, SPI_SUB_MODE_0,
                               (SPI_SW_CTRL_CS |
                                SPI_4PIN_MODE |
                                SPI_TURBO_OFF |
                                SPI_CS_ACTIVEHIGH |
                                SPI_WL_8));

        // enable SPI
        MAP_SPIEnable(GSPI_BASE);

        // start OLED
        Adafruit_Init();

        fillScreen(BLACK);

        setCursor(0, 0);
        Outstr("-> Current Font Color <-");

        setTextColor(CYAN, BLACK);
        setCursor(0, 80);
        Outstr("A: ! '' # $ % & '");
        setCursor(0, 90);
        Outstr("B: ( ) * + , - . /");
        setCursor(0, 100);
        Outstr("C : : ; < = > ? @");
        setCursor(0, 110);
        Outstr("D : [ \\ ] ^  `");

        setTextColor(WHITE, BLACK);

        // read ADC timer interrupt
        Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
        Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, readADC);
        // 5000 for 16khz
        MAP_TimerLoadSet(TIMERA0_BASE, TIMER_A, 5000);
        MAP_TimerEnable(TIMERA0_BASE, TIMER_A);

        // track time since last button pressed
        Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC_UP, TIMER_A, 0);
        TimerEnable(TIMERA1_BASE, TIMER_A);
        TimerValueSet(TIMERA1_BASE, TIMER_A, 0);
        timer = TimerValueGet(TIMERA1_BASE, TIMER_A);

        // Link UART interrupt to UARTMessageInHandler
        UARTIntEnable(UARTA1_BASE, UART_INT_RX);
        UARTIntRegister(UARTA1_BASE, UARTMessageInHandler);

        while (1)
        {
                // once 410 samples are collected, check if a button was actually pressed using goertzel algorithm
                if (samples_index == 410)
                {
                        // disable sample interrupts from disrupting the calculation
                        MAP_TimerDisable(TIMERA0_BASE, TIMER_A);

                        // reset sample array for collecting another 410 samples
                        samples_index = 0;

                        // perform calculations with goertzel algorithm
                        for (i = 0; i < 8; i++)
                        {
                                power_all[i] = goertzel(samples, coeff[i], 410);
                        }

                        // see post_test() for explanation
                        post_test();

                        // re-enable sample interrupts for collecting another 410 samples
                        MAP_TimerEnable(TIMERA0_BASE, TIMER_A);
                }
        }
}
