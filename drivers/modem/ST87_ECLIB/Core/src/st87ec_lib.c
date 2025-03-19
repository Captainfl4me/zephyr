/**
  ******************************************************************************
  * @file    st87ec_lib.c
  * @author  APMS Application Team
  * @brief   EC lib API functions
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
#include "../inc/st87ec_lib.h"
#include "../inc/st87ec_engine.h"
#include "../inc/st87ec_sequence_nbiot.h"
#include "../inc/st87ec_sequence_sys.h"
#include "../inc/st87ec_wrapper.h"
#include <zephyr/kernel.h>

/* Exported functions --------------------------------------------------------*/

/* Constants -----------------------------------------------------------------*/

/* Global variables ----------------------------------------------------------*/

/**
 * @brief EC library initialization API
 *
 * To be called first, include ST87 module HW initialization
 * @param pGenericErrorCallbackFunc: Pointer to EC Lib generic error callback
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Init(ST87EC_Lib_GenericErrorCallback_t* pGenericErrorCallbackFunc)
{
    uint8_t status = 0;
    ST87EC_Lib_Result_t result = RESULT_OK;

    EcLibVars.pGenericErrorCallback = pGenericErrorCallbackFunc;

    /* Reset of the whole system as init */
    status += (uint8_t)ST87EC_Lib_Reset();

    /* Trig cold parameter initialization sequence */
    status += (uint8_t)ST87EC_Lib_TrigColdParamInit();

    if (status > 0) {
        result = RESULT_KO;
    }

    return (result);
}

/**
 * @brief EC library reset API
 *
 * Reset of EC lib (including ST87 module HW reset)
 * @param : None
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Reset(void)
{
    uint8_t status = 0;
    ST87EC_Lib_Result_t result = RESULT_OK;

    /* Wrapper initialization */
    status = (uint8_t)ST87EC_Wrapper_Init();

    /* Initialize the main sequence */
    status += (uint8_t)ST87EC_Lib_SequenceMainInit();

    /* Reset ST87 module */
    status += (uint8_t)ST87EC_Wrapper_DriveResetPin(RESET_PIN_PULSE);

    ST87EC_Wrapper_Delay(500); /* Wait after ST87 Hw reset prior to proceeding */

    if (status > 0) {
        result = RESULT_KO;
    }

    return (result);
}

/**
 * @brief EC library Scheduler API
 *
 * Main task for the EC lib, to be scheduled by the system
 * @param : None
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Scheduler(void)
{
    uint8_t status = 0;
    ST87EC_Lib_Result_t result = RESULT_OK;

    /* Run ST87 msg parsing */
    status += (uint8_t)ST87EC_Wrapper_St87RxMsgHandler();

    /* Run EC Lib main sequence */
    status += (uint8_t)ST87EC_Lib_SequenceMain();

    if (status > 0) {
        result = RESULT_KO;
    }
    return (result);
}

/**
 * @brief API to get the NB-IOT or EC library status
 *
 * @param pState: Pointer to the output state structure
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_GetState(ST87EC_Lib_Status_t* pState)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if (pState == NULL) {
        result = RESULT_BAD_PARAM;
    } else {
        /* Initialize the structure to be aware on the result which fields have been updated */
        pState->ConnectionStatus = CONN_STATUS_UNKNOWN;
        pState->SimStatus = SIM_STATUS_UNKNOWN;
        pState->RegistrationStatus = NOT_REGISTERED;
        pState->OnGoingSequence = SEQUENCE_NONE;
        pState->SleepWakeupstatus = STATUS_WAKEUP;
        pState->InitComplete = INIT_NOT_COMPLETE;

        /* Give data to customer */
        memcpy(pState, &(EcLibVars.ModuleStatus), sizeof(ST87EC_Lib_Status_t));
        pState->OnGoingSequence = EcLibVars.OnGoingSequence;
    }

    return (result);
}

