#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "MT_SYS.h"

#include "nwk_util.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_ms.h"
#include "zcl_diagnostic.h"
#include "zcl_RealApp.h"

#include "bdb.h"
#include "bdb_interface.h"
#include "gp_interface.h"

#include "debug.h"

#include "onboard.h"

/* HAL */
#include "hal_lcd.h" // HalLcd_HW_Init()
#include "hal_led.h"
#include "hal_key.h"
#include "hal_drivers.h"
#include "hal_sleep.h"
#include "hal_adc.h"

#include "epd2in9.h"
#include "imagedata.h"
#include "epdpaint.h"
#include "epdtest.h"

/*********************************************************************
 * MACROS
 */


/*********************************************************************
 * CONSTANTS
 */


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte zclRealApp_TaskID;
//extern bool requestNewTrustCenterLinkKey;

/*********************************************************************
 * GLOBAL FUNCTIONS
 */
 
/*********************************************************************
 * LOCAL VARIABLES
 */

//uint8 giGenAppScreenMode = GENERIC_MAINMODE;   // display the main screen mode first

//uint8 gPermitDuration = 0;    // permit joining default to disabled (for LCD)

devStates_t zclRealApp_NwkState = DEV_INIT;

// 
static uint8 halKeySavedKeys;     /* used to store previous key state in polling mode */
//static RealApphalKeyCBack_t pHalKeyProcessFunction;
//static uint8 HalKeyConfigured;
bool Hal_KeyIntEnable;            /* interrupt enable/disable flag */
// state relay
uint8 RELAY_STATE = 0;
// Battery Voltage
uint8 zclRealApp_BatteryVoltage;
uint8 zclRealApp_BatteryPercentageRemaining;
// Temperature 
int16 zclRealApp_TemperatureMeasuredValue;
// 
afAddrType_t zclRealApp_DstAddr;
// 
uint8 SeqNum = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
void halProcessKeyInterrupt(void);

static void zclRealApp_HandleKeys( byte shift, byte keys );
static void zclRealApp_BasicResetCB( void );
static void zclRealApp_ProcessIdentifyTimeChange( uint8 endpoint );
static void zclRealApp_BindNotification( bdbBindNotificationData_t *data );
#if ( defined ( BDB_TL_TARGET ) && (BDB_TOUCHLINK_CAPABILITY_ENABLED == TRUE) )
static void zclRealApp_ProcessTouchlinkTargetEnable( uint8 enable );
#endif

static void zclRealApp_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);

// app display functions
//static void zclRealApp_LcdDisplayUpdate( void );
#ifdef LCD_SUPPORTED
static void zclRealApp_LcdDisplayMainMode( void );
static void zclRealApp_LcdDisplayHelpMode( void );
#endif

// Functions to process ZCL Foundation incoming Command/Response messages
static void zclRealApp_ProcessIncomingMsg( zclIncomingMsg_t *msg );
#ifdef ZCL_READ
static uint8 zclRealApp_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif
#ifdef ZCL_WRITE
static uint8 zclRealApp_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif
static uint8 zclRealApp_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );
#ifdef ZCL_DISCOVER
static uint8 zclRealApp_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclRealApp_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclRealApp_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg );
#endif

static void zclSampleApp_BatteryWarningCB( uint8 voltLevel);

/*********************************************************************
 * STATUS STRINGS
 */
#ifdef LCD_SUPPORTED
const char sDeviceName[]   = "  Generic App";
const char sClearLine[]    = " ";
const char sSwRealApp[]      = "SW1:GENAPP_TODO";  // RealApp_TODO
const char sSwBDBMode[]     = "SW2: Start BDB";
char sSwHelp[]             = "SW4: Help       ";  // last character is * if NWK open
#endif

// 
static void updateRelay( bool );
// 
static void applyRelay( void );
// 
void zclRealApp_LeaveNetwork( void );
// Report OnOff
void zclRealApp_ReportOnOff( void );
//Report BatteryVoltage
void zclRealApp_ReportBatteryVoltage( void );
// read Battery Voltage
int16 readBatteryVoltage16(void);
//
void zclRealApp_ReportTemp( void );
//
int16 readTemperature(void);
//
__interrupt void IRQ_P2INT(void);
__interrupt void IRQ_P1INT(void);
void _delay_us(uint16 microSecs);
void _delay_ms(uint16 milliSecs);

unsigned long time_now_s;

/*********************************************************************
 * 
 */
static zclGeneral_AppCallbacks_t zclRealApp_CmdCallbacks =
{
  zclRealApp_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify Trigger Effect command
  zclRealApp_OnOffCB,                    // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                  // Scene Store Request command
  NULL,                                  // Scene Recall Request command
  NULL,                                  // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                  // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                  // Get Event Log command
  NULL,                                  // Publish Event Log command
#endif
  NULL,                                  // RSSI Location command
  NULL                                   // RSSI Location Response command
};

