// Names:
// Alexander Arasawa
// Jiawei Zheng

// Standard includes
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Simplelink includes
#include "simplelink.h"

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
#include "pinmux.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"
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

#define MAX_URI_SIZE 128
#define URI_SIZE MAX_URI_SIZE + 1

#define APPLICATION_NAME "SSL"
#define APPLICATION_VERSION "1.1.1.EEC.Spring2018"
#define SERVER_NAME "a2sfw25r1ow8ml-ats.iot.us-east-1.amazonaws.com"
#define GOOGLE_DST_PORT 8443

#define SL_SSL_CA_CERT "/cert/rootca.der" // starfield class2 rootca (from firefox) // <-- this one works
#define SL_SSL_PRIVATE "/cert/private.der"
#define SL_SSL_CLIENT "/cert/client.der"

// NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE 21   /* Current Date */
#define MONTH 5   /* Month 1-12 */
#define YEAR 2022 /* Current year */
#define HOUR 0    /* Time - hours */
#define MINUTE 0  /* Time - minutes */
#define SECOND 0  /* Time - seconds */

#define POSTHEADER "POST /things/CC3200_Thing/shadow HTTP/1.1\n\r"
#define GETHEADER "GET /things/CC3200_Thing/shadow HTTP/1.1\n\r"
#define HOSTHEADER "Host: a2sfw25r1ow8ml-ats.iot.us-east-1.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

// For help with post json to device shadow
#define DATA1 "{\"state\": {\r\n\"desired\" : {\r\n\"default\": \"\",\r\n\"email\": \""
#define DATA2 "\"\r\n}}}\r\n\r\n"

// Application specific status/error codes
typedef enum
{
        // Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
        LAN_CONNECTION_FAILED = -0x7D0,
        INTERNET_CONNECTION_FAILED = LAN_CONNECTION_FAILED - 1,
        DEVICE_NOT_IN_STATION_MODE = INTERNET_CONNECTION_FAILED - 1,

        STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;

typedef struct
{
        /* time */
        unsigned long tm_sec;
        unsigned long tm_min;
        unsigned long tm_hour;
        /* date */
        unsigned long tm_day;
        unsigned long tm_mon;
        unsigned long tm_year;
        unsigned long tm_week_day; // not required
        unsigned long tm_year_day; // not required
        unsigned long reserved[3];
} SlDateTime;

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long g_ulStatus = 0;              // SimpleLink Status
unsigned long g_ulPingPacketsRecv = 0;              // Number of Ping Packets received
unsigned long g_ulGatewayIP = 0;                    // Network Gateway IP address
unsigned char g_ucConnectionSSID[SSID_LEN_MAX + 1]; // Connection SSID
unsigned char g_ucConnectionBSSID[BSSID_LEN_MAX];   // Connection BSSID
signed char *g_Host = SERVER_NAME;
SlDateTime g_time;
#if defined(ccs) || defined(gcc)
extern void (*const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End: df
//*****************************************************************************

//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
static long WlanConnect();
static int set_time();
static void BoardInit(void);
static long InitializeAppVariables();
static int tls_connect();
static int connectToAccessPoint();
static int http_post(int);
static int http_get(int);

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************

//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
        if (!pWlanEvent)
        {
                return;
        }

        switch (pWlanEvent->Event)
        {
        case SL_WLAN_CONNECT_EVENT:
        {
                SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

                //
                // Information about the connected AP (like name, MAC etc) will be
                // available in 'slWlanConnectAsyncResponse_t'.
                // Applications can use it if required
                //
                //  slWlanConnectAsyncResponse_t *pEventData = NULL;
                // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
                //

                // Copy new connection SSID and BSSID to global parameters
                memcpy(g_ucConnectionSSID, pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_name,
                       pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
                memcpy(g_ucConnectionBSSID,
                       pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                       SL_BSSID_LENGTH);

                UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s , "
                           "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                           g_ucConnectionSSID, g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
                slWlanConnectAsyncResponse_t *pEventData = NULL;

                CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
                CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

                pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

                // If the user has initiated 'Disconnect' request,
                //'reason_code' is SL_USER_INITIATED_DISCONNECTION
                if (SL_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
                {
                        UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
                                   "BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
                                   g_ucConnectionSSID, g_ucConnectionBSSID[0],
                                   g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                                   g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                                   g_ucConnectionBSSID[5]);
                }
                else
                {
                        UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s, "
                                   "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                                   g_ucConnectionSSID, g_ucConnectionBSSID[0],
                                   g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                                   g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                                   g_ucConnectionBSSID[5]);
                }
                memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
                memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
        }
        break;

        default:
        {
                UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                           pWlanEvent->Event);
        }
        break;
        }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
        if (!pNetAppEvent)
        {
                return;
        }

        switch (pNetAppEvent->Event)
        {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        {
                SlIpV4AcquiredAsync_t *pEventData = NULL;

                SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

                // Ip Acquired Event Data
                pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

                // Gateway IP address
                g_ulGatewayIP = pEventData->gateway;

                UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
                           "Gateway=%d.%d.%d.%d\n\r",
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 3),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 2),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 1),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 0),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 3),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 2),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 1),
                           SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 0));
        }
        break;

        default:
        {
                UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                           pNetAppEvent->Event);
        }
        break;
        }
}

