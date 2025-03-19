/**
  ******************************************************************************
  * @file    st87ec_lib_hal.h
  * @author  APMS Application Team
  * @brief   Easy Connect lib HAL Interface header file
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
#ifndef ST87EC_HAL_LIB_H
#define ST87EC_HAL_LIB_H
#include "../../Core/inc/st87ec_lib.h"
#include <stdbool.h>
#include <stdint.h>

/* Exported constants and macros-----------------------------------------------*/
#define ST87EC_UART_FLAG_RX_NOTEMPTY (0x01) /**< ST87 UART Rx Hw buffer still contains data  */
#define ST87EC_UART_FLAG_TX_COMPLETE (0x02) /**< ST87 UART Tx byte transfer is finished  */

#define ST87EC_UART_CLEAR_FLAG_RXOVERRUN (0x01) /**< ST87 UART Rx overrun situation  */
#define ST87EC_UART_CLEAR_FLAG_TXCOMPLETE (0x02) /**< ST87 UART Tx transfer is finished  */

/* Types ---------------------------------------------------------------------*/
typedef enum {
    UART_CHANNEL_RX = 0, /**< ST87 UART Rx channel id  */
    UART_CHANNEL_TX = 1 /**< ST87 UART Tx channel id  */
} ST87EC_Lib_UartChannel_t;

/* Exported functions --------------------------------------------------------*/
ST87EC_Lib_Result_t ST87EC_Lib_Hal_GpioInit(void);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_DriveResetGpio(uint32_t State);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_RingPinIsr(void);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_EnableUartInterrupts(uint8_t Flag);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_UartIsr(void);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_UartTxByte(uint8_t Byte);
uint8_t ST87EC_Lib_Hal_UartRxByte(void);
bool ST87EC_Lib_Hal_UartGetStatusFlag(ST87EC_Lib_UartChannel_t UartChan, uint8_t StsFlagToGet);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_UartClearFlags(ST87EC_Lib_UartChannel_t UartChan, uint8_t FlagToClear);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_TickIsr(void);
ST87EC_Lib_Result_t ST87EC_Lib_Hal_SetHostUartBaudrate(uint32_t BaudRate);
uint32_t ST87EC_Lib_Hal_GetHostUartBaudrate(void);

#endif /* ST87EC_HAL_LIB_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
