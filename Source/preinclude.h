#define SECURE 1
#define TC_LINKKEY_JOIN
#define NV_INIT
#define NV_RESTORE

//#define TP2_LEGACY_ZC
//#define ZDSECMGR_TC_ATTEMPT_DEFAULT_KEY TRUE

#define NWK_AUTO_POLL
#define MULTICAST_ENABLED FALSE

//#define xZTOOL_P1
//#define MT_TASK
//#define MT_APP_FUNC
//#define MT_SYS_FUNC
//#define MT_ZDO_FUNC
//#define MT_ZDO_MGMT
//#define MT_APP_CNF_FUNC
//#define LEGACY_LCD_DEBUG
#define HAL_LCD TRUE
//#define LCD_SUPPORTED DEBUG

#define ZCL_READ
#define ZCL_WRITE
#define ZCL_BASIC
#define ZCL_IDENTIFY
#define ZCL_SCENES
#define ZCL_GROUPS
#define ZCL_ON_OFF
#define ZCL_REPORTING_DEVICE

#define INTER_PAN

//#define POWER_SAVING

#define ISR_KEYINTERRUPT

#define DISABLE_GREENPOWER_BASIC_PROXY
#define DEFAULT_CHANLIST 0x07FFF800  

#define HAL_SONOFF
//#define HAL_ADC FALSE
//#define OSC32K_CRYSTAL_INSTALLED FALSE

    #define HAL_UART TRUE
//    #define HAL_UART_ISR 1
//    #define HAL_UART_DMA 2

#include "hal_board_cfg_RealApp.h"