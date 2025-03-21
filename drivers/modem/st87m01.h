/* Copyright (c) 2025 Nicolas THIERRY <nicolas.thierry@estaca.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef STM87M01_H
#define STM87M01_H

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/net/net_context.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/offloaded_netdev.h>

#include "modem_receiver.h"

/* Exported constants and macros-----------------------------------------------*/

#define MDM_UART_NODE DT_INST_BUS(0)
#define MDM_UART_DEV  DEVICE_DT_GET(MDM_UART_NODE)

/* Types ---------------------------------------------------------------------*/

static struct mdm_receiver_context mctx;

static const struct gpio_dt_spec reset_gpio = GPIO_DT_SPEC_INST_GET(0, mdm_reset_gpios);
static const struct gpio_dt_spec ring_gpio = GPIO_DT_SPEC_INST_GET(0, mdm_ring_gpios);

/* Exported functions --------------------------------------------------------*/

static void modem_reset(void);
static void offload_iface_init(struct net_if *iface);

static struct offloaded_if_api api_funcs = {
	.iface_api.init = offload_iface_init,
};

#endif // #STM87M01_H