/*********************************************************************
 * RealApp_TODO: Add other callback structures for any additional application specific 
 *       Clusters being used, see available callback structures below.
 *
 *       bdbTL_AppCallbacks_t 
 *       zclApplianceControl_AppCallbacks_t 
 *       zclApplianceEventsAlerts_AppCallbacks_t 
 *       zclApplianceStatistics_AppCallbacks_t 
 *       zclElectricalMeasurement_AppCallbacks_t 
 *       zclGeneral_AppCallbacks_t 
 *       zclGp_AppCallbacks_t 
 *       zclHVAC_AppCallbacks_t 
 *       zclLighting_AppCallbacks_t 
 *       zclMS_AppCallbacks_t 
 *       zclPollControl_AppCallbacks_t 
 *       zclPowerProfile_AppCallbacks_t 
 *       zclSS_AppCallbacks_t  
 *
 */

/*********************************************************************
 * @fn          zclRealApp_Init
 *
 * @brief       Initialization function for the zclGeneral layer.
 *
 * @param       none
 *
 * @return      none
 */
void zclRealApp_Init( byte task_id )
{
  DebugInit();
  
  // this is important to allow connects throught routers
  // to make this work, coordinator should be compiled with this flag #define TP2_LEGACY_ZC
//  requestNewTrustCenterLinkKey = FALSE;
  
  zclRealApp_TaskID = task_id;

  // This app is part of the Home Automation Profile 
  bdb_RegisterSimpleDescriptor( &zclRealApp_SimpleDesc );

  // Register the ZCL General Cluster Library callback functions
  zclGeneral_RegisterCmdCallbacks( RealApp_ENDPOINT, &zclRealApp_CmdCallbacks );
  
  // RealApp_TODO: Register other cluster command callbacks here

  // Register the application's attribute list
  zcl_registerAttrList( RealApp_ENDPOINT, zclRealApp_NumAttributes, zclRealApp_Attrs );

  // Register the Application to receive the unprocessed Foundation command/response messages
  zcl_registerForMsg( zclRealApp_TaskID );

#ifdef ZCL_DISCOVER
  // Register the application's command list
  zcl_registerCmdList( RealApp_ENDPOINT, zclCmdsArraySize, zclRealApp_Cmds );
#endif

  // Register low voltage NV memory protection application callback
  RegisterVoltageWarningCB( zclSampleApp_BatteryWarningCB );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( zclRealApp_TaskID );

  bdb_RegisterCommissioningStatusCB( zclRealApp_ProcessCommissioningStatus );
  bdb_RegisterIdentifyTimeChangeCB( zclRealApp_ProcessIdentifyTimeChange );
  bdb_RegisterBindNotificationCB( zclRealApp_BindNotification );

#if ( defined ( BDB_TL_TARGET ) && (BDB_TOUCHLINK_CAPABILITY_ENABLED == TRUE) )
  bdb_RegisterTouchlinkTargetEnableCB( zclRealApp_ProcessTouchlinkTargetEnable );
#endif

#ifdef ZCL_DIAGNOSTIC
  // Register the application's callback function to read/write attribute data.
  // This is only required when the attribute data format is unknown to ZCL.
  zcl_registerReadWriteCB( RealApp_ENDPOINT, zclDiagnostic_ReadWriteAttrCB, NULL );

  if ( zclDiagnostic_InitStats() == ZSuccess )
  {
    // Here the user could start the timer to save Diagnostics to NV
  }
#endif


#ifdef LCD_SUPPORTED
  HalLcdWriteString ( (char *)sDeviceName, HAL_LCD_LINE_3 );
#endif  // LCD_SUPPORTED

  // 
  if ( SUCCESS == osal_nv_item_init( NV_RealApp_RELAY_STATE_ID, 1, &RELAY_STATE ) ) {
    // 
    osal_nv_read( NV_RealApp_RELAY_STATE_ID, 0, 1, &RELAY_STATE );
  }
  // 
  applyRelay();

  // autojoin
//  bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING |
//                         BDB_COMMISSIONING_MODE_FINDING_BINDING);
  // 
  bdb_StartCommissioning(BDB_COMMISSIONING_MODE_PARENT_LOST);
  
  // 
//  osal_start_reload_timer( zclRealApp_TaskID, HAL_KEY_EVENT, 100);
    
    ZMacSetTransmitPower(TX_PWR_PLUS_4); // set 4dBm

  // init hal spi
  HalLcd_HW_Init();
  // epd start screen
  EpdInit(lut_full_update);
  EpdSetFrameMemory(IMAGE_DATA); 
  EpdDisplayFrame();
  _delay_ms(2000);
  EpdClearFrameMemory(0xFF);
  EpdDisplayFrame();
  _delay_ms(2000);
  // epd partial screen
  EpdInit(lut_partial_update);
  EpdtestNotRefresh();
  EpdtestNotRefresh();
  time_now_s = 0;
  EpdtestRefresh(RELAY_STATE, time_now_s);
  
  osal_start_reload_timer (zclRealApp_TaskID, RealApp_REPORTING_EVT, 1000);
}

/*********************************************************************
 * @fn          zclSample_event_loop
 *
 * @brief       Event Loop Processor for zclGeneral.
 *
 * @param       none
 *
 * @return      none
 */
