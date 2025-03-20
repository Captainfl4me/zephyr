/**
  ******************************************************************************
  * @file    st87ec_wrapper.c
  * @author  APMS Application Team
  * @brief   HW drivers wrapper functions
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
#include "../inc/st87ec_wrapper.h"
#include "../../Cust/inc/st87ec_lib_hal.h"
#include "../inc/st87ec_engine.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../modem_context.h"
#include "../../../st87m01.h"

#include <zephyr/logging/log.h>

/* Public variables ---------------------------------------------------------*/

/* Private constants ---------------------------------------------------------*/
#define ST87EC_WRP_PRINTF_MAX_SIZE                                                                 \
	(1600U) /*!< in nb of char elements (aligned with ST87 max data packet size) */
#define ST87EC_WRP_BOOT_AT_CMD_MAX_SIZE (160U) /*!< in nb of char elements */
#define ST87EC_WRP_RX_FIFO_SIZE                                                                    \
	(2048U) /*!< in nb of char elements (beware: power of 2 value expected!) */
#define ST87EC_WRP_TX_FIFO_SIZE                                                                    \
	(2048U) /*!< in nb of char elements (beware: power of 2 value expected!) */
#define ST87EC_WRP_RX_FIFO_SIZ_MSK (ST87EC_WRP_RX_FIFO_SIZE - 1U)
#define ST87EC_WRP_TX_FIFO_SIZ_MSK (ST87EC_WRP_TX_FIFO_SIZE - 1U)
#define ST87EC_WRP_RX_TAG_MAX_SIZE (16U)
#define ST87EC_WRP_CRLF_CHAR_SIZE  (2U)

LOG_MODULE_REGISTER(ST87EC_Wrapper);

/* Private macros ------------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/
typedef enum {
	ST87EC_WRP_DATA_TO_TX_NONE,    /**< Data transfer to UART Tx: no transfer   */
	ST87EC_WRP_DATA_TO_TX_STARTED, /**< Data transfer to UART Tx: transfer started  */
	ST87EC_WRP_DATA_TO_TX_HANDLED  /**< Data transfer to UART Tx: transfer on-going  */
} ST87EC_Wrapper_TxDataState_t;

typedef enum {
	ST87EC_WRP_NONE_FOUND,        /**< No tag nor specific response found in UART Rx flow   */
	ST87EC_WRP_TAG_FOUND,         /**< Tag found in UART Rx flow  */
	ST87EC_WRP_SPECIFIC_RSP_FOUND /**< Specific response found in UART Rx flow  */
} ST87EC_Wrapper_RspFound_t;

typedef struct {
	uint16_t DataMsgTagLen; /**< UART Rx context: tag length of the incoming message   */
	uint8_t TagString[ST87EC_WRP_RX_TAG_MAX_SIZE]; /**< UART Rx context: storage buffer for the
							  tag of the incoming message   */
	uint16_t DataMsgLen; /**< UART Rx context: length of the incoming message   */
	ST87EC_Wrapper_RspFound_t
		RspFound;    /**< UART Rx context: tag or specific response found in Rx flow  */
	uint32_t FifoRdIdx;  /**< UART Rx context: FIFO read pointer index   */
	uint32_t FifoRdIdx0; /**< UART Rx context: FIFO initial read pointer index   */
	uint32_t FifoWrIdx;  /**< UART Rx context: FIFO write pointer index   */
	uint8_t Fifo[ST87EC_WRP_RX_FIFO_SIZE]; /**< UART Rx context: UART Rx FIFO   */
} ST87EC_Wrapper_UartRxCtx_t;

typedef struct {
	ST87EC_Wrapper_TxDataState_t TxState;  /**< UART Tx context: state of the UART Tx      */
	uint32_t FifoRdIdx;                    /**< UART Tx context: FIFO read pointer index   */
	uint32_t FifoWrIdx;                    /**< UART Tx context: FIFO write pointer index   */
	uint8_t Fifo[ST87EC_WRP_TX_FIFO_SIZE]; /**< UART Tx context: UART Rx FIFO   */
} ST87EC_Wrapper_UartTxCtx_t;

