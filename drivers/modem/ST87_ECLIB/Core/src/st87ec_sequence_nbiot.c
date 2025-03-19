/**
  ******************************************************************************
  * @file    st87ec_sequence_nbiot.c
  * @author  APMS Application Team
  * @brief   Interface header file
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
#include <stdio.h>
#include <stdlib.h>
#include "../inc/st87ec_sequence_nbiot.h"
#include "../../Cust/inc/st87ec_config.h"
#include "../inc/st87ec_wrapper.h"

/* Constants -----------------------------------------------------------------*/
#define ST87EC_MQTT_NOT_CONNECT_STATUS ('0')
#define ST87EC_MQTT_CONNECT_INIT_STATUS (-1)

#define ST87EC_TCPCONN_NOT_CONNECTED_STATUS ('3')
#define ST87EC_TCPCONN_CONNECTING_STATUS    ('4')
#define ST87EC_TCPCONN_CONNECTED_STATUS     ('5')
#define ST87EC_TCPCONN_WAIT_CLOSE_STATUS    ('6')

#define ST87EC_LF_ASCII_CODE                (0x0A)   /* Line Feed ('\n')       */
#define ST87EC_CR_ASCII_CODE                (0x0D)   /* Carriage Return ('\r') */
#define ST87EC_SPACE_ASCII_CODE             (0x20)   /* Space (' ')            */
#define ST87EC_NULL_ASCII_CODE              (0x00)   /* Null ('\0')            */

#define ST87EC_FORMAT_MQTT_CFG_CMD(CN, CTO, PTO, PR, KA) "AT#MQTTCFG=" CN "," STR(CTO) "," STR(PTO) "," STR(PR) "," STR(KA)
#define ST87EC_MQTT_CFG_CMD ST87EC_FORMAT_MQTT_CFG_CMD(MQTTCFG_CLIENT_NAME, MQTTCFG_CONNEXION_TIMEOUT,\
    MQTTCFG_PROTOCOL_TIMEOUT, MQTTCFG_PUBLISH_RETRY, MQTTCFG_KEEP_ALIVE_PB_MSG)

#define ST87EC_FORMAT_MQTT_CON_CMD(CI, SI, BA, BP) "AT#MQTTCONNECT=%d,%d,%s,%d",CI,SI,BA,BP
#define ST87EC_MQTT_CON_CMD ST87EC_FORMAT_MQTT_CON_CMD( \
    EcLibVars.SequenceNbiot.TransferCfg.contextId,\
    EcLibVars.SequenceNbiot.TransferCfg.socketId, \
    EcLibVars.SequenceNbiot.MqttPublish.Params.BrokerAddress,\
    EcLibVars.SequenceNbiot.MqttPublish.Params.BrokerPort)

#define ST87EC_FORMAT_MQTT_PUB_CMD(TOPIC,MESSAGE) "AT#MQTTPUB=%s,%s,1",TOPIC,MESSAGE
#define ST87EC_MQTT_PUB_CMD ST87EC_FORMAT_MQTT_PUB_CMD( \
    EcLibVars.SequenceNbiot.MqttPublish.Params.Topic,\
    EcLibVars.SequenceNbiot.MqttPublish.Params.Message)

#define ST87EC_FORMAT_COAP_CFG_CMD(CID,SID,IPADDR,PORT) "AT#COAPCFG=%d,%d,%s,%d",CID,SID,IPADDR,PORT
#define ST87EC_COAP_CFG_CMD ST87EC_FORMAT_COAP_CFG_CMD( \
    EcLibVars.SequenceNbiot.TransferCfg.contextId, \
    EcLibVars.SequenceNbiot.TransferCfg.socketId, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.pIpAddress, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.PortNb)

#define ST87EC_FORMAT_COAP_SEND_DATA_CMD(MTY,MTH,MID,TOK,PAT,TDF,PLD) "AT#COAPSEND=%d,%d,%d,%s,%s,%d,%s",MTY,MTH,MID,TOK,PAT,TDF,PLD
#define ST87EC_COAP_SEND_DATA_CMD ST87EC_FORMAT_COAP_SEND_DATA_CMD( \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.MessageType, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.Method, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.Mid, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.pToken, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.pPath, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.TxDataFormat, \
    EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.pPayload)

#define ST87EC_FORMAT_DNS_CMD(CI, HOS) "AT#DNS=%d,0,%s",CI,HOS
#define ST87EC_DNS_CMD ST87EC_FORMAT_DNS_CMD( \
    EcLibVars.SequenceNbiot.TransferCfg.contextId,\
    EcLibVars.SequenceNbiot.HttpTransfer.Params.pHost)

#define ST87EC_FORMAT_HTTP_METHOD_CMD(ME,HO,PA,KA) "AT#HTTPMETHOD=%s,%s,%s,%d",ME,HO,PA,KA
#define ST87EC_HTTP_METHOD_CMD ST87EC_FORMAT_HTTP_METHOD_CMD( \
    EcLibVars.SequenceNbiot.HttpTransfer.Params.pMethod,\
    EcLibVars.SequenceNbiot.HttpTransfer.Params.pHost,\
    EcLibVars.SequenceNbiot.HttpTransfer.Params.pPath,\
        EcLibVars.SequenceNbiot.HttpTransfer.Params.KeepAlive)

#define ST87EC_SEEKDATA                                                     \
    while (EcLibVars.RspData[idx] != ',')                                   \
    {                                                                       \
      idx++;                                                                \
    }                                                                       \
    idx++;

#define ST87EC_COAP_RCV_CALLBACK_FUNC(pDATAsTRUCT)                                       \
    if (EcLibVars.SequenceNbiot.CoapTransfer.Params.pCoapReceiveCallbackFunc != NULL)    \
    {                                                                                    \
      EcLibVars.SequenceNbiot.CoapTransfer.Params.pCoapReceiveCallbackFunc(pDATAsTRUCT); \
    }

#define ST87EC_HTTP_PARSE_METHOD_AND_PATH(pTR)                                           \
    /* Parse HTTP Method */                                                              \
    EcLibVars.SequenceNbiot.HttpTransfer.Params.pMethod = pTR;                           \
    while ((*pTR != ST87EC_SPACE_ASCII_CODE) && (*pTR != ST87EC_NULL_ASCII_CODE))        \
    {                                                                                    \
      pTR++;                                                                             \
    }                                                                                    \
    if (*pTR == ST87EC_SPACE_ASCII_CODE)                                                 \
    {                                                                                    \
      *pTR = ST87EC_NULL_ASCII_CODE;                                                     \
      pTR++;                                                                             \
    }                                                                                    \
    else                                                                                 \
    {                                                                                    \
      pTR = NULL;                                                                        \
    }                                                                                    \
    if (pTR != NULL)                                                                     \
    {                                                                                    \
      /* Parse HTTP Path */                                                              \
      EcLibVars.SequenceNbiot.HttpTransfer.Params.pPath = pTR;                           \
      while ((*pTR != ST87EC_SPACE_ASCII_CODE) && (*pTR != ST87EC_NULL_ASCII_CODE))      \
      {                                                                                  \
        pTR++;                                                                           \
      }                                                                                  \
      if (*pTR == ST87EC_SPACE_ASCII_CODE)                                               \
      {                                                                                  \
        *pTR = ST87EC_NULL_ASCII_CODE;                                                   \
        pTR++;                                                                           \
      }                                                                                  \
      else                                                                               \
      {                                                                                  \
        pTR = NULL;                                                                      \
      }                                                                                  \
      if (pTR != NULL)                                                                   \
      {                                                                                  \
        /* Skip the HTTP version */                                                      \
        while (   (*pTR != ST87EC_CR_ASCII_CODE)                                         \
               && (*pTR != ST87EC_LF_ASCII_CODE)                                         \
               && (*pTR != ST87EC_NULL_ASCII_CODE) )                                     \
        {                                                                                \
          pTR++;                                                                         \
        }                                                                                \
        if (*pTR == ST87EC_CR_ASCII_CODE)                                                \
        {                                                                                \
          *pTR = ST87EC_NULL_ASCII_CODE;                                                 \
          pTR++;                                                                         \
        }                                                                                \
        if (*pTR == ST87EC_LF_ASCII_CODE)                                                \
        {                                                                                \
          *pTR = ST87EC_NULL_ASCII_CODE;                                                 \
          pTR++;                                                                         \
        }                                                                                \
      }                                                                                  \
    }