/**
 * @brief API to get the ST87M01 system time
 *
 * The time returned via the callback is in format YY/MM/DD,hh:mm:ss±zz\n
 * where characters indicate year (two last digits), month, day, hour,
 * minute, second and time zone (indicates the difference, expressed in\n
 * quarters of an hour, between the local time and GMT; the range is -96 to +96).\n
 * For instance, "6th of May 2014, 22:10:00 GMT+2" equals "14/05/06,22:10:00+08"
 * @param pGetTimeCallbackFunc: Pointer to the output callback function
 * @param TimeoutMs: Timeout in ms after which the request is cancelled
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_GetTime(ST87EC_Lib_GetTimeCallback_t* pGetTimeCallbackFunc, uint32_t TimeoutMs)
{
    ST87EC_Lib_Result_t result;

    if (pGetTimeCallbackFunc == NULL) {
        result = RESULT_BAD_PARAM;
    } else if (EcLibVars.OnGoingSequence == SEQUENCE_NONE) {
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_GET_TIME;
        EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_INIT;
        /* Save the parameters */
        EcLibVars.SequenceSys.GetTime.Params.pGetTimeCallbackFunc = pGetTimeCallbackFunc;
        EcLibVars.SequenceSys.GetTime.Params.TimeoutMs = TimeoutMs;
        result = RESULT_OK;
    } else {
        result = RESULT_BUSY;
    }

    return (result);
}

/**
 * @brief API to get the GNSS ST87M01 position.
 *
 * The position returned by the callback is in ST_AT format by default and can be set to NMEA format in the config file.
 *
 * For instance for ST_AT: "#GNSSFIX: 2291,461282763,48.15370,-01.56719,130.2,33.6,00.0,00.0,52.2,2.4,9.5,7.7,5,11,2.4,30,-0.1,20,1.4,09,1.7,06,-0.3" \n
 * In detail: " #GNSSFIX: <week_number>,<time_of_week>,<latitude>,<longitude>,<altitude>,<accuracy> " \n
 * " [,<std_dev_latitude>,<std_dev_longitude>,<std_dev_altitude>,<hdop>,<gdop>,<pdop>] " <- If accuracy data is enabled  in config \n
 * " [,<number_of_satellites>,<satellite1_id>,<satellite1_residual>,...,<satelliten_id>,<satelliten_residual>] " <- If satellites info is enabled  in config \n
 * " [,<orientation_degree>,<semi_major>,<semi_minor>] " <- If orientation info is enabled  in config \n\n
 *
 * For instance for NMEA: "$GPGLL,4809.2231,N,00134.281,W,091218,A,A*6C"\n
 * For further information: https://en.wikipedia.org/wiki/NMEA_0183
 *
 * @param NbPosition: Number of GNSS position to sample
 * @param pGetPosCallbackFunc: Pointer to the output callback function
 * @param TimeoutMs: Timeout in ms after which the request is cancelled
 * @retval API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_GNSS_GetFix(uint32_t NbPosition, ST87EC_Lib_GNSS_GetPosCallback_t* pGetPosCallbackFunc, uint32_t TimeoutMs)
{
    ST87EC_Lib_Result_t result;

    if (NbPosition == 0 || NbPosition > GNSS_NB_MAX_POSITION || pGetPosCallbackFunc == NULL) {
        result = RESULT_BAD_PARAM;
    }
    /* Register the request in the EC library */
    else if (EcLibVars.OnGoingSequence == SEQUENCE_NONE) {
        EcLibVars.OnGoingSequence = SEQUENCE_GNSS_GET_FIX;
        EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_INIT;

        /* Init the sequence parameters */
        EcLibVars.SequenceGnss.GetFix.Params.NbPosition = NbPosition;
        EcLibVars.SequenceGnss.GetFix.Params.pGetFixCallbackFunc = pGetPosCallbackFunc;
        EcLibVars.SequenceGnss.GetFix.Params.TimeoutMs = TimeoutMs;
        EcLibVars.SequenceGnss.GetFix.Params.CountPos = 0;
        EcLibVars.SequenceGnss.GetFix.Params.InitDone = 0;

        result = RESULT_OK;
    } else {
        result = RESULT_BUSY;
    }

    return (result);
}