typedef struct {
	ST87EC_Wrapper_UartRxCtx_t Rx; /**< UART Rx context     */
	ST87EC_Wrapper_UartTxCtx_t Tx; /**< UART Tx context     */
	uint8_t TxBuffer[ST87EC_WRP_PRINTF_MAX_SIZE +
			 (2U * ST87EC_WRP_CRLF_CHAR_SIZE)]; /**< Tx buffer to send to Tx Fifo */
	uint32_t TxBufSize;                                 /**< Data length in Tx buffer */
} ST87EC_Wrapper_UartCtx_t;

/**
 * Structure for the timer object
 */
typedef struct {
	uint32_t TickCount;                           /**< Timer counter value                   */
	uint32_t Timeout;                             /**< Timeout of the timer                  */
	volatile ST87EC_Wrapper_TimerStatus_t Status; /**< Current status of the timer           */
} ST8EC_Wrapper_Timer_t;

typedef struct {
	uint8_t ResetGpioState;
	uint8_t RingPinState;
	ST8EC_Wrapper_Timer_t
		TimerTable[TIMER_NB]; /**< Structure containing timer table information.    */
} ST87EC_wrapper_vars_t;

/* Private variables ---------------------------------------------------------*/
ST87EC_wrapper_vars_t ST87EC_WrapperVars;

/* Private functions ---------------------------------------------------------*/
static inline void ST87EC_Wrapper_UartRxHandler(uint8_t RxByte);
static inline void ST87EC_Wrapper_UartTxHandler(uint8_t *TxByte);
static void ST87EC_Wrapper_UartTxStart(void);
static inline ST87EC_Lib_Result_t ST87EC_Wrapper_RxDataParse(ST87EC_Wrapper_UartRxCtx_t *pUartRx);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Wrapper init function
 *
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_Init(void)
{
	uint8_t tmp_ret = 0;
	ST87EC_Lib_Result_t result = RESULT_OK;

	/* Init wrapper vars */
	ST87EC_WrapperVars.ResetGpioState = 0;
	ST87EC_WrapperVars.RingPinState = 0;

	/* Initialize GPIOs*/
	tmp_ret += (uint8_t)ST87EC_Lib_Hal_GpioInit();

	/* Init timers */
	tmp_ret += (uint8_t)ST87EC_Wrapper_TimerInit();

	if (tmp_ret > 0) {
		result = RESULT_KO;
	}

	return (result);
}

/**
 * @brief ST87MXX Reset pin management
 *
 * @param State: State to set on the Reset pin
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_DriveResetPin(ST87EC_wrapper_reset_pin_t State)
{
	ST87EC_Lib_Result_t result = RESULT_OK;

	switch (State) {
	case RESET_PIN_ON:
		result = ST87EC_Lib_Hal_DriveResetGpio(1);
		break;

	case RESET_PIN_OFF:
		result = ST87EC_Lib_Hal_DriveResetGpio(0);
		break;

	case RESET_PIN_PULSE:
		ST87EC_Lib_Hal_DriveResetGpio(0);
		ST87EC_Wrapper_Delay(15);
		result = ST87EC_Lib_Hal_DriveResetGpio(1);
		break;
	}
	ST87EC_WrapperVars.ResetGpioState = (uint8_t)(State != RESET_PIN_OFF);

	return (result);
}

/**
 * @brief ST87 Ring Pin Isr function
 *
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_RingPinIsr(void)
{
	ST87EC_Lib_Result_t result = RESULT_OK;
	ST87EC_WrapperVars.RingPinState = 1;
	return (result);
}

/**
 * @brief Returns the ST87 Ring Pin status
 *
 * @retval Ring Pin GPIO Value
 */
uint8_t ST87EC_Wrapper_GetRingPinSts(void)
{
	uint8_t state = ST87EC_WrapperVars.RingPinState;
	ST87EC_WrapperVars.RingPinState = 0;

	return (state);
}

/**
 * @brief ST87 Start Timer
 *
 * @param  ValueMs : Value in ms to elapse the timer
 * @retval Id of timer started (return ST87EC_TIMER_ERROR if id_Timer is invalid)
 */
