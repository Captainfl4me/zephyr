/**
  ******************************************************************************
  * @file    st87ec_config.h
  * @author  APMS Application Team
  * @brief   Configuration header file
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
#ifndef ST87EC_CONFIG_H
#define ST87EC_CONFIG_H

#include "../../Core/inc/st87ec_config_default.h"

/* Private constants and macros-----------------------------------------------*/

/* Parameters to be configured for internal timers */
#define TIMER_NB                      10           /*!< Number of timers                                                                           */
#define TIMER_TICK_PERIOD_MS          1            /*!< Tick period of timers in ms                                                                */

/* Parameters to be configured for the socket creation */
#define SOCKET_SEND_TIMEOUT           10           /*<! Maximum time needed for transmission of packet over the air and reception of acknowledgment */
#define SOCKET_RECEIVE_TIMEOUT        10           /*<! Maximum time needed for transmission of packet over the air                                 */
#define SOCKET_FRAME_RECEIVED_URC     1            /*<! Indicates if an URC (#IPRECV) shall be generated when TCP or UDP data are available and can
                                                        be read with AT#IPREAD command */
#define SOCKET_SECURITY_PROFILE_ID    0            /*<! If specified and if the security profile exists, a TLS link will be used for the socket.    */

/* Parameters to be configured for the UDP transfer */
#define UDP_DATA_TYPE                 0            /*<!Type of data to transmit:
                                                      0: ASCII String as next parameter
                                                      1: Binary data
                                                      2: Hex data in text mode: 01A34B... = 0x01 0xA3 0x4B*/

/* Parameters to be configured for MQTT transfer */
#define MQTTCFG_CLIENT_NAME            "ST87"       /*!< Name of the MQTT client that is provided to the broker.*/
#define MQTTCFG_CONNEXION_TIMEOUT      10           /*!< Connection to the broker timeout in seconds.            */
#define MQTTCFG_PROTOCOL_TIMEOUT       10           /*!< Connection to the broker timeout in seconds.            */
#define MQTTCFG_PUBLISH_RETRY          10           /*!< Number of time a publish message is sent to the broker in case of an issue.*/
#define MQTTCFG_KEEP_ALIVE_PB_MSG      5            /*!< Number of time a publish message is sent to the broker in case of an issue.*/

/* Parameters to be configured for the TCP transfer */
#define TCP_DATA_TYPE                  0            /*<!Type of data to transmit:
                                                      0: ASCII String as next parameter
                                                      1: Binary data
                                                      2: Hex data in text mode: 01A34B... = 0x01 0xA3 0x4B*/

/* Parameters to be configured for GNSS config */
#define GNSS_CONSTELLATION_ID          0            /*!< Release assistance information.\n
                                                          0: GPS,\n
                                                          1: GALLILEO,\n
                                                          2: GPS+GALLILEO */

#define GNSS_FORMAT_TYPE               0            /*!< Release assistance information.\n
                                                           0: ST_AT,
                                                           1: NMEA */

/* Parameter used if ST_AT format enabled */
#define GNSS_FORMAT_ST_POSITION        1            /*!< Position data            */
#define GNSS_FORMAT_ST_ACCURACY        0            /*!< Accuracy data            */
#define GNSS_FORMAT_ST_SATELLITES      0            /*!< Satellites info          */
#define GNSS_FORMAT_ST_ORIENTATION     0            /*!< Orientation info         */

/* Parameter used if NMEA format enabled */
/* Warning: If the NMEA format is active, please change the size of GNSS_FIX_MAX_DATA_SIZE */
#define GNSS_NMEA_GPGGA                0            /*!< $GPGGA info              */
#define GNSS_NMEA_GPGSA                0            /*!< $GPGSA info              */
#define GNSS_NMEA_GPGSV                0            /*!< $GPGSV info              */
#define GNSS_NMEA_GPGLL                1            /*!< $GPGLL info              */
#define GNSS_NMEA_GPRMC                0            /*!< $GPRMS info              */
#define GNSS_NMEA_GPVTG                0            /*!< $GPVTG info              */

#define GNSS_NB_MAX_POSITION           100          /*!< Maximum number of sample positions       */

/**
 *  WMBUS USER CONFIGURATION
 *  @{
 */
#define WMBUS_TX_BUFFER_SIZE          (290)                           /*!< TX Buffer Size (maximum payload size required by application)  */
#define WMBUS_WAKEUP_BACKOFF_MS       (100)                           /*!< Time to wakeup device before ::WMBUS_CFG_Tnom, after completion of synch transfer  */
#define WMBUS_PHY_MODE                WMBUS_PHY_MODE_T1               /*!< Use T1 or C1 mode */
#define WMBUS_TX_POWER                (0)                             /*!< TX power range @note must be between [::WMBUS_TxPower_MIN, ::WMBUS_TxPower_MAX]  */
#define WMBUS_T_NOM                   (20)                            /*!< Nominal synch transmission interval in [sec] @note must be between [::WMBUS_Tnom_MIN, WMBUS_Tnom_MAX] */
#define WMBUS_DEV_TYPE                WMBUS_DEV_TYPE_METER            /*!< @note user can only select meter mode */
#define WMBUS_FRAME_FORMAT            WMBUS_FRAME_FORMAT_A            /*!< Frame format A or B */
#define WMBUS_POSTAMPLE_LENGTH        WMBUS_POSTAMBLE_LENGTH_T1_MIN    /*!< only for T1 mode */
#define WMBUS_HEADER_LENGTH           WMBUS_DEF_HEADER_LENGTH
/** @} */

/* Types ---------------------------------------------------------------------*/




#endif /* ST87EC_CONFIG_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
