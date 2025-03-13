/* Copyright (c) 2025 Nicolas THIERRY <nicolas.thierry@estaca.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/net/net_context.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/offloaded_netdev.h>

#include "modem_receiver.h"

#define DT_DRV_COMPAT st_87m01

#define MDM_UART_NODE DT_INST_BUS(0)
#define MDM_UART_DEV  DEVICE_DT_GET(MDM_UART_NODE)

#define MDM_MAX_DATA_LENGTH 1024
#define MDM_RECV_MAX_BUF    30
#define MDM_RECV_BUF_SIZE   128

#define MDM_MAX_SOCKETS 6

/*
 * Default length of modem data.
 */
#define MDM_MANUFACTURER_LENGTH 12
#define MDM_MODEL_LENGTH        16
#define MDM_REVISION_LENGTH     64
#define MDM_IMEI_LENGTH         16
#define MDM_IMSI_LENGTH         16
#define MDM_ICCID_LENGTH        32


/* driver data */
struct modem_data {
	/* modem data */
	char mdm_manufacturer[MDM_MANUFACTURER_LENGTH];
	char mdm_model[MDM_MODEL_LENGTH];
	char mdm_revision[MDM_REVISION_LENGTH];
	char mdm_imei[MDM_IMEI_LENGTH];
	char mdm_imsi[MDM_IMSI_LENGTH];
	int mdm_rssi;
};

static struct modem_data mdata;
static struct mdm_receiver_context mctx;

static uint8_t mdm_recv_buf[MDM_MAX_DATA_LENGTH];

static void modem_reset(void);
static void offload_iface_init(struct net_if *iface);

static struct offloaded_if_api api_funcs = {
	.iface_api.init = offload_iface_init,
};