//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent, SlHttpServerResponse_t *pHttpResponse)
{
        // Unused in this application
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
        if (!pDevEvent)
        {
                return;
        }

        //
        // Most of the general errors are not FATAL are are to be handled
        // appropriately by the application
        //
        UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
                   pDevEvent->EventData.deviceEvent.status,
                   pDevEvent->EventData.deviceEvent.sender);
}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
        if (!pSock)
        {
                return;
        }

        switch (pSock->Event)
        {
        case SL_SOCKET_TX_FAILED_EVENT:
                switch (pSock->socketAsyncEvent.SockTxFailData.status)
                {
                case SL_ECLOSE:
                        UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
                                   "failed to transmit all queued packets\n\n",
                                   pSock->socketAsyncEvent.SockTxFailData.sd);
                        break;
                default:
                        UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                                   "(%d) \n\n",
                                   pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
                        break;
                }
                break;

        default:
                UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n", pSock->Event);
                break;
        }
}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End breadcrumb: s18_df
//*****************************************************************************

//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    0 on success else error code
//!
//! \return None
//!
//*****************************************************************************
static long InitializeAppVariables()
{
        g_ulStatus = 0;
        g_ulGatewayIP = 0;
        g_Host = SERVER_NAME;
        memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
        memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
        return SUCCESS;
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
        SlVersionFull ver = {0};
        _WlanRxFilterOperationCommandBuff_t RxFilterIdMask = {0};

        unsigned char ucVal = 1;
        unsigned char ucConfigOpt = 0;
        unsigned char ucConfigLen = 0;
        unsigned char ucPower = 0;

        long lRetVal = -1;
        long lMode = -1;

        lMode = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(lMode);

        // If the device is not in station-mode, try configuring it in station-mode
        if (ROLE_STA != lMode)
        {
                if (ROLE_AP == lMode)
                {
                        // If the device is in AP mode, we need to wait for this event
                        // before doing anything
                        while (!IS_IP_ACQUIRED(g_ulStatus))
                        {
#ifndef SL_PLATFORM_MULTI_THREADED
                                _SlNonOsMainLoopTask();
#endif
                        }
                }

                // Switch to STA role and restart
                lRetVal = sl_WlanSetMode(ROLE_STA);
                ASSERT_ON_ERROR(lRetVal);

                lRetVal = sl_Stop(0xFF);
                ASSERT_ON_ERROR(lRetVal);

                lRetVal = sl_Start(0, 0, 0);
                ASSERT_ON_ERROR(lRetVal);

                // Check if the device is in station again
                if (ROLE_STA != lRetVal)
                {
                        // We don't want to proceed if the device is not coming up in STA-mode
                        return DEVICE_NOT_IN_STATION_MODE;
                }
        }

        // Get the device's version-information
        ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
        ucConfigLen = sizeof(ver);
        lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
                            &ucConfigLen, (unsigned char *)(&ver));
        ASSERT_ON_ERROR(lRetVal);

        UART_PRINT("Host Driver Version: %s\n\r", SL_DRIVER_VERSION);
        UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
                   ver.NwpVersion[0], ver.NwpVersion[1], ver.NwpVersion[2], ver.NwpVersion[3],
                   ver.ChipFwAndPhyVersion.FwVersion[0], ver.ChipFwAndPhyVersion.FwVersion[1],
                   ver.ChipFwAndPhyVersion.FwVersion[2], ver.ChipFwAndPhyVersion.FwVersion[3],
                   ver.ChipFwAndPhyVersion.PhyVersion[0], ver.ChipFwAndPhyVersion.PhyVersion[1],
                   ver.ChipFwAndPhyVersion.PhyVersion[2], ver.ChipFwAndPhyVersion.PhyVersion[3]);

        // Set connection policy to Auto + SmartConfig
        //      (Device's default connection policy)
        lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                   SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Remove all profiles
        lRetVal = sl_WlanProfileDel(0xFF);
        ASSERT_ON_ERROR(lRetVal);

        //
        // Device in station-mode. Disconnect previous connection if any
        // The function returns 0 if 'Disconnected done', negative number if already
        // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
        // other return-codes
        //
        lRetVal = sl_WlanDisconnect();
        if (0 == lRetVal)
        {
                // Wait
                while (IS_CONNECTED(g_ulStatus))
                {
#ifndef SL_PLATFORM_MULTI_THREADED
                        _SlNonOsMainLoopTask();
#endif
                }
        }

        // Enable DHCP client
        lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &ucVal);
        ASSERT_ON_ERROR(lRetVal);

        // Disable scan
        ucConfigOpt = SL_SCAN_POLICY(0);
        lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN, ucConfigOpt, NULL, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Set Tx power level for station mode
        // Number between 0-15, as dB offset from max power - 0 will set max power
        ucPower = 0;
        lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                             WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
        ASSERT_ON_ERROR(lRetVal);

        // Set PM policy to normal
        lRetVal = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Unregister mDNS services
        lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
        ASSERT_ON_ERROR(lRetVal);

        // Remove  all 64 filters (8*8)
        memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
        lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                                     sizeof(_WlanRxFilterOperationCommandBuff_t));
        ASSERT_ON_ERROR(lRetVal);

        lRetVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(lRetVal);

        InitializeAppVariables();

        return lRetVal; // Success
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
static void BoardInit(void)
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

