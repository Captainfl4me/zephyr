/**
  ******************************************************************************
  * @file    st87ec_sequence_wmbus.h
  * @author  APMS Application Team
  * @brief   System sequences header file
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
#ifndef ST87EC_SEQUENCE_EXAMPLE_H
#define ST87EC_SEQUENCE_EXAMPLE_H

#include "st87ec_engine.h"

/* Private constants and macros-----------------------------------------------*/

/**
 *  Configuration default usage (if not defined)
 *  @{
 */
#ifndef WMBUS_TX_BUFFER_SIZE
  #define WMBUS_TX_BUFFER_SIZE          (256)
#endif

#ifndef WMBUS_WAKEUP_BACKOFF_MS
  #define WMBUS_WAKEUP_BACKOFF_MS      (100)
#endif

#ifndef WMBUS_PHY_MODE
  #define WMBUS_PHY_MODE               WMBUS_DEF_PHY_MODE
#endif

#ifndef WMBUS_TX_POWER
  #define WMBUS_TX_POWER               WMBUS_DEF_TX_POWER
#endif

#ifndef WMBUS_T_NOM
  #define WMBUS_T_NOM                  WMBUS_DEF_T_NOM
#endif

#ifndef WMBUS_DEV_TYPE
  #define WMBUS_DEV_TYPE               WMBUS_DEF_DEV_TYPE
#endif

#ifndef WMBUS_FRAME_FORMAT
  #define WMBUS_FRAME_FORMAT           WMBUS_DEF_FRAME_FORMAT
#endif

#ifndef WMBUS_POSTAMPLE_LENGTH
  #define WMBUS_POSTAMPLE_LENGTH       WMBUS_DEF_POSTAMBLE_LENGTH
#endif

#ifndef WMBUS_HEADER_LENGTH
  #define WMBUS_HEADER_LENGTH          WMBUS_DEF_HEADER_LENGTH
#endif

#ifndef WMBUS_ASYNCH_SLEEP_DURATION_MS
  #define WMBUS_ASYNCH_SLEEP_DURATION_MS WMBUS_DEF_ASYNCH_SLEEP_DURATION
#endif
/** @} */

/**
 *  Definition used internally
 *  @{
 */
#define WMBUS_SLEEP_DURATION_MS ((WMBUS_T_NOM * 1000) - WMBUS_WAKEUP_BACKOFF_MS)
/** @} */
/**
 *  Configuration range check
 *  @{
 */
#if (WMBUS_TX_POWER < WMBUS_TX_POWER_MIN) || (WMBUS_TX_POWER > WMBUS_TX_POWER_MAX)
  #error "[WMBUS] invalid WMBUS_TX_POWER"
#endif

#if (WMBUS_T_NOM < WMBUS_T_NOM_MIN) || (WMBUS_T_NOM > WMBUS_T_NOM_MAX)
  #error "[WMBUS] invalid WMBUS_T_NOM"
#endif

#if (WMBUS_HEADER_LENGTH < WMBUS_HEADER_LENGTH_T1_MIN)
  #error "[WMBUS] invalid WMBUS_HEADER_LENGTH"
#endif

#if (WMBUS_TX_BUFFER_SIZE < WMBUS_TX_SIZE_MIN) || (WMBUS_TX_BUFFER_SIZE > WMBUS_TX_SIZE_MAX)
  #error "[WMBUS] invalid WMBUS_TX_BUFFER_SIZE"
#endif

#if WMBUS_WAKEUP_BACKOFF_MS >= (WMBUS_T_NOM * 1000)
  #error "[WMBUS] WMBUS_WAKEUP_BACKOFF_MS must be less than WMBUS_T_NOM"
#endif

#if (WMBUS_SLEEP_DURATION_MS < WMBUS_SLEEP_DURATION_MS_MIN) || (WMBUS_SLEEP_DURATION_MS > WMBUS_SLEEP_DURATION_MS_MAX)
  #error "[WMBUS] Invalid WMBUS_SLEEP_DURATION_MS"
#endif

#if (WMBUS_ASYNCH_SLEEP_DURATION_MS < WMBUS_SLEEP_DURATION_MS_MIN) || (WMBUS_ASYNCH_SLEEP_DURATION_MS > WMBUS_SLEEP_DURATION_MS_MAX)
  #error "[WMBUS] Invalid WMBUS_ASYNCH_SLEEP_DURATION_MS"
#endif
/** @} */

/* Types ---------------------------------------------------------------------*/

/* Global variables ----------------------------------------------------------*/


/* Exported functions --------------------------------------------------------*/
ST87EC_Lib_Result_t ST87EC_SequenceWmbus_Transfer(void);

#endif /* ST87EC_SEQUENCE_EXAMPLE_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