uint16 zclRealApp_event_loop( uint8 task_id, uint16 events )
{
  
  afIncomingMSGPacket_t *MSGpkt;

  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclRealApp_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          // Incoming ZCL Foundation command/response messages
          zclRealApp_ProcessIncomingMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          zclRealApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          zclRealApp_NwkState = (devStates_t)(MSGpkt->hdr.status);

          // now on the network
          if ( (zclRealApp_NwkState == DEV_ZB_COORD) ||
               (zclRealApp_NwkState == DEV_ROUTER)   ||
               (zclRealApp_NwkState == DEV_END_DEVICE) )
          {
            // 
            osal_stop_timerEx(zclRealApp_TaskID, HAL_LED_BLINK_EVENT);
            HalLedSet( HAL_LED_2, HAL_LED_MODE_OFF );
            
//            giGenAppScreenMode = GENERIC_MAINMODE;
//            zclRealApp_LcdDisplayUpdate();
            
            // 
            zclRealApp_ReportOnOff();
          }
          break;

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

#if ZG_BUILD_ENDDEVICE_TYPE    
  if ( events & RealApp_END_DEVICE_REJOIN_EVT )
  {
    bdb_ZedAttemptRecoverNwk();
    return ( events ^ RealApp_END_DEVICE_REJOIN_EVT );
  }
#endif

  /* RealApp_TODO: handle app events here */
  
  // RealApp_EVT_NEWJOIN
  if ( events & RealApp_EVT_NEWJOIN )  
  {
    // 
    if ( bdbAttributes.bdbNodeIsOnANetwork )
    {
      //
      zclRealApp_LeaveNetwork();
    }
    else 
    {
      // 
      bdb_StartCommissioning(
        BDB_COMMISSIONING_MODE_NWK_FORMATION | 
        BDB_COMMISSIONING_MODE_NWK_STEERING | 
        BDB_COMMISSIONING_MODE_FINDING_BINDING | 
        BDB_COMMISSIONING_MODE_INITIATOR_TL
      );
      // 
 //     osal_start_timerEx(zclRealApp_TaskID, RealApp_EVT_BLINK, 500);
    }
    
    return ( events ^ RealApp_EVT_NEWJOIN );
  }
   
  // 
  if (events & HAL_KEY_EVENT)
  {
    LREPMaster("HAL_KEY_EVENT\r\n");
    /* */
    RealApp_HalKeyPoll();
    zclRealApp_ReportTemp();
    return events ^ HAL_KEY_EVENT;
  }
  
  if (events & RealApp_REPORTING_EVT)
  {
    LREPMaster("RealApp_REPORTING_EVT\r\n");
    time_now_s = time_now_s +1 ;
    EpdtestRefresh(RELAY_STATE, time_now_s);
    /* */
//    zclRealApp_ReportBatteryVoltage();
//    zclRealApp_ReportTemp();
    return events ^ RealApp_REPORTING_EVT;
  }
//
  
  // Discard unknown events
  return 0;
}

/*********************************************************************
 * @fn      zclRealApp_LcdDisplayUpdate
 *
 * @brief   Called to update the LCD display.
 *
 * @param   none
 *
 * @return  none
*/ 
/*
void zclRealApp_LcdDisplayUpdate( void )
{
#ifdef LCD_SUPPORTED
  if ( giGenAppScreenMode == GENERIC_HELPMODE )
  {
    zclRealApp_LcdDisplayHelpMode();
  }
  else
  {
    zclRealApp_LcdDisplayMainMode();
  }
#endif
}
*/
#ifdef LCD_SUPPORTED
/*********************************************************************
 * @fn      zclRealApp_LcdDisplayMainMode
 *
 * @brief   Called to display the main screen on the LCD.
 *
 * @param   none
 *
 * @return  none
 */
static void zclRealApp_LcdDisplayMainMode( void )
{
  // display line 1 to indicate NWK status
  if ( zclRealApp_NwkState == DEV_ZB_COORD )
  {
    zclHA_LcdStatusLine1( ZCL_HA_STATUSLINE_ZC );
  }
  else if ( zclRealApp_NwkState == DEV_ROUTER )
  {
    zclHA_LcdStatusLine1( ZCL_HA_STATUSLINE_ZR );
  }
  else if ( zclRealApp_NwkState == DEV_END_DEVICE )
  {
   zclHA_LcdStatusLine1( ZCL_HA_STATUSLINE_ZED );
  }

  // end of line 3 displays permit join status (*)
  if ( gPermitDuration )
  {
    sSwHelp[15] = '*';
  }
  else
  {
    sSwHelp[15] = ' ';
  }
    HalLcdWriteString( (char *)sSwHelp, HAL_LCD_LINE_3 );
}

/*********************************************************************
 * @fn      zclRealApp_LcdDisplayHelpMode
 *
 * @brief   Called to display the SW options on the LCD.
 *
 * @param   none
 *
 * @return  none
 */
