/**
 ******************************************************************************
 * @file    st87ec_lib_hal.h
 * @author  APMS Application Team
 * @brief   Easy Connect lib HAL  functions
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
#include "../inc/st87ec_lib_hal.h"
#include "../../Core/inc/st87ec_lib.h"
#include "../../Core/inc/st87ec_wrapper.h"
// #include "../inc/st87ec_debug.h"
// #include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Constants -----------------------------------------------------------------*/
#define MDM_UART_DEV DEVICE_DT_GET(DT_ALIAS(mdmuartnode))
LOG_MODULE_REGISTER(ST87EC_Lib_Hal);

/* Global variables ----------------------------------------------------------*/
static const struct gpio_dt_spec reset_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(mdmrst), gpios);
static const struct gpio_dt_spec dtr_gpio = GPIO_DT_SPEC_GET(DT_ALIAS(mdmdtr), gpios);
static struct gpio_callback ring_gpio_cb_data;
static const struct device* uart_dev = MDM_UART_DEV;

/* ISR Zephyr Callback Wrapper --------------------------------------------------------*/
static void ring_ping_cb(const struct device* dev, struct gpio_callback* cb, uint32_t pins)
{
    ST87EC_Lib_Hal_RingPinIsr();
}
static void uart_cb(const struct device* dev, void* ctx)
{
    uart_irq_update(uart_dev);
    ST87EC_Lib_Hal_UartIsr();
}
static void timer_cb(struct k_timer* timer_id)
{
    ST87EC_Lib_Hal_TickIsr();
}
K_TIMER_DEFINE(ticker_timer, timer_cb, NULL);

/* Exported functions --------------------------------------------------------*/
/**
 * @brief ST87 Set the GPIO for the ST87 Reset pin
 *
 * @param State : State to set on the Reset pin
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_GpioInit(void)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    /* ST87EC USER CODE BEGIN  */
    if (!gpio_is_ready_dt(&reset_gpio) || !gpio_is_ready_dt(&dtr_gpio)) {
        result = RESULT_KO;
    }
    /* Configure a GPO for ST87 Reset GPIO */
    if (gpio_pin_configure_dt(&reset_gpio, GPIO_OUTPUT) < 0) {
        result = RESULT_KO;
    }
    /* Configure a GPI for ST87 ring pin, when ring pin rises call ST87EC_Lib_Hal_RingPinIsr */
    if (gpio_pin_configure_dt(&dtr_gpio, GPIO_INPUT) < 0) {
        result = RESULT_KO;
    } else {
        if (gpio_pin_interrupt_configure_dt(&dtr_gpio, GPIO_INT_EDGE_TO_ACTIVE) < 0) {
            result = RESULT_KO;
        } else {
            gpio_init_callback(&ring_gpio_cb_data, ring_ping_cb, BIT(dtr_gpio.pin));
            gpio_add_callback(dtr_gpio.port, &ring_gpio_cb_data);
        }
    }

    k_timer_start(&ticker_timer, K_MSEC(1), K_MSEC(1));
    /* ST87EC USER CODE END  */

    return (result);
}

/**
 * @brief ST87 Set the GPIO for the ST87 Reset pin
 *
 * @param State : State to set on the Reset pin
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_DriveResetGpio(uint32_t State)
{
    ST87EC_Lib_Result_t result = RESULT_OK;
    /* ST87EC USER CODE BEGIN  */
    /* Configure a GPO to Reset the ST87 */
    if (gpio_pin_set_dt(&reset_gpio, State) != 0) {
        result = RESULT_KO;
    }
    /* ST87EC USER CODE END */

    return (result);
}

/**
 * @brief ST87 Ring Pin Isr function
 *
 * @param : None
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_RingPinIsr(void)
{
    ST87EC_Lib_Result_t result = RESULT_OK;

    ST87EC_Wrapper_RingPinIsr();

    return (result);
}

/**
 * @brief ST87 Rx/Tx UART interrupts enabling function.
 *
 * @param Flags : bitmap of interrupts to enable
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_EnableUartInterrupts(uint8_t Flags)
{
    uart_irq_callback_set(uart_dev, uart_cb);
    if ((Flags & ST87EC_UART_FLAG_RX_NOTEMPTY) == ST87EC_UART_FLAG_RX_NOTEMPTY) {
        /* ST87EC USER CODE BEGIN  */
        /* Enables interrupts for the UART RX communicating with the ST87 */
        /*LOG_INF("Enable RX");*/
        uart_irq_rx_enable(uart_dev);
        /* ST87EC USER CODE END  */
    }

    if ((Flags & ST87EC_UART_FLAG_TX_COMPLETE) == ST87EC_UART_FLAG_TX_COMPLETE) {
        /* ST87EC USER CODE BEGIN  */
        /* Enables interrupts for the UART TX communicating with the ST87 */
        /*LOG_INF("Enable TX");*/
        uart_irq_tx_enable(uart_dev);
        /* ST87EC USER CODE END  */
    }

    return RESULT_OK;
}