#define ST87EC_HTTP_PARSE_HEADERS(pTR)                                                   \
    if (   (*pTR != ST87EC_NULL_ASCII_CODE)                                              \
        && (*pTR != ST87EC_CR_ASCII_CODE)                                                \
        && (*pTR != ST87EC_LF_ASCII_CODE)   )                                            \
    {                                                                                    \
      EcLibVars.SequenceNbiot.HttpTransfer.Params.Header.pField = pTR;                   \
      while ((*pTR != ':') && (*pTR != ST87EC_NULL_ASCII_CODE))                          \
      {                                                                                  \
        pTR++;                                                                           \
      }                                                                                  \
      if (*pTR == ':')                                                                   \
      {                                                                                  \
        *pTR = ST87EC_NULL_ASCII_CODE;                                                   \
        pTR++;                                                                           \
        while (*pTR == ST87EC_SPACE_ASCII_CODE)                                          \
        {                                                                                \
          pTR++;                                                                         \
        }                                                                                \
        EcLibVars.SequenceNbiot.HttpTransfer.Params.Header.pValue = pTR;                 \
        while (   (*pTR != ST87EC_CR_ASCII_CODE)                                         \
               && (*pTR != ST87EC_LF_ASCII_CODE)                                         \
               && (*pTR != ST87EC_NULL_ASCII_CODE)   )                                   \
        {                                                                                \
          pTR++;                                                                         \
        }                                                                                \
        if (*pTR == ST87EC_CR_ASCII_CODE)                                                \
        {                                                                                \
          *pTR = ST87EC_NULL_ASCII_CODE;                                                 \
          pTR++;                                                                         \
        }                                                                                \
        if (*pTR == ST87EC_LF_ASCII_CODE)                                                \
        {                                                                                \
          *pTR = ST87EC_NULL_ASCII_CODE;                                                 \
          pTR++;                                                                         \
        }                                                                                \
      }                                                                                  \
      else                                                                               \
      {                                                                                  \
        pTR = NULL;                                                                      \
      }                                                                                  \
    }

#define ST87EC_HTTP_PARSE_BODY(pTR)                                                      \
    /* Skip empty line(s) between headers and body */                                    \
    while ((*pTR == ST87EC_CR_ASCII_CODE) || (*pTR == ST87EC_LF_ASCII_CODE))             \
    {                                                                                    \
      pTR++;                                                                             \
    }                                                                                    \
    /* Parse HTTP Body */                                                                \
    if (*pTR != ST87EC_NULL_ASCII_CODE)                                                  \
    {                                                                                    \
      EcLibVars.SequenceNbiot.HttpTransfer.Params.pBody = pTR;                           \
    }                                                                                    \
    else                                                                                 \
    {                                                                                    \
      EcLibVars.SequenceNbiot.HttpTransfer.Params.pBody = NULL;                          \
    }


/* Private variables ---------------------------------------------------------*/

/* Internal functions --------------------------------------------------------*/
static ST87EC_Lib_Result_t ST87EC_SubSequenceNBIOT_SocketCreation(ST87EC_Lib_SocketType_t SocketType);
static inline void ST87EC_UdpTcpTransferStateIpSend(void);
static ST87EC_Lib_Result_t ST87EC_UdpTcpTransferTcpConnectHandling(void);
static void ST87EC_UdpTcpTransferEndingProcedure(void);
static void ST87EC_UdpTcpTransferEndSequence(void);

/* Exported functions --------------------------------------------------------*/

/**
* @brief Sequence for the MQTT Publish request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_MqttPublish(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.MqttPublish.FsmState != MQTTPUBLISH_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.MqttPublish.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.MqttPublish.FsmState)
  {
    case MQTTPUBLISH_STATE_INIT:
      /* Start the timeout */
      EcLibVars.SequenceNbiot.MqttPublish.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.MqttPublish.Params.TimeoutMs);
      if (EcLibVars.SequenceNbiot.MqttPublish.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_ERROR;
      }
      else
      {
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_WAIT_FOR_SOCKET;
      }
      break;

    case MQTTPUBLISH_STATE_WAIT_FOR_SOCKET:
      result = ST87EC_SubSequenceNBIOT_SocketCreation(SOCKET_TCP_TYPE);
      if (result == RESULT_OK)
      {
        EcLibVars.SequenceNbiot.MqttPublish.DelayTimerValueMs = 10;
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_START_DELAY;
      }
      else if (result == RESULT_KO)
      {
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_ERROR;
      }
      else
      {
        /* Wait and stay in this state */
        result = RESULT_OK;
      }
      break;

    case MQTTPUBLISH_STATE_START_DELAY:
      /* Start the timeout */
      EcLibVars.SequenceNbiot.MqttPublish.DelayTimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.MqttPublish.DelayTimerValueMs);
      if (EcLibVars.SequenceNbiot.MqttPublish.DelayTimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_ERROR;
      }
      else
      {
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_WAIT;
      }
      break;

    case MQTTPUBLISH_STATE_WAIT:
      if (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.MqttPublish.DelayTimerId) == TIMER_STATUS_ELAPSED)
      {
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_SEND_COMMANDS;
      }
      break;

    case MQTTPUBLISH_STATE_SEND_COMMANDS:
      /*Init the mqtt connect status*/
      EcLibVars.RspData[MQTTCONNECT_TAG_SIZE] = ST87EC_MQTT_CONNECT_INIT_STATUS;
      EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_WAIT_RSP;
      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
      EcLibVars.RspInfo[CME_ERROR_INDEX].Validity = RSP_AWAITED;
      EcLibVars.RspInfo[MQTTCONNECT_INDEX].Validity = RSP_NONE;

      switch(EcLibVars.SequenceNbiot.MqttPublish.Command)
      {
        case MQTTPUBLISH_COMMAND_CONFIG:
          ST87EC_Wrapper_SendCmd(ST87EC_MQTT_CFG_CMD);
          break;

        case MQTTPUBLISH_COMMAND_CHECK_CONNECT:
          EcLibVars.RspInfo[MQTTCONNECT_INDEX].Validity = RSP_AWAITED;
          ST87EC_Wrapper_SendCmd("AT#MQTTCONNECT?");
          break;

        case MQTTPUBLISH_COMMAND_CONNECT:
          ST87EC_Wrapper_SendCmd(ST87EC_MQTT_CON_CMD);
          break;

        case MQTTPUBLISH_COMMAND_PUBLISH:
          EcLibVars.SequenceNbiot.MqttPublish.Status = MQTT_CONNECTED;
          ST87EC_Wrapper_SendCmd(ST87EC_MQTT_PUB_CMD);
          break;

        case MQTTPUBLISH_COMMAND_DISCONNECT:
          /* MQTT disconnect integrates a Socket close */
          EcLibVars.SequenceNbiot.MqttPublish.Status = MQTT_NOT_CONNECTED;
          EcLibVars.ModuleStatus.TransferOnGoing = 0;
          ST87EC_Wrapper_SendCmd("AT#MQTTDISC");
          break;

        default:
        case MQTTPUBLISH_COMMAND_END:
          /* Stop the timer */
          ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.MqttPublish.TimerId);
          ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.MqttPublish.DelayTimerId);

          /* Stop the sequence */
          EcLibVars.OnGoingSequence = SEQUENCE_NONE;
          EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_INIT;
          break;
      }
      break;

    case MQTTPUBLISH_STATE_WAIT_RSP:
      /* Check if awaited response is received */
      if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        /* Set up the next command */
        /* As long as it is not the last publication requested by the user, jump over transfer close steps and directly go to seq. end */
        if (EcLibVars.SequenceNbiot.MqttPublish.Params.LastPublish == LAST_PKT_FALSE && EcLibVars.SequenceNbiot.MqttPublish.Command == MQTTPUBLISH_COMMAND_PUBLISH)
        {
          EcLibVars.SequenceNbiot.MqttPublish.Command = MQTTPUBLISH_COMMAND_END;
        }
        /* The ST87 is not connected to the broker */
        else if (EcLibVars.RspData[MQTTCONNECT_TAG_SIZE] == ST87EC_MQTT_NOT_CONNECT_STATUS)
        {
          EcLibVars.SequenceNbiot.MqttPublish.Command = MQTTPUBLISH_COMMAND_CONFIG;
        }
        else
        {
          EcLibVars.SequenceNbiot.MqttPublish.Command++;
        }
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_START_DELAY;
      }
      else if (EcLibVars.RspInfo[CME_ERROR_INDEX].Validity == RSP_RECEIVED)
      {
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_ERROR;
      }
      break;

    default:
    case MQTTPUBLISH_STATE_ERROR:
      EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_START_DELAY;

      /* Catch the CME ERROR status*/
      if (strcmp(&EcLibVars.RspData[CME_ERROR_TAG_SIZE+1], "2218") == 0) /*2218: not connected*/
      {
        EcLibVars.SequenceNbiot.MqttPublish.Status = MQTT_NOT_CONNECTED;
      }
      if (strcmp(&EcLibVars.RspData[CME_ERROR_TAG_SIZE+1], "2220") == 0) /*2220: already connected */
      {
        EcLibVars.SequenceNbiot.MqttPublish.Command = MQTTPUBLISH_COMMAND_PUBLISH;
      }
      else if (strcmp(&EcLibVars.RspData[CME_ERROR_TAG_SIZE+1], "2221") == 0) /*2221: not configured*/
      {
        EcLibVars.SequenceNbiot.MqttPublish.Command = MQTTPUBLISH_COMMAND_CONFIG;
      }
      else if (EcLibVars.SequenceNbiot.MqttPublish.Status == MQTT_CONNECTED)
      {
        EcLibVars.SequenceNbiot.MqttPublish.Command = MQTTPUBLISH_COMMAND_DISCONNECT;
      }
      else
      {
        /* Stop the timer */
        ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.MqttPublish.TimerId);
        ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.MqttPublish.DelayTimerId);

        /* Stop the sequence */
        EcLibVars.OnGoingSequence = SEQUENCE_NONE;
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_INIT;

        result = RESULT_KO;
      }
      break;
  }
  return result;
}