/**
 * @brief API to publish data with MQTT
 *
 * @param pMqttObject: The MQTT object structure
 * @retval API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_MqttPublish(const ST87EC_Lib_MqttObject_t* pMqttObject)
{
    ST87EC_Lib_Result_t result;

    if ((EcLibVars.OnGoingSequence == SEQUENCE_NONE)
        && ((EcLibVars.ModuleStatus.TransferOnGoing == 0) || (EcLibVars.ModuleStatus.TransferOnGoing == (uint8_t)SEQUENCE_NBIOT_MQTT_PUBLISH))) {
        /* Init the sequence parameter */
        EcLibVars.SequenceNbiot.MqttPublish.Params.BrokerAddress = pMqttObject->IpAddress;
        EcLibVars.SequenceNbiot.MqttPublish.Params.BrokerPort = pMqttObject->PortNumber;
        EcLibVars.SequenceNbiot.MqttPublish.Params.Message = pMqttObject->pMessage;
        EcLibVars.SequenceNbiot.MqttPublish.Params.Topic = pMqttObject->pTopic;
        EcLibVars.SequenceNbiot.MqttPublish.Params.LastPublish = pMqttObject->LastPublish;
        EcLibVars.SequenceNbiot.MqttPublish.Params.TimeoutMs = pMqttObject->TimeoutMs;

        EcLibVars.SequenceNbiot.MqttPublish.Status = MQTT_NOT_CONNECTED;
        EcLibVars.SequenceNbiot.MqttPublish.Command = MQTTPUBLISH_COMMAND_CHECK_CONNECT;
        EcLibVars.SequenceNbiot.MqttPublish.FsmState = MQTTPUBLISH_STATE_INIT;

        EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_TEST;

        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_NBIOT_MQTT_PUBLISH;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

        result = RESULT_OK;
    } else {
        result = RESULT_BUSY;
    }

    return (result);
}

/**
 * @brief API to enable a UDP transfer
 *
 * @param pUdpObject: The UDP object structure
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_UdpTransferData(ST87EC_Lib_UdpTcpObject_t* pUdpObject)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((pUdpObject->pDataTx == NULL) && (pUdpObject->pTransferReadCallbackFunc == NULL)) {
        /* Are available: Rx/Tx transfer or at least Rx transfer only or Tx transfer only */
        result = RESULT_BAD_PARAM;
    } else if ((EcLibVars.OnGoingSequence == SEQUENCE_NONE)
        && ((EcLibVars.ModuleStatus.TransferOnGoing == 0) || (EcLibVars.ModuleStatus.TransferOnGoing == (uint8_t)SEQUENCE_UDP_TRANSFER))) {
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_UDP_TRANSFER;
        if (EcLibVars.ModuleStatus.TransferOnGoing == 0) {
            EcLibVars.ModuleStatus.NbUdpPacketsSent = 0; /* Reset Nb packet sent in case it is a brand new UDP transfer */
        }
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

        /* Save the parameters */
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.IpAddr = pUdpObject->IpAddr;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb = pUdpObject->PortNb;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.pDataTx = pUdpObject->pDataTx;
#if (UDP_DATA_TYPE == 1)
        {
            /* Means binary data : the DataLength parameter is present */
            EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.DataTxLength = pUdpObject->DataTxLength;
        }
#endif

        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.LastPacket = pUdpObject->LastPacket;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.TimeoutMs = pUdpObject->TimeoutMs;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.p_TransferCallbackFunc = pUdpObject->pTransferReadCallbackFunc;

        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_INIT;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.EndSubFsmState = UDPTCP_TRANSFER_ENDPROC_STATE_START;
        EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_TEST;

        result = RESULT_OK;
    } else {
        result = RESULT_BUSY;
    }
    return result;
}