static void zclRealApp_LcdDisplayHelpMode( void )
{
  HalLcdWriteString( (char *)sSwRealApp, HAL_LCD_LINE_1 );
  HalLcdWriteString( (char *)sSwBDBMode, HAL_LCD_LINE_2 );
  HalLcdWriteString( (char *)sSwHelp, HAL_LCD_LINE_3 );
}
#endif  // LCD_SUPPORTED

/*********************************************************************
 * @fn      zclRealApp_ProcessCommissioningStatus
 *
 * @brief   Callback in which the status of the commissioning process are reported
 *
 * @param   bdbCommissioningModeMsg - Context message of the status of a commissioning process
 *
 * @return  none
 */
static void zclRealApp_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //After formation, perform nwk steering again plus the remaining commissioning modes that has not been process yet
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING | bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
      }
      else
      {
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
        //We are on the nwk, what now?
      }
      else
      {
        //See the possible errors for nwk steering procedure
        //No suitable networks found
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_FINDING_BINDING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
      }
      else
      {
        //YOUR JOB:
        //retry?, wait for user interaction?
      }
    break;
    case BDB_COMMISSIONING_INITIALIZATION:
      //Initialization notification can only be successful. Failure on initialization
      //only happens for ZED and is notified as BDB_COMMISSIONING_PARENT_LOST notification

      //YOUR JOB:
      //We are on a network, what now?

    break;
#if ZG_BUILD_ENDDEVICE_TYPE    
    case BDB_COMMISSIONING_PARENT_LOST:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)
      {
        //We did recover from losing parent
      }
      else
      {
        //Parent not found, attempt to rejoin again after a fixed delay
        osal_start_timerEx(zclRealApp_TaskID, RealApp_END_DEVICE_REJOIN_EVT, RealApp_END_DEVICE_REJOIN_DELAY);
      }
    break;
#endif 
  }
}

/*********************************************************************
 * @fn      zclRealApp_ProcessIdentifyTimeChange
 *
 * @brief   Called to process any change to the IdentifyTime attribute.
 *
 * @param   endpoint - in which the identify has change
 *
 * @return  none
 */
static void zclRealApp_ProcessIdentifyTimeChange( uint8 endpoint )
{
  (void) endpoint;

  if ( zclRealApp_IdentifyTime > 0 )
  {
    HalLedBlink ( HAL_LED_2, 0xFF, HAL_LED_DEFAULT_DUTY_CYCLE, HAL_LED_DEFAULT_FLASH_TIME );
  }
  else
  {
    HalLedSet ( HAL_LED_2, HAL_LED_MODE_OFF );
  }
}

/*********************************************************************
 * @fn      zclRealApp_BindNotification
 *
 * @brief   Called when a new bind is added.
 *
 * @param   data - pointer to new bind data
 *
 * @return  none
 */
static void zclRealApp_BindNotification( bdbBindNotificationData_t *data )
{
  // RealApp_TODO: process the new bind information
}


/*********************************************************************
 * @fn      zclRealApp_ProcessTouchlinkTargetEnable
 *
 * @brief   Called to process when the touchlink target functionality
 *          is enabled or disabled
 *
 * @param   none
 *
 * @return  none
 */
#if ( defined ( BDB_TL_TARGET ) && (BDB_TOUCHLINK_CAPABILITY_ENABLED == TRUE) )
static void zclRealApp_ProcessTouchlinkTargetEnable( uint8 enable )
{
  if ( enable )
  {
    HalLedSet ( HAL_LED_1, HAL_LED_MODE_ON );
  }
  else
  {
    HalLedSet ( HAL_LED_1, HAL_LED_MODE_OFF );
  }
}
#endif

/*********************************************************************
 * @fn      zclRealApp_BasicResetCB
 *
 * @brief   Callback from the ZCL General Cluster Library
 *          to set all the Basic Cluster attributes to default values.
 *
 * @param   none
 *
 * @return  none
 */
static void zclRealApp_BasicResetCB( void )
{

  /* RealApp_TODO: remember to update this function with any
     application-specific cluster attribute variables */
  
  zclRealApp_ResetAttributesToDefaultValues();
  
}
/*********************************************************************
 * @fn      zclSampleApp_BatteryWarningCB
 *
 * @brief   Called to handle battery-low situation.
 *
 * @param   voltLevel - level of severity
 *
 * @return  none
 */
void zclSampleApp_BatteryWarningCB( uint8 voltLevel )
{
  if ( voltLevel == VOLT_LEVEL_CAUTIOUS )
  {
    // Send warning message to the gateway and blink LED
  }
  else if ( voltLevel == VOLT_LEVEL_BAD )
  {
    // Shut down the system
  }
}

/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

/*********************************************************************
 * @fn      zclRealApp_ProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  none
 */
static void zclRealApp_ProcessIncomingMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
      zclRealApp_ProcessInReadRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE_RSP:
      zclRealApp_ProcessInWriteRspCmd( pInMsg );
      break;
