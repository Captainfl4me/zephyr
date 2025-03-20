/**
  ******************************************************************************
  * @file    st87ec_wrapper.h
  * @author  APMS Application Team
  * @brief   HW drivers wrapper header file
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
#ifndef ST87EC_WRAPPER_H
#define ST87EC_WRAPPER_H
#include <stdint.h>
#include "st87ec_lib.h"
#include "st87ec_engine.h"
#include "../../Cust/inc/st87ec_lib_hal.h"

/* Exported constants -------------------------------------------------------*/

#define ST87EC_TIMER_ERROR (0xFFFFFFFF)

/* Exported macros ----------------------------------------------------------*/

/* Types ---------------------------------------------------------------------*/

/**
 * Output value of the Timer status.
 */
typedef enum{
  TIMER_STATUS_IDLE = 0,                 /**< The Timer is ready to run.              */
  TIMER_STATUS_BUSY = 1,                 /**< The Timer is running.                   */
  TIMER_STATUS_ELAPSED = 2,              /**< The Timer is elapsed.                   */
  TIMER_STATUS_ERROR   = 3,              /**< Wrong timer id                          */
} ST87EC_Wrapper_TimerStatus_t;

/**
 * ST87 Reset states
 */
typedef enum{
  RESET_PIN_OFF = 0,           /**< Reset Pin off      */
  RESET_PIN_ON = 1,            /**< Reset Pin on       */
  RESET_PIN_PULSE = 2          /**< Reset Pin toggle   */
} ST87EC_wrapper_reset_pin_t;

/* Exported functions --------------------------------------------------------*/

ST87EC_Lib_Result_t ST87EC_Wrapper_Init(void);
ST87EC_Lib_Result_t ST87EC_Wrapper_DriveResetPin(ST87EC_wrapper_reset_pin_t State);
ST87EC_Lib_Result_t ST87EC_Wrapper_RingPinIsr(void);
uint8_t ST87EC_Wrapper_GetRingPinSts(void);

uint32_t ST87EC_Wrapper_StartTimer(uint32_t ValueMs);
ST87EC_Lib_Result_t ST87EC_Wrapper_StopTimer(uint32_t TimerId);
ST87EC_Lib_Result_t ST87EC_Wrapper_Delay(uint32_t ValueMs);
ST87EC_Wrapper_TimerStatus_t ST87EC_Wrapper_GetTimerStatus(uint32_t TimerId);
ST87EC_Lib_Result_t ST87EC_Wrapper_TimerInit(void);
ST87EC_Lib_Result_t  ST87EC_Wrapper_TickIsr(void);


ST87EC_Lib_Result_t ST87EC_Wrapper_UartIsr(void);
ST87EC_Lib_Result_t ST87EC_Wrapper_St87RxMsgHandler(void);
uint32_t ST87EC_Wrapper_SendCmd(const char * format,...);
uint32_t ST87EC_Wrapper_SendByte(uint8_t * pByteBuffer, uint32_t Length);
uint32_t ST87EC_Wrapper_GetSt87CurrRxMsg(char * pRxMsgHandle, uint32_t ExpectedLength);
uint32_t ST87EC_Wrapper_WakeUp(void);
uint32_t ST87EC_Wrapper_SendCmdToBoot(const char * format,...);
ST87EC_Lib_Result_t ST87EC_Wrapper_GetSt87BootNbBytesToUpload(uint8_t * ByteToRead);
#endif /* ST87EC_WRAPPER_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