/**
* @brief Sequence for an UDP or TCP Transfer request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_UdpTcp(void)
{
  ST87EC_Lib_Result_t subresult = RESULT_OK;
  ST87EC_Lib_Result_t result = RESULT_OK;
  uint32_t tmp;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState != UDPTCP_TRANSFER_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.UdpTcpTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    ST87EC_WAKEUP_MODULE();
  }

  switch (EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState)
  {
  case UDPTCP_TRANSFER_STATE_INIT:
    /* Start the timeout */
    EcLibVars.SequenceNbiot.UdpTcpTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.TimeoutMs);

    if (EcLibVars.SequenceNbiot.UdpTcpTransfer.TimerId == ST87EC_TIMER_ERROR)
    {
      /* Error occurred */
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    else
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_SOCKET;
    }
    break;

  case UDPTCP_TRANSFER_STATE_WAIT_FOR_SOCKET:
    if (EcLibVars.OnGoingSequence == SEQUENCE_UDP_TRANSFER)
    {
      subresult = ST87EC_SubSequenceNBIOT_SocketCreation(SOCKET_UDP_TYPE);
    }
    else if (EcLibVars.OnGoingSequence == SEQUENCE_TCP_TRANSFER)
    {
      subresult = ST87EC_SubSequenceNBIOT_SocketCreation(SOCKET_TCP_TYPE);
    }
    else {}
    if ((subresult == RESULT_OK) && (EcLibVars.OnGoingSequence == SEQUENCE_UDP_TRANSFER))
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_IP_SEND;
    }
    else if ((subresult == RESULT_OK) && (EcLibVars.OnGoingSequence == SEQUENCE_TCP_TRANSFER))
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_TCP_CONNECT;
    }
    else if (subresult == RESULT_KO)
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    else
    {
      /* Wait and stay in this state */
    }
    break;

  case UDPTCP_TRANSFER_STATE_TCP_CONNECT:
    subresult = ST87EC_UdpTcpTransferTcpConnectHandling();
    if (subresult == RESULT_OK)
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_IP_SEND;
    }
    else if (subresult == RESULT_KO)
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    else
    {
      /* Wait and stay in this state */
    }
    break;

  case UDPTCP_TRANSFER_STATE_IP_SEND:
    ST87EC_UdpTcpTransferStateIpSend();
    break;

  case UDPTCP_TRANSFER_STATE_SEND_DATA_BIN:
    if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
    {
      /* Send Bytes without CRLF */
      ST87EC_Wrapper_SendByte((uint8_t*)EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.pDataTx, \
          EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.DataTxLength);
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_UDPTCP_RSP;
    }
    else if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
    break;

  case UDPTCP_TRANSFER_STATE_WAIT_FOR_UDPTCP_RSP:
    if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
    {
      if (EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.p_TransferCallbackFunc == NULL)
      {
        /* No Rx is expected (#IPRECV URC will not be received), then initiate the end of the transfer procedure */
        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_END_PROCEDURE;
      }
      else
      {
        /* Rx is expected, then start waiting for #IPRECV URC */
        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_IPRECV;
        EcLibVars.RspInfo[IPRECV_INDEX].Validity = RSP_AWAITED;
      }
    }
    else if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
    {
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
    break;

  case UDPTCP_TRANSFER_STATE_WAIT_FOR_IPRECV:
    if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
    {
      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    else if (EcLibVars.RspInfo[IPRECV_INDEX].Validity == RSP_RECEIVED)
    {
      EcLibVars.RspInfo[IPRECV_INDEX].Validity = RSP_NONE;

      /* Read the data via AT#IPREAD and give back data in the customer callback */
      ST87EC_Wrapper_SendCmd("AT#IPREAD=%d,%d",EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.socketId);

      EcLibVars.RspInfo[IPREAD_INDEX].Validity = RSP_AWAITED;
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_IPREAD;
    }
    break;

  case UDPTCP_TRANSFER_STATE_WAIT_FOR_IPREAD:
    if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
    {
      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_ERROR;
    }
    else if (EcLibVars.RspInfo[IPREAD_INDEX].Validity == RSP_RECEIVED)
    {
      /* As soon as #IPREAD URC is received, parse it to get its raw data length
         in order to immediately catch coming raw data with the correct expected length. */

      sscanf((const char *)&EcLibVars.RspData[IPREAD_MAX_DATA_SIZE - 4],"%d",(int *)&tmp);
      EcLibVars.RawDataRsp.ReceivedLength = 0;
      EcLibVars.RawDataRsp.ExpectedLength = (uint16_t)tmp;

      EcLibVars.RspInfo[IPREAD_INDEX].Validity = RSP_NONE;
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_IPREAD_RAWDATA;
    }
    break;

  case UDPTCP_TRANSFER_STATE_WAIT_FOR_IPREAD_RAWDATA:
    if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
    {
      /* As soon as RawDataRsp.ExpectedLength is cleared, it means raw data is available */
      if (EcLibVars.RawDataRsp.ExpectedLength == 0)
      {
        EcLibVars.RawDataRsp.Data[EcLibVars.RawDataRsp.ReceivedLength] = '\0';
        /* Call the customer callback with the read data as parameter */
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.p_TransferCallbackFunc(EcLibVars.RawDataRsp.Data);

        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_END_PROCEDURE;
      }
    }
    break;

  case UDPTCP_TRANSFER_STATE_ERROR:
    EcLibVars.ErrorOccurredInSeq = 1;
  case UDPTCP_TRANSFER_STATE_END_PROCEDURE:
    ST87EC_UdpTcpTransferEndingProcedure();
    break;

  case UDPTCP_TRANSFER_STATE_END_SEQUENCE:
    ST87EC_UdpTcpTransferEndSequence();
    if (EcLibVars.ErrorOccurredInSeq == 1)
    {
      result = RESULT_KO;
    }
    break;


  }
  return result;
}


/**
* @brief Sequence for a CoAP Open request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapOpen(void)
{
  ST87EC_Lib_Result_t subresult = RESULT_BUSY;
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState != COAP_OPEN_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.CoapTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState)
  {
    case COAP_OPEN_STATE_INIT:
      /* Start the timeout */
      EcLibVars.SequenceNbiot.CoapTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.CoapTransfer.Params.Timeout);
      if (EcLibVars.SequenceNbiot.CoapTransfer.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_ERROR;
      }
      else
      {
        /* Check CoAP session initialization level */
        EcLibVars.RspInfo[COAPSTART_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
        ST87EC_Wrapper_SendCmd("AT#COAPSTART?");
        EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_WAIT_FOR_COAP_INIT_LVL;
      }
      break;

    case COAP_OPEN_STATE_WAIT_FOR_COAP_INIT_LVL:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
         EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        if (EcLibVars.RspInfo[COAPSTART_INDEX].Validity == RSP_RECEIVED)
        {
          EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.Command = (ST87EC_Lib_CoapOpen_Command_t)(EcLibVars.RspData[12] - CHAR_OFFSET);
          /* Returned value shall be in the [0;3] range, if not, raise error */
          if (EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.Command > COAP_OPEN_CMD_END)
          {
            EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_ERROR;
          }
          else
          {
            EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_SEND_CMDS;
          }
        }
        else { /* Unexpected case where OK is received and not response (tag+data) yet */}
      }
      else {}
      break;

    case COAP_OPEN_STATE_SEND_CMDS:
      switch (EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.Command)
      {
        case COAP_OPEN_CMD_START:
          subresult = ST87EC_SubSequenceNBIOT_SocketCreation(SOCKET_UDP_TYPE);
          if (subresult == RESULT_KO)
          {
            EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_ERROR;
          }
          else if (subresult == RESULT_OK)
          {
            EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
            EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_WAIT_RSP;
            ST87EC_Wrapper_SendCmd("AT#COAPSTART");
          }
          else { /* Socket creation on-going... */}
          break;
        case COAP_OPEN_CMD_CFG:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd(ST87EC_COAP_CFG_CMD);
          break;
        case COAP_OPEN_CMD_CONNECT:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd("AT#COAPCONNECT");
          break;
        case COAP_OPEN_CMD_END:
          EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_END_SEQUENCE;
          break;
        default:
          break;
      }
      break;

    case COAP_OPEN_STATE_WAIT_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        /* go to next command processing */
        (uint32_t)EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.Command++;
        EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_SEND_CMDS;
      }
      break;

    case COAP_OPEN_STATE_ERROR:
      EcLibVars.ErrorOccurredInSeq = 1;
    case COAP_OPEN_STATE_END_SEQUENCE:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.CoapTransfer.TimerId);

      /* End the sequence */
      EcLibVars.RspInfo[COAPSTART_INDEX].Validity = RSP_NONE;
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      if (EcLibVars.ErrorOccurredInSeq == 1)
      {
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)SEQUENCE_NONE;
        result = RESULT_KO;
      }
      else
      {
        EcLibVars.SequenceNbiot.CoapTransfer.FsmState = COAP_TRANSFER_OPENED;  /* Seq ended OK: update CoAP FSM state */
      }
      break;

    default:
      break;
  }

  return result;
}


