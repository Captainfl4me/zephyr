/**
  ******************************************************************************
  * @file    st87ec_lib.h
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
#ifndef ST87EC_LIB_H
#define ST87EC_LIB_H
#include <stdint.h>

/* Exported constants and macros-----------------------------------------------*/
/**
 * ST87 EC library version
 */
#define ST87EC_LIB_MAJOR_VERSION                                            (1)
#define ST87EC_LIB_MINOR_VERSION                                            (3)
#define ST87EC_LIB_INTERNAL_VERSION                                         (0)


#define ST87EC_LIB_TCPIP_IPV6_MAX_TEXTUAL_ADDRESS                   (8 * 4 + 7)
#define ST87EC_LIB_COAP_TOKEN_STR_MAX_LEN                               (8 + 1)
#define ST87EC_LIB_COAP_OPTION_STR_MAX_LEN                            (512 + 1)
#define ST87EC_LIB_COAP_PAYLOAD_STR_MAX_LEN                           (512 + 1)


#define ST87EC_LIB_HTTP_HEADERS_NB_MAX                                     (20)

/* Types ---------------------------------------------------------------------*/

/** 
 * Output value of the API functions.
 */
typedef enum{
  RESULT_OK = 0,                                  /**< Function execution status is OK.       */
  RESULT_KO = 1,                                  /**< Function execution status is KO.       */
  RESULT_BUSY = 2,                                /**< Function execution status is busy      */
  RESULT_BAD_PARAM = 3,                           /**< Function gets bad parameter            */
  RESULT_BAD_SEQUENCING = 4                       /**< Function gets bad FSM state sequencing */
} ST87EC_Lib_Result_t;

/** 
 * Output value of the SIM status.
 */
typedef enum{
  SIM_STATUS_SIM_INVALID = 0,                     /**< SIM is invalid.            */
  SIM_STATUS_SIM_VALID   = 1,                     /**< SIM is valid.              */
  SIM_STATUS_UNKNOWN     = 0xFF,                  /**< SIM status is unknown.     */
} ST87EC_Lib_SimStatus_t; 

/**
 * Output value of the module initialization.
 */
typedef enum{
  INIT_NOT_COMPLETE = 0,                          /**< Module initialization is not complete. */
  INIT_COMPLETE = 1,                              /**< Module initialization is complete.     */
} ST87Ec_Lib_InitComplete_t;

/** 
 * Output value of the connection status.
 */
typedef enum{
  CONN_STATUS_IDLE         = 0,                   /**< The stack is in IDLE state.          */
  CONN_STATUS_CONNECTED    = 1,                   /**< The stack is in CONNECTED state.     */
  CONN_STATUS_UNKNOWN      = 0xFF,                /**< The stack is in unknown state.       */
} ST87EC_Lib_ConnectionStatus_t; 

/**
 * Output value of the registration status.
 */
typedef enum{
  NOT_REGISTERED           = 0,                   /**< The module is not registered to the network.*/
  REGISTERED               = 1,                   /**< The module is registered to the network.    */
} ST87EC_Lib_RegistrationStatus_t;

/**
 * Status of the Ring pin
 */
typedef enum {
  RINGPIN_DISABLE           = 0,                  /**< Disable the Ring pin                 */
  RINGPIN_ENABLE            = 1,                  /**< Enable the Ring pin                  */
} ST87EC_Lib_RingPinStatus_t;


/**
 * Indication if last data chunk to transfer
 */
typedef enum {
  LAST_PKT_FALSE                    = 0,          /**< It is not the last packet to transfer, do not close the link  */
  LAST_PKT_TRUE                     = 1,          /**< It is the last packet to transfer, close the link                 */
} ST87EC_Lib_LastPacket_t;


/** 
 * Sleep/wakeup status of the module.
 */
typedef enum{
  STATUS_SLEEP               = 0,                 /**< The Module is sleeping.              */
  STATUS_WAKEUP              = 1,                 /**< The module is woken up.              */
} ST87EC_Lib_SleepWakeupstatus_t;

/**
 * List of possible sequences.
 */
