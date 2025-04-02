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
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_pkt.h>
#include "../modem_receiver.h"

/* Exported constants and macros-----------------------------------------------*/

#define MDM_AT_CMD_TIMEOUT 5000
#define MDM_RECV_MAX_BUF   30
#define MDM_RECV_BUF_SIZE  128

#define SOCKET_SEND_TIMEOUT       10
#define SOCKET_RECEIVE_TIMEOUT    10
#define SOCKET_FRAME_RECEIVED_URC 1

#define BUF_ALLOC_TIMEOUT K_SECONDS(1)

#define CHAR_OFFSET     48
#define MDM_MAX_SOCKETS 3

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
	struct net_if *iface;
	uint8_t mac_addr[6];

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
 * Output value of the SIM status.
 */
typedef enum {
	SIM_STATUS_SIM_INVALID = 0, /**< SIM is invalid.            */
	SIM_STATUS_SIM_VALID = 1,   /**< SIM is valid.              */
	SIM_STATUS_UNKNOWN = 0xFF,  /**< SIM status is unknown.     */
} eclib_sim_status_t;

/**
 * Output value of the connection status.
 */
typedef enum {
	CONN_STATUS_IDLE = 0,       /**< The stack is in IDLE state.          */
	CONN_STATUS_CONNECTED = 1,  /**< The stack is in CONNECTED state.     */
	CONN_STATUS_UNKNOWN = 0xFF, /**< The stack is in unknown state.       */
} eclib_connection_status_t;

/**
 * Output value of the registration status.
 */
typedef enum {
	NOT_REGISTERED = 0, /**< The module is not registered to the network.*/
	REGISTERED = 1,     /**< The module is registered to the network.    */
} eclib_registration_status_t;

/**
 * Values of IpMode.
 */
typedef enum {
	IPV4_MODE = 0, /**< The module is in IPV4 mode.*/
	IPV6_MODE = 1, /**< The module is in IPV6 mode.*/
} eclib_ip_mode_t;

/**
 * Values of SocketType.
 */
typedef enum {
	UDP = 0, /**< UDP socket type.*/
	TCP = 1, /**< TCP socket type.*/
	RAW = 2, /**< RAW socket type.*/
} eclib_socket_type_t;

typedef struct {
	int id;
	eclib_socket_type_t type;
	eclib_ip_mode_t ip_mode;
	struct net_context *context;
	sa_family_t family;
	enum net_ip_protocol ip_proto;

	/** socket callbacks */
	net_context_recv_cb_t recv_cb;
	void *recv_user_data;
} eclib_socket_t;

/**
 * Static Data for ECLIB
 */
struct eclib_register {
	struct mdm_receiver_context *mctx;
	struct gpio_dt_spec *reset_gpio;
	struct gpio_dt_spec *ring_gpio;
};

static uint8_t mdm_recv_buf[CONFIG_MODEM_ST87M01_MAX_RX_DATA_LENGTH];

/* Exported functions --------------------------------------------------------*/

eclib_result_t eclib_init(struct eclib_register *eclib_register);
eclib_result_t eclib_reset();
eclib_result_t eclib_wait_for_cereg_cscon();
unsigned int eclib_send_sync_at(unsigned int timeout, const char *format, ...);
unsigned int eclib_send_sync_with_bin_at(unsigned int timeout, const char *format, ...);
eclib_result_t eclib_cold_param_init(void);
eclib_result_t eclib_get_socket(struct net_context **context, enum net_ip_protocol ip_proto, sa_family_t family);
eclib_result_t eclib_create_socket(eclib_socket_t *socket);
eclib_result_t eclib_recv_socket(eclib_socket_t *socket, net_context_recv_cb_t cb, void *user_data);
eclib_result_t eclib_close_socket(eclib_socket_t *socket);
int eclib_send_to_socket(eclib_socket_t *socket, const struct sockaddr *dst_addr,
			 struct net_pkt *pkt);
eclib_result_t eclib_read_socket(eclib_socket_t *socket);

#endif // #ECLIB_H