uint32_t ST87EC_Wrapper_StartTimer(uint32_t ValueMs)
{
	uint32_t id_timer = ST87EC_TIMER_ERROR;
	uint8_t i;

	for (i = 0; i < TIMER_NB; i++) {
		if ((ST87EC_WrapperVars.TimerTable[i].Status == TIMER_STATUS_IDLE) &&
		    (ValueMs > 0)) {
			ST87EC_WrapperVars.TimerTable[i].TickCount = 0;
			ST87EC_WrapperVars.TimerTable[i].Timeout = ValueMs;
			ST87EC_WrapperVars.TimerTable[i].Status = TIMER_STATUS_BUSY;

			id_timer = i;
			break;
		}
	}

	return (id_timer);
}

/**
 * @brief ST87 Stop Timer
 *
 * @param  TimerId : Id of the timer to be stopped
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_StopTimer(uint32_t TimerId)
{
	ST87EC_Lib_Result_t result = RESULT_KO;

	if (TimerId < TIMER_NB) {
		if (ST87EC_WrapperVars.TimerTable[TimerId].Status != TIMER_STATUS_IDLE) {
			ST87EC_WrapperVars.TimerTable[TimerId].Status = TIMER_STATUS_IDLE;
			result = RESULT_OK;
		}
	}

	return (result);
}

/**
 * @brief ST87 Delay Timer
 *
 * @param  ValueMs : The time in ms corresponding to the chosen delay
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_Delay(uint32_t ValueMs)
{
	uint32_t idTimerDelay = 0;
	ST87EC_Lib_Result_t result = RESULT_KO;
	idTimerDelay = ST87EC_Wrapper_StartTimer(ValueMs);

	if (idTimerDelay != ST87EC_TIMER_ERROR) {
		result = RESULT_OK;
		while (ST87EC_WrapperVars.TimerTable[idTimerDelay].Status != TIMER_STATUS_ELAPSED) {
		}

		ST87EC_Wrapper_StopTimer(idTimerDelay);
	}

	return (result);
}

/**
 * @brief ST87 Tick Isr function
 *
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_TickIsr(void)
{
	ST87EC_Lib_Result_t result = RESULT_OK;
	uint32_t i;

	for (i = 0; i < TIMER_NB; i++) {
		if (ST87EC_WrapperVars.TimerTable[i].Status == TIMER_STATUS_BUSY) {
			ST87EC_WrapperVars.TimerTable[i].TickCount += TIMER_TICK_PERIOD_MS;

			if (ST87EC_WrapperVars.TimerTable[i].TickCount >=
			    ST87EC_WrapperVars.TimerTable[i].Timeout) {
				ST87EC_WrapperVars.TimerTable[i].Status = TIMER_STATUS_ELAPSED;
			}
		}
	}

	return (result);
}

/**
 * @brief ST87 Get Timer Status
 *
 * @param  TimerId : Id of timer to check status
 * @retval Timer execution status
 */
ST87EC_Wrapper_TimerStatus_t ST87EC_Wrapper_GetTimerStatus(uint32_t TimerId)
{
	ST87EC_Wrapper_TimerStatus_t result = TIMER_STATUS_ERROR;

	if (TimerId < TIMER_NB) {
		result = ST87EC_WrapperVars.TimerTable[TimerId].Status;

		if (result == TIMER_STATUS_ELAPSED) {
			ST87EC_WrapperVars.TimerTable[TimerId].Status = TIMER_STATUS_IDLE;
		}
	}

	return (result);
}