typedef enum{
  SEQUENCE_NONE                  = 0,              /**< No sequence running.                                    */
  SEQUENCE_COLD_INIT             = 1,              /**< Registration of the EC Cold parameter init request.     */
  SEQUENCE_GET_TIME              = 2,              /**< Registration of the EC Get Time request.                */
  SEQUENCE_GET_STATE             = 3,              /**< Registration of the EC Get State request.               */
  SEQUENCE_GNSS_GET_FIX          = 4,              /**< Registration of the EC GNSS Get Fix request.            */
  SEQUENCE_NBIOT_MQTT_PUBLISH    = 5,              /**< Registration of the EC NBIO Mqtt Publish request.       */
  SEQUENCE_UDP_TRANSFER          = 6,              /**< Registration of the UDP transfer request.               */
  SEQUENCE_TCP_TRANSFER          = 7,              /**< Registration of the TCP transfer request.               */
  SEQUENCE_COAP_OPEN             = 8,              /**< Registration of the CoAP open request.                  */
  SEQUENCE_COAP_RXTX             = 9,              /**< Registration of the CoAP transmit or receive request.   */
  SEQUENCE_COAP_CLOSE            = 10,             /**< Registration of the CoAP close request.                 */
  SEQUENCE_HTTP_OPEN             = 11,             /**< Registration of the HTTP open request.                  */
  SEQUENCE_HTTP_TRANSFER         = 12,             /**< Registration of the HTTP transfer request.              */
  SEQUENCE_HTTP_CLOSE            = 13,             /**< Registration of the HTTP close request.                 */
  SEQUENCE_WMBUS_TRANSFER        = 14,
  SEQUENCE_LOADER                = 15,             /**< Registration of the Loader request.                     */
} ST87EC_Lib_SequenceValue_t;

/**
 * EC Lib internal error codes.
 */
typedef enum{
  ERROR_NO_CODE                  = 0,              /**< No error code specified                                 */
  ERROR_ST87_WATCHDOG            = 1,              /**< Error code indicating ST87 raised a watchdog            */
} ST87EC_Lib_ErrorCode_t;

typedef enum{
  ST87_SEC_BINARY                = 0,              /**< Binary to load to ST87 Boot is: SEC                     */
  ST87_ALP_BINARY                = 1,              /**< Binary to load to ST87 Boot is: ALP                     */
  ST87_MODEM_BINARY              = 2,              /**< Binary to load to ST87 Boot is: MODEM                   */
  ST87_GNSS_BINARY               = 3,              /**< Binary to load to ST87 Boot is: GNSS                    */
  ST87_CAP_BINARY                = 4,              /**< Binary to load to ST87 Boot is: CAP                     */
} ST87EC_Lib_BinToLoad_t;

typedef enum{
  ST87_BIN_DATA_TRANSFER_OK      = 0,              /**< Binary transfer status OK user side for EC Lib ST87 Loader   */
  ST87_BIN_DATA_TRANSFER_KO      = 1,              /**< Binary transfer status KO user side for EC Lib ST87 Loader   */
  ST87_BIN_DATA_TRANSFER_BUSY    = 2,              /**< Binary transfer status BUSY user side for EC Lib ST87 Loader */
} ST87EC_Lib_BinTransferStatus_t;

/** 
 * Structure for the status request.
 */
typedef struct{
  ST87Ec_Lib_InitComplete_t InitComplete;            /**< Module initialization is complete                    */
  ST87EC_Lib_SimStatus_t SimStatus;                  /**< SIM status request value.                            */
  ST87EC_Lib_ConnectionStatus_t ConnectionStatus;    /**< NB-IOT connection status request value.              */
  ST87EC_Lib_RegistrationStatus_t RegistrationStatus;/**< EPS network registration status value.               */
  ST87EC_Lib_SleepWakeupstatus_t SleepWakeupstatus;  /**< Sleep or Wakeup status value.                        */
  ST87EC_Lib_SequenceValue_t  OnGoingSequence;       /**< Name of the current sequence                         */
  uint32_t NbUdpPacketsSent;                         /**< Number of UDP packets sent counter
                                                            (keeps 0 if NB_PACKET_SENT cold param value is 0)  */
  uint8_t TransferOnGoing;                           /**< Indication that a data transfer (TCP, UDP,...)
                                                                                 is on-going (0: no / >0: yes) */
} ST87EC_Lib_Status_t;

/**
* @brief Callback called to read the data sent by UDP transfer
*
* @param pString: pointer to the returned string (read only pointer).
*/
typedef void (ST87EC_Lib_UdpTcpTransferReadCallback_t) (char const * const pString);

/**
 * Structure for the interface for UDP transfer.
 */
