/* Copyright (c) 2025 Nicolas THIERRY <nicolas.thierry@estaca.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "./ST87_ECLIB/Core/inc/st87ec_lib.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include "st87m01.h"

LOG_MODULE_REGISTER(modem_st87m01, CONFIG_MODEM_LOG_LEVEL);

static void modem_reset(void);
static void offload_iface_init(struct net_if *iface);

static struct offloaded_if_api api_funcs = {
	.iface_api.init = offload_iface_init,
};

void eclib_error_cb(ST87EC_Lib_SequenceValue_t FaillingSequence, ST87EC_Lib_ErrorCode_t Error)
{
    LOG_ERR("ECLIB ERR: SEQ = %d, ERROR = %d", FaillingSequence, Error);
}

/*
 * Initializes modem handlers and context.
 * After successful init this function calls
 * modem_setup.
 */
static int modem_init(const struct device *dev)
{
	int ret = 0;
	ARG_UNUSED(dev);
	LOG_DBG("ST87M01 Init");
	ST87EC_Lib_Status_t pstate;
	ST87EC_Lib_Result_t res = ST87EC_Lib_Init(eclib_error_cb);

	mctx.data_manufacturer = mdata.mdm_manufacturer;
	mctx.data_model = mdata.mdm_model;
	mctx.data_revision = mdata.mdm_revision;
#if defined(CONFIG_MODEM_SIM_NUMBERS)
	mctx.data_imei = mdata.mdm_imei;
	mctx.data_imsi = mdata.mdm_imsi;
#endif
	mctx.data_rssi = &mdata.mdm_rssi;

	ret = mdm_receiver_register(&mctx, MDM_UART_DEV, mdm_recv_buf, sizeof(mdm_recv_buf));
	if (ret < 0) {
		LOG_ERR("Error registering modem context: %d", ret);
		goto error;
	}

	modem_reset();

error:
	return ret;
}

static void modem_reset(void)
{
}

/* Setup the Modem NET Interface. */
static void offload_iface_init(struct net_if *iface)
{
}

/* Register device with the networking stack. */
NET_DEVICE_DT_INST_OFFLOAD_DEFINE(0, modem_init, NULL, &mdata, NULL,
				  CONFIG_MODEM_ST87M01_INIT_PRIORITY, &api_funcs,
				  MDM_MAX_DATA_LENGTH);