/**
* @brief Sequence for a CoAP Transmit request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapTransmit(void)
{
  uint32_t tmp_data;
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState != COAP_TX_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.CoapTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState)
  {
    case COAP_TX_STATE_INIT:
      /* Start the timeout */
      EcLibVars.SequenceNbiot.CoapTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.CoapTransfer.Params.Timeout);
      if (EcLibVars.SequenceNbiot.CoapTransfer.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_ERROR;
      }
      else
      {
        /* Check if data different from '0' is present at pOption pointer:
            if yes: send it to ST87, if no: directly send CoAP data */
        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
        if (EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.pOption[0] == '0')
        {
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.RspInfo[COAPSEND_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
          ST87EC_Wrapper_SendCmd(ST87EC_COAP_SEND_DATA_CMD);
          EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_WAIT_SENDDATA_RSP;
        }
        else
        {
          ST87EC_Wrapper_SendCmd("AT#COAPOPT=%s", EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.pOption);
          EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_WAIT_OPT_RSP;
        }
      }
      break;

    case COAP_TX_STATE_WAIT_OPT_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.RspInfo[COAPSEND_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
          ST87EC_Wrapper_SendCmd(ST87EC_COAP_SEND_DATA_CMD);
          EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_WAIT_SENDDATA_RSP;
      }
      else {}
      break;

    case COAP_TX_STATE_WAIT_SENDDATA_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        if (EcLibVars.RspInfo[COAPSEND_INDEX].Validity == RSP_RECEIVED)
        {
          /* Get #COAPSEND and check CoAP message Id */
          sscanf((const char *)&EcLibVars.RspData[11],"%d",(int *)&tmp_data);
          if (tmp_data != (uint32_t)EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams.Mid)
          {
            EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_ERROR;
          }
          else
          {
            EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_END_SEQUENCE;
          }
        }
      }
      else {}
      break;

    case COAP_TX_STATE_ERROR:
      EcLibVars.ErrorOccurredInSeq = 1;
    case COAP_TX_STATE_END_SEQUENCE:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.CoapTransfer.TimerId);

      /* End the sequence */
      EcLibVars.RspInfo[COAPSEND_INDEX].Validity = RSP_NONE;
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      if (EcLibVars.ErrorOccurredInSeq == 1)
      {
        result = RESULT_KO;
      }
      else
      {
        EcLibVars.SequenceNbiot.CoapTransfer.FsmState = COAP_TRANSFER_RXTX; /* Seq ended OK: update CoAP FSM state */
      }
      break;

    default:
      break;
  }

  return result;
}


/**
* @brief Sequence for a CoAP Receive request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapReceive(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;
  ST87EC_Lib_CoapRxData_t * p_coap_rx_data;
  uint32_t tmp1, tmp2, tmp3;
  uint16_t idx, idx2;

  if (   (EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState == COAP_RX_STATE_URC_RECV)
      && (EcLibVars.RspInfo[COAPDATA_INDEX].Validity == RSP_RECEIVED) )
  {
    /* Parse '#COAPRECV: xxx' URC tag: is it 'packet', 'option' or 'payload'?
       Get 3rd tag letter for discrimination (resp: 'c', 't' or 'y')  */
    p_coap_rx_data = EcLibVars.SequenceNbiot.CoapTransfer.Params.pCoapRxData;
    switch (EcLibVars.RspData[13])
    {
      case 'c': /* 'paCket' #COAPRECV found */
        idx = 18; /* Set index to packet data start point */
        sscanf((const char *)&EcLibVars.RspData[idx],"%d,%d,%d,",(int *)&(tmp1), (int *)&(tmp2), (int *)&(tmp3));
        p_coap_rx_data->Mid = (uint8_t)tmp1;
        p_coap_rx_data->MessageType = (uint8_t)tmp2;
        p_coap_rx_data->ResponseCode = (uint16_t)tmp3;
        ST87EC_SEEKDATA  /* reach next data in buffer */
        ST87EC_SEEKDATA
        ST87EC_SEEKDATA
        /* Get and copy likely Token data */
        idx2 = 0;
        while (EcLibVars.RspData[idx] != ',')
        {
          if (idx2 < ST87EC_LIB_COAP_TOKEN_STR_MAX_LEN - 1)
          {
            *(p_coap_rx_data->pToken + idx2) = EcLibVars.RspData[idx];
            idx2++;
          }
          idx++;
        }
        idx++;
        *(p_coap_rx_data->pToken + ST87EC_LIB_COAP_TOKEN_STR_MAX_LEN - 1) = 0; /* Ensure closing string with '\0' char */
        sscanf((const char *)&EcLibVars.RspData[idx],"%d,%d", (int *)&(tmp1), (int *)&(tmp2));
        p_coap_rx_data->OptionNb = (uint16_t)tmp1;
        p_coap_rx_data->PayloadLength = (uint16_t)tmp2;
        /* If no more Rx message data is expected, then launch user callback for CoAP Rx data collection. */
        if ((p_coap_rx_data->OptionNb == 0) && (p_coap_rx_data->PayloadLength) == 0)
        {
          ST87EC_COAP_RCV_CALLBACK_FUNC(p_coap_rx_data);
        }
        break;

      case 't': /* 'opTion' #COAPRECV found */
        idx = 18; /* Set index to option data start point */
        sscanf((const char *)&EcLibVars.RspData[idx],"%d,",(int *)&(tmp1));  /* Get Mid data */
        ST87EC_SEEKDATA  /* Go to Option data */
        /* Get and copy likely Option data */
        idx2 = 0;
        while (EcLibVars.RspData[idx] != 0)
        {
          if (idx2 < ST87EC_LIB_COAP_OPTION_STR_MAX_LEN - 1)
          {
            *(p_coap_rx_data->pCurOptions + idx2) = EcLibVars.RspData[idx];
            idx2++;
          }
          idx++;
        }
        *(p_coap_rx_data->pCurOptions + idx2) = 0; /* Ensure closing string with '\0' char */
        /* If current Option URC corresponds to current Rx message and no more Rx message data is expected
           then launch user callback for CoAP Rx data collection. */
        if ((p_coap_rx_data->Mid == (uint8_t)tmp1) && (p_coap_rx_data->PayloadLength) == 0)
        {
          ST87EC_COAP_RCV_CALLBACK_FUNC(p_coap_rx_data);
        }
        break;

      case 'y': /* 'paYload' #COAPRECV found */
        idx = 19; /* Set index to payload data start point */
        sscanf((const char *)&EcLibVars.RspData[idx],"%d,",(int *)&(tmp1));  /* Get Mid data */
        if (p_coap_rx_data->Mid == (uint8_t)tmp1)
        {
          if (p_coap_rx_data->PayloadLength > 0)
          {
            /* Now immediately catch coming raw data payload with the correct expected length */
            EcLibVars.RawDataRsp.ReceivedLength = 0;
            EcLibVars.RawDataRsp.ExpectedLength = p_coap_rx_data->PayloadLength;
            EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState = COAP_RX_STATE_PAYLOAD_DATA_RECV;
          }
          else
          {
            /* No payload: exit payload parsing */
          }
        }
        else
        {
          /* Msg ID mismatch: exit with error */
          result = RESULT_KO;
        }
        break;

      default:
        break;
    }
    /* Enable back CoAP URC data reception */
    EcLibVars.RspInfo[COAPDATA_INDEX].Validity = RSP_AWAITED;
  }
  else if (EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState == COAP_RX_STATE_PAYLOAD_DATA_RECV)
  {
    /* As soon as RawDataRsp.ExpectedLength is cleared, it means raw data is available */
    if (EcLibVars.RawDataRsp.ExpectedLength == 0)
    {
      p_coap_rx_data = EcLibVars.SequenceNbiot.CoapTransfer.Params.pCoapRxData;
      /* Copy raw data into user payload, then launch user callback for CoAP Rx data collection. */
      memcpy(p_coap_rx_data->pPayload, EcLibVars.RawDataRsp.Data, EcLibVars.RawDataRsp.ReceivedLength);
      ST87EC_COAP_RCV_CALLBACK_FUNC(p_coap_rx_data);
      EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState = COAP_RX_STATE_URC_RECV;
    }
  }
  else {}

  return result;
}