//****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint
//!
//!  This function connects to the required AP (SSID_NAME) with Security
//!  parameters specified in te form of macros at the top of this file
//!
//! \param  None
//!
//! \return  0 on success else error code
//!
//! \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long WlanConnect()
{
        SlSecParams_t secParams = {0};
        long lRetVal = 0;

        secParams.Key = SECURITY_KEY;
        secParams.KeyLen = strlen(SECURITY_KEY);
        secParams.Type = SECURITY_TYPE;

        UART_PRINT("Attempting connection to access point: ");
        UART_PRINT(SSID_NAME);
        UART_PRINT("... ...");
        lRetVal = sl_WlanConnect(SSID_NAME, strlen(SSID_NAME), 0, &secParams, 0);
        ASSERT_ON_ERROR(lRetVal);

        UART_PRINT(" Connected!!!\n\r");

        // Wait for WLAN Event
        while ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus)))
        {
                // Toggle LEDs to Indicate Connection Progress
                _SlNonOsMainLoopTask();
                GPIO_IF_LedOff(MCU_IP_ALLOC_IND);
                MAP_UtilsDelay(800000);
                _SlNonOsMainLoopTask();
                GPIO_IF_LedOn(MCU_IP_ALLOC_IND);
                MAP_UtilsDelay(800000);
        }

        return SUCCESS;
}

long printErrConvenience(char *msg, long retVal)
{
        UART_PRINT(msg);
        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return retVal;
}

//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time()
{
        long retVal;

        g_time.tm_day = DATE;
        g_time.tm_mon = MONTH;
        g_time.tm_year = YEAR;
        g_time.tm_sec = HOUR;
        g_time.tm_hour = MINUTE;
        g_time.tm_min = SECOND;

        retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                           SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                           sizeof(SlDateTime), (unsigned char *)(&g_time));

        ASSERT_ON_ERROR(retVal);
        return SUCCESS;
}

