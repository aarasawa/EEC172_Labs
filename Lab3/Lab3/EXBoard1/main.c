// Names:
// Alexander Arasawa
// Jiawei Zheng

// Standard includes
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Driverlib includes
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

// Common interface includes
#include "uart_if.h"
#include "pin_mux_config.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

// Remote definitions
#define zero 685179135
#define one 685211775
#define two 685195455
#define three 685228095
#define four 685183215
#define five 685215855
#define six 685199535
#define seven 685232175
#define eight 685181175
#define nine 685213815
#define mute 685201575
#define volumedown 685185255
#define scrollup 685230135
#define scrolldown 685197495

// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define GREEN 0x07E0
#define CYAN 0x07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

#if defined(ccs)
extern void (*const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

typedef struct PinSetting
{
        unsigned long port;
        unsigned int pin;
} PinSetting;

//Remote IN Signal
static PinSetting switch2 = {.port = GPIOA1_BASE, .pin = 0x1};

static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
        //
        // Set vector table base
        //
#if defined(ccs)
        MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
        MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
        //
        // Enable Processor
        //
        MAP_IntMasterEnable();
        MAP_IntEnable(FAULT_SYSTICK);

        PRCMCC3200MCUInit();
}

//as explained in lab report, our times array for keeping track of the times in which a falling edge occurs
unsigned long times[35];

//for iterating through times array
unsigned long times_index = 0;

//falling edge handler for signal in from IR remote
static void FallingEdgeHandler(void)
{
        //start of new signal
        if (times_index == 0)
        {
                //reset timer
                HWREG(0xE000E018) = 1;
                //add time value to time arrays
                times[times_index] = SysTickValueGet();
                times_index++;
        }
        else if (times_index == 1)
        {
                times[times_index] = SysTickValueGet();

                //check if start signal occurred
                if (times[times_index - 1] - times[times_index] > 1060000 && times[times_index - 1] - times[times_index] < 1070000)
                {
                        //if start signal did indeed occurred, we keep appending times to the time array
                        times_index++;
                }
                else
                {
                        //else start signal did not occurred, so we reset time array to try again
                        times_index = 0;
                        //fix buggy offsets of wrong times in the time array by waiting a while before we read in falling edges again.
                        //This is used in other parts of code for the same reason.
                        UtilsDelay(999999);
                }
        }
        else if (times_index == 34)
        {
                //time array is filled, so we should stop
                times[times_index] = SysTickValueGet();
        }
        else if (times_index > 1 && times_index < 34)
        {
                times[times_index] = SysTickValueGet();
                times_index++;
        }

        unsigned long ulStatus;
        ulStatus = MAP_GPIOIntStatus(switch2.port, true);
        MAP_GPIOIntClear(switch2.port, ulStatus); // clear interrupts on GPIOA2
}

//our letter struct. Makes it easier to store and send
//strings that contain different colored characters
typedef struct Letter
{
        unsigned int x;
        char alpha;
        int color;
} Letter;

//global x and y position on OLED for composing message
int c_x = 12;
int c_y = 120;

//global x position for aid with appending received messages to the message log
//6 becacuse received messages should be on the left side of the screen
int glob_x_pos = 6;

//for iterating through messages array
int glob_msg_index = 0;

//for storing the current composing message
//user adds to composing messsage by pressing buttons
Letter c_message[32];
int c_message_index = -1; // initially, invalid

//our msg struct for each message in the message log
typedef struct msg
{
    Letter let[32];
    int length;
} msg;

//message array or message log
msg messages[64];
int messages_index = -1; // initially, invalid

// clear composing message
void c_clear()
{
        drawFastHLine(12, 120, 128, BLACK);
        drawFastHLine(12, 121, 128, BLACK);
        drawFastHLine(12, 122, 128, BLACK);
        drawFastHLine(12, 123, 128, BLACK);
        drawFastHLine(12, 124, 128, BLACK);
        drawFastHLine(12, 125, 128, BLACK);
        drawFastHLine(12, 126, 128, BLACK);
        drawFastHLine(12, 127, 128, BLACK);
        c_message_index = -1;
        c_x = 12;
        c_y = 120;
}

// clear last composing message character
void c_char_clear()
{
    drawChar(c_message[c_message_index].x, 120, c_message[c_message_index].alpha, BLACK, BLACK, 1);
}

//color array to cycle through so characters can be different colors, as requested by specifications
int color_arr[5] = {WHITE, RED, GREEN, BLUE, YELLOW};
int color_arr_index = 0;