#endif
    case ZCL_CMD_CONFIG_REPORT:
    case ZCL_CMD_CONFIG_REPORT_RSP:
    case ZCL_CMD_READ_REPORT_CFG:
    case ZCL_CMD_READ_REPORT_CFG_RSP:
    case ZCL_CMD_REPORT:
      //bdb_ProcessIncomingReportingMsg( pInMsg );
      break;
      
    case ZCL_CMD_DEFAULT_RSP:
      zclRealApp_ProcessInDefaultRspCmd( pInMsg );
      break;
#ifdef ZCL_DISCOVER
    case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
      zclRealApp_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
      zclRealApp_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_RSP:
      zclRealApp_ProcessInDiscAttrsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
      zclRealApp_ProcessInDiscAttrsExtRspCmd( pInMsg );
      break;
#endif
    default:
      break;
  }

  if ( pInMsg->attrCmd )
    osal_mem_free( pInMsg->attrCmd );
}

#ifdef ZCL_READ
/*********************************************************************
 * @fn      zclRealApp_ProcessInReadRspCmd
 *
 * @brief   Process the "Profile" Read Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclRealApp_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8 i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
    // Notify the originator of the results of the original read attributes
    // attempt and, for each successfull request, the value of the requested
    // attribute
  }

  return ( TRUE );
}
#endif // ZCL_READ

#ifdef ZCL_WRITE
/*********************************************************************
 * @fn      zclRealApp_ProcessInWriteRspCmd
 *
 * @brief   Process the "Profile" Write Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclRealApp_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8 i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < writeRspCmd->numAttr; i++ )
  {
    // Notify the device of the results of the its original write attributes
    // command.
  }

  return ( TRUE );
}
#endif // ZCL_WRITE

/*********************************************************************
 * @fn      zclRealApp_ProcessInDefaultRspCmd
 *
 * @brief   Process the "Profile" Default Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclRealApp_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;

  // Device is notified of the Default Response command.
  (void)pInMsg;

  return ( TRUE );
}

#ifdef ZCL_DISCOVER
/*********************************************************************
 * @fn      zclRealApp_ProcessInDiscCmdsRspCmd
 *
 * @brief   Process the Discover Commands Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclRealApp_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverCmdsCmdRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverCmdsCmdRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numCmd; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

/*********************************************************************
 * @fn      zclRealApp_ProcessInDiscAttrsRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclRealApp_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsRspCmd_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}

/*********************************************************************
 * @fn      zclRealApp_ProcessInDiscAttrsExtRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Extended Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclRealApp_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsExtRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsExtRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return ( TRUE );
}
#endif // ZCL_DISCOVER

// 
void RealApp_HalKeyInit( void )
{
  /*  */
  halKeySavedKeys = 0;
  
  PUSH1_SEL &= ~(PUSH1_BV); /* Set pin function to GPIO */
  PUSH1_DIR &= ~(PUSH1_BV); /* Set pin direction to Input */
  
    PICTL |= PUSH1_EDGEBIT;
    PUSH1_ICTL |= PUSH1_ICTLBIT; /* Enable interrupt generation at the port */
    PUSH1_IEN |= PUSH1_IENBIT;   /* Enable CPU interrupt */
    PUSH1_PXIFG = ~(PUSH1_SBIT); 

  
  PUSH2_SEL &= ~(PUSH2_BV); /* Set pin function to GPIO */
  PUSH2_DIR &= ~(PUSH2_BV); /* Set pin direction to Input */
    
    PICTL |= PUSH2_EDGEBIT;  
    PUSH2_ICTL |= PUSH2_ICTLBIT; /* Enable interrupt generation at the port */
    PUSH2_IEN |= PUSH2_IENBIT;   /* Enable CPU interrupt */
    PUSH2_PXIFG = ~(PUSH2_SBIT); 
  
    /* Initialize callback function */
//  pHalKeyProcessFunction  = NULL;

  /* Start with key is not configured */
//  HalKeyConfigured = FALSE; 
  
  // Enable/Disable Interrupt or 
  Hal_KeyIntEnable = TRUE;
  
//  RealApp_HalKeyConfig (HAL_KEY_INTERRUPT_ENABLE, OnBoard_KeyCallback);
}