/**
 * @brief API to enable a TCP transfer
 *
 * @param pTcpObject: The TCP object structure
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_TcpTransferData(ST87EC_Lib_UdpTcpObject_t* pTcpObject)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((pTcpObject->pDataTx == NULL) && (pTcpObject->pTransferReadCallbackFunc == NULL)) {
        /* Are available: Rx/Tx transfer or at least Rx transfer only or Tx transfer only */
        result = RESULT_BAD_PARAM;
    } else if ((EcLibVars.OnGoingSequence == SEQUENCE_NONE)
        && ((EcLibVars.ModuleStatus.TransferOnGoing == 0) || (EcLibVars.ModuleStatus.TransferOnGoing == (uint8_t)SEQUENCE_TCP_TRANSFER))) {
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_TCP_TRANSFER;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

        /* Save the parameters */
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.IpAddr = pTcpObject->IpAddr;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb = pTcpObject->PortNb;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.pDataTx = pTcpObject->pDataTx;
#if (TCP_DATA_TYPE == 1)
        /* Means binary data : the DataLength parameter is present */
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.DataTxLength = pTcpObject->DataTxLength;
#endif

        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.LastPacket = pTcpObject->LastPacket;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.TimeoutMs = pTcpObject->TimeoutMs;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.p_TransferCallbackFunc = pTcpObject->pTransferReadCallbackFunc;

        EcLibVars.SequenceNbiot.UdpTcpTransfer.FsmState = UDPTCP_TRANSFER_STATE_INIT;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.TcpConnSubFsmState = UDPTCP_TRANSFER_TCPCONN_STATE_TEST;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.EndSubFsmState = UDPTCP_TRANSFER_ENDPROC_STATE_START;
        EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_TEST;

        result = RESULT_OK;
    } else {
        result = RESULT_BUSY;
    }
    return result;
}

/**
 * @brief API to open a CoAP Tx and/or Rx transfer session
 *
 * @param pIpAddr: IP address of the remote target (IPV4 or IPV6 address string)
 * @param PortNb: CoAP port number
 * @param Timeout: Timeout in ms after which the request is cancelled
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapOpen(char* pIpAddr, uint32_t PortNb, uint32_t Timeout)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((pIpAddr == NULL) || (PortNb == 0) || (Timeout == 0)) {
        result = RESULT_BAD_PARAM;
    } else if ((EcLibVars.OnGoingSequence != SEQUENCE_NONE) || (EcLibVars.ModuleStatus.TransferOnGoing != 0)) {
        result = RESULT_BUSY;
    } else if (EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_NONE) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_COAP_OPEN;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

        /* Save the parameters */
        EcLibVars.SequenceNbiot.CoapTransfer.Params.pIpAddress = pIpAddr;
        EcLibVars.SequenceNbiot.CoapTransfer.Params.PortNb = PortNb;
        EcLibVars.SequenceNbiot.CoapTransfer.Params.Timeout = Timeout;
        EcLibVars.SequenceNbiot.CoapTransfer.CoapOpen.FsmState = COAP_OPEN_STATE_INIT;
        EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_TEST;
    }

    return result;
}

/**
 * @brief API to transmit a CoAP command (auto-IP configuration)
 *
 * @param pCoapTxObject: the CoAP Tx parameter structure
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapTransmit(ST87EC_Lib_CoapTxObject_t* pCoapTxObject)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((EcLibVars.OnGoingSequence != SEQUENCE_NONE)
        || (EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState == COAP_RX_STATE_PAYLOAD_DATA_RECV)) {
        /* If any EC Lib sequence is on-going or a CoAP payload data reception is on-going, raise busy */
        result = RESULT_BUSY;
    } else if ((EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_OPENED)
        && (EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_RXTX)) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_COAP_RXTX;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

        /* Save the parameters */
        memcpy(&EcLibVars.SequenceNbiot.CoapTransfer.Params.CoapTxParams, pCoapTxObject, sizeof(ST87EC_Lib_CoapTxObject_t));

        EcLibVars.SequenceNbiot.CoapTransfer.CoapTx.FsmState = COAP_TX_STATE_INIT;
    }

    return result;
}