void SendMessage()
{
    if(c_message_index != -1)
    {
        int i;

        //start of message, for other board to know there is a incoming message
        while(UARTBusy(UARTA1_BASE));
        UtilsDelay(999999*3);
        UARTCharPut(UARTA1_BASE,'5');

        for(i = 0; i <= c_message_index; i++)
        {
            // wait for UART to be available
            while(UARTBusy(UARTA1_BASE));
            UtilsDelay(999999*3);

            //send color of Letter first
            if(c_message[i].color == 0)
            {
                UARTCharPut(UARTA1_BASE, '0');
            }
            if(c_message[i].color == 1)
            {
                UARTCharPut(UARTA1_BASE, '1');
            }
            if(c_message[i].color == 2)
            {
                UARTCharPut(UARTA1_BASE, '2');
            }
            if(c_message[i].color == 3)
            {
                UARTCharPut(UARTA1_BASE, '3');
            }
            if(c_message[i].color == 4)
            {
                UARTCharPut(UARTA1_BASE, '4');
            }

            while(UARTBusy(UARTA1_BASE));
            UtilsDelay(999999*3);

            //send the Letter afterwards
            UARTCharPut(UARTA1_BASE, c_message[i].alpha);
        }

        //end of message, for other board to know that the message received has no more characters
        while(UARTBusy(UARTA1_BASE));
        UtilsDelay(999999*3);
        UARTCharPut(UARTA1_BASE,'5');
    }
}

//clear all the messages currently displayed on the OLED
clearOLED()
{
    int i;

    for(i = 0; i <= 115; i++)
    {
        drawFastHLine(0, i, 128, BLACK);
    }
}

int scroll = 0; //the user manipulates this variable to scroll up and down the message log

//draw the new current working set of messages to the OLED
updateOLED()
{
    if(messages_index != -1)
    {
        int y_pos = 0;

        int i = scroll;

        for(; i <= messages_index; i++)
        {
            int j;

            for(j = 0; j < messages[i].length; j++)
            {
                drawChar(messages[i].let[j].x, y_pos, messages[i].let[j].alpha, color_arr[messages[i].let[j].color], BLACK, 1);
            }

            y_pos += 10;

            if(y_pos > 100) break;
        }
    }
}

//for knowing when a message is completely send through so we update OLED or not.
//For example, the OLED should automatically scroll down through the message log
//when the new received message does not fit onto the screen
int start_end_signal = 0;

void UARTMessageInHandler()
{
    while(UARTCharsAvail(UARTA1_BASE))
    {
        char c = UARTCharGet(UARTA1_BASE);

        //received the color of the Letter
        if(c >= '0' && c <= '4')
        {
            if(c == '0')
            {
                messages[messages_index].let[glob_msg_index].color = 0;
            }
            if(c == '1')
            {
                messages[messages_index].let[glob_msg_index].color = 1;
            }
            if(c == '2')
            {
                messages[messages_index].let[glob_msg_index].color = 2;
            }
            if(c == '3')
            {
                messages[messages_index].let[glob_msg_index].color = 3;
            }
            if(c == '4')
            {
                messages[messages_index].let[glob_msg_index].color = 4;
            }
        }
        else if(c == '5')
        {
            //prepare a new message entry in the message log
            start_end_signal++;
            messages_index++;
            glob_msg_index = 0;
            glob_x_pos = 6;
            messages[messages_index].length = 0;

            if(start_end_signal == 2)
            {
                if(messages_index - scroll > 10)
                {
                    //the OLED should automatically scroll down through the message log
                    //when the new received message does not fit onto the screen
                    clearOLED();
                    scroll++;
                }

                updateOLED();

                start_end_signal = 0;
            }
        }
        else
        {
            //else, received the Letters itself.
            //add received message to message log
            messages[messages_index].let[glob_msg_index].alpha = c;
            messages[messages_index].let[glob_msg_index].x = glob_x_pos;
            messages[messages_index].length = messages[messages_index].length + 1;
            glob_msg_index++;
            glob_x_pos += 6;
        }
    }

    //clear UART interrupt
    UARTIntClear(UARTA1_BASE,UART_INT_RX);
}

//global variable for storing previous characters
//initially, not set to any of the buttons that map to characters
int prevbutton = mute;