//void RealApp_HalKeyConfig (bool interruptEnable, halKeyCBack_t cback)
/*
void RealApp_HalKeyConfig (bool interruptEnable, RealApphalKeyCBack_t cback)
{
  // Enable/Disable Interrupt or 
  Hal_KeyIntEnable = interruptEnable;

  // Register the callback fucntion 
//  pHalKeyProcessFunction = cback;

  // Determine if interrupt is enable or not 
  if (Hal_KeyIntEnable)
  {
    // Rising/Falling edge configuratinn 

    PICTL &= ~(PUSH2_EDGEBIT);    // Clear the edge bit 
    // For falling edge, the bit must be set.
    #if (PUSH2_EDGE == HAL_KEY_FALLING_EDGE)
      PICTL |= PUSH2_EDGEBIT;
    #endif

    // Interrupt configuration:
     // - Enable interrupt generation at the port
     // - Enable CPU interrupt
     // - Clear any pending interrupt
     //
    PUSH2_ICTL |= PUSH2_ICTLBIT; // Enable interrupt generation at the port 
    PUSH2_IEN |= PUSH2_IENBIT;   // Enable CPU interrupt 
    PUSH2_PXIFG = ~(PUSH2_SBIT); 

    if (HalKeyConfigured == TRUE)
    {
      osal_stop_timerEx(Hal_TaskID, HAL_KEY_EVENT);  // Cancel polling if active 
    }
  }
  else    // Interrupts NOT enabled 
  {
    PUSH1_ICTL &= ~(PUSH1_ICTLBIT); // Disable interrupt generation at the port  
    PUSH1_IEN &= ~(PUSH1_IENBIT);   // Disable CPU interrupt 
    
    PUSH2_ICTL &= ~(PUSH2_ICTLBIT); // Disable interrupt generation at the port  
    PUSH2_IEN &= ~(PUSH2_IENBIT);   // Disable CPU interrupt 

    osal_set_event(Hal_TaskID, HAL_KEY_EVENT);
  }

  // Key now is configured 
  HalKeyConfigured = TRUE;

}
*/
// 
void RealApp_HalKeyPoll (void)
{
  uint8 keys = 0;
 
   /* If interrupts are not enabled, previous key status and current key status
   * are compared to find out if a key has changed status.
   */ 
  if (!Hal_KeyIntEnable)
  {
    if (keys == halKeySavedKeys)
    {
      /* Exit - since no keys have changed */
      return;
    }
    /* Store the current keys for comparation next time */
    halKeySavedKeys = keys;
    
  }
  else
  {
    /* Key interrupt handled here */
    //  1
    if (HAL_PUSH_BUTTON1())
    {
      keys |= HAL_KEY_SW_1;
    }
    //  2
    if (HAL_PUSH_BUTTON2())
    {
      keys |= HAL_KEY_SW_2;
    }

  }   
  // 
  OnBoard_SendKeys(keys, HAL_KEY_STATE_NORMAL);
  
//  if (pHalKeyProcessFunction)
//  {
//    (pHalKeyProcessFunction) (keys, HAL_KEY_STATE_NORMAL);
//  }
  
}

// 
static void zclRealApp_HandleKeys( byte shift, byte keys )
{
  if ( keys & HAL_KEY_SW_1 )
  {
    // new join/disable join
    osal_start_timerEx (zclRealApp_TaskID, RealApp_EVT_NEWJOIN, HAL_KEY_DEBOUNCE_VALUE);
  } 
  if ( keys & HAL_KEY_SW_2 )
  {
    // update relay end send Report OnOff
    updateRelay(RELAY_STATE == 0);
  }
}

// 
void updateRelay ( bool value )
{
  if (value) {
    RELAY_STATE = 1;
  } else {
    RELAY_STATE = 0;
  }
  // 
  osal_nv_write(NV_RealApp_RELAY_STATE_ID, 0, 1, &RELAY_STATE);
  // 
  applyRelay();
  // 
  zclRealApp_ReportOnOff();
}
  
// 
void applyRelay ( void )
{
  // 
  if (RELAY_STATE == 0) {
    // 
    HalLedSet ( HAL_LED_3, HAL_LED_MODE_OFF );    
  } else {
    // 
    HalLedSet ( HAL_LED_3, HAL_LED_MODE_ON );   
  }
  EpdtestRefresh(RELAY_STATE, time_now_s);  
}

// 
void zclRealApp_LeaveNetwork( void )
{
  zclRealApp_ResetAttributesToDefaultValues();
  
  NLME_LeaveReq_t leaveReq;
  // Set every field to 0
  osal_memset(&leaveReq, 0, sizeof(NLME_LeaveReq_t));

  // This will enable the device to rejoin the network after reset.
  leaveReq.rejoin = FALSE;

  // Set the NV startup option to force a "new" join.
  zgWriteStartupOptions(ZG_STARTUP_SET, ZCD_STARTOPT_DEFAULT_NETWORK_STATE);

  // Leave the network, and reset afterwards
  if (NLME_LeaveReq(&leaveReq) != ZSuccess) {
    // Couldn't send out leave; prepare to reset anyway
    ZDApp_LeaveReset(FALSE);
  }
}

// 
static void zclRealApp_OnOffCB(uint8 cmd)
{
  // 
  // 
  afIncomingMSGPacket_t *pPtr = zcl_getRawAFMsg();
  zclRealApp_DstAddr.addr.shortAddr = pPtr->srcAddr.addr.shortAddr;
  
  // 
  if (cmd == COMMAND_ON) {
    updateRelay(TRUE);
  }
  // 
  else if (cmd == COMMAND_OFF) {
    updateRelay(FALSE);
  }
  // 
  else if (cmd == COMMAND_TOGGLE) {
    updateRelay(RELAY_STATE == 0);
  }
}

