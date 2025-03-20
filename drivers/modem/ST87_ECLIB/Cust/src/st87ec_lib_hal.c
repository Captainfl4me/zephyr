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
#include "../../../st87m01.h"
#include "../inc/st87ec_lib_hal.h"
#include "../../Core/inc/st87ec_lib.h"
#include "../../Core/inc/st87ec_wrapper.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Constants -----------------------------------------------------------------*/
LOG_MODULE_REGISTER(ST87EC_Lib_Hal);

/* Global variables ----------------------------------------------------------*/

/* ISR Zephyr Callback Wrapper --------------------------------------------------------*/
static void ring_ping_cb(const struct device* dev, struct gpio_callback* cb, uint32_t pins)
{
    ST87EC_Lib_Hal_RingPinIsr();
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
 * @brief ST87 Tick Isr function
 *
 * @param : None
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_Lib_Hal_TickIsr(void)
{
    return ST87EC_Wrapper_TickIsr();
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
