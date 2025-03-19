/**
  ******************************************************************************
  * @file    st87ec_engine.c
  * @author  APMS Application Team
  * @brief   EC lib engine functions
  *
  @verbatim
  @endverbatim
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics International N.V.
  * All rights reserved.
  *
  ******************************************************************************
  */

#include <string.h>
#include "../inc/st87ec_engine.h"
#include "../inc/st87ec_wrapper.h"
#include "../inc/st87ec_sequence_sys.h"
#include "../inc/st87ec_sequence_gnss.h"
#include "../inc/st87ec_sequence_nbiot.h"

#include "../inc/st87ec_sequence_wmbus.h"

/* Constants -----------------------------------------------------------------*/
#if GNSS_CONSTELLATION_ID == 0 /* GPS */
#define TAG_PREFIX "$GP"
#elif GNSS_CONSTELLATION_ID == 1 /* GALILEO */
#define TAG_PREFIX "$GA"
#elif GNSS_CONSTELLATION_ID == 2 /* Combined GALILEO + GPS */
#define TAG_PREFIX "$GN"
#endif

/* Global variables --------------------------------------------------------*/
ST87EC_Lib_LocalStatus_t EcLibVars;

/* URC information table */
const ST87EC_Lib_RspTab_t EcLibRspTab[MAX_NB_OF_RSP] =
{
    [ SIMST_INDEX       ] = { .pTag = "#SIMST"        , .MaxDataSize = SIMST_MAX_DATA_SIZE        },
    [ CCLK_INDEX        ] = { .pTag = "+CCLK"         , .MaxDataSize = CCLK_MAX_DATA_SIZE         },
    [ CSCON_INDEX       ] = { .pTag = "+CSCON"        , .MaxDataSize = CSCON_MAX_DATA_SIZE        },
    [ CEREG_INDEX       ] = { .pTag = "+CEREG"        , .MaxDataSize = CEREG_MAX_DATA_SIZE        },
    [ SLEEP_INDEX       ] = { .pTag = "#SLEEP"        , .MaxDataSize = SLEEP_MAX_DATA_SIZE        },
    [ WAKEUP_INDEX      ] = { .pTag = "#WAKEUP"       , .MaxDataSize = WAKEUP_MAX_DATA_SIZE       },
    [ GNSS_INIT_INDEX   ] = { .pTag = "#GNSSINIT"     , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
#if GNSS_FORMAT_TYPE == 1 /* NMEA format */
    [ GNSS_GGA_INDEX    ] = { .pTag = TAG_PREFIX"GGA" , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
    [ GNSS_GSV_INDEX    ] = { .pTag = TAG_PREFIX"GSV" , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
    [ GNSS_GSA_INDEX    ] = { .pTag = TAG_PREFIX"GSA" , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
    [ GNSS_GLL_INDEX    ] = { .pTag = TAG_PREFIX"GLL" , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
    [ GNSS_RMC_INDEX    ] = { .pTag = TAG_PREFIX"RMC" , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
    [ GNSS_VTG_INDEX    ] = { .pTag = TAG_PREFIX"VTG" , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
#elif GNSS_FORMAT_TYPE == 0 /* ST format */
    [ GNSS_FIX_INDEX    ] = { .pTag = "#GNSSFIX"      , .MaxDataSize = GNSS_MAX_DATA_SIZE         },
#endif /* GNSS_FORMAT_TYPE */
    [CME_ERROR_INDEX    ] = { .pTag = "+CME"          , .MaxDataSize = CME_ERROR_MAX_DATA_SIZE    },
    [IPCFG_INDEX        ] = { .pTag = "#IPCFG"        , .MaxDataSize = IPCFG_MAX_DATA_SIZE        },
    [SOCKETCREATE_INDEX ] = { .pTag = "#SOCKETCREATE" , .MaxDataSize = SOCKETCREATE_MAX_DATA_SIZE },
    [IPRECV_INDEX       ] = { .pTag = "#IPRECV"       , .MaxDataSize = IPRECV_MAX_DATA_SIZE       },
    [IPREAD_INDEX       ] = { .pTag = "#IPREAD"       , .MaxDataSize = IPREAD_MAX_DATA_SIZE       },
    [MQTTCONNECT_INDEX  ] = { .pTag = "#MQTTCONNECT"  , .MaxDataSize = MQTTCONNECT_MAX_DATA_SIZE  },
    [NVMREAD_INDEX      ] = { .pTag = "#NVMRD"        , .MaxDataSize = NVMREAD_MAX_DATA_SIZE      },
    [TCPCONN_INDEX      ] = { .pTag = "#TCPCONNECT"   , .MaxDataSize = TCPCONN_MAX_DATA_SIZE      },
    [COAPSTART_INDEX    ] = { .pTag = "#COAPSTART"    , .MaxDataSize = COAP_MAX_DATA_SIZE         },
    [COAPSEND_INDEX     ] = { .pTag = "#COAPSEND"     , .MaxDataSize = COAP_MAX_DATA_SIZE         },
    [COAPDATA_INDEX     ] = { .pTag = "#COAPRECV"     , .MaxDataSize = COAP_MAX_DATA_SIZE         },
    [HTTPSTART_INDEX    ] = { .pTag = "#HTTPSTART"    , .MaxDataSize = HTTP_MAX_DATA_SIZE         },
    [HTTPRECV_INDEX     ] = { .pTag = "#HTTPRECV"     , .MaxDataSize = HTTPRECV_MAX_DATA_SIZE     },
    [DNS_INDEX          ] = { .pTag = "#DNS"          , .MaxDataSize = DNS_MAX_DATA_SIZE          },
    [HTTPREAD_INDEX     ] = { .pTag = "#HTTPREAD"     , .MaxDataSize = HTTPREAD_MAX_DATA_SIZE     },
    [WMBUS_INIT_INDEX   ] = { .pTag = "#WMBUS_INIT"   , .MaxDataSize = WMBUS_INIT_MAX_DATA_SIZE   },
    [WMBUS_SENT_INDEX   ] = { .pTag = "#WMBUS_SENT"   , .MaxDataSize = WMBUS_SENT_MAX_DATA_SIZE   },
};

/* Private variables ---------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static ST87EC_Lib_Result_t ST87EC_Lib_HandleLocalStatus(uint32_t Index);
static ST87EC_Lib_Result_t ST87EC_Lib_RspTableInit(void);
static inline void ST87EC_Lib_ExtraSequenceProcessHandling(void);

/* Exported functions --------------------------------------------------------*/

/**
* @brief EC library main sequence initialization
*
* @param : None
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_Lib_SequenceMainInit(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  EcLibVars.ModuleStatus.InitComplete = INIT_NOT_COMPLETE;
  EcLibVars.ModuleStatus.ConnectionStatus = CONN_STATUS_UNKNOWN;
  EcLibVars.ModuleStatus.SimStatus = SIM_STATUS_UNKNOWN;
  EcLibVars.ModuleStatus.RegistrationStatus = NOT_REGISTERED;
  EcLibVars.ModuleStatus.SleepWakeupstatus = STATUS_WAKEUP;
  EcLibVars.ModuleStatus.NbUdpPacketsSent = 0;
  EcLibVars.ModuleStatus.TransferOnGoing = 0;
  EcLibVars.OnGoingSequence = SEQUENCE_NONE;
  EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
  EcLibVars.SequenceNbiot.TransferCfg.SecProfileId = (-1);
  EcLibVars.SequenceNbiot.CoapTransfer.FsmState = COAP_TRANSFER_NONE;
  EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState = COAP_RX_STATE_NONE;
  EcLibVars.SequenceNbiot.HttpTransfer.FsmState = HTTP_TRANSFER_NONE;

  /* Initialize the URC/RSP table */
  result = ST87EC_Lib_RspTableInit();

  /* Register to URCs needed from startup */
  EcLibVars.RspInfo[SIMST_INDEX].Validity = RSP_AWAITED;
  EcLibVars.RspInfo[CEREG_INDEX].Validity = RSP_AWAITED;
  EcLibVars.RspInfo[CSCON_INDEX].Validity = RSP_AWAITED;
  EcLibVars.RspInfo[SLEEP_INDEX].Validity = RSP_AWAITED;
  EcLibVars.RspInfo[WAKEUP_INDEX].Validity = RSP_AWAITED;
  EcLibVars.RspInfo[IPCFG_INDEX].Validity = RSP_AWAITED;
  /* Default set ST87 messages raw data reception to disabled */
  EcLibVars.RawDataRsp.ExpectedLength = 0;

  EcLibVars.BootMode = NORMAL_MODE;
  EcLibVars.BootIf.SendAtToBootStep = LOADER_AT_STEP_NONE;

  return (result);
}


/**
 * @brief  Compute the hash code of a string provided to the function.
 *
 * @param  pString:  The string to hash
 * @param  Len:  Length of the string to hash
 * @retval hash computed.
 */
uint32_t ST87EC_Lib_ComputeHash(const char * pString, uint16_t Len)
{
  uint32_t hash = 5381;
  uint8_t i = 0;
  char c;

  while(i<Len)
  {
    i++;
    c = *pString++;
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }

  return hash;
}


/**
* @brief Main Sequence handling
*
* @retval Result: Function execution status
*/
ST87EC_Lib_Result_t ST87EC_Lib_SequenceMain(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;
  ST87EC_Lib_SequenceValue_t ongoing_sequence = EcLibVars.OnGoingSequence; /* save the seq. id in case it is flushed within seq. execution (e.g. due to error) */

  ST87EC_Lib_ExtraSequenceProcessHandling();

  /* Call the requested sequence */
  switch (EcLibVars.OnGoingSequence)
  {
    case SEQUENCE_NONE:
      break;

    case SEQUENCE_COLD_INIT:
      result = ST87EC_SequenceSys_ColdParamInit();
      break;

    case SEQUENCE_GET_TIME:
      result = ST87EC_SequenceSys_GetTime();
      break;

    case SEQUENCE_GNSS_GET_FIX:
      result = ST87EC_SequenceGNSS_GetFix();
      break;

    case SEQUENCE_NBIOT_MQTT_PUBLISH:
      result =  ST87EC_SequenceNBIOT_MqttPublish();
      break;

    case SEQUENCE_UDP_TRANSFER:
    case SEQUENCE_TCP_TRANSFER:
      result = ST87EC_SequenceNBIOT_UdpTcp();
      break;

    case SEQUENCE_COAP_OPEN:
      result = ST87EC_SequenceNBIOT_CoapOpen();
      break;

    case SEQUENCE_COAP_RXTX:
      result = ST87EC_SequenceNBIOT_CoapTransmit();
      break;

    case SEQUENCE_COAP_CLOSE:
      result = ST87EC_SequenceNBIOT_CoapClose();
      break;

    case SEQUENCE_HTTP_OPEN:
      result = ST87EC_SequenceNBIOT_HttpOpen();
      break;

    case SEQUENCE_HTTP_TRANSFER:
      result = ST87EC_SequenceNBIOT_HttpTransfer();
      break;

    case SEQUENCE_HTTP_CLOSE:
      result = ST87EC_SequenceNBIOT_HttpClose();
      break;

    case SEQUENCE_WMBUS_TRANSFER:
      result = ST87EC_SequenceWmbus_Transfer();
      break;

    case SEQUENCE_LOADER:
      result = ST87EC_SequenceSys_Loader();
      break;

    default:
      break;
  }

  if (EcLibVars.SequenceNbiot.CoapTransfer.FsmState == COAP_TRANSFER_RXTX)
  {
    result = ST87EC_SequenceNBIOT_CoapReceive();
  }

  if ((result != RESULT_OK) && (EcLibVars.pGenericErrorCallback != NULL))
  {
    EcLibVars.pGenericErrorCallback(ongoing_sequence, ERROR_NO_CODE);
  }

  return result;
}


/**
* @brief EC library function that indicates the reception of specific response
*
* @param Response: type of specific response received
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_Lib_HandleSpecificRsp(ST87EC_Lib_SpecificResponse_t Response)
{
  ST87EC_Lib_Result_t result = RESULT_OK;
  EcLibVars.RspReceived = Response;

  return result;
}

/**
* @brief EC library function that updates the RspInfo[] table and
* copy the URC/RSP data to EcLibVars.RspData[] table when a URC/RSP arrives.
*
* @param pTagData: pointer to the URC/RSP tag
* @param RspTagLen: Length of the URC/RSP tag
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_Lib_HandleGenericRsp(char * pTagData, uint16_t RspTagLen)
{
  ST87EC_Lib_Result_t result = RESULT_OK;
  uint32_t hash, i, rsp_index;

  /* Compute the Hash of the Tag */
  hash = ST87EC_Lib_ComputeHash(pTagData, RspTagLen);

  /* Look for offset entry in Response table */
  for (i= 0; i< MAX_NB_OF_RSP; i++)
  {
    if (EcLibVars.RspInfo[i].Validity == RSP_AWAITED)
    {
      if (EcLibVars.RspInfo[i].Hash == hash)
      {
        rsp_index = i;
        break;
      }
    }
  }

  if (i >= MAX_NB_OF_RSP)
  {
    /* offset not found in table */
    result = RESULT_KO;
  }
  else
  {
    /* Copy Data into the response table */
    i = ST87EC_Wrapper_GetSt87CurrRxMsg(EcLibVars.RspData, EcLibRspTab[rsp_index].MaxDataSize);
    EcLibVars.RspData[i] = '\0';

    /* Update validity */
    EcLibVars.RspInfo[rsp_index].Validity = RSP_RECEIVED;

    /* Update Local Status if needed */
    ST87EC_Lib_HandleLocalStatus(rsp_index);
  }

  return result;
}

/**
* @brief Trig Cold parameter initialization sequence
*
* @param : None
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_Lib_TrigColdParamInit(void)
{
  ST87EC_Lib_Result_t result;

  if (EcLibVars.OnGoingSequence == SEQUENCE_NONE)
  {
    /* Register the request in the EC library */
    EcLibVars.OnGoingSequence = SEQUENCE_COLD_INIT;
    EcLibVars.SequenceSys.ColdInit.SequenceIndex = 0;
    EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_INIT;
    result = RESULT_OK;
  }
  else
  {
    result = RESULT_BUSY;
  }

  return(result);
}

/**
* @brief EC library function that indicates the reception of a raw data response
*
* @param RawRspLen: data length of the raw response received
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_Lib_HandleRawRsp(uint16_t RawRspLen)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  if (EcLibVars.RawDataRsp.ExpectedLength > 0)
  {
    uint32_t recvNum = 0;

    if (EcLibVars.RawDataRsp.ExpectedLength > ST87EC_RAW_BUFFER_SIZE)
    {
      EcLibVars.RawDataRsp.ExpectedLength = ST87EC_RAW_BUFFER_SIZE;
    }

    /* Copy raw using given handle/length */
    recvNum = ST87EC_Wrapper_GetSt87CurrRxMsg(
        &EcLibVars.RawDataRsp.Data[EcLibVars.RawDataRsp.ReceivedLength],
        EcLibVars.RawDataRsp.ExpectedLength);

    /* Set back ST87 messages raw data reception to disabled */
    EcLibVars.RawDataRsp.ExpectedLength -= recvNum;
    EcLibVars.RawDataRsp.ReceivedLength += recvNum;
  }

  return(result);
}



/* Private functions --------------------------------------------------------*/


/**
* @brief EC library function initializes the UrcInfo[] table
*
* @retval Function execution status
*/
static ST87EC_Lib_Result_t ST87EC_Lib_RspTableInit(void)
{
  uint32_t i;
  ST87EC_Lib_Result_t result = RESULT_OK;

  for (i=0; i< MAX_NB_OF_RSP; i++)
  {
    const ST87EC_Lib_RspTab_t * p_item = &EcLibRspTab[i];

    EcLibVars.RspInfo[i].Validity = RSP_NONE;
    EcLibVars.RspInfo[i].Hash = ST87EC_Lib_ComputeHash(p_item->pTag,strlen(p_item->pTag));
  }

  memset((uint8_t *)EcLibVars.RspData, 0, TOTAL_DATA_LENGTH);

  return (result);
}


/**
* @brief EC library function that fills the Module status when an URC arrives
*
* @param Index: index in the EcLibVars.RspData[] table.
* @retval Function execution status
*/
static ST87EC_Lib_Result_t ST87EC_Lib_HandleLocalStatus(uint32_t Index)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  switch (Index)
  {
    case SIMST_INDEX:
      /* Check last data of the URC */
      if (EcLibVars.RspData[SIMST_MAX_DATA_SIZE - 2] == '1')
      {
        EcLibVars.ModuleStatus.SimStatus = SIM_STATUS_SIM_VALID;
        EcLibVars.ModuleStatus.InitComplete = INIT_COMPLETE;
      }
      else
      {
        EcLibVars.ModuleStatus.SimStatus = SIM_STATUS_SIM_INVALID;
      }
      break;

    case CSCON_INDEX:
      /* Check last data of the response */
      if (EcLibVars.RspData[CSCON_MAX_DATA_SIZE - 2] == '1')
      {
        EcLibVars.ModuleStatus.ConnectionStatus = CONN_STATUS_CONNECTED;
      }
      else
      {
        EcLibVars.ModuleStatus.ConnectionStatus = CONN_STATUS_IDLE;
      }
      EcLibVars.RspInfo[CSCON_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
      break;

    case CEREG_INDEX:
       /* Check last data of the response */
       if ((EcLibVars.RspData[CEREG_MAX_DATA_SIZE - 2] == '1')     /* 1 = registered, home network */
           || (EcLibVars.RspData[CEREG_MAX_DATA_SIZE - 2] == '5')) /* 5 = registered, roaming      */
       {
         EcLibVars.ModuleStatus.RegistrationStatus = REGISTERED;
       }
       else
       {
         EcLibVars.ModuleStatus.RegistrationStatus = NOT_REGISTERED;
       }
       EcLibVars.RspInfo[CEREG_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
       break;

    case SLEEP_INDEX:
      EcLibVars.ModuleStatus.SleepWakeupstatus = STATUS_SLEEP;
      EcLibVars.RspInfo[WAKEUP_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
      break;

    case WAKEUP_INDEX:
      EcLibVars.ModuleStatus.SleepWakeupstatus = STATUS_WAKEUP;
      EcLibVars.RspInfo[SLEEP_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
      break;

    case IPCFG_INDEX:
      EcLibVars.SequenceNbiot.TransferCfg.contextId = EcLibVars.RspData[8] - CHAR_OFFSET; /* Translate to decimal value */
      EcLibVars.SequenceNbiot.TransferCfg.ipMode = EcLibVars.RspData[IPCFG_MAX_DATA_SIZE - 4] - CHAR_OFFSET;
      break;

    default:
      break;
  }
  return result;
}


/**
* @brief EC Engine function that handles processes running out of any sequences
*
*/
static inline void ST87EC_Lib_ExtraSequenceProcessHandling(void)
{
  /* Handles centralized Wakeup/Sleep of ST87 module */
  if (EcLibVars.OnGoingSequence != SEQUENCE_NONE)
  {
    if (EcLibVars.PreviousSequence == SEQUENCE_NONE)
    {
      /* Wake up the module when new sequence */
      ST87EC_WAKEUP_MODULE();
      EcLibVars.PreviousSequence = EcLibVars.OnGoingSequence; 
      EcLibVars.ErrorOccurredInSeq = 0; /* reset error indication at start of seq */
    }
  }
  else
  {
    if (EcLibVars.PreviousSequence != SEQUENCE_NONE)
    {
      /* Module in sleep at the end of the sequence */
      ST87EC_MODULE_SLEEP();
      EcLibVars.PreviousSequence = EcLibVars.OnGoingSequence;
    }
  }

  /* Handles UDP transfer 'NB_SENT' async. URC if received */
  if (EcLibVars.RspReceived == SPECIFIC_RSP_NBSENT)
  {
    EcLibVars.ModuleStatus.NbUdpPacketsSent++;
    EcLibVars.RspReceived = SPECIFIC_RSP_NONE;  /* clear to get ready for next specific URC detection */
  }

  /* Handles ST87 watchdog '#REBOOT_WD' async. URC if received */
  if ((EcLibVars.RspReceived == SPECIFIC_RSP_WATCHDOG) && (EcLibVars.pGenericErrorCallback != NULL))
  {
    EcLibVars.pGenericErrorCallback(EcLibVars.OnGoingSequence, ERROR_ST87_WATCHDOG);
    EcLibVars.RspReceived = SPECIFIC_RSP_NONE;  /* clear to get ready for next specific URC detection */
  }

}



/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