//*****************************************************************************
//
//! This function demonstrates how certificate can be used with SSL.
//! The procedure includes the following steps:
//! 1) connect to an open AP
//! 2) get the server name via a DNS request
//! 3) define all socket options and point to the CA certificate
//! 4) connect to the server via TCP
//!
//! \param None
//!
//! \return  0 on success else error code
//! \return  LED1 is turned solid in case of success
//!    LED2 is turned solid in case of failure
//!
//*****************************************************************************
static int tls_connect()
{
        SlSockAddrIn_t Addr;
        int iAddrSize;
        unsigned char ucMethod = SL_SO_SEC_METHOD_TLSV1_2;
        unsigned int uiIP;
        //    unsigned int uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
        unsigned int uiCipher = SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256;
        // SL_SEC_MASK_SSL_RSA_WITH_RC4_128_SHA
        // SL_SEC_MASK_SSL_RSA_WITH_RC4_128_MD5
        // SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA
        // SL_SEC_MASK_TLS_DHE_RSA_WITH_AES_256_CBC_SHA
        // SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
        // SL_SEC_MASK_TLS_ECDHE_RSA_WITH_RC4_128_SHA
        // SL_SEC_MASK_TLS_RSA_WITH_AES_128_CBC_SHA256
        // SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA256
        // SL_SEC_MASK_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
        // SL_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 // does not work (-340, handshake fails)
        long lRetVal = -1;
        int iSockID;

        lRetVal = sl_NetAppDnsGetHostByName(g_Host, strlen((const char *)g_Host),
                                            (unsigned long *)&uiIP, SL_AF_INET);

        if (lRetVal < 0)
        {
                return printErrConvenience("Device couldn't retrieve the host name \n\r", lRetVal);
        }

        Addr.sin_family = SL_AF_INET;
        Addr.sin_port = sl_Htons(GOOGLE_DST_PORT);
        Addr.sin_addr.s_addr = sl_Htonl(uiIP);
        iAddrSize = sizeof(SlSockAddrIn_t);
        //
        // opens a secure socket
        //
        iSockID = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_SEC_SOCKET);
        if (iSockID < 0)
        {
                return printErrConvenience("Device unable to create secure socket \n\r", lRetVal);
        }

        //
        // configure the socket as TLS1.2
        //
        lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECMETHOD, &ucMethod,
                                sizeof(ucMethod));
        if (lRetVal < 0)
        {
                return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
        }
        //
        // configure the socket as ECDHE RSA WITH AES256 CBC SHA
        //
        lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &uiCipher,
                                sizeof(uiCipher));
        if (lRetVal < 0)
        {
                return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
        }

        /////////////////////////////////
        // START: COMMENT THIS OUT IF DISABLING SERVER VERIFICATION
        //
        // configure the socket with CA certificate - for server verification
        //
        lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
                                SL_SO_SECURE_FILES_CA_FILE_NAME,
                                SL_SSL_CA_CERT,
                                strlen(SL_SSL_CA_CERT));

        if (lRetVal < 0)
        {
                return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
        }
        // END: COMMENT THIS OUT IF DISABLING SERVER VERIFICATION
        /////////////////////////////////

        // configure the socket with Client Certificate - for server verification
        //
        lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
                                SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME,
                                SL_SSL_CLIENT,
                                strlen(SL_SSL_CLIENT));

        if (lRetVal < 0)
        {
                return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
        }

        // configure the socket with Private Key - for server verification
        //
        lRetVal = sl_SetSockOpt(iSockID, SL_SOL_SOCKET,
                                SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME,
                                SL_SSL_PRIVATE,
                                strlen(SL_SSL_PRIVATE));

        if (lRetVal < 0)
        {
                return printErrConvenience("Device couldn't set socket options \n\r", lRetVal);
        }

        /* connect to the peer device - Google server */
        lRetVal = sl_Connect(iSockID, (SlSockAddr_t *)&Addr, iAddrSize);

        if (lRetVal >= 0)
        {
                UART_PRINT("Device has connected to the website:");
                UART_PRINT(SERVER_NAME);
                UART_PRINT("\n\r");
        }
        else if (lRetVal == SL_ESECSNOVERIFY)
        {
                UART_PRINT("Device has connected to the website (UNVERIFIED):");
                UART_PRINT(SERVER_NAME);
                UART_PRINT("\n\r");
        }
        else if (lRetVal < 0)
        {
                UART_PRINT("Device couldn't connect to server:");
                UART_PRINT(SERVER_NAME);
                UART_PRINT("\n\r");
                return printErrConvenience("Device couldn't connect to server \n\r", lRetVal);
        }

        GPIO_IF_LedOff(MCU_RED_LED_GPIO);
        GPIO_IF_LedOn(MCU_GREEN_LED_GPIO);
        return iSockID;
}