/**
 * @brief API to register for CoAP data reception (auto-IP configuration)
 *
 * @param pCoapReceiveCallbackFunc: Pointer to user callback function
 *        that is called when some CoAP data are received
 * @param pCoapRxData: Pointer to the CoAP Rx data structure
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapReceive(ST87EC_Lib_CoapReceiveCallback_t* pCoapReceiveCallbackFunc, ST87EC_Lib_CoapRxData_t* pCoapRxData)
{
    ST87EC_Lib_Result_t result = RESULT_OK;
    /* This function only registers for CoAP data reception, there is no sequencing.
       The only condition to enable CoAP Rx is that CoAP transfer is already open  */
    if ((EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_OPENED)
        && (EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_RXTX)) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Save the parameters */
        EcLibVars.SequenceNbiot.CoapTransfer.Params.pCoapReceiveCallbackFunc = pCoapReceiveCallbackFunc;
        EcLibVars.SequenceNbiot.CoapTransfer.Params.pCoapRxData = pCoapRxData;
        /* Update CoAP FSM state */
        EcLibVars.SequenceNbiot.CoapTransfer.FsmState = COAP_TRANSFER_RXTX;
        EcLibVars.SequenceNbiot.CoapTransfer.CoapRxState = COAP_RX_STATE_URC_RECV;
        /* Enable CoAP data reception */
        EcLibVars.RspInfo[COAPDATA_INDEX].Validity = RSP_AWAITED;
    }

    return result;
}

/**
 * @brief API to close a CoAP transfer session
 *
 * @param Timeout: Timeout in ms after which the request is cancelled
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapClose(uint32_t Timeout)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((EcLibVars.OnGoingSequence != SEQUENCE_NONE)
        || ((EcLibVars.ModuleStatus.TransferOnGoing != SEQUENCE_COAP_OPEN) /* if another transfer (TCP,..) is on-going, do not close CoAP as it will close the other transfer's socket! */
            && (EcLibVars.ModuleStatus.TransferOnGoing != SEQUENCE_COAP_RXTX))) {
        result = RESULT_BUSY;
    } else if ((EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_OPENED)
        && (EcLibVars.SequenceNbiot.CoapTransfer.FsmState != COAP_TRANSFER_RXTX)) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Save the parameter */
        EcLibVars.SequenceNbiot.CoapTransfer.Params.Timeout = Timeout;
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_COAP_CLOSE;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;
        EcLibVars.SequenceNbiot.CoapTransfer.CoapClose.FsmState = COAP_CLOSE_STATE_INIT;
    }
    return result;
}

/**
 * @brief API to start a HTTP/HTTPS session
 *
 * @param pHost: URL of the remote server (ex: st.com)
 * @param PortNb: TCP port number
 * @param SecureId: Secure Profile Id (relevant for HTTPS, set to -1 for HTTP)
 * @param Timeout: Timeout in ms after which the request is cancelled
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_HttpOpen(char* pHost, uint32_t PortNb, int32_t SecureId, uint32_t Timeout)
{
    ST87EC_Lib_Result_t result = RESULT_OK;
    if ((pHost == NULL) || (PortNb == 0) || (Timeout == 0) || (SecureId < (-1))) {
        result = RESULT_BAD_PARAM;
    } else if ((EcLibVars.OnGoingSequence != SEQUENCE_NONE) || (EcLibVars.ModuleStatus.TransferOnGoing != 0)) {
        result = RESULT_BUSY;
    } else if (EcLibVars.SequenceNbiot.HttpTransfer.FsmState != HTTP_TRANSFER_NONE) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_HTTP_OPEN;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

        /* Save the parameters */
        EcLibVars.SequenceNbiot.HttpTransfer.Params.pHost = pHost;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.Params.PortNb = PortNb;
        EcLibVars.SequenceNbiot.TransferCfg.SecProfileId = SecureId;
        EcLibVars.SequenceNbiot.HttpTransfer.Params.Timeout = Timeout;

        EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.FsmState = HTTP_OPEN_STATE_INIT;
        EcLibVars.SequenceNbiot.HttpTransfer.HttpOpen.Command = HTTP_OPEN_CMD_SOCKET_CREATE;
        EcLibVars.SequenceNbiot.SocketCreation.FsmState = SOCKET_CREATION_TEST;
        EcLibVars.SequenceNbiot.UdpTcpTransfer.TcpConnSubFsmState = UDPTCP_TRANSFER_TCPCONN_STATE_TEST;
    }
    return result;
}