void main()
{
        //
        // Initialize Board configurations
        //
        BoardInit();

        //
        // Muxing UART and SPI lines.
        //
        PinMuxConfig();

        // configure UART
        UARTConfigSetExpClk(UARTA1_BASE, 80000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
        UART_CONFIG_PAR_NONE));
        UARTEnable(UARTA1_BASE);
        UARTDMADisable(UARTA1_BASE, (UART_DMA_RX | UART_DMA_TX));
        UARTFIFODisable(UARTA1_BASE);

        UARTIntEnable( UARTA1_BASE, UART_INT_RX) ;
        UARTIntRegister(UARTA1_BASE, UARTMessageInHandler);

        //
        // Enable the SPI module clock
        //
        MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);

        //
        // Reset the peripheral
        //
        MAP_PRCMPeripheralReset(PRCM_GSPI);

        //start OLED
        Adafruit_Init();

        fillScreen(BLACK);

        //initially systick
        SysTickPeriodSet(16777216);
        SysTickIntUnregister();
        SysTickEnable();

        //
        // Register the interrupt handlers
        //
        MAP_GPIOIntRegister(switch2.port, FallingEdgeHandler);

        //
        // Configure rising edge interrupts on SW2 and SW3
        //
        MAP_GPIOIntTypeSet(switch2.port, switch2.pin, GPIO_FALLING_EDGE); // SW2

        unsigned long ulStatus;

        ulStatus = MAP_GPIOIntStatus(switch2.port, false);
        MAP_GPIOIntClear(switch2.port, ulStatus); // clear interrupts on GPIOA2

        // Enable SW2 and SW3 interrupts
        MAP_GPIOIntEnable(switch2.port, switch2.pin);

        //link UART interrupt to our own UART handler and enable UART interrupt
        UARTIntEnable(UARTA1_BASE, UART_INT_RX) ;
        UARTIntRegister(UARTA1_BASE, UARTMessageInHandler);

        InitTerm();

        ClearTerm();

        Message("\t\t****************************************************\n\r");
        Message("\t\t\tTwo Board Communication\n\r");
        Message("\t\t ****************************************************\n\r");
        Message("\n\n\n\r");

        //the composing message starts on the bottom of the screen, like other message API
        //this cursor is for display what the current text color is
        setCursor(0, 120);
        Outstr("->");

        // configure timer_a in timera0
        Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC_UP, TIMER_A, 0);
        TimerEnable(TIMERA0_BASE, TIMER_A);
        TimerValueSet(TIMERA0_BASE, TIMER_A, 0);

        //global variable for aid with time interval since last button pressed
        long int timer = TimerValueGet(TIMERA0_BASE, TIMER_A);

        while (1)
        {
                if (times_index == 34)
                {
                        long int binary = 0;

                        int binary_index = 31;

                        int i;

                        //iterate through time array to calculate the decimal notation of the binary number in the data field of the waveform received
                        for (i = 1; i < 33; i++)
                        {
                                if ((times[i] - times[i + 1]) > 170000)
                                {
                                        binary |= 1 << binary_index;
                                }
                                binary_index--;
                        }

                        //get time interval since last button pressed and restart Timer_A
                        int time_since_last_press = TimerValueGet(TIMERA0_BASE, TIMER_A) - timer;
                        TimerValueSet(TIMERA0_BASE, TIMER_A, 0);
                        timer = TimerValueGet(TIMERA0_BASE, TIMER_A);

                        //for aid with display send message on the right side of OLED
                        int x_pos = 128 - (c_message_index + 1) * 6;

                        switch (binary)
                        {
                        case zero:
                                //space
                                //add Letter ' ' to composing message
                                Message("Zero\n\r");
                                c_message[++c_message_index].alpha = ' ';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr(" ");

                                //update composing message x position for next character
                                c_x += 6;

                                //update previous button pressed
                                prevbutton = zero;
                                break;
                        case one:
                                //update current text color and change the cursor -> color so the user
                                //know what is the current text color
                                Message("One\n\r");
                                setCursor(0, 120);
                                if (++color_arr_index == 5)
                                {
                                        color_arr_index = 0;
                                }

                                setTextColor(color_arr[color_arr_index], BLACK);
                                Outstr("->");

                                //update previous button pressed
                                prevbutton = one;
                                break;
                        case two:
                                //b, c
                                Message("Two\n\r");//debugging purposes
                                if(prevbutton == two && c_message[c_message_index].alpha != 'c' && time_since_last_press < 90000000)
                                {
                                    //clear previous character so we can update it to the new one
                                    //update the last character in the composing message
                                    c_char_clear();
                                    c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                    c_x = c_message[c_message_index].x;
                                    setCursor(c_x, c_y);
                                    if(c_message[c_message_index].alpha == 'b')
                                    {
                                        Outstr("b");
                                    }
                                    if(c_message[c_message_index].alpha == 'c')
                                    {
                                        Outstr("c");
                                    }
                                    //update composing message x position for next character
                                    c_x += 6;
                                }
                                else
                                {
                                    //a
                                    //add Letter a to composing message
                                    c_message[++c_message_index].alpha = 'a';
                                    c_message[c_message_index].x = c_x;
                                    c_message[c_message_index].color = color_arr_index;
                                    setCursor(c_x, c_y);
                                    Outstr("a");

                                    //update composing message x position for next character
                                    c_x += 6;

                                    //update previous button pressed
                                    prevbutton = two;
                                }
                                break;
                        //------------------------IMPORTANT----------------------------
                        //for case three to nine, please look at case two. The code is similar
                        //so the explanation in case two is sufficient for understanding case three to nine
                        case three:
                            Message("Three\n\r");
                            if(prevbutton == three && c_message[c_message_index].alpha != 'f' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'e')
                                {
                                    Outstr("e");
                                }
                                if(c_message[c_message_index].alpha == 'f')
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
                                prevbutton = three;
                            }
                            break;
                        case four:
                            Message("Four\n\r");
                            if(prevbutton == four && c_message[c_message_index].alpha != 'i' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'h')
                                {
                                    Outstr("h");
                                }
                                if(c_message[c_message_index].alpha == 'i')
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
                                prevbutton = four;
                            }
                            break;
                        case five:
                            Message("Five\n\r");
                            if(prevbutton == five && c_message[c_message_index].alpha != 'l' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'k')
                                {
                                    Outstr("k");
                                }
                                if(c_message[c_message_index].alpha == 'l')
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
                                prevbutton = five;
                            }
                            break;
                        case six:
                            Message("Six\n\r");
                            if(prevbutton == six && c_message[c_message_index].alpha != 'o' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'n')
                                {
                                    Outstr("n");
                                }
                                if(c_message[c_message_index].alpha == 'o')
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
                                prevbutton = six;
                            }
                            break;
                        case seven:
                            Message("Seven\n\r");
                            if(prevbutton == seven && c_message[c_message_index].alpha != 's' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'q')
                                {
                                    Outstr("q");
                                }
                                if(c_message[c_message_index].alpha == 'r')
                                {
                                    Outstr("r");
                                }
                                if(c_message[c_message_index].alpha == 's')
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
                                prevbutton = seven;
                            }
                            break;
                        case eight:
                            Message("Eight\n\r");
                            if(prevbutton == eight && c_message[c_message_index].alpha != 'v' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'u')
                                {
                                    Outstr("u");
                                }
                                if(c_message[c_message_index].alpha == 'v')
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
                                prevbutton = eight;
                            }
                            break;
                        case nine:
                            Message("Nine\n\r");
                            if(prevbutton == nine && c_message[c_message_index].alpha != 'z' && time_since_last_press < 90000000)
                            {
                                c_char_clear();
                                c_message[c_message_index].alpha = ++c_message[c_message_index].alpha;
                                c_x = c_message[c_message_index].x;
                                setCursor(c_x, c_y);
                                if(c_message[c_message_index].alpha == 'x')
                                {
                                    Outstr("x");
                                }
                                if(c_message[c_message_index].alpha == 'y')
                                {
                                    Outstr("y");
                                }
                                if(c_message[c_message_index].alpha == 'z')
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
                                prevbutton = nine;
                            }
                            break;
                        case mute:
                                Message("Mute\n\r");

                                //add composed message to message log
                                messages_index++;

                                for(i = 0; i <= c_message_index; i++)
                                {
                                    messages[messages_index].let[i].alpha = c_message[i].alpha;
                                    messages[messages_index].let[i].color = c_message[i].color;
                                    messages[messages_index].let[i].x = x_pos;
                                    x_pos += 6;
                                }

                                messages[messages_index].length = c_message_index + 1;

                                //send composed message over UART1
                                SendMessage();
                                //clear current working set of messages on OLED
                                c_clear();
                                //reset composing message array
                                c_message_index = -1;

                                if(messages_index - scroll > 10)
                                {
                                    //the OLED should automatically scroll down through the message log
                                    //when the new send message does not fit onto the screen
                                    clearOLED();
                                    scroll++;
                                }

                                updateOLED();

                                //update previous button pressed
                                prevbutton = mute;
                                break;
                        case volumedown: // delete last character
                                Message("Volumedown\n\r");
                                c_char_clear();
                                c_x -= 6;
                                setCursor(c_x, c_y);
                                c_message_index--;
                                prevbutton = volumedown;
                                break;
                        case scrollup:
                            scroll--;
                            clearOLED();
                            updateOLED();
                            break;
                        case scrolldown:
                            scroll++;
                            clearOLED();
                            updateOLED();
                            break;
                        }
                        times_index = 0;
                }
        }
}