int connectToAccessPoint()
{
        long lRetVal = -1;
        GPIO_IF_LedConfigure(LED1 | LED3);

        GPIO_IF_LedOff(MCU_RED_LED_GPIO);
        GPIO_IF_LedOff(MCU_GREEN_LED_GPIO);

        lRetVal = InitializeAppVariables();
        ASSERT_ON_ERROR(lRetVal);

        //
        // Following function configure the device to default state by cleaning
        // the persistent settings stored in NVMEM (viz. connection profiles &
        // policies, power policy etc)
        //
        // Applications may choose to skip this step if the developer is sure
        // that the device is in its default state at start of applicaton
        //
        // Note that all profiles and persistent settings that were done on the
        // device will be lost
        //
        lRetVal = ConfigureSimpleLinkToDefaultState();
        if (lRetVal < 0)
        {
                if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
                        UART_PRINT("Failed to configure the device in its default state \n\r");

                return lRetVal;
        }

        UART_PRINT("Device is configured in default state \n\r");

        CLR_STATUS_BIT_ALL(g_ulStatus);

        ///
        // Assumption is that the device is configured in station mode already
        // and it is in its default state
        //
        UART_PRINT("Opening sl_start\n\r");
        lRetVal = sl_Start(0, 0, 0);
        if (lRetVal < 0 || ROLE_STA != lRetVal)
        {
                UART_PRINT("Failed to start the device \n\r");
                return lRetVal;
        }

        UART_PRINT("Device started as STATION \n\r");

        //
        // Connecting to WLAN AP
        //
        lRetVal = WlanConnect();
        if (lRetVal < 0)
        {
                UART_PRINT("Failed to establish connection w/ an AP \n\r");
                GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                return lRetVal;
        }

        UART_PRINT("Connection established w/ AP and IP is aquired \n\r");
        return 0;
}

// our times array for keeping track of the times in which a falling edge occurs
unsigned long times[35];

// for iterating through times array
unsigned long times_index = 0;

