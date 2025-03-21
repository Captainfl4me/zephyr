/**
  ******************************************************************************
  * @file    st87ec_cold_config.h
  * @author  APMS Application Team
  * @brief   Cold configuration header file
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
#ifndef ST87EC_COLD_CONFIG_H
#define ST87EC_COLD_CONFIG_H

/* Private constants and macros-----------------------------------------------*/
#define ST87EC_COLD_CONFIG_VERSION     2

#define ST87EC_COLD_VERSION_INDEX      (8)
#define ST87EC_COLD_VERSION_NVM_PAGE   5
#define ST87EC_COLD_VERSION_NVM_OFFSET 12

/****************************** HW configuration ****************************/
/* Temperature limit */
#define TEMP_LOW_SHUTDOWN  -45   /*!< Signed integer type Temperature low threshold in Celsius degree for shutdown display and shutdown */
#define TEMP_HIGH_SHUTDONW 110   /*!< Signed integer type Temperature high threshold in Celsius degree for shutdown display and shutdown */
#define TEMP_SHUTDOWN      1     /*!< Integer type 0: Disable shutdown if the shutdown threshold is reached
                                                 1: Enable shutdown if the shutdown threshold is reached */

/* Battery level settings */
#define VBAT_LOW_SHUTDOWN  2000 /*!< Integer type Low battery threshold in mV for shutdown display and shutdown */
#define VBAT_HIGH_SHUTDOWN 3200 /*!< Integer type High battery threshold in mV for shutdown display and shutdown */
#define VBAT_SHUTDOWN      1    /*!< Integer type 0: Disable shutdown if the shutdown threshold is reached
                                                1: Enable shutdown if the shutdown threshold is reached */

/****************************** NBIOT configuration ****************************/
/* Band selection : configure the band usage and split between various NMO */
#define BANDLIST      "20,8"              /*!< Band selected comma separated  */
#define BANDCFG       "0,0,20,01,7910"    /*!< Band=20, Option=01, StartFreq=7910 */
#define BANDCFG_NMO1  "0,1,0,2,1,100,0"   /*!< Pref=0, Guard=2, In=1, BW=100, OffsetFreq=0 */
#define BANDCFG_NMO2  "0,2,0,2,1,100,100" /*!< Pref=0, Guard=2, In=1, BW=100, OffsetFreq=100 */
#define BANDCFG_NMO3  "0,3,0,2,1,100,200" /*!< Pref=0, Guard=2, In=1, BW=100, OffsetFreq=200 */

/* EDRX Setting */
#define EDRX_VALUE    1011       /*!< Requested EDRX value "1011" -> 655.36s See 3GPP 24.008 Table 10.5.5.32 */

/* Paging Time Window*/
#define PTW_VALUE     0011       /*!< PTW value "0011" -> 10.24s See 3GPP TS 24.008 Table 10.5.5.32 */

/* Power saving mode setting */
#define PSM_ENABLE    1
#define PERIODIC_TAU  00100001  /*!< TAU value (T3412) "00100001" -> 1H See 3GPP TS 24.008 Table 10.5.5.32 */
                                /*!< TAU value (T3412) "00111000" -> 24H See 3GPP TS 24.008 Table 10.5.5.32 */
#define ACTIVE_TIME   00000101  /*!< Active time (T3324) "00000101" */

/* IP configuration */
#define NB_PACKET_SENT_ENABLE  1  /*!< Activation of the counting of the number of UDP packets actually received and acknowledged by the eNodeB.
                                       However, it does not guarantee that the packet has been received by the remote server.
                                       If NB_PACKET_SENT is 1, counting is active.
                                          NbUdpPacketsSent counter var in EC Lib State structure (ST87EC_Lib_Status_t) will reflect the counting.
                                       If NB_PACKET_SENT is 0, counting is inactive.
                                          NbUdpPacketsSent counter var in EC Lib State structure (ST87EC_Lib_Status_t) keeps 0 value.           */
#define DOMAIN_NAME "8.8.8.8"    /*!< IP address for DNS resolution    */

/****************************** ST87 configuration ****************************/
/* Sleep mode configuration */
#define HOLD_TIME  10   /*!< Integer type Time in seconds between the last AT command and the sleep mode entry */
#define AWAKE_TIME 0    /*!< Integer type Define the timeout in seconds that the module is awake at each wake up (telecom activity, AT command activity..)*/

/*Parameters to be configured for Ring pin setup*/
#define RING_PIN_ENABLE      0           /*!< Ring pin enable */
#define RING_PIN_GPIO        10          /*!< Ring pin number to be set for ST87M01. The GPIO number between 8 and 31                    */
#define RING_PIN_POLARITY    1           /*!< Ring pin voltage polarity (0: active low and 1: active high).                              */
#define RING_PIN_DELAY       200         /*!< The time in ms when ring pin is active (min value: 10ms and max value:300ms by 10ms steps) */


#endif /* ST87EC_COLD_CONFIG_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