/**
* @brief Sequence for a CoAP Close request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapClose(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState != COAP_CLOSE_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.CoapTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState)
  {
    case COAP_CLOSE_STATE_INIT:
      /* Stop the likely on-going CoAP Receive */
      EcLibVars.RspInfo[COAPDATA_INDEX].Validity = RSP_NONE;
      /* Start the timeout */
      EcLibVars.SequenceNbiot.CoapTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.CoapTransfer.Params.Timeout);
      if (EcLibVars.SequenceNbiot.CoapTransfer.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_ERROR;
      }
      else
      {
        EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.Command = COAP_CLOSE_CMD_DISC;
        EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_CLOSE_ONGOING;
      }
      break;

    case COAP_CLOSE_STATE_CLOSE_ONGOING:
      switch (EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.Command)
      {
        case COAP_CLOSE_CMD_DISC:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd("AT#COAPDISCONNECT");
          break;
        case COAP_CLOSE_CMD_STOP:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd("AT#COAPSTOP");
          break;
        default:
          break;
      }
      break;

    case COAP_CLOSE_STATE_WAIT_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        /* Go to next command processing */
        if (EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.Command < COAP_CLOSE_CMD_STOP)
        {
          (uint32_t)EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.Command++;
          EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_CLOSE_ONGOING;
        }
        else
        {
          EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_END_SEQUENCE;
        }
      }
      break;

    case COAP_CLOSE_STATE_ERROR:
      EcLibVars.ErrorOccurredInSeq = 1;
    case COAP_CLOSE_STATE_END_SEQUENCE:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.CoapTransfer.TimerId);

      /* End the sequence */
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_INIT;
      EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState = COAP_RX_STATE_NONE;
      EcLibVars.SequenceNbiot.CoapTransfer.FsmState = COAP_TRANSFER_NONE;
      EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)SEQUENCE_NONE;
      if (EcLibVars.ErrorOccurredInSeq == 1)
      {
        result = RESULT_KO;
      }
      break;

    default:
      break;
  }

  return result;
}

/**
* @brief Sequence for a HTTP Open request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_HttpOpen(void)
{
  ST87EC_Lib_Result_t subresult = RESULT_BUSY;
  ST87EC_Lib_Result_t result = RESULT_OK;
  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState != HTTP_OPEN_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.HttpTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState)
  {
    case HTTP_OPEN_STATE_INIT:
      /* Start the timeout */
      EcLibVars.SequenceNbiot.HttpTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.HttpTransfer.Params.Timeout);
      if (EcLibVars.SequenceNbiot.HttpTransfer.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_ERROR;
      }
      else
      {
        /* Check HTTP session initialization level */
        EcLibVars.RspInfo[HTTPSTART_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
        ST87EC_Wrapper_SendCmd("AT#HTTPSTART?");
        EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_WAIT_FOR_HTTP_INIT_LVL;
      }
      break;

    case HTTP_OPEN_STATE_WAIT_FOR_HTTP_INIT_LVL:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
         EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        if (EcLibVars.RspInfo[HTTPSTART_INDEX].Validity == RSP_RECEIVED)
        {
          if ((EcLibVars.RspData[HTTP_TAG_SIZE] - CHAR_OFFSET) == 1)
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_END_SEQUENCE;
          }
          else
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_SEND_CMDS;
          }
        }
        else { /* Unexpected case where OK is received and not response (tag+data) yet */}
      }
      else {}
      break;

    case HTTP_OPEN_STATE_SEND_CMDS:
      switch (EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.Command)
      {
        case HTTP_OPEN_CMD_SOCKET_CREATE:
          subresult = ST87EC_SubSequenceNBIOT_SocketCreation(SOCKET_TCP_TYPE);
          if (subresult == RESULT_KO)
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_ERROR;
          }
          else if (subresult == RESULT_OK)
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.Command++;
          }
          else { /* Socket creation on-going... */}
          break;
        case HTTP_OPEN_CMD_DNS_GET:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.RspInfo[DNS_INDEX].Validity = RSP_AWAITED;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd(ST87EC_DNS_CMD);
          break;
        case HTTP_OPEN_CMD_SOCKET_CONNECT:
          subresult = ST87EC_UdpTcpTransferTcpConnectHandling();
          if (subresult == RESULT_KO)
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_ERROR;
          }
          else if (subresult == RESULT_OK)
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.Command++;
          }
          else { /* Socket connect on-going... */}
          break;
        case HTTP_OPEN_CMD_START:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd("AT#HTTPSTART");
          break;
        case HTTP_OPEN_CMD_END:
          EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_END_SEQUENCE;
          break;
        default:
          break;
      }
      break;

    case HTTP_OPEN_STATE_WAIT_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_ERROR;
      }
      else if (EcLibVars.RspInfo[DNS_INDEX].Validity == RSP_RECEIVED)
      {
        /* Awaited response is received( +tag_length to remove the "#DNS: " ) */
        uint32_t idx = 0;
        while ((EcLibVars.RspData[idx+DNS_TAG_SIZE] != ',') && (idx < (IP_ADDRESS_IPV4_LEN-1)))
        {
          EcLibVars.SequenceNbiot.HttpTransfer.Params.pIpAddress[idx] = EcLibVars.RspData[idx+DNS_TAG_SIZE];
          idx++;
        }
        EcLibVars.SequenceNbiot.HttpTransfer.Params.pIpAddress[idx] = '\0';
        EcLibVars.RspInfo[DNS_INDEX].Validity = RSP_NONE;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.IpAddr = EcLibVars.SequenceNbiot.HttpTransfer.Params.pIpAddress;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        /* go to next command processing */
        (uint32_t)EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.Command++;
        EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_SEND_CMDS;
      }
      break;

    case HTTP_OPEN_STATE_ERROR:
      EcLibVars.ErrorOccurredInSeq = 1;
    case HTTP_OPEN_STATE_END_SEQUENCE:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.HttpTransfer.TimerId);

      /* End the sequence */
      EcLibVars.RspInfo[HTTPSTART_INDEX].Validity = RSP_NONE;
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      if (EcLibVars.ErrorOccurredInSeq == 1)
      {
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)SEQUENCE_NONE;
        EcLibVars.SequenceNbiot.TransferCfg.SecProfileId = (-1);
        result = RESULT_KO;
      }
      else
      {
        EcLibVars.SequenceNbiot.HttpTransfer.FsmState = HTTP_TRANSFER_OPENED;  /* Seq ended OK: update HTTP FSM state */
      }
      break;

    default:
      break;
  }

  return result;
}