/**
 * @brief ST87 Init Timers
 *
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_TimerInit(void)
{
	uint32_t i;
	for (i = 0; i < TIMER_NB; i++) {
		ST87EC_WrapperVars.TimerTable[i].TickCount = 0;
		ST87EC_WrapperVars.TimerTable[i].Timeout = 0;
		ST87EC_WrapperVars.TimerTable[i].Status = TIMER_STATUS_IDLE;
	}

	return (RESULT_OK);
}

/**
 * @brief This function handles ST87 UART data Rx FIFO unstacking
 *
 * @param : None
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Wrapper_St87RxMsgHandler(void)
{
	ST87EC_Lib_Result_t result = RESULT_OK;
	/**/
	/*if (EcLibVars.BootMode != BOOT_UPLOAD_MODE) {*/
	/*	uint8_t cur_rx_byte;*/
	/*	while (ST87EC_WrapperVars.UartCtxt.Rx.FifoRdIdx !=*/
	/*	       ST87EC_WrapperVars.UartCtxt.Rx.FifoWrIdx) {*/
	/*		result = ST87EC_Wrapper_RxDataParse(&ST87EC_WrapperVars.UartCtxt.Rx);*/
	/*		cur_rx_byte = ST87EC_WrapperVars.UartCtxt.Rx*/
	/*				      .Fifo[ST87EC_WrapperVars.UartCtxt.Rx.FifoRdIdx];*/
	/*		ST87EC_WrapperVars.UartCtxt.Rx.FifoRdIdx =*/
	/*			(ST87EC_WrapperVars.UartCtxt.Rx.FifoRdIdx + 1U) &*/
	/*			ST87EC_WRP_RX_FIFO_SIZ_MSK;*/
	/*		if (result != RESULT_OK) {*/
	/*			LOG_ERR("Error in ST87 Rx msg handler");*/
	/*			break;*/
	/*		} else if (cur_rx_byte == '\n') {*/
	/* An end of Rx message is being found:*/
	/*			   escape loop to allow immediate msg parsing */
	/*			break;*/
	/*		} else if (EcLibVars.BootMode == BOOT_UPLOAD_MODE) {*/
	/* BootMode has just changed into UPLOAD: immediate exit */
	/*			break;*/
	/*		}*/
	/*	}*/
	/*}*/
	/**/
	return result;
}

/**
 * @brief This function sends string commands through ST87 UART in a printf fashion
 * @note Caution: maximum command length is ST87EC_WRP_PRINTF_MAX_SIZE bytes!
 *
 * @param format : variadic parameters (printf-like input parameters)
 * @retval Number of bytes sent
 */
uint32_t ST87EC_Wrapper_SendCmd(const char *format, ...)
{
	int ret;
	/* Parsing command */
	va_list args;
	va_start(args, format);
	uint32_t length_sent = vsprintf((char *)(mdm_tx_buf), (const char *)format, args);
	va_end(args);

	LOG_DBG("OUT: [%s]", mdm_tx_buf);
	mdm_receiver_send(&mctx, mdm_tx_buf, length_sent);
	mdm_receiver_send(&mctx, "\r\n", 2);

	k_sem_reset(&mdata.response_sem);
	ret = k_sem_take(&mdata.response_sem, K_MSEC(MDM_AT_CMD_TIMEOUT));

	if (ret == -EAGAIN) {
		return 0;
	}

	return (length_sent);
}

/**
 * @brief This function wakes-up ST87 by sending a char
 *
 * @param : None
 * @retval Number of bytes sent
 */
uint32_t ST87EC_Wrapper_WakeUp(void)
{
	char *cr = "\r";

	return ST87EC_Wrapper_SendByte((uint8_t *)cr, 1U);
}

/**
 * @brief This function sends string AT commands with preamble through ST87 UART to ST87 Boot
 * @note Caution: maximum command length is ST87EC_WRP_BOOT_AT_CMD_MAX_SIZE bytes!
 *
 * @param format : variadic parameters (printf-like input parameters)
 * @retval Number of bytes sent
 */
