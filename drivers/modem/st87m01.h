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

static void offload_iface_init(struct net_if *iface);

static int offload_get(sa_family_t family, enum net_sock_type type, enum net_ip_protocol ip_proto,
		       struct net_context **context);
static int offload_bind(sa_family_t family, enum net_sock_type type, enum net_ip_protocol ip_proto,
			struct net_context **context);
static int offload_listen(sa_family_t family, enum net_sock_type type,
			  enum net_ip_protocol ip_proto, struct net_context **context);
static int offload_connect(sa_family_t family, enum net_sock_type type,
			   enum net_ip_protocol ip_proto, struct net_context **context);
static int offload_accept(sa_family_t family, enum net_sock_type type,
			  enum net_ip_protocol ip_proto, struct net_context **context);
static int offload_send(sa_family_t family, enum net_sock_type type, enum net_ip_protocol ip_proto,
			struct net_context **context);
static int offload_sendto(sa_family_t family, enum net_sock_type type,
			  enum net_ip_protocol ip_proto, struct net_context **context);
static int offload_recv(sa_family_t family, enum net_sock_type type, enum net_ip_protocol ip_proto,
			struct net_context **context);
static int offload_put(sa_family_t family, enum net_sock_type type, enum net_ip_protocol ip_proto,
		       struct net_context **context);

static struct net_offload offload_funcs = {
	.get = offload_get,
	.bind = offload_bind,
	.listen = offload_listen,
	.connect = offload_connect,
	.accept = offload_accept,
	.send = offload_send,
	.sendto = offload_sendto,
	.recv = offload_recv,
	.put = offload_put,
};

static struct offloaded_if_api api_funcs = {
	.iface_api.init = offload_iface_init,
};

#endif // #STM87M01_H