// Report On/Off
void zclRealApp_ReportOnOff(void) {
  const uint8 NUM_ATTRIBUTES = 1;

  zclReportCmd_t *pReportCmd;

  pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) +
                              (NUM_ATTRIBUTES * sizeof(zclReport_t)));
  if (pReportCmd != NULL) {
    pReportCmd->numAttr = NUM_ATTRIBUTES;

    pReportCmd->attrList[0].attrID = ATTRID_ON_OFF;
    pReportCmd->attrList[0].dataType = ZCL_DATATYPE_BOOLEAN;
    pReportCmd->attrList[0].attrData = (void *)(&RELAY_STATE);

    zclRealApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    zclRealApp_DstAddr.addr.shortAddr = 0;
    zclRealApp_DstAddr.endPoint = 1;

    zcl_SendReportCmd(RealApp_ENDPOINT, &zclRealApp_DstAddr,
                      ZCL_CLUSTER_ID_GEN_ON_OFF, pReportCmd,
                      ZCL_FRAME_CLIENT_SERVER_DIR, false, SeqNum++);
  }

  osal_mem_free(pReportCmd);
}

// Report Battery Voltage
void zclRealApp_ReportBatteryVoltage( void )
{
  // Read Battery Voltage
  uint16 voltage16 = readBatteryVoltage16();          // 16-bit
  zclRealApp_BatteryVoltage = (uint8)(voltage16/100);
  
  if (zclRealApp_BatteryVoltage >=27 && zclRealApp_BatteryVoltage <= 30)
  {
    float percent = ((voltage16 - 2700)/ 3.0);
    zclRealApp_BatteryPercentageRemaining = (uint8)percent*2;
  }
  if (zclRealApp_BatteryVoltage < 27)
  {
    zclRealApp_BatteryPercentageRemaining = 0;
  }
  if (zclRealApp_BatteryVoltage > 30)
  {
    zclRealApp_BatteryPercentageRemaining = 200;
  }
   
  const uint8 NUM_ATTRIBUTES = 2;

  zclReportCmd_t *pReportCmd;

  pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) +
                              (NUM_ATTRIBUTES * sizeof(zclReport_t)));
  if (pReportCmd != NULL) {
    pReportCmd->numAttr = NUM_ATTRIBUTES;

    pReportCmd->attrList[0].attrID = ATTRID_POWER_CFG_BATTERY_VOLTAGE;
    pReportCmd->attrList[0].dataType = ZCL_DATATYPE_UINT8;
    pReportCmd->attrList[0].attrData = (void *)(&zclRealApp_BatteryVoltage);
    
    pReportCmd->attrList[1].attrID = ATTRID_POWER_CFG_BATTERY_PERCENTAGE_REMAINING;
    pReportCmd->attrList[1].dataType = ZCL_DATATYPE_UINT8;
    pReportCmd->attrList[1].attrData = (void *)(&zclRealApp_BatteryPercentageRemaining);

    zclRealApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    zclRealApp_DstAddr.addr.shortAddr = 0;
    zclRealApp_DstAddr.endPoint = 1;

    zcl_SendReportCmd(RealApp_ENDPOINT, &zclRealApp_DstAddr,
                      ZCL_CLUSTER_ID_GEN_POWER_CFG, pReportCmd,
                      ZCL_FRAME_CLIENT_SERVER_DIR, false, SeqNum++);
  }

  osal_mem_free(pReportCmd);
}

int16 readBatteryVoltage16(void)
{
  float result = 0;
  HalAdcSetReference ( HAL_ADC_REF_125V );
  // for 2.5V ADC 6200
  result = 2500+((HalAdcRead (HAL_ADC_CHANNEL_VDD, HAL_ADC_RESOLUTION_14)-6200)/2.56);
//  result = HalAdcRead (HAL_ADC_CHANNEL_VDD, HAL_ADC_RESOLUTION_14);
  LREP("Battery voltage=%d\r\n", (uint16)result);
  return (uint16)result;
}

void zclRealApp_ReportTemp( void )
{
  zclRealApp_TemperatureMeasuredValue = readTemperature();
//  zclRealApp_TemperatureMeasuredValue = readBatteryVoltage16();
  const uint8 NUM_ATTRIBUTES = 1;

  zclReportCmd_t *pReportCmd;

  pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) +
                              (NUM_ATTRIBUTES * sizeof(zclReport_t)));
  if (pReportCmd != NULL) {
    pReportCmd->numAttr = NUM_ATTRIBUTES;

    pReportCmd->attrList[0].attrID = ATTRID_MS_TEMPERATURE_MEASURED_VALUE;
    pReportCmd->attrList[0].dataType = ZCL_DATATYPE_INT16;
    pReportCmd->attrList[0].attrData = (void *)(&zclRealApp_TemperatureMeasuredValue);

    zclRealApp_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    zclRealApp_DstAddr.addr.shortAddr = 0;
    zclRealApp_DstAddr.endPoint = 1;

    zcl_SendReportCmd(RealApp_ENDPOINT, &zclRealApp_DstAddr,
                      ZCL_CLUSTER_ID_MS_TEMPERATURE_MEASUREMENT, pReportCmd,
                      ZCL_FRAME_CLIENT_SERVER_DIR, false, SeqNum++);
  }

  osal_mem_free(pReportCmd);
}

