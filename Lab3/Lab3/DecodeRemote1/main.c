
//*****************************************************************************
//
// Application Name     - int_sw
// Application Overview - The objective of this application is to demonstrate
//                          GPIO interrupts using SW2 and SW3.
//                          NOTE: the switches are not debounced!
//
//*****************************************************************************

//****************************************************************************
//
//! \addtogroup int_sw
//! @{
//
//****************************************************************************

// Standard includes
#include <stdio.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "hw_apps_rcm.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "gpio.h"
#include "utils.h"
#include "systick.h"
#include "hw_timer.h"
#include "string.h"

// Common interface includes
#include "uart_if.h"

#include "pin_mux_config.h"

//definitions

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

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);

//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

// an example of how you can use structs to organize your pin settings for easier maintenance
typedef struct PinSetting {
    unsigned long port;
    unsigned int pin;
} PinSetting;

static PinSetting switch2 = { .port = GPIOA1_BASE, .pin = 0x1};

//*****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//*****************************************************************************
static void BoardInit(void);

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************
unsigned long times[35];

unsigned long times_index = 0;

static void FallingEdgeHandler(void) {  // SW2 handler
    if(times_index == 0)
    {
        HWREG(0xE000E018) = 1;
        times[times_index] = SysTickValueGet();
        times_index++;
    }
    else if(times_index == 1)
    {
        times[times_index] = SysTickValueGet();

        if(times[times_index-1] - times[times_index] > 1070000 && times[times_index-1] - times[times_index] < 1080000)
        {
            times_index++;
        }
        else
        {
            times_index = 0;
            UtilsDelay(999999);
        }
    }
    else if(times_index == 34)
    {
        times[times_index] = SysTickValueGet();
    }
    else if(times_index > 1 && times_index < 34)
    {
        times[times_index] = SysTickValueGet();
        times_index++;
    }

    unsigned long ulStatus;
    ulStatus = MAP_GPIOIntStatus (switch2.port, true);
    MAP_GPIOIntClear(switch2.port, ulStatus);       // clear interrupts on GPIOA2
}
//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void) {
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);

    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}
//****************************************************************************
//
//! Main function
//!
//! \param none
//!
//!
//! \return None.
//
//****************************************************************************
int main() {
    unsigned long ulStatus;

    BoardInit();

    PinMuxConfig();

    SysTickPeriodSet(16777216);
    SysTickIntUnregister();
    SysTickEnable();

    InitTerm();

    ClearTerm();

    //
    // Register the interrupt handlers
    //
    MAP_GPIOIntRegister(switch2.port, FallingEdgeHandler);

    //
    // Configure rising edge interrupts on SW2 and SW3
    //
    MAP_GPIOIntTypeSet(switch2.port, switch2.pin, GPIO_FALLING_EDGE);   // SW2

    ulStatus = MAP_GPIOIntStatus (switch2.port, false);
    MAP_GPIOIntClear(switch2.port, ulStatus);           // clear interrupts on GPIOA2

    // Enable SW2 and SW3 interrupts
    MAP_GPIOIntEnable(switch2.port, switch2.pin);

    Message("\t\t****************************************************\n\r");
    Message("\t\t\tIR\n\r");
    Message("\t\t ****************************************************\n\r");
    Message("\n\n\n\r");

    while (1) {
        if(times_index == 34)
        {
            long int binary = 0;

            int binary_index = 31;

            int i = 1;

            for(;i<33;i++)
            {
                if((times[i] - times[i+1]) > 170000)
                {
                    binary |= 1 << binary_index;
                }
                binary_index--;
            }

            switch(binary)
            {
            case zero:
                Message("Zero\n\r");
                break;
            case one:
                Message("One\n\r");
                break;
            case two:
                Message("Two\n\r");
                break;
            case three:
                Message("Three\n\r");
                break;
            case four:
                Message("Four\n\r");
                break;
            case five:
                Message("Five\n\r");
                break;
            case six:
                Message("Six\n\r");
                break;
            case seven:
                Message("Seven\n\r");
                break;
            case eight:
                Message("Eight\n\r");
                break;
            case nine:
                Message("Nine\n\r");
                break;
            case mute:
                Message("Mute\n\r");
                break;
            case volumedown:
                Message("Volumedown\n\r");
                break;
            }
            times_index = 0;
        }
    }
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