/**
 * @brief ST87 UART ISR handler wrapper function
 *
 * @param : None
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_UartIsr(void)
{
    return ST87EC_Wrapper_UartIsr();
}

/**
 * @brief  Function wrapping the driver function wri10:30 AMting a byte on ST87 UART Tx
 *
 * @param Byte : Char to send over UART
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_UartTxByte(uint8_t Byte)
{
    /* ST87EC USER CODE BEGIN  */
    /* Sends byte to UART TX to ST87 */
    uart_fifo_fill(uart_dev, &Byte, 1);
    /* ST87EC USER CODE END  */

    return RESULT_OK;
}

/**
 * @brief  Function wrapping the driver function reading a byte on ST87 UART Rx
 *
 * @param : None
 * @retval Byte read from ST87 UART
 */
uint8_t ST87EC_Lib_Hal_UartRxByte(void)
{
    uint8_t RxByte = 0U;

    /* ST87EC USER CODE BEGIN  */
    /* Receive byte from UART RX from ST87 */
    if (uart_fifo_read(uart_dev, &RxByte, 1) <= 0) {
        RxByte = 0U;
    }
    /* ST87EC USER CODE END  */

    return RxByte;
}

/**
 * @brief ST87 Rx/Tx UART driver status reading function.
 *
 * @param UartChan : UART channel selection (Rx or Tx)
 * @param StsFlagToGet : status flag to read (only one flag indication expected, not a bitmap)
 * @retval flag value read
 */
bool ST87EC_Lib_Hal_UartGetStatusFlag(ST87EC_Lib_UartChannel_t UartChan, uint8_t StsFlagToGet)
{
    bool status = false;

    if (UartChan == UART_CHANNEL_RX) {
        if (StsFlagToGet == ST87EC_UART_FLAG_RX_NOTEMPTY) {
            /* ST87EC USER CODE BEGIN  */
            /* Get host UART RX flag */
            status = uart_irq_rx_ready(uart_dev) == 1;
            /* ST87EC USER CODE END  */

        } else {
        }
    } else if (UartChan == UART_CHANNEL_TX) {
        if (StsFlagToGet == ST87EC_UART_FLAG_TX_COMPLETE) {
            /* ST87EC USER CODE BEGIN  */
            /* Get host UART TX flag */
            status = uart_irq_tx_complete(uart_dev) == 1;
            /* ST87EC USER CODE END  */
        } else {
        }
    } else {
    }

    return status;
}

/**
 * @brief ST87 Rx/Tx UART driver status flag clearing function.
 *
 * @param UartChan : UART channel selection (Rx or Tx)
 * @param FlagsToClear : bitmap of status flag to clear
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_UartClearFlags(ST87EC_Lib_UartChannel_t UartChan, uint8_t FlagsToClear)
{

    if (UartChan == UART_CHANNEL_RX) {
        if ((FlagsToClear & ST87EC_UART_CLEAR_FLAG_RXOVERRUN) == ST87EC_UART_CLEAR_FLAG_RXOVERRUN) {
            /* ST87EC USER CODE BEGIN  */
            /* Clear host UART RX flag */
            // uart_irq_rx_disable(uart_dev);
            /* ST87EC USER CODE END  */
        }
    } else if (UartChan == UART_CHANNEL_TX) {
        if ((FlagsToClear & ST87EC_UART_CLEAR_FLAG_TXCOMPLETE) == ST87EC_UART_CLEAR_FLAG_TXCOMPLETE) {
            /* ST87EC USER CODE BEGIN  */
            /* Clear host UART TX flag */
            uart_irq_tx_disable(uart_dev);
            /* ST87EC USER CODE END  */
        }
    } else {
    }

    return RESULT_OK;
}

/**
 * @brief ST87 Tick Isr function
 *
 * @param : None
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_TickIsr(void)
{
    return ST87EC_Wrapper_TickIsr();
}

/**
 * @brief Host baudrate setting for ST87 UART
 *
 * @param: BaudRate
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_SetHostUartBaudrate(uint32_t BaudRate)
{
    /* ST87EC USER CODE BEGIN  */
    ST87EC_Lib_Result_t result = RESULT_OK;
    /* Change Host UART baudrate by stopping/restarting UART drivers */
    struct uart_config cfg;
    if (uart_config_get(uart_dev, &cfg) == 0) {
        cfg.baudrate = BaudRate;

        if (uart_configure(uart_dev, &cfg) != 0) {
            result = RESULT_KO;
        }
    } else {
        result = RESULT_KO;
    }
    /* Enable back UART interrupts */
    ST87EC_Lib_Hal_EnableUartInterrupts(ST87EC_UART_FLAG_RX_NOTEMPTY | ST87EC_UART_FLAG_TX_COMPLETE);
    /* ST87EC USER CODE END  */

    return result;
}

/**
 * @brief Host baudrate setting for ST87 UART
 *
 * @param: None
 * @retval Returned baudrate
 */
uint32_t ST87EC_Lib_Hal_GetHostUartBaudrate(void)
{
    uint32_t baudrate = 0;
    /* ST87EC USER CODE BEGIN  */
    struct uart_config cfg;
    if (uart_config_get(uart_dev, &cfg) == 0) {
        baudrate = cfg.baudrate;
    } else {
        baudrate = 0;
    }
    /* ST87EC USER CODE END  */
    return baudrate;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