/**
* @brief Sequence for a HTTP Transfer
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_HttpTransfer(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState != HTTP_TRANSFER_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.HttpTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState)
  {
    case HTTP_TRANSFER_STATE_INIT:

      /* Start the timeout */
      EcLibVars.SequenceNbiot.HttpTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.HttpTransfer.Params.Timeout);
      if (EcLibVars.SequenceNbiot.HttpTransfer.TimerId == ST87EC_TIMER_ERROR)
      {
      /* Error occurred */
        EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
      }
      else
      {
        /* Parse input HTTP data to get Method and Path */
        EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData = EcLibVars.SequenceNbiot.HttpTransfer.Params.pHttpRawInStrCopy;
        ST87EC_HTTP_PARSE_METHOD_AND_PATH(EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData)
        if (EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData == NULL)
        {
          /* Parsing error occurred */
          EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
        }
        else
        {
          /* Send Method command */
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd(ST87EC_HTTP_METHOD_CMD);
        }
      }
      break;

    case HTTP_TRANSFER_STATE_SEND_CMDS:
      switch (EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Command)
        {
          case HTTP_TRANSFER_CMD_HEADER:
            /* Parse and send each HTTP request Header(s) if any */
            uint32_t http_data_addr = (uint32_t)EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData;
            ST87EC_HTTP_PARSE_HEADERS(EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData)
            if (EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData == NULL)
            {
              /* Parsing error occurred */
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
            }
            else if ((uint32_t)EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData != http_data_addr)
            {
              /* A header has been parsed, now send it */
              EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_WAIT_RSP;
              ST87EC_Wrapper_SendCmd("AT#HTTPHEADER=%s,%s",EcLibVars.SequenceNbiot.HttpTransfer.Params.Header.pField,
              EcLibVars.SequenceNbiot.HttpTransfer.Params.Header.pValue);
            }
            else
            {
              /* All Headers have been parsed, proceed with next command */
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Command = HTTP_TRANSFER_CMD_SEND;
            }
            break;
          case HTTP_TRANSFER_CMD_SEND:
            /* Parse HTTP Body data */
            ST87EC_HTTP_PARSE_BODY(EcLibVars.SequenceNbiot.HttpTransfer.pHttpInData)
            /* Test whether the HTTP request contains body data */
            if (EcLibVars.SequenceNbiot.HttpTransfer.Params.pBody != NULL)
            {
              EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
              EcLibVars.RspInfo[HTTPRECV_INDEX].Validity = RSP_AWAITED;
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.BodyLen = 0;
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.HeaderLen = 0;
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.ContentLen = 0;
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Flag.Value = 0;
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_WAIT_RSP;
              ST87EC_Wrapper_SendCmd("AT#HTTPSEND=%d,%d,%s",
                  EcLibVars.SequenceNbiot.TransferCfg.contextId,
                  EcLibVars.SequenceNbiot.TransferCfg.socketId,
                  EcLibVars.SequenceNbiot.HttpTransfer.Params.pBody);
            }
            else
            {
              /* Proceed with next command */
              EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_WAIT_FOR_HTTPRECV;
            }
            break;
          case HTTP_TRANSFER_CMD_END:
            EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_END_SEQUENCE;
            break;
          default:
            break;
        }
      break;

    case HTTP_TRANSFER_STATE_WAIT_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_SEND_CMDS;

        /* Test whether the user has provided a callback for data reception: if yes proceed with data reception step */
        if (EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Command == HTTP_TRANSFER_CMD_SEND && EcLibVars.SequenceNbiot.HttpTransfer.Params.HttpTransferReadCallbackFunc != NULL)
        {
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_WAIT_FOR_HTTPRECV;
        }
        else if (EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Command != HTTP_TRANSFER_CMD_HEADER)
        {
          /* Proceed with next command, except if some likely HTTP headers remain for their processing */
          EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Command++;
        }
        else{}
      }
      else{}
      break;

    case HTTP_TRANSFER_STATE_WAIT_FOR_HTTPRECV:

      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
      }
      else if (EcLibVars.RspInfo[HTTPRECV_INDEX].Validity == RSP_RECEIVED)
      {
        int urcType;
        int length;
        int num;

        ST87EC_Lib_HttpRxTx_t * pHttpRxTx = &EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx;
        const char * ptr = &EcLibVars.RspData[HTTPRECV_TAG_SIZE];

        num = sscanf(ptr,"%d,%d", &urcType, &length);

        if (num >= 2)
        {
          if (pHttpRxTx->Flag.Bit.HeaderFound)
          {
            /* Body reception */
            if (urcType == 1)
            {
              pHttpRxTx->BodyLen += length;

              if (pHttpRxTx->Flag.Bit.ContentLengthFound)
              {
                if (pHttpRxTx->BodyLen >= pHttpRxTx->ContentLen)
                {
                  pHttpRxTx->TotReadLen = pHttpRxTx->HeaderLen + pHttpRxTx->BodyLen;
                  pHttpRxTx->FsmState = HTTP_TRANSFER_STATE_SEND_HTTPREAD;
                }
              }
              else
              {
                /* should not happen since if content-length not present, then only the header will be printed */
              }
            }
          }
          else
          {
            /* Header reception */
            if ((urcType == 2) || (urcType == 0))
            {
              pHttpRxTx->HeaderLen += length;
            }

            if (urcType == 0)
            {
              int code;
              int contentLength;

              pHttpRxTx->Flag.Bit.HeaderFound = 1;

              /* skip commas of previous 2 parameters*/
              ptr = strchr(ptr, ','); /* this comma is always present since previous sscanf() call*/
              ptr++;
              ptr = strchr(ptr, ',');

              if (ptr)
              {
                ptr++;
                num = sscanf(ptr,"%d,%d", &code, &contentLength);

                if (num == 2)
                {
                  pHttpRxTx->Flag.Bit.ContentLengthFound = 1;
                  pHttpRxTx->ContentLen = contentLength;

                  if (contentLength == 0)
                  {
                    pHttpRxTx->TotReadLen = pHttpRxTx->HeaderLen;
                    pHttpRxTx->FsmState = HTTP_TRANSFER_STATE_SEND_HTTPREAD;
                  }
                }
                else
                {
                  /* content-length not present: just print received header */
                  pHttpRxTx->TotReadLen = pHttpRxTx->HeaderLen;
                  pHttpRxTx->FsmState = HTTP_TRANSFER_STATE_SEND_HTTPREAD;
                }
              }
            }
          } /* if (pHttpRxTx->Flag.Bit.HeaderFound) */
        } /* if (num >= 2) */

        if (EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState == HTTP_TRANSFER_STATE_SEND_HTTPREAD)
        {
          EcLibVars.RspInfo[HTTPRECV_INDEX].Validity = RSP_NONE;

          /* Expected size allocation */
          EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload = malloc(pHttpRxTx->TotReadLen + 1);
          if (EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload == NULL)
          {
            pHttpRxTx->FsmState = HTTP_TRANSFER_STATE_ERROR;
          }
          else
          {
            pHttpRxTx->ReadIdx = 0;
            memset(EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload, 0, pHttpRxTx->TotReadLen);   /*Clear the Rx Payload*/
          }
        }
        else
        {
          EcLibVars.RspInfo[HTTPRECV_INDEX].Validity = RSP_AWAITED;
        }
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
      }
      break;

    case HTTP_TRANSFER_STATE_SEND_HTTPREAD:
    {
      EcLibVars.RspInfo[HTTPREAD_INDEX].Validity = RSP_AWAITED;
      EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_WAIT_FOR_HTTPREAD;
      /* Read the data via AT#HTTPREAD and give back data in the customer callback */
      ST87EC_Wrapper_SendCmd("AT#HTTPREAD");
      break;
    }

    case HTTP_TRANSFER_STATE_WAIT_FOR_HTTPREAD:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
        EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
      }
      else if (EcLibVars.RspInfo[HTTPREAD_INDEX].Validity == RSP_RECEIVED)
      {
        int num = 0;
        ST87EC_Lib_HttpRxTx_t * pHttpRxTx = &EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx;

        EcLibVars.RspInfo[HTTPREAD_INDEX].Validity = RSP_NONE;
        /* As soon as #HTTPREAD URC is received, parse it to get its raw data length
           in order to immediately catch coming raw data with the correct expected length. */
        if (1 == sscanf((const char *)&EcLibVars.RspData[HTTPREAD_TAG_SIZE],"%d", &num))
        {
          if ((num + pHttpRxTx->ReadIdx) > pHttpRxTx->TotReadLen)
          {
            EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
            EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
          }
          else if (num > 0)
          {
            EcLibVars.RawDataRsp.ReceivedLength = 0;
            EcLibVars.RawDataRsp.ExpectedLength = (uint16_t) num;
            pHttpRxTx->FsmState = HTTP_TRANSFER_STATE_WAIT_FOR_HTTPREAD_RAWDATA;
          }
          else
          {
            pHttpRxTx->FsmState = HTTP_TRANSFER_STATE_PRINT_DATA;
          }
        }
        else
        {
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_ERROR;
        }
      }
      break;

    case HTTP_TRANSFER_STATE_WAIT_FOR_HTTPREAD_RAWDATA:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
       /* As soon as RawDataRsp.ExpectedLength is cleared, it means raw data is available */
        if (EcLibVars.RawDataRsp.ExpectedLength == 0)
        {
          ST87EC_Lib_HttpRxTx_t * pHttpRxTx = &EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx;
          char * pPayload = &EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload[pHttpRxTx->ReadIdx];

         /* Substract just got data amount to still to be received one */
          memcpy(pPayload, EcLibVars.RawDataRsp.Data, EcLibVars.RawDataRsp.ReceivedLength);
          pHttpRxTx->ReadIdx += EcLibVars.RawDataRsp.ReceivedLength;

          if (pHttpRxTx->ReadIdx >= pHttpRxTx->TotReadLen)
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_PRINT_DATA;
          }
          else
          {
            EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_SEND_HTTPREAD;
          }
        }
      }
      break;

    case HTTP_TRANSFER_STATE_PRINT_DATA:
    {
      ST87EC_Lib_HttpRxTx_t * pHttpRxTx = &EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx;

      EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_END_SEQUENCE;
      /* Call the customer callback with the read data as parameter */
      EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload[pHttpRxTx->ReadIdx] = '\0';
      EcLibVars.SequenceNbiot.HttpTransfer.Params.HttpTransferReadCallbackFunc(EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload);
      free(EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload);
      EcLibVars.SequenceNbiot.HttpTransfer.Params.pPayload = NULL;

      break;
    }

    case HTTP_TRANSFER_STATE_ERROR:
      EcLibVars.ErrorOccurredInSeq = 1;
    case HTTP_TRANSFER_STATE_END_SEQUENCE:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.HttpTransfer.TimerId);

      /* End the sequence */
      free(EcLibVars.SequenceNbiot.HttpTransfer.Params.pHttpRawInStrCopy); /* Free mem initially allocated at ST87EC_Lib_NBIOT_HttpTransfer() API call */
      EcLibVars.RspInfo[HTTPRECV_INDEX].Validity = RSP_NONE;
      EcLibVars.RspInfo[HTTPREAD_INDEX].Validity = RSP_NONE;
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      if (EcLibVars.ErrorOccurredInSeq == 1)
      {
        result = RESULT_KO;
      }
      else
      {
        EcLibVars.SequenceNbiot.HttpTransfer.FsmState = HTTP_TRANSFER_RXTX; /* Seq ended OK: update CoAP FSM state */
      }
      break;

    default:
      break;
  }
  return result;
}