// falling edge handler for signal in from IR remote
static void FallingEdgeHandler(void)
{
        // start of new signal
        if (times_index == 0)
        {
                // reset timer
                HWREG(0xE000E018) = 1;
                times[times_index] = SysTickValueGet();
                times_index++;
        }
        else if (times_index == 1)
        {
                times[times_index] = SysTickValueGet();

                // check if start signal occurred
                if (times[times_index - 1] - times[times_index] > 1060000 && times[times_index - 1] - times[times_index] < 1070000)
                {
                        // if start signal did indeed occurred, we keep appending times to the time array
                        times_index++;
                }
                else
                {
                        // else start signal did not occurred, so we reset time array to try again
                        times_index = 0;

                        // fix buggy offsets of wrong times in the time array by waiting a while before we read in falling edges again.
                        // This is used in other parts of code for the same reason.
                        UtilsDelay(999999);
                }
        }
        else if (times_index == 34)
        {
                // time array is filled, so we should stop
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

// our letter struct. Makes it easier to store and send
// strings that contain different colored characters
typedef struct Letter
{
        unsigned int x;
        unsigned int y;
        char alpha;
        int color;
} Letter;

// global x and y position on OLED for composing message
int c_x = 0;
int c_y = 10;

// global x and y position on OLED for receiving message
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
        drawChar(c_message[c_message_index].x, c_message[c_message_index].y, c_message[c_message_index].alpha, BLACK, BLACK, 1);
}

// color array to cycle through so characters can be different colors, as requested by specifications
int color_arr[5] = {WHITE, RED, GREEN, BLUE, YELLOW};
int color_arr_index = 0;

long lRetVal = -1;

void SendMessage()
{
        if (c_message_index != -1)
        {
                // post composed message to device shadow
                http_post(lRetVal);
                UART_PRINT("--------------------------------------------------------------------------------------------------------------\n\r");
                // get composed message from device shadow
                http_get(lRetVal);

                c_message_index = -1;
        }
}

// global variable for storing previous characters
// initially, not set to any of the buttons that map to characters
int prevbutton = mute;

void main()
{
        //
        // Initialize board configuration
        //
        BoardInit();

        PinMuxConfig();

        InitTerm();
        ClearTerm();

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

        // settup and start systick
        SysTickPeriodSet(16777216);
        SysTickIntUnregister();
        SysTickEnable();

        //
        // Register the interrupt handler for receiver port
        //
        MAP_GPIOIntRegister(switch2.port, FallingEdgeHandler);

        //
        // Configure rising edge interrupts on receiver port
        //
        MAP_GPIOIntTypeSet(switch2.port, switch2.pin, GPIO_FALLING_EDGE); // SW2

        unsigned long ulStatus;

        ulStatus = MAP_GPIOIntStatus(switch2.port, false);
        MAP_GPIOIntClear(switch2.port, ulStatus); // clear interrupts on for

        // Enable interrupt handler for receiver port
        MAP_GPIOIntEnable(switch2.port, switch2.pin);

        setCursor(0, 0);
        Outstr("-> Current Font Color <-");

        // configure timer_a in timera0
        Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC_UP, TIMER_A, 0);
        TimerEnable(TIMERA0_BASE, TIMER_A);
        TimerValueSet(TIMERA0_BASE, TIMER_A, 0);

        // variable for aid with time interval since last button pressed
        long int timer = TimerValueGet(TIMERA0_BASE, TIMER_A);

        // Connect the CC3200 to the local access point
        lRetVal = connectToAccessPoint();
        // Set time so that encryption can be used
        lRetVal = set_time();
        if (lRetVal < 0)
        {
                UART_PRINT("Unable to set time in the device");
                LOOP_FOREVER();
        }
        // Connect to the website with TLS encryption
        lRetVal = tls_connect();
        if (lRetVal < 0)
        {
                ERR_PRINT(lRetVal);
        }

        while (1)
        {
                if (times_index == 34)
                {
                        long int binary = 0;

                        int binary_index = 31;

                        int i = 1;

                        // iterate through time array to calculate the decimal notation of the binary number in the data field of the waveform received
                        for (; i < 33; i++)
                        {
                                if ((times[i] - times[i + 1]) > 170000)
                                {
                                        binary |= 1 << binary_index;
                                }
                                binary_index--;
                        }

                        // get time interval since last button pressed and restart Timer_A
                        int time_since_last_press = TimerValueGet(TIMERA0_BASE, TIMER_A) - timer;
                        TimerValueSet(TIMERA0_BASE, TIMER_A, 0);
                        timer = TimerValueGet(TIMERA0_BASE, TIMER_A);

                        switch (binary)
                        {
                        case zero:
                                // space
                                // add Letter ' ' to composing message
                                Message("Zero\n\r");
                                c_message[++c_message_index].alpha = ' ';
                                c_message[c_message_index].x = c_x;
                                c_message[c_message_index].y = c_y;
                                c_message[c_message_index].color = color_arr_index;
                                setCursor(c_x, c_y);
                                Outstr(" ");
                                // update composing message x position for next character
                                c_x += 6;
                                // update previous button pressed
                                prevbutton = zero;
                                break;
                        case one:
                                // update current text color and change the cursor -> color so the user
                                // know what is the current text color
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
                                // b, c
                                Message("Two\n\r");
                                if (prevbutton == two && c_message[c_message_index].alpha != 'c' && time_since_last_press < 90000000)
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
                                        c_x += 6;
                                }
                                else
                                {
                                        // a
                                        // add Letter a to composing message
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
                        //------------------------IMPORTANT----------------------------
                        // for case three to nine, please look at case two. The code is similar
                        // so the explanation in case two is sufficient for understanding case three to nine
                        case three:
                                Message("Three\n\r");
                                if (prevbutton == three && c_message[c_message_index].alpha != 'f' && time_since_last_press < 90000000)
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
                                if (prevbutton == four && c_message[c_message_index].alpha != 'i' && time_since_last_press < 90000000)
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
                                if (prevbutton == five && c_message[c_message_index].alpha != 'l' && time_since_last_press < 90000000)
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
                                if (prevbutton == six && c_message[c_message_index].alpha != 'o' && time_since_last_press < 90000000)
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
                                if (prevbutton == seven && c_message[c_message_index].alpha != 's' && time_since_last_press < 90000000)
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
                                if (prevbutton == eight && c_message[c_message_index].alpha != 'v' && time_since_last_press < 90000000)
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
                                if (prevbutton == nine && c_message[c_message_index].alpha != 'z' && time_since_last_press < 90000000)
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

                        // reset times array for reading in another button press
                        times_index = 0;
                }
        }
}

// parse json correctly for POST then post json to device shadow
static int http_post(int iTLSSockID)
{
        char acSendBuff[512];
        char acRecvbuff[1460];
        char cCLLength[200];
        char *pcBufHeaders;
        int lRetVal = 0;

        pcBufHeaders = acSendBuff;
        strcpy(pcBufHeaders, POSTHEADER);
        pcBufHeaders += strlen(POSTHEADER);
        strcpy(pcBufHeaders, HOSTHEADER);
        pcBufHeaders += strlen(HOSTHEADER);
        strcpy(pcBufHeaders, CHEADER);
        pcBufHeaders += strlen(CHEADER);
        strcpy(pcBufHeaders, "\r\n\r\n");

        int dataLength = strlen(DATA1) + c_message_index + 1 + strlen(DATA2);

        strcpy(pcBufHeaders, CTHEADER);
        pcBufHeaders += strlen(CTHEADER);
        strcpy(pcBufHeaders, CLHEADER1);

        pcBufHeaders += strlen(CLHEADER1);
        sprintf(cCLLength, "%d", dataLength);

        strcpy(pcBufHeaders, cCLLength);
        pcBufHeaders += strlen(cCLLength);
        strcpy(pcBufHeaders, CLHEADER2);
        pcBufHeaders += strlen(CLHEADER2);

        strcpy(pcBufHeaders, DATA1);
        pcBufHeaders += strlen(DATA1);

        int i;

        // add char one by one from composing message array to json array
        for (i = 0; i <= c_message_index; i++)
        {
                char buf[1] = c_message[i].alpha;
                char *buf_ptr;
                buf_ptr = buf;
                strcpy(pcBufHeaders, buf_ptr);
                pcBufHeaders += 1;
        }

        strcpy(pcBufHeaders, DATA2);
        pcBufHeaders += strlen(DATA2);

        int testDataLength = strlen(pcBufHeaders);

        UART_PRINT(acSendBuff);

        //
        // Send the packet to the server */
        //
        lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
        if (lRetVal < 0)
        {
                UART_PRINT("POST failed. Error Number: %i\n\r", lRetVal);
                sl_Close(iTLSSockID);
                GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                return lRetVal;
        }
        lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
        if (lRetVal < 0)
        {
                UART_PRINT("Received failed. Error Number: %i\n\r", lRetVal);
                // sl_Close(iSSLSockID);
                GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                return lRetVal;
        }
        else
        {
                acRecvbuff[lRetVal + 1] = '\0';
                UART_PRINT(acRecvbuff);
                UART_PRINT("\n\r\n\r");
        }

        return 0;
}

// parse json correctly for GET then get json from device shadow
static int http_get(int iTLSSockID)
{
        char acSendBuff[512];
        char acRecvbuff[1460];
        char *pcBufHeaders;
        int lRetVal = 0;

        pcBufHeaders = acSendBuff;
        strcpy(pcBufHeaders, GETHEADER);
        pcBufHeaders += strlen(GETHEADER);
        strcpy(pcBufHeaders, HOSTHEADER);
        pcBufHeaders += strlen(HOSTHEADER);
        strcpy(pcBufHeaders, CHEADER);
        pcBufHeaders += strlen(CHEADER);
        strcpy(pcBufHeaders, "\r\n\r\n");

        UART_PRINT(acSendBuff);

        //
        // Send the packet to the server */
        //
        lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
        if (lRetVal < 0)
        {
                UART_PRINT("POST failed. Error Number: %i\n\r", lRetVal);
                sl_Close(iTLSSockID);
                GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                return lRetVal;
        }
        lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
        if (lRetVal < 0)
        {
                UART_PRINT("Received failed. Error Number: %i\n\r", lRetVal);
                // sl_Close(iSSLSockID);
                GPIO_IF_LedOn(MCU_RED_LED_GPIO);
                return lRetVal;
        }
        else
        {
                acRecvbuff[lRetVal + 1] = '\0';
                UART_PRINT(acRecvbuff);
                UART_PRINT("\n\r\n\r");
        }

        return 0;
}