/**
 * @brief API for using HTTP method (GET,POST,HEAD or PUT). Need to start a HTTP session first.
 * For Instance:
 * pHttpRawInStr= "POST /Path HTTP/1.1\r\n"
 *           "Host: st.com\r\n"
 *           "Accept: application/json\r\n"
 *           "Content-Type: application/json\r\n"
 *           "Content-Length: 18\r\n"
 *           "\r\n"
 *           "{"Hello":"World"}"
 *
 * @param pHttpTransferObject: Pointer to the HTTP transfer input parameter object
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_HttpTransfer(ST87EC_Lib_HttpTransferObject_t* pHttpTransferObject)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((pHttpTransferObject->pHttpRawInStr == NULL)
        || (pHttpTransferObject->Timeout == 0)
        || ((pHttpTransferObject->KeepAlive != 0) && (pHttpTransferObject->KeepAlive != 1))) {
        result = RESULT_BAD_PARAM;
    } else if (EcLibVars.OnGoingSequence != SEQUENCE_NONE) {
        result = RESULT_BUSY;
    } else if ((EcLibVars.SequenceNbiot.HttpTransfer.FsmState != HTTP_TRANSFER_OPENED)
        && (EcLibVars.SequenceNbiot.HttpTransfer.FsmState != HTTP_TRANSFER_RXTX)) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Save the parameters */
        EcLibVars.SequenceNbiot.HttpTransfer.Params.pHttpRawInStrCopy = strdup(pHttpTransferObject->pHttpRawInStr);
        if (EcLibVars.SequenceNbiot.HttpTransfer.Params.pHttpRawInStrCopy == NULL) {
            result = RESULT_KO;
        } else {
            EcLibVars.SequenceNbiot.HttpTransfer.Params.Timeout = pHttpTransferObject->Timeout;
            EcLibVars.SequenceNbiot.HttpTransfer.Params.KeepAlive = pHttpTransferObject->KeepAlive;
            EcLibVars.SequenceNbiot.HttpTransfer.Params.HttpTransferReadCallbackFunc = pHttpTransferObject->pHttpRxCallbackFunc;

            /* Register the request in the EC library */
            EcLibVars.OnGoingSequence = SEQUENCE_HTTP_TRANSFER;
            EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;
            EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.FsmState = HTTP_TRANSFER_STATE_INIT;
            EcLibVars.SequenceNbiot.HttpTransfer.HttpRxTx.Command = HTTP_TRANSFER_CMD_METHOD;
        }
    }
    return result;
}

/**
 * @brief API stop a HTTP transfer session. Need to start a HTTP session first.
 *
 * @param Timeout: Timeout in ms after which the request is cancelled
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_HttpClose(uint32_t Timeout)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((EcLibVars.SequenceNbiot.HttpTransfer.FsmState != HTTP_TRANSFER_OPENED)
        && (EcLibVars.SequenceNbiot.HttpTransfer.FsmState != HTTP_TRANSFER_RXTX)) {
        result = RESULT_BAD_SEQUENCING;
    } else {
        /* Save the parameter */
        EcLibVars.SequenceNbiot.HttpTransfer.Params.Timeout = Timeout;

        /* Register the request in the EC library */
        EcLibVars.OnGoingSequence = SEQUENCE_HTTP_CLOSE;
        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;
        EcLibVars.SequenceNbiot.HttpTransfer.HttpClose.FsmState = HTTP_CLOSE_STATE_INIT;
    }
    return result;
}