int16 readTemperature(void)
{
  float result = 0;
  HalAdcSetReference ( HAL_ADC_REF_125V );
  uint16 volt = readBatteryVoltage16();
  TR0 = 1;      
  ATEST = 1;    
//  for power 3.3 V  25.30°C ADC 6274 + compensation battery 0.40°C for 0.1V
  result = 2530+((HalAdcRead (HAL_ADC_CHANNEL_TEMP, HAL_ADC_RESOLUTION_14)-6274)*10) + ((33-(uint16)(volt/100))*40);
  ATEST = 0;      
  TR0 = 0;        
  return (uint16)result;
}

/*
#pragma vector=P1INT_VECTOR 
__interrupt void IRQ_P1INT(void)
{
  if (P1IFG & (1<<3))	// Interrupt flag for P2.0
  {
    _delay_ms(20);
    if (!P1_3)
    {
    updateRelay(RELAY_STATE == 0);
//      halProcessKeyInterrupt();
  
    }
  }
  // Clear interrupt flags
//  PUSH1_PXIFG = 0;
//  HAL_KEY_CPU_PORT_1_IF = 0;
    P1IFG &= ~(1<<3);	// Clear Interrupt flag for P1.3 (KEY)
    IRCON2 &= ~(1<<3);	// Clear Interrupt flag for Port 1
}
*/
void _delay_us(uint16 microSecs)
{
  while(microSecs--)
  {
    asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
  }
}

void _delay_ms(uint16 milliSecs)
{
  while(milliSecs--)
  {
    _delay_us(1000);
  }

}

/**************************************************************************************************
 * @fn      halProcessKeyInterrupt
 *
 * @brief   Checks to see if it's a valid key interrupt, saves interrupt driven key states for
 *          processing by HalKeyRead(), and debounces keys by scheduling HalKeyRead() 25ms later.
 *
 * @param
 *
 * @return
 **************************************************************************************************/
void halProcessKeyInterrupt (void)
{
    osal_start_timerEx (zclRealApp_TaskID, HAL_KEY_EVENT, HAL_KEY_DEBOUNCE_VALUE);    
}


/**************************************************************************************************
 * @fn      HalKeyEnterSleep
 *
 * @brief  - Get called to enter sleep mode
 *
 * @param
 *
 * @return
 **************************************************************************************************/
void HalKeyEnterSleep ( void )
{
}

/**************************************************************************************************
 * @fn      HalKeyExitSleep
 *
 * @brief   - Get called when sleep is over
 *
 * @param
 *
 * @return  - return saved keys
 **************************************************************************************************/
uint8 HalKeyExitSleep ( void )
{
  /* Wake up and read keys */
  return ( HalKeyRead () );
}

/***************************************************************************************************
 *                                    INTERRUPT SERVICE ROUTINE
 ***************************************************************************************************/

/**************************************************************************************************
 * @fn      halKeyPort0Isr
 *
 * @brief   Port0 ISR
 *
 * @param
 *
 * @return
 **************************************************************************************************/

HAL_ISR_FUNCTION( halKeyPort2Isr, P2INT_VECTOR )
{ 
  _delay_ms(20); 
  HAL_ENTER_ISR();
  
  if (PUSH2_PXIFG)
  {
    if (!PUSH2_SBIT)
    {
      halProcessKeyInterrupt();
      LREPMaster("PUSH2\r\n");
    }
  }
 
  //  Clear the CPU interrupt flag for Port_0
  //  PxIFG has to be cleared before PxIF
  
  PUSH2_PXIFG = ~(PUSH2_BV);
  HAL_KEY_CPU_PORT_2_IF = 0;
  
  CLEAR_SLEEP_MODE();
  HAL_EXIT_ISR();
}

HAL_ISR_FUNCTION( halKeyPort1Isr, P1INT_VECTOR )
{ 
  _delay_ms(20); 
  HAL_ENTER_ISR();

  if (PUSH1_PXIFG)
  {
    if (!PUSH1_SBIT)
    {
      halProcessKeyInterrupt();
      LREPMaster("PUSH1\r\n");
    }
  }
  
  //  Clear the CPU interrupt flag for Port_0
  //  PxIFG has to be cleared before PxIF
  
  PUSH1_PXIFG = ~(PUSH1_BV);
  HAL_KEY_CPU_PORT_1_IF = 0;
  
  CLEAR_SLEEP_MODE();
  HAL_EXIT_ISR();
}

HAL_ISR_FUNCTION( halKeyPort0Isr, P0INT_VECTOR )
{ 
  _delay_ms(20); 
  HAL_ENTER_ISR();
  
  if (PUSH1_PXIFG)
  {
    if (!PUSH1_SBIT)
    {
      halProcessKeyInterrupt();
      LREPMaster("PUSH1\r\n");
    }
  }
  
  //  Clear the CPU interrupt flag for Port_0
  //  PxIFG has to be cleared before PxIF
  
  PUSH1_PXIFG = ~(PUSH1_BV);
  HAL_KEY_CPU_PORT_0_IF = 0;
  
  CLEAR_SLEEP_MODE();
  HAL_EXIT_ISR();
}

void RealApp_OnBoard_KeyCallback ( uint8 keys, uint8 state )
{
}