uint32_t ST87EC_Wrapper_SendCmdToBoot(const char *format, ...)
{
	va_list args;
	uint32_t length_sent = 0U;

	/* Send preamble to ensure ST87 Boot is well in AT Parser state
	   by sending 128 <BS> ending with <CR> */
	/*memset(ST87EC_WrapperVars.UartCtxt.TxBuffer, '\b', 128U);*/
	/*ST87EC_WrapperVars.UartCtxt.TxBuffer[128U] = '\r';*/
	/*ST87EC_Wrapper_SendByte(ST87EC_WrapperVars.UartCtxt.TxBuffer, (128U + 1U));*/

	/* Parse args and prepare command */
	/*va_start(args, format);*/
	/*ST87EC_WrapperVars.UartCtxt.TxBufSize = vsprintf(*/
	/*	(char *)(ST87EC_WrapperVars.UartCtxt.TxBuffer), (const char *)format, args);*/
	/*va_end(args);*/
	/*ST87EC_WrapperVars.UartCtxt.TxBuffer[ST87EC_WrapperVars.UartCtxt.TxBufSize++] = '\r';*/

	/* Send AT command string to ST87 Boot */
	/*if (ST87EC_WrapperVars.UartCtxt.TxBufSize > (1U)) {*/
	/*	length_sent = ST87EC_Wrapper_SendByte(ST87EC_WrapperVars.UartCtxt.TxBuffer,*/
	/*					      ST87EC_WrapperVars.UartCtxt.TxBufSize);*/
	/*}*/

	return (length_sent);
}

/**
 * @brief ST87 UART Rx data current message copy function
 *
 * @param pRxMsgHandle : pointer on the location where to copy
 * @param ExpectedLength : number of expected (i.e max) number bytes to copy
 * @retval Number of bytes copied
 */
uint32_t ST87EC_Wrapper_GetSt87CurrRxMsg(char *pRxMsgHandle, uint32_t ExpectedLength)
{
	uint32_t i;
	/*uint32_t len_to_copy = ST87EC_WrapperVars.UartCtxt.Rx.DataMsgLen;*/

	/*if ((len_to_copy > 0U) && pRxMsgHandle) {*/
	/* Latest message is currently available in the UART Rx Fifo */
	/*if (len_to_copy > ExpectedLength) {*/
	/*	len_to_copy =*/
	/*ExpectedLength; */
	/* Clamp to caller expected (i.e. max) length */
	/*	}*/
	/*	for (i = 0U; i < len_to_copy; i++) {*/
	/*		pRxMsgHandle[i] =*/
	/*			ST87EC_WrapperVars.UartCtxt.Rx*/
	/*				.Fifo[(ST87EC_WrapperVars.UartCtxt.Rx.FifoRdIdx0 + i) &*/
	/*				      ST87EC_WRP_RX_FIFO_SIZ_MSK];*/
	/*	}*/
	/*}*/

	return 0; // len_to_copy;
}

/* Private functions --------------------------------------------------------*/

/**
 * @brief This function handles bytes received from UART Rx
 * @note This function shall be called within interrupt context.
 *
 * @param RxByte: data byte received from UART
 */
static inline void ST87EC_Wrapper_UartRxHandler(uint8_t RxByte)
{
	/*ST87EC_WrapperVars.UartCtxt.Rx.Fifo[ST87EC_WrapperVars.UartCtxt.Rx.FifoWrIdx] = RxByte;*/
	/*ST87EC_WrapperVars.UartCtxt.Rx.FifoWrIdx =*/
	/*	(ST87EC_WrapperVars.UartCtxt.Rx.FifoWrIdx + 1U) & ST87EC_WRP_RX_FIFO_SIZ_MSK;*/
}

/**
 * @brief This function handles bytes sending on UART Tx
 * @note This function shall be called in within interrupt context.
 *
 * @param pTxByte: pointer on data byte to be sent over UART
 */
static inline void ST87EC_Wrapper_UartTxHandler(uint8_t *pTxByte)
{

	/*if (ST87EC_WrapperVars.UartCtxt.Tx.FifoRdIdx == ST87EC_WrapperVars.UartCtxt.Tx.FifoWrIdx)
	 * {*/
	/* No more byte transmit */
	/*	ST87EC_WrapperVars.UartCtxt.Tx.TxState = ST87EC_WRP_DATA_TO_TX_NONE;*/
	/*} else {*/
	/* Output byte to transmit */
	/*	*pTxByte = ST87EC_WrapperVars.UartCtxt.Tx*/
	/*			   .Fifo[ST87EC_WrapperVars.UartCtxt.Tx.FifoRdIdx];*/
	/*	ST87EC_WrapperVars.UartCtxt.Tx.FifoRdIdx =*/
	/*		((ST87EC_WrapperVars.UartCtxt.Tx.FifoRdIdx + 1U) &*/
	/*		 ST87EC_WRP_TX_FIFO_SIZ_MSK);*/
	/**/
	/*	ST87EC_WrapperVars.UartCtxt.Tx.TxState = ST87EC_WRP_DATA_TO_TX_HANDLED;*/
	/*}*/
}

