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

unsigned long times[35];

unsigned long times_index = 0;

static void FallingEdgeHandler(void)
{ // SW2 handler
        if (times_index == 0)
        {
                HWREG(0xE000E018) = 1;
                times[times_index] = SysTickValueGet();
                times_index++;
        }
        else if (times_index == 1)
        {
                times[times_index] = SysTickValueGet();

                if (times[times_index - 1] - times[times_index] > 1060000 && times[times_index - 1] - times[times_index] < 1070000)
                {
                        times_index++;
                }
                else
                {
                        times_index = 0;
                        UtilsDelay(999999);
                }
        }
        else if (times_index == 34)
        {
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
typedef struct Letter
{
        unsigned int x;
        unsigned int y;
        char alpha;
        int color;
} Letter;

int c_x = 0;
int c_y = 10;

int r_x = 0;
int r_y = 64;

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
    drawChar(c_message[c_message_index].x, c_message[c_message_index].y, c_message[c_message_index].alpha, BLACK, BLACK, 1);
}

int color_arr[5] = {WHITE, RED, GREEN, BLUE, YELLOW};
int color_arr_index = 0;

void SendMessage()
{
    if(c_message_index != -1)
    {
        int i;

        //beginning of new message
        while(UARTBusy(UARTA1_BASE));
        UtilsDelay(999999*3);
        UARTCharPut(UARTA1_BASE,'5');

        for(i = 0; i <= c_message_index; i++)
        {
            // wait for UART to be available
            while(UARTBusy(UARTA1_BASE));
            UtilsDelay(999999*3);

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

            UARTCharPut(UARTA1_BASE, c_message[i].alpha);
        }

        c_message_index = -1;
    }
}

int color;

void UARTMessageInHandler()
{
    while(UARTCharsAvail(UARTA1_BASE))
    {
        char c = UARTCharGet(UARTA1_BASE);

        if(c >= '0' && c <= '4')
        {
            if(c == '0')
            {
                color = WHITE;
            }
            if(c == '1')
            {
                color = RED;
            }
            if(c == '2')
            {
                color = GREEN;
            }
            if(c == '3')
            {
                color = BLUE;
            }
            if(c == '4')
            {
                color = YELLOW;
            }
        }
        else if(c == '5')
        {
            r_clear();
        }
        else
        {
            drawChar(r_x, r_y, c, color, BLACK, 1);
            r_x += 6;
        }
    }

    //clear UART interrupt
    UARTIntClear(UARTA1_BASE,UART_INT_RX);
}

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

        Adafruit_Init();

        fillScreen(BLACK);

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

        UARTIntEnable(UARTA1_BASE, UART_INT_RX) ;
        UARTIntRegister(UARTA1_BASE, UARTMessageInHandler);

        InitTerm();

        ClearTerm();

        Message("\t\t****************************************************\n\r");
        Message("\t\t\tTwo Board Communication\n\r");
        Message("\t\t ****************************************************\n\r");
        Message("\n\n\n\r");

        setCursor(0, 0);
        Outstr("-> Current Font Color <-");

        // configure timer_a in timera0
        Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC_UP, TIMER_A, 0);
        TimerEnable(TIMERA0_BASE, TIMER_A);
        TimerValueSet(TIMERA0_BASE, TIMER_A, 0);
        long int timer = TimerValueGet(TIMERA0_BASE, TIMER_A);

        while (1)
        {
                if (times_index == 34)
                {
                        long int binary = 0;

                        int binary_index = 31;

                        int i = 1;

                        for (; i < 33; i++)
                        {
                                if ((times[i] - times[i + 1]) > 170000)
                                {
                                        binary |= 1 << binary_index;
                                }
                                binary_index--;
                        }

                        int time_since_last_press = TimerValueGet(TIMERA0_BASE, TIMER_A) - timer;
                        TimerValueSet(TIMERA0_BASE, TIMER_A, 0);
                        timer = TimerValueGet(TIMERA0_BASE, TIMER_A);

                        switch (binary)
                        {
                        case zero:
                                Message("Zero\n\r");
                                c_message[++c_message_index].alpha = ' ';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].y = c_y;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr(" ");
                                c_x += 6;
                                prevbutton = zero;
                                break;
                        case one:
                                Message("One\n\r");
                                setCursor(0, 0);
                                if (++color_arr_index == 5)
                                {
                                        color_arr_index = 0;
                                }

                                setTextColor(color_arr[color_arr_index], BLACK);
                                Outstr("-> Current Font Color <-");
                                prevbutton = one;
                                break;
                        case two:
                                Message("Two\n\r");
                                if(prevbutton == two && c_message[c_message_index].alpha != 'c' && time_since_last_press < 90000000)
                                {
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
                                    c_x += 6;
                                }
                                else
                                {
                                    c_message[++c_message_index].alpha = 'a';
                                    c_message[c_message_index].x = c_x;
                                    c_message[c_message_index].y = c_y;
                                    c_message[c_message_index].color = color_arr_index;
                                    setCursor(c_x, c_y);
                                    Outstr("a");
                                    c_x += 6;
                                    prevbutton = two;
                                }
                                break;
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
                                c_message[c_message_index].y = c_y;
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
                                c_message[c_message_index].y = c_y;
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
                                c_message[c_message_index].y = c_y;
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
                                c_message[c_message_index].y = c_y;
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
                                c_message[c_message_index].y = c_y;
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
                                c_message[c_message_index].y = c_y;
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
                                c_message[c_message_index].y = c_y;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr("w");
                                c_x += 6;
                                prevbutton = nine;
                            }
                            break;
                        case mute: // send message
                                Message("Mute\n\r");
                                SendMessage();
                                c_clear();
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
                        }
                        times_index = 0;
                }
        }
}
