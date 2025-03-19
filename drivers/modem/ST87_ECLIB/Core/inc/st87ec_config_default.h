/**
  ******************************************************************************
  * @file    st87ec_config_default.h
  * @author  APMS Application Team
  * @brief   Default configuration header file
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
#ifndef ST87EC_CONFIG_DEFAULT_H
#define ST87EC_CONFIG_DEFAULT_H

/* Private constants and macros-----------------------------------------------*/

/**
 *  WMBUS
 *  @{
 */
/**
 *  CONSTANTS (shall not be changed)
 *  @{
 */
#define WMBUS_PHY_MODE_T1                (0x03)      /**< FSK 100 kcps – code 3 out of 6 */
#define WMBUS_PHY_MODE_C1                (0x05)      /**< FSK 100 kcps – code NRZ */
#define WMBUS_TX_POWER_MIN               (-20)       /**< Minimum RF output power [dBm] */
#define WMBUS_TX_POWER_MAX               (20)        /**< Maximum RF output power [dBm] */
#define WMBUS_T_NOM_MIN                  (16)        /**< Minimum Nominal Transmission interval [seconds] */
#define WMBUS_T_NOM_MAX                  (900)       /**< Maximum Nominal Transmission interval [seconds] */
#define WMBUS_DEV_TYPE_METER             (0)         /**< only supported mode */
#define WMBUS_DEV_TYPE_OTHER             (1)
#define WMBUS_DEV_TYPE_NOT_CONFIGURED    (2)
#define WMBUS_FRAME_FORMAT_A             (0)         /**< Frame format and synch pattern A */
#define WMBUS_FRAME_FORMAT_B             (1)         /**< Frame format and synch pattern B */
#define WMBUS_HEADER_LENGTH_T1_MIN       (0x0013)    /**< Must be >= 19 for T1 */
#define WMBUS_HEADER_LENGTH_T1_MAX       (0x001B)    /**< Must be <= 27 for T1 */
#define WMBUS_HEADER_LENGTH_C1           (0x0010)    /**< Must be == 16 for C1 */
#define WMBUS_POSTAMBLE_LENGTH_T1_MIN    (0x01)      /**< only for T1 */
#define WMBUS_POSTAMBLE_LENGTH_T1_MAX    (0x04)      /**< only for T1 */

#define WMBUS_SLEEP_DURATION_MS_MIN      (107)         /**< Min size that works with r0.2.5*/
#define WMBUS_SLEEP_DURATION_MS_MAX      (86400000)    /**< Max sleep duration: 24 hours */
#define WMBUS_TX_SIZE_MIN                (1)
#define WMBUS_TX_SIZE_MAX                (290)         /**< Max size that works with r0.2.5 */
/** @} */

/**
 *  DEFAULT VALUES (shall not be changed)
 *  @{
 */
#define WMBUS_DEF_PHY_MODE                   WMBUS_PHY_MODE_T1
#define WMBUS_DEF_TX_POWER                   (0x00)
#define WMBUS_DEF_T_NOM                      (20)
#define WMBUS_DEF_DEV_TYPE                   (WMBUS_DEV_TYPE_METER)
#define WMBUS_DEF_FRAME_FORMAT               (WMBUS_FRAME_FORMAT_A)
#define WMBUS_DEF_HEADER_LENGTH              WMBUS_HEADER_LENGTH_T1_MIN
#define WMBUS_DEF_POSTAMBLE_LENGTH           WMBUS_POSTAMBLE_LENGTH_T1_MIN
#define WMBUS_DEF_ASYNCH_SLEEP_DURATION      (3600000)
/** @} */

/** @} */ /* WMBUS */

/* Types ---------------------------------------------------------------------*/




#endif /* ST87EC_CONFIG_DEFAULT_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