/**
 * @brief This function parses data read from UART Rx FIFO
 *
 * @param pUartRx : pointer on UART Rx context structure
 * @retval Function execution status
 */
static inline ST87EC_Lib_Result_t ST87EC_Wrapper_RxDataParse(ST87EC_Wrapper_UartRxCtx_t *pUartRx)
{
	ST87EC_Lib_Result_t result = RESULT_OK;

	if (pUartRx->RspFound == ST87EC_WRP_NONE_FOUND) {
		if (!((pUartRx->Fifo[pUartRx->FifoRdIdx] == '\r') ||
		      (pUartRx->Fifo[pUartRx->FifoRdIdx] == '\n') ||
		      (pUartRx->Fifo[pUartRx->FifoRdIdx] == ':') ||
		      (pUartRx->Fifo[pUartRx->FifoRdIdx] == ',') ||
		      (pUartRx->Fifo[pUartRx->FifoRdIdx] == ' '))) {
			/* Exclude '\n', '\r' and ':' ',' char from tag length counting */
			if (pUartRx->DataMsgTagLen < ST87EC_WRP_RX_TAG_MAX_SIZE) {
				pUartRx->TagString[pUartRx->DataMsgTagLen] =
					pUartRx->Fifo[pUartRx->FifoRdIdx];
				pUartRx->DataMsgTagLen++;
			}
		}

		if ((pUartRx->Fifo[pUartRx->FifoRdIdx] == ':') ||
		    (pUartRx->Fifo[pUartRx->FifoRdIdx] == ',') ||
		    (pUartRx->Fifo[pUartRx->FifoRdIdx] == ' ') ||
		    (pUartRx->Fifo[pUartRx->FifoRdIdx] == '\r')) {
			if (pUartRx->DataMsgTagLen > 0) {
				pUartRx->RspFound = ST87EC_WRP_TAG_FOUND;
			}
		}

		/* Specific "OK" tag string or ST87 Boot "0" OK string tag detection */
		if (((pUartRx->TagString[0U] == 'O') && (pUartRx->TagString[1U] == 'K') &&
		     (pUartRx->DataMsgTagLen >= 2U)) ||
		    ((pUartRx->TagString[0U] == '0') &&
		     (pUartRx->Fifo[pUartRx->FifoRdIdx] == '\r') &&
		     (pUartRx->DataMsgTagLen == 1U) && (EcLibVars.BootMode == BOOT_AT_MODE))) {
			ST87EC_Lib_HandleSpecificRsp(SPECIFIC_RSP_OK);
			pUartRx->RspFound = ST87EC_WRP_SPECIFIC_RSP_FOUND;
		}
		/* Specific "OK" tag for ST87 Boot handshake */
		if ((pUartRx->TagString[0U] == 0x9A) && (pUartRx->DataMsgTagLen == 1U) &&
		    (EcLibVars.BootMode == BOOT_AT_MODE)) {
			ST87EC_Lib_HandleSpecificRsp(SPECIFIC_RSP_HANDSHAKE_OK);
			pUartRx->RspFound = ST87EC_WRP_SPECIFIC_RSP_FOUND;
		}

		/* Specific "CME" error tag or ST87 Boot "40XX" error string tag detection  */
		if (((pUartRx->TagString[0U] == '+') && (pUartRx->TagString[1U] == 'C') &&
		     (pUartRx->TagString[2U] == 'M') && (pUartRx->TagString[3U] == 'E') &&
		     (pUartRx->DataMsgTagLen >= 4U)) ||
		    ((pUartRx->TagString[0U] == '4') && (pUartRx->TagString[1U] == '0') &&
		     (pUartRx->DataMsgTagLen == 4U) && (EcLibVars.BootMode == BOOT_AT_MODE))) {
			ST87EC_Lib_HandleSpecificRsp(SPECIFIC_RSP_CME);
			pUartRx->RspFound = ST87EC_WRP_SPECIFIC_RSP_FOUND;
		}

		/* Specific "NB_SENT" UDP tag string detection */
		if ((pUartRx->TagString[0U] == 'N') && (pUartRx->TagString[1U] == 'B') &&
		    (pUartRx->TagString[2U] == '_') && (pUartRx->TagString[3U] == 'S') &&
		    (pUartRx->DataMsgTagLen == 7U)) {
			ST87EC_Lib_HandleSpecificRsp(SPECIFIC_RSP_NBSENT);
			pUartRx->RspFound = ST87EC_WRP_SPECIFIC_RSP_FOUND;
		}

		/* Specific "#REBOOT_WD" ST87 watchdog tag string detection */
		if ((pUartRx->TagString[0U] == '#') && (pUartRx->TagString[1U] == 'R') &&
		    (pUartRx->TagString[3U] == 'B') && (pUartRx->TagString[7U] == '_') &&
		    (pUartRx->TagString[8U] == 'W') && (pUartRx->TagString[9U] == 'D') &&
		    (pUartRx->DataMsgTagLen == 10U)) {
			ST87EC_Lib_HandleSpecificRsp(SPECIFIC_RSP_WATCHDOG);
			pUartRx->RspFound = ST87EC_WRP_SPECIFIC_RSP_FOUND;
		}
	}

	pUartRx->DataMsgLen++;

	if ((pUartRx->Fifo[pUartRx->FifoRdIdx] == '\n') /* String end in Normal mode */
	    || ((EcLibVars.BootMode == BOOT_AT_MODE) &&
		(pUartRx->Fifo[pUartRx->FifoRdIdx] == '\r'))) /* String end in Boot mode */
	{
		if ((pUartRx->RspFound != ST87EC_WRP_SPECIFIC_RSP_FOUND) &&
		    (pUartRx->DataMsgTagLen > 0)) {
			/* Warn EC Lib Engine about msg availability with likely expected tag
			 * information */
			ST87EC_Lib_HandleGenericRsp((char *)pUartRx->TagString,
						    pUartRx->DataMsgTagLen);
		}

		/* Warn EC Lib Engine about msg availability for likely usage of raw data */
		ST87EC_Lib_HandleRawRsp(pUartRx->DataMsgLen);

		/* Customer option (for debug, display,...): forward Rx data to other UART,... */
		if (pUartRx->FifoRdIdx0 > pUartRx->FifoRdIdx) {
			LOG_DBG("RX SPY: %.*s",
				((uint32_t)(ST87EC_WRP_RX_FIFO_SIZE) + 1U - pUartRx->FifoRdIdx0),
				&pUartRx->Fifo[pUartRx->FifoRdIdx0]);
			LOG_DBG("RX SPY (split due to FIFO): %.*s", pUartRx->FifoRdIdx,
				&pUartRx->Fifo[0]);
		} else {
			LOG_DBG("RX SPY: %.*s", pUartRx->FifoRdIdx - pUartRx->FifoRdIdx0,
				&pUartRx->Fifo[pUartRx->FifoRdIdx0]);
		}

		/* Reset all Rx Uart context vars for next msg reception */
		pUartRx->FifoRdIdx0 =
			(pUartRx->FifoRdIdx + 1U) &
			ST87EC_WRP_RX_FIFO_SIZ_MSK; /* get address just after the one containing \n
						       (with circ. buff indexing management) */
		pUartRx->DataMsgTagLen = 0U;
		memset(pUartRx->TagString, 0x0U, ST87EC_WRP_RX_TAG_MAX_SIZE);
		pUartRx->DataMsgLen = 0U;
		pUartRx->RspFound = ST87EC_WRP_NONE_FOUND;

		if (EcLibVars.BootIf.LastMsgBeforeBootUpload) {
			EcLibVars.BootMode = BOOT_UPLOAD_MODE;
			EcLibVars.BootIf.LastMsgBeforeBootUpload = false;
		}
	}

	return result;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