ST87EC_Lib_Result_t ST87EC_Lib_WMBUS_Transfer(const ST87EC_Lib_WmbusObject_t* pWmbusObject)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if (pWmbusObject->DataLen > WMBUS_TX_BUFFER_SIZE) {
        result = RESULT_BAD_PARAM;
    } else {
        if ((0 == EcLibVars.ModuleStatus.TransferOnGoing) || (SEQUENCE_WMBUS_TRANSFER == EcLibVars.ModuleStatus.TransferOnGoing)) {
            if ((EcLibVars.OnGoingSequence == SEQUENCE_NONE) || (EcLibVars.OnGoingSequence == SEQUENCE_WMBUS_TRANSFER)) {
                ST87EC_Lib_WmbusTransfer_t* pCtx = &EcLibVars.SequenceWmbus.Transfer;

                if (EcLibVars.OnGoingSequence == SEQUENCE_NONE) {
                    if (pWmbusObject->DataLen > 0) {
                        pCtx->FsmState = WMBUS_TRANSFER_STATE_INIT;
                        pCtx->DataLen = 0;
                        pCtx->NAcc = 0;
                        EcLibVars.OnGoingSequence = SEQUENCE_WMBUS_TRANSFER;
                        EcLibVars.ModuleStatus.TransferOnGoing = (uint8_t)EcLibVars.OnGoingSequence;

                        pCtx->LastPacket = pWmbusObject->LastPacket;
                        pCtx->Callback = pWmbusObject->Callback;
                        pCtx->UsrPtr = pWmbusObject->UsrPtr;
                        pCtx->Asynch = pWmbusObject->Asynch;

                        memcpy(&pCtx->DataBuffer[0], &pWmbusObject->DataPtr[0], pWmbusObject->DataLen);
                        pCtx->DataLen = pWmbusObject->DataLen;

                        result = RESULT_OK;
                    } else {
                        /* Avoid start WMBUS sequence if first frame is zero length */
                        result = RESULT_BAD_PARAM;
                    }
                }
                /* If sequence is already running */
                else {
                    /* check no other transfer is ongoing */
                    if (0 == pCtx->DataLen) {
                        /* Zero length is allowed only for Last Frame: it is used to stop sequence immediately */
                        if ((pWmbusObject->DataLen > 0) || (LAST_PKT_TRUE == pWmbusObject->LastPacket)) {
                            pCtx->LastPacket = pWmbusObject->LastPacket;
                            pCtx->Callback = pWmbusObject->Callback;
                            pCtx->UsrPtr = pWmbusObject->UsrPtr;
                            pCtx->Asynch = pWmbusObject->Asynch;

                            if (pWmbusObject->DataLen > 0) {
                                memcpy(&pCtx->DataBuffer[0], &pWmbusObject->DataPtr[0], pWmbusObject->DataLen);
                            }

                            pCtx->DataLen = pWmbusObject->DataLen;

                            result = RESULT_OK;
                        } else {
                            result = RESULT_BAD_PARAM;
                        }
                    } else {
                        result = RESULT_BUSY;
                    }
                }
            } else {
                result = RESULT_BUSY;
            }
        } else {
            result = RESULT_BUSY;
        }
    }

    return (result);
}

/**
 * @brief API launching a ST87 binary loading
 *
 * @param BinaryId: name of the binary to upload to ST87 Boot
 * @param BinLength: Length of the binary to load
 * @param Timeout: Timeout in ms after which the request is cancelled
 * @retval Result: API execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_Loader(ST87EC_Lib_BinToLoad_t BinaryId, uint32_t BinLength, uint32_t Timeout)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    if ((BinLength == 0) || (Timeout == 0)) {
        result = RESULT_BAD_PARAM;
    } else if (EcLibVars.OnGoingSequence != SEQUENCE_NONE) {
        result = RESULT_BUSY;
    } else {
        /* Save the parameter */
        EcLibVars.SequenceSys.Loader.Params.BinaryId = BinaryId;
        EcLibVars.SequenceSys.Loader.Params.BinaryLength = BinLength;
        EcLibVars.SequenceSys.Loader.Params.Timeout = Timeout;
        /* Register the request in the EC library */

        EcLibVars.OnGoingSequence = SEQUENCE_LOADER;
        EcLibVars.SequenceSys.Loader.FsmState = LOADER_STATE_INIT;
    }
    return result;
}

ST87EC_Lib_BinTransferStatus_t ST87EC_Lib_NBIOT_GetBinDataForLoaderCallback(uint32_t * pBinDataAddr, uint8_t NbBytesToTransfer) {
	pBinDataAddr = 0x00;
	return ST87_BIN_DATA_TRANSFER_KO;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