/**
* @brief Sequence for a HTTP Close request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_HttpClose(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState != HTTP_CLOSE_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceNbiot.HttpTransfer.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_ERROR;
  }

  switch (EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState)
  {
    case HTTP_CLOSE_STATE_INIT:
      /* Start the timeout */
      EcLibVars.SequenceNbiot.HttpTransfer.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceNbiot.HttpTransfer.Params.Timeout);
      if (EcLibVars.SequenceNbiot.HttpTransfer.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_ERROR;
      }
      else
      {
        EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.Command = HTTP_CLOSE_CMD_STOP;
        EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_CLOSE_ONGOING;
      }
      break;

    case HTTP_CLOSE_STATE_CLOSE_ONGOING:
      switch (EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.Command)
      {
        case HTTP_CLOSE_CMD_STOP:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd("AT#HTTPSTOP");
          break;
        case HTTP_CLOSE_CMD_CLOSE:
          EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_WAIT_RSP;
          ST87EC_Wrapper_SendCmd("AT#SOCKETCLOSE=%d,%d", \
              EcLibVars.SequenceNbiot.TransferCfg.contextId,\
              EcLibVars.SequenceNbiot.TransferCfg.socketId);
          break;
        default:
          break;
      }
      break;

    case HTTP_CLOSE_STATE_WAIT_RSP:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_ERROR;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        /* Go to next command processing */
        if (EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.Command < HTTP_CLOSE_CMD_CLOSE)
        {
          (uint32_t)EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.Command++;
          EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_CLOSE_ONGOING;
        }
        else
        {
          EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_END_SEQUENCE;
        }
      }
      break;

    case HTTP_CLOSE_STATE_ERROR:
      EcLibVars.ErrorOccurredInSeq = 1;
    case HTTP_CLOSE_STATE_END_SEQUENCE:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.HttpTransfer.TimerId);

      /* End the sequence */
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_INIT;
      EcLibVars.SequenceNbiot.HttpTransfer.FsmState = HTTP_TRANSFER_NONE;
      EcLibVars.ModuleStatus.TransferOnGoing = 0;
      EcLibVars.SequenceNbiot.TransferCfg.SecProfileId = (-1);
      if (EcLibVars.ErrorOccurredInSeq == 1)
      {
        result = RESULT_KO;
      }
      break;

    default:
      break;
  }
  return result;
}

/* Private functions --------------------------------------------------------*/

/**
* @brief Sequence for the socket creation
*
* @param SocketType: the type of socket to create
* @retval Function execution status
*/
static ST87EC_Lib_Result_t ST87EC_SubSequenceNBIOT_SocketCreation(ST87EC_Lib_SocketType_t SocketType)
{
  ST87EC_Lib_Result_t result = RESULT_BUSY;
  char * socket_type_cmd;

  switch(EcLibVars.SequenceNbiot.SocketCreation.FsmState)
  {
    case SOCKET_CREATION_TEST:
      /* Check if a socket needs to be created */
      EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */

      ST87EC_Wrapper_SendCmd("AT#SOCKETCREATE?");
      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;

      EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_WAIT_FOR_SOCKET_CHECK;
      break;

    case SOCKET_CREATION_WAIT_FOR_SOCKET_CHECK:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        result = RESULT_KO;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        if (EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity == RSP_RECEIVED)
        {
          EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity = RSP_AWAITED;

          if (EcLibVars.RspData[SOCKETCREATE_MAX_DATA_SIZE - 5] == 0x20) /* 0x20 = space, meaning a socketId value exists in the response */
          {
            /* Save the current socketId and go further, no need to create a new socket */
            /* socketId is only 1 digit, as most of the time it is 0, and at most 3 sockets can be created */
            EcLibVars.SequenceNbiot.TransferCfg.socketId = EcLibVars.RspData[SOCKETCREATE_MAX_DATA_SIZE - 2] - CHAR_OFFSET;

            EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity = RSP_NONE;
            /* Leave this function and go further in the sequence main FSM */
            result = RESULT_OK;
          }
          else
          {
            /* No socket created yet, create a new one */
            EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_WAIT_FOR_SOCKET_CREATION;
            if (SocketType == SOCKET_UDP_TYPE)
            {
              ST87EC_Wrapper_SendCmd("AT#SOCKETCREATE=%d,%d,%s,%d,%d,%d,%d", \
                  EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.ipMode,\
                  "UDP",EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb,SOCKET_SEND_TIMEOUT,SOCKET_RECEIVE_TIMEOUT,SOCKET_FRAME_RECEIVED_URC);
            }
            else if (EcLibVars.SequenceNbiot.TransferCfg.SecProfileId == (-1))
            {
              /* Socket creation for a non-secured transfer is requested */
              if (SocketType == SOCKET_TCP_TYPE)
              {
                socket_type_cmd = "TCP";
              }
              else if (SocketType == SOCKET_RAW_TYPE)
              {
               socket_type_cmd = "RAW";
              }
              else {}
              ST87EC_Wrapper_SendCmd("AT#SOCKETCREATE=%d,%d,%s,,%d,%d,%d", \
                  EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.ipMode,\
                  socket_type_cmd,SOCKET_SEND_TIMEOUT,SOCKET_RECEIVE_TIMEOUT,SOCKET_FRAME_RECEIVED_URC);
            }
            else
            {
              /* Socket creation for a secured transfer (TLS) is requested */
              ST87EC_Wrapper_SendCmd("AT#SOCKETCREATE=%d,%d,%s,,%d,%d,%d,%d", \
                  EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.ipMode,\
                  "TCP",SOCKET_SEND_TIMEOUT,SOCKET_RECEIVE_TIMEOUT,SOCKET_FRAME_RECEIVED_URC,\
                  EcLibVars.SequenceNbiot.TransferCfg.SecProfileId);
            }
          }
        }
        else { /* Unexpected case where OK is received and not response (tag+data) yet */}
      }
      else {}

      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
      break;

    case SOCKET_CREATION_WAIT_FOR_SOCKET_CREATION:
      /* Check if awaited response is received */
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
        result = RESULT_KO;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        if (EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity == RSP_RECEIVED)
        {
          /* socketId is only 1 digit, as most of the time it is 0, and at most 3 sockets can be created */
          EcLibVars.SequenceNbiot.TransferCfg.socketId = EcLibVars.RspData[SOCKETCREATE_MAX_DATA_SIZE - 4] - CHAR_OFFSET;

          EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity = RSP_NONE;

          result = RESULT_OK;
        }
        else { /* Unexpected case where OK is received and not response (tag+data) yet */}
      }
      else {}
      break;

    default:
      break;
  }
  return result;
}