typedef struct{
  char * IpAddr;                                  /**< IP address                                 */
  uint32_t PortNb;                                /**< Port number                                */
  char * pDataTx;                                 /**< Pointer on the data to transmit            */
  uint16_t DataTxLength;                          /**< Length of the data to transmit, meaningful
                                                                      only in case of binary transfer */
  ST87EC_Lib_UdpTcpTransferReadCallback_t * pTransferReadCallbackFunc; /**< Callback function that contains
                                                                      the data read.                  */
  ST87EC_Lib_LastPacket_t LastPacket;             /** Bit to indicate the link has to be closed   */
  uint32_t TimeoutMs;                             /**< Timeout in ms after which the request is cancelled.*/
} ST87EC_Lib_UdpTcpObject_t;


/**
 * Interface structure for MQTT Publish.
 */
typedef char string_t[];

typedef struct{
  char* IpAddress;                                /**< IPv4 or IPv6 address of the remote Broker.           */
  uint32_t PortNumber;                            /**< Port number of the remote Broker.                    */
  char* pTopic;                                   /**< Name of the topic to which the message will be sent. */
  char* pMessage;                                 /**< The content of the message to be sent.               */
  ST87EC_Lib_LastPacket_t LastPublish;             /**< Indicates if it's the last data to be published.     */
  uint32_t TimeoutMs;                             /**< Timeout in ms after which the request is cancelled.  */
} ST87EC_Lib_MqttObject_t;


/**
 * Interface structure for CoAP Tx transfer.
 */
typedef struct{
  char * pOption;                                /**< String containing the likely CoAP options to be appended.                            */
  uint8_t MessageType;                           /**< CoAP message Type (0: Confirmable, 1: Non-confirmable, 2: Acknowledgment, 3: Reset). */
  uint8_t Method;                                /**< CoAP method Type (1: GET, 2: POST, 3: PUT, 4: DELETE).                               */
  uint16_t Mid;                                  /**< CoAP message Id (integer value sequentially assigned).                               */
  uint8_t TxDataFormat;                          /**< Format type of the data to transmit (0: ASCII string, 2: Hex data in text mode).     */
  char * pToken;                                 /**< Pointer to the token Id (string of a hexadecimal random value).                      */
  char * pPath;                                  /**< Pointer to the string containing the resource URI path (max. length is 50 bytes).    */
  char * pPayload;                               /**< Pointer to the string data to transmit (max. length is 128 bytes).                   */
} ST87EC_Lib_CoapTxObject_t;


/**
 * Interface structure for CoAP Rx transfer.
 */
typedef struct{
  uint8_t Mid;                                   /**< Echoed CoAP message Id.                                                              */
  uint8_t MessageType;                           /**< CoAP message Type (0: Confirmable, 1: Non-confirmable, 2: Acknowledgment, 3: Reset). */
  uint16_t ResponseCode;                         /**< CoAP Response code.                                                                  */
  char * pToken;                                 /**< Echoed pointer to the token Id string (if available).                                */
  uint16_t OptionNb;                             /**< Number of defined options.                                                           */
  uint16_t PayloadLength;                        /**< Length of the payload in bytes.                                                      */
  char * pCurOptions;                            /**< String containing the current active options.                                        */
  char * pPayload;                               /**< Pointer to the received data string.                                                 */
} ST87EC_Lib_CoapRxData_t;


/**
 * Interface structure for HTTP transfer.
 */
typedef void (ST87EC_Lib_HttpTransferReadCallback_t) (char const * const pString);

typedef struct{
  char * pHttpRawInStr;                                        /**< Pointer to HTTP input raw data string.                                               */
                                                               /**<   This shall include method, header and potentially body.                            */
  ST87EC_Lib_HttpTransferReadCallback_t * pHttpRxCallbackFunc; /**< Pointer to callback function called by EC Lib at the end of the transfer.            */
  uint8_t KeepAlive;                                           /**< Connection Keep Alive information:                                                   */
                                                               /**<   0 : Indicate to HTTP server to released connection when data is received           */
                                                               /**<   1 : Indicate to HTTP server to keep alive the connection after data reception      */
  uint32_t Timeout;                                            /**< Timeout in ms after which the request is cancelled.                                  */
} ST87EC_Lib_HttpTransferObject_t;

typedef enum {
    WMBUS_NTY_Ready     ,   /**< keep-alive sent when device wakes up but there're no data to send */
    WMBUS_NTY_Sent      ,   /**< data has been successfully sent and sequence is ready to accept a new transfer */
    WMBUS_NTY_Done      ,   /**< sequence completed */
    WMBUS_NTY_Fail      ,   /**< fail during sequence */
}ST87EC_Lib_WmbusNty_t;

