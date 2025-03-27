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
static int offload_bind(struct net_context *context, const struct sockaddr *addr,
			socklen_t addrlen);
static int offload_listen(struct net_context *context, int backlog);
static int offload_connect(struct net_context *context, const struct sockaddr *addr,
			   socklen_t addrlen, net_context_connect_cb_t cb, int32_t timeout,
			   void *user_data);
static int offload_accept(struct net_context *context, net_tcp_accept_cb_t cb, int32_t timeout,
			  void *user_data);
static int offload_send(struct net_pkt *pkt, net_context_send_cb_t cb, int32_t timeout,
			void *user_data);
static int offload_sendto(struct net_pkt *pkt, const struct sockaddr *dst_addr, socklen_t addrlen,
			  net_context_send_cb_t cb, int32_t timeout, void *user_data);
static int offload_recv(struct net_context *context, net_context_recv_cb_t cb, int32_t timeout,
			void *user_data);
static int offload_put(struct net_context *context);

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
