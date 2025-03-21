/* Copyright (c) 2025 Nicolas THIERRY <nicolas.thierry@estaca.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ECLIB_H
#define ECLIB_H

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "../modem_receiver.h"

/* Exported constants and macros-----------------------------------------------*/

#define MDM_MAX_DATA_LENGTH 1024
#define MDM_MAX_TX_DATA_LENGTH 1024
#define MDM_AT_CMD_TIMEOUT 5000
#define MDM_RECV_MAX_BUF    30
#define MDM_RECV_BUF_SIZE   128

#define BUF_ALLOC_TIMEOUT K_SECONDS(1)

/*#define MDM_MAX_SOCKETS 6*/

/*
 * Default length of modem data.
 */
#define MDM_MANUFACTURER_LENGTH 16
#define MDM_MODEL_LENGTH        8
#define MDM_REVISION_LENGTH     22
#define MDM_IMEI_LENGTH         16
#define MDM_IMSI_LENGTH         16
#define MDM_ICCID_LENGTH        32

/* Types ---------------------------------------------------------------------*/

/* Modem data */
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

/**
 * Output value of the API functions.
 */
typedef enum {
	RESULT_OK = 0,            /**< Function execution status is OK.       */
	RESULT_KO = 1,            /**< Function execution status is KO.       */
	RESULT_BUSY = 2,          /**< Function execution status is busy      */
	RESULT_BAD_PARAM = 3,     /**< Function gets bad parameter            */
	RESULT_BAD_SEQUENCING = 4 /**< Function gets bad FSM state sequencing */
} eclib_result_t;

/**
 * Static Data for ECLIB
 */
struct eclib_register {
	struct mdm_receiver_context *mctx;
	struct gpio_dt_spec *reset_gpio;
	struct gpio_dt_spec *ring_gpio;
};

static uint8_t mdm_recv_buf[MDM_MAX_DATA_LENGTH];
static uint8_t mdm_tx_buf[MDM_MAX_TX_DATA_LENGTH];

/* Exported functions --------------------------------------------------------*/

eclib_result_t eclib_init(struct eclib_register *eclib_register);
eclib_result_t eclib_reset();
unsigned int eclib_send_sync_at(unsigned int timeout, const char *format, ...);
eclib_result_t eclib_cold_param_init(void);

#endif // #ECLIB_H