typedef void (*ST87EC_Lib_WmbusCallback_t) (ST87EC_Lib_WmbusNty_t Nty, void * UsrPtr);

typedef struct {
    void * UsrPtr;
    ST87EC_Lib_WmbusCallback_t Callback;
    const uint8_t * DataPtr;
    uint16_t DataLen;
    ST87EC_Lib_LastPacket_t LastPacket;
    uint8_t Asynch;                       /**< 0x00: synch transfer, !0x00: asynch transfer (send immediately) */
}ST87EC_Lib_WmbusObject_t;

/* Exported functions --------------------------------------------------------*/
/**
* @brief Callback called to inform User of an EC Lib internal error
*
* @param FaillingSequence: Id number of the on-going sequence (0 otherwise).
* @param Error: generic error code.
*/
typedef void (ST87EC_Lib_GenericErrorCallback_t) (ST87EC_Lib_SequenceValue_t FaillingSequence, ST87EC_Lib_ErrorCode_t Error);

/**
* @brief Callback called to get the ST87M01 system time
* 
* @param pString: pointer to the returned string (read only pointer).
*/
typedef void (ST87EC_Lib_GetTimeCallback_t) (char const * const pString);

/**
* @brief Callback called to get the GNSS ST87M01 position
*
* @param pString: pointer to the returned string (read only pointer).
*/
typedef void (ST87EC_Lib_GNSS_GetPosCallback_t) (char const * const pString);

/**
* @brief Callback called upon CoAP data reception
*
* @param pCoapRxData: pointer to the returned ST87EC_Lib_CoapRxData_t data structure.
*/
typedef void (ST87EC_Lib_CoapReceiveCallback_t) (ST87EC_Lib_CoapRxData_t * pCoapRxData);

ST87EC_Lib_Result_t ST87EC_Lib_Init(ST87EC_Lib_GenericErrorCallback_t * pGenericErrorCallbackFunc);
ST87EC_Lib_Result_t ST87EC_Lib_Reset(void);
ST87EC_Lib_Result_t ST87EC_Lib_Scheduler(void);
ST87EC_Lib_Result_t ST87EC_Lib_GetTime(ST87EC_Lib_GetTimeCallback_t * pGetTimeCallbackFunc, uint32_t TimeoutMs);
ST87EC_Lib_Result_t ST87EC_Lib_GetState(ST87EC_Lib_Status_t * pState);
ST87EC_Lib_Result_t ST87EC_Lib_GNSS_GetFix(uint32_t NbPosition, ST87EC_Lib_GNSS_GetPosCallback_t * pGetPosCallbackFunc, uint32_t TimeoutMs);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_MqttPublish(const ST87EC_Lib_MqttObject_t * pMqttObject);
ST87EC_Lib_Result_t ST87EC_Lib_UdpTransferData(ST87EC_Lib_UdpTcpObject_t * pUdpObject);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_TcpTransferData(ST87EC_Lib_UdpTcpObject_t * pTcpObject);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapOpen(char * pIpAddr, uint32_t PortNb, uint32_t Timeout);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapTransmit(ST87EC_Lib_CoapTxObject_t * pCoapTxObject);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapReceive(ST87EC_Lib_CoapReceiveCallback_t * pCoapReceiveCallbackFunc, ST87EC_Lib_CoapRxData_t * pCoapRxData);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_CoapClose(uint32_t Timeout);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_HttpOpen(char * pHost, uint32_t PortNb, int32_t SecureId, uint32_t Timeout);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_HttpTransfer(ST87EC_Lib_HttpTransferObject_t * pHttpTransferObject);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_HttpClose(uint32_t Timeout);
ST87EC_Lib_Result_t ST87EC_Lib_WMBUS_Transfer(const ST87EC_Lib_WmbusObject_t * pWmbusObject);
ST87EC_Lib_Result_t ST87EC_Lib_NBIOT_Loader(ST87EC_Lib_BinToLoad_t BinaryId, uint32_t BinLength, uint32_t Timeout);
ST87EC_Lib_BinTransferStatus_t ST87EC_Lib_NBIOT_GetBinDataForLoaderCallback(uint32_t * pBinDataAddr, uint8_t NbBytesToTransfer);

#endif /* ST87EC_LIB_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
