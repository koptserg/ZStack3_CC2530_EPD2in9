/**************************************************************************************************
  Filename:       zcl_RealApp.h
  Revised:        $Date: 2014-06-19 08:38:22 -0700 (Thu, 19 Jun 2014) $
  Revision:       $Revision: 39101 $

  Description:    This file contains the ZigBee Cluster Library Home
                  Automation Sample Application.


  Copyright 2006-2014 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

#ifndef ZCL_RealApp_H
#define ZCL_RealApp_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "zcl.h"


// Added to include ZLL Target functionality
#if defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  #include "zcl_general.h"
  #include "bdb_tlCommissioning.h"
#endif

/*********************************************************************
 * CONSTANTS
 */
//#define RealApp_ENDPOINT            8
#define RealApp_ENDPOINT            1
// Added to include ZLL Target functionality
#define RealApp_NUM_GRPS            2


// Application Events
//#define RealApp_MAIN_SCREEN_EVT          0x0001
//#define RealApp_LEVEL_CTRL_EVT           0x0002
//#define RealApp_END_DEVICE_REJOIN_EVT    0x0004  
  
/* RealApp_TODO: define app events here */

#define RealApp_EVT_BLINK                0x0001
#define RealApp_EVT_NEWJOIN              0x0002
#define RealApp_END_DEVICE_REJOIN_EVT    0x0004
#define RealApp_REPORTING_EVT            0x0008

// NVM IDs
#define NV_RealApp_RELAY_STATE_ID        0x0402
  
//#define RealApp_EVT_1                    0x0008
/*
#define RealApp_EVT_2                    0x0010
#define RealApp_EVT_3                    0x0020
*/

// Application Display Modes
#define GENERIC_MAINMODE      0x00
#define GENERIC_HELPMODE      0x01
  
#define RealApp_END_DEVICE_REJOIN_DELAY 10000

/*********************************************************************
 * MACROS
 */
/*********************************************************************
 * TYPEDEFS
 */
typedef void (*RealApphalKeyCBack_t) (uint8 keys, uint8 state);

/*********************************************************************
 * VARIABLES
 */

// Added to include ZLL Target functionality
#if defined ( BDB_TL_INITIATOR ) || defined ( BDB_TL_TARGET )
  extern bdbTLDeviceInfo_t tlRealApp_DeviceInfo;
#endif

extern SimpleDescriptionFormat_t zclRealApp_SimpleDesc;

extern CONST zclCommandRec_t zclRealApp_Cmds[];

extern CONST uint8 zclCmdsArraySize;

// attribute list
extern CONST zclAttrRec_t zclRealApp_Attrs[];
extern CONST uint8 zclRealApp_NumAttributes;

// Identify attributes
extern uint16 zclRealApp_IdentifyTime;
extern uint8  zclRealApp_IdentifyCommissionState;

// RealApp_TODO: Declare application specific attributes here


/*********************************************************************
 * FUNCTIONS
 */

 /*
  * Initialization for the task
  */
extern void zclRealApp_Init( byte task_id );

/*
 *  Event Process for the task
 */
extern UINT16 zclRealApp_event_loop( byte task_id, UINT16 events );

/*
 *  Reset all writable attributes to their default values.
 */
extern void zclRealApp_ResetAttributesToDefaultValues(void);

// 
extern void RealApp_HalKeyInit( void );
extern void RealApp_HalKeyConfig( bool interruptEnable, const RealApphalKeyCBack_t cback);
//extern void RealApp_HalKeyConfig( bool interruptEnable);
extern void RealApp_HalKeyPoll ( void );
void RealApp_OnBoard_KeyCallback ( uint8 keys, uint8 state );

// 
static void zclRealApp_OnOffCB(uint8);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ZCL_RealApp_H */