/**
* @brief UDP/TCP FSM sub-function sending the UDP or TCP message
*
* @param : None
*/
static inline void ST87EC_UdpTcpTransferStateIpSend(void)
{
  EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
  uint8_t rai;
  if (EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.pDataTx != NULL)
  {
    /* There are some data to send */

    if (EcLibVars.OnGoingSequence == SEQUENCE_UDP_TRANSFER)
    {
      if ((EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.LastPacket == LAST_PKT_TRUE) && (EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.p_TransferCallbackFunc == NULL))
      {
        /* if it is last packet and current transfer is Tx only, set RAI to 1 */
        rai = 1;   /* RAI=1: TE will send only 1 UL packet and no DL packets expected */
      }
      else if (EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.LastPacket == LAST_PKT_TRUE)
      {
        /* if it is last packet and current transfer is Tx+Rx, set RAI to 2 */
        rai = 2;   /* RAI=2: TE will send only 1 UL packet and only 1 DL packet expected */
      }
      else
      {
        rai = 0;   /* RAI=0: no information available (or none of the other options apply) */
      }
#if (UDP_DATA_TYPE == 0) || (UDP_DATA_TYPE == 2)
      /* ASCII string or Hex data */
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_UDPTCP_RSP;
      ST87EC_Wrapper_SendCmd("AT#IPSENDUDP=%d,%d,%s,%d,%d,%d,%s", \
          EcLibVars.SequenceNbiot.TransferCfg.contextId, EcLibVars.SequenceNbiot.TransferCfg.socketId, \
          EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.IpAddr, EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb, \
          rai, UDP_DATA_TYPE, \
          EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.pDataTx);
#elif (UDP_DATA_TYPE == 1)
      /* Binary data */
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_SEND_DATA_BIN;
      ST87EC_Wrapper_SendCmd("AT#IPSENDUDP=%d,%d,%s,%d,%d,%d,%d", \
          EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.socketId, \
          EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.IpAddr,EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb, \
          rai, UDP_DATA_TYPE, \
          EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.DataTxLength);
#endif /* UDP_DATA_TYPE */
    }
    else if (EcLibVars.OnGoingSequence == SEQUENCE_TCP_TRANSFER)
    {
#if (TCP_DATA_TYPE == 0) || (TCP_DATA_TYPE == 2)
      /* ASCII string or Hex data */
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_UDPTCP_RSP;
      ST87EC_Wrapper_SendCmd("AT#IPSENDTCP=%d,%d,%d,%s", \
          EcLibVars.SequenceNbiot.TransferCfg.contextId, EcLibVars.SequenceNbiot.TransferCfg.socketId, \
          TCP_DATA_TYPE, EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.pDataTx);
#elif (TCP_DATA_TYPE == 1)
      /* Binary data */
      EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_SEND_DATA_BIN;
      ST87EC_Wrapper_SendCmd("AT#IPSENDTCP=%d,%d,%d,%d", \
          EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.socketId, \
          TCP_DATA_TYPE, EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.DataTxLength);
#endif /* TCP_DATA_TYPE */
    }
    else {}
  }
  else
  {
    /* No data to send, only read data */
    /* Read the data via AT#IPREAD and give back data in the customer callback */
    ST87EC_Wrapper_SendCmd("AT#IPREAD=%d,%d",EcLibVars.SequenceNbiot.TransferCfg.contextId, \
        EcLibVars.SequenceNbiot.TransferCfg.socketId);

    EcLibVars.RspInfo[IPREAD_INDEX].Validity = RSP_AWAITED;
    EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_WAIT_FOR_IPREAD;
  }
}

/**
* @brief UDP/TCP FSM sub-function handling the transfer ending procedure
*
* @param : None
*/
static void ST87EC_UdpTcpTransferEndingProcedure(void)
{
  switch (EcLibVars.SequenceNbiot.UdpTcpTransfer.EndSubFsmState)
  {
    case UDPTCP_TRANSFER_ENDPROC_STATE_START:
      if (EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.LastPacket == LAST_PKT_TRUE)
      {
        /* It is the last data, close the socket */
        ST87EC_Wrapper_SendCmd("AT#SOCKETCLOSE=%d,%d", \
        EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.socketId);

        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.LastPacket = LAST_PKT_FALSE;
        EcLibVars.ModuleStatus.TransferOnGoing = 0;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.EndSubFsmState = UDPTCP_TRANSFER_ENDPROC_STATE_WAIT_FOR_SOCKETCLOSE;
      }
     else
      {
        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_END_SEQUENCE;
      }
      break;

    case UDPTCP_TRANSFER_ENDPROC_STATE_WAIT_FOR_SOCKETCLOSE:
      /* Check if awaited response is received */
      if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_END_SEQUENCE;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_END_SEQUENCE;
        EcLibVars.ErrorOccurredInSeq = 1;
      }
      else {}
      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
      break;

    default:
      break;
  }
}

/**
* @brief UDP/TCP transfer FSM sub-function handling the TCP Connect procedure
*
* @param : None
* @retval Function execution status
*/
static ST87EC_Lib_Result_t ST87EC_UdpTcpTransferTcpConnectHandling(void)
{
  ST87EC_Lib_Result_t result = RESULT_BUSY;

  switch (EcLibVars.SequenceNbiot.UdpTcpTransfer.TcpConnSubFsmState)
  {
    case UDPTCP_TRANSFER_TCPCONN_STATE_TEST:
    /* Check if a TCP Connection is needed */
      EcLibVars.RspInfo[TCPCONN_INDEX].Validity = RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
      EcLibVars.RspReceived = SPECIFIC_RSP_NONE;          /* reset specific response as a CME can occur */
      ST87EC_Wrapper_SendCmd("AT#TCPCONNECT?");
      EcLibVars.SequenceNbiot.UdpTcpTransfer.TcpConnSubFsmState = UDPTCP_TRANSFER_TCPCONN_STATE_WAIT_FOR_CHECK;
      break;

    case UDPTCP_TRANSFER_TCPCONN_STATE_WAIT_FOR_CHECK:
      /* Check if awaited response is received */
      if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        result = RESULT_KO;
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        if (EcLibVars.RspInfo[TCPCONN_INDEX].Validity == RSP_RECEIVED)
        {
          if (EcLibVars.RspData[TCPCONN_MAX_DATA_SIZE - 2] == ST87EC_TCPCONN_NOT_CONNECTED_STATUS)
          {
            EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
            EcLibVars.SequenceNbiot.UdpTcpTransfer.TcpConnSubFsmState = UDPTCP_TRANSFER_TCPCONN_STATE_WAIT_FOR_TCPCONN;
            ST87EC_Wrapper_SendCmd("AT#TCPCONNECT=%d,%d,%s,%d", \
                EcLibVars.SequenceNbiot.TransferCfg.contextId,EcLibVars.SequenceNbiot.TransferCfg.socketId,\
                EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.IpAddr, EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb);
          }
          else if (EcLibVars.RspData[TCPCONN_MAX_DATA_SIZE - 2] == ST87EC_TCPCONN_CONNECTED_STATUS)
          {
            result = RESULT_OK;  /* Already connected OK: end of TcpConnect handling */
          }
          else if (EcLibVars.RspData[TCPCONN_MAX_DATA_SIZE - 2] == ST87EC_TCPCONN_CONNECTING_STATUS)
          {
            EcLibVars.SequenceNbiot.UdpTcpTransfer.TcpConnSubFsmState = UDPTCP_TRANSFER_TCPCONN_STATE_TEST; /* Connection on-going, retry until "Connected" state */
          }
          else /* ST87EC_TCPCONN_WAIT_CLOSE_STATUS */
          {
            result = RESULT_KO;
          }
        }
        else { /* Unexpected case where OK is received and not response (tag+data) yet */}

      }
      else {}
      break;

    case UDPTCP_TRANSFER_TCPCONN_STATE_WAIT_FOR_TCPCONN:
      if (EcLibVars.RspReceived == SPECIFIC_RSP_OK)
      {
        result = RESULT_OK;  /* Connect OK: end of TcpConnect handling */
      }
      else if (EcLibVars.RspReceived == SPECIFIC_RSP_CME)
      {
        result = RESULT_KO;
      }
      else {}
    break;

    default:
      break;
  }

  return result;
}

/**
* @brief UDP/TCP FSM sub-function ending the sequence
*
* @param : None
*/
static void ST87EC_UdpTcpTransferEndSequence(void)
{
  /* Stop the timer */
  ST87EC_Wrapper_StopTimer(EcLibVars.SequenceNbiot.UdpTcpTransfer.TimerId);

  /* End the sequence */
  EcLibVars.RspInfo[SOCKETCREATE_INDEX].Validity = RSP_NONE;
  EcLibVars.OnGoingSequence = SEQUENCE_NONE;
  EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_INIT;
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
