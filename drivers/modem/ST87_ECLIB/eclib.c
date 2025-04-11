#include "eclib.h"
#include "at_commands.h"

#include <zephyr/net_buf.h>

#include <string.h>
#include <stdarg.h>

LOG_MODULE_REGISTER(eclib, CONFIG_MODEM_LOG_LEVEL);

/* Private macros ------------------------------------------------------------*/

#define CMD_HANDLER(cmd_, cb_)                                                                     \
	{.cmd = cmd_, .cmd_len = (uint16_t)sizeof(cmd_) - 1, .func = on_cmd_##cb_}

/* Private types -------------------------------------------------------------*/

/**
 * Static Data for ECLIB
 */
struct eclib_data {
	struct net_if *iface;
	struct mdm_receiver_context *mctx;
	struct gpio_dt_spec *reset_gpio;
	struct gpio_dt_spec *ring_gpio;

	struct k_sem response_sem;
	uint8_t last_response_error;

	uint8_t cold_init_version;
	eclib_sim_status_t sim_status;
	eclib_connection_status_t connection_status;
	eclib_registration_status_t registration_status;
	eclib_sleep_wakeup_status_t sleep_wakeup_status;
	uint8_t context_id;
	eclib_ip_mode_t ip_mode;
	int last_socket_id;
	uint8_t last_ipread_size;

	void (*next_is_raw_cb)(struct net_buf **buf, uint16_t len);

	eclib_socket_t sockets[MDM_MAX_SOCKETS];
	struct in_addr ipv4_addr;
};

/**
 * ST87 Reset states
 */
typedef enum {
	RESET_PIN_OFF = 0,  /**< Reset Pin off      */
	RESET_PIN_ON = 1,   /**< Reset Pin on       */
	RESET_PIN_PULSE = 2 /**< Reset Pin toggle   */
} eclib_reset_pin_t;

/**
 * AT CMD Handler descriptor
 */
struct cmd_handler {
	const char *cmd;
	uint16_t cmd_len;
	void (*func)(struct net_buf **buf, uint16_t len);
};

/* Private variables ---------------------------------------------------------*/

static struct eclib_data eclib_data;
static struct gpio_callback ring_gpio_callback_data;

/* RX thread structures */
K_KERNEL_STACK_DEFINE(eclib_rx_stack, CONFIG_MODEM_ST87M01_RX_STACK_SIZE);
struct k_thread eclib_rx_thread;

NET_BUF_POOL_DEFINE(mdm_recv_pool, MDM_RECV_MAX_BUF, MDM_RECV_BUF_SIZE, 0, NULL);

/* Private functions ---------------------------------------------------------*/

eclib_result_t eclib_gpio_init(void);
eclib_result_t eclib_drive_reset_pin(eclib_reset_pin_t State);

static void eclib_rx();
static void ring_ping_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

static void on_cmd_atok(struct net_buf **buf, uint16_t len);
static void on_cmd_aterror(struct net_buf **buf, uint16_t len);
static void on_cmd_sim_status(struct net_buf **buf, uint16_t len);
static void on_cmd_connection_status(struct net_buf **buf, uint16_t len);
static void on_cmd_registration_status(struct net_buf **buf, uint16_t len);
static void on_cmd_ip_config_status(struct net_buf **buf, uint16_t len);
static void on_cmd_sleep(struct net_buf **buf, uint16_t len);
static void on_cmd_wakeup(struct net_buf **buf, uint16_t len);
static void on_cmd_nvmread(struct net_buf **buf, uint16_t len);
static void on_cmd_socket_create(struct net_buf **buf, uint16_t len);
static void on_cmd_socket_iprecv(struct net_buf **buf, uint16_t len);
static void on_cmd_socket_ipread(struct net_buf **buf, uint16_t len);
#ifdef CONFIG_MODEM_ST87M01_RX_AT_FULL_LOG
static void on_cmd_fullmatch(struct net_buf **buf, uint16_t len);
#endif

static uint8_t is_crlf(uint8_t c);
static void net_buf_skipcrlf(struct net_buf **buf);
static uint16_t net_buf_findcrlf(struct net_buf *buf, struct net_buf **frag, uint16_t *offset);
static int net_buf_ncmp(struct net_buf *buf, const uint8_t *s2, size_t n);
char *net_sprint_ip_addr(const struct sockaddr *addr);

/* Exported functions --------------------------------------------------------*/

eclib_result_t eclib_init(struct eclib_register *eclib_register)
{
	LOG_DBG("ECLIB Init");
	uint8_t status = 0;
	eclib_result_t result = RESULT_OK;

	/* Register Data to Local eclib_data */
	eclib_data.mctx = eclib_register->mctx;
	eclib_data.reset_gpio = eclib_register->reset_gpio;
	eclib_data.ring_gpio = eclib_register->ring_gpio;

	/* Init response semaphore */
	k_sem_init(&eclib_data.response_sem, 0, 1);

	eclib_data.sim_status = SIM_STATUS_UNKNOWN;
	eclib_data.connection_status = CONN_STATUS_UNKNOWN;
	eclib_data.registration_status = NOT_REGISTERED;
	eclib_data.sleep_wakeup_status = STATUS_WAKEUP;

	eclib_data.context_id = 0;
	eclib_data.ip_mode = 0;
	eclib_data.last_socket_id = -1;

	eclib_data.last_ipread_size = 0;
	eclib_data.next_is_raw_cb = NULL;

	for (uint8_t i = 0; i < MDM_MAX_SOCKETS; i++) {
		eclib_data.sockets[i].context = NULL;
		eclib_data.sockets[i].id = -1;
	}

	/* Start RX thread */
	k_thread_create(&eclib_rx_thread, eclib_rx_stack, K_KERNEL_STACK_SIZEOF(eclib_rx_stack),
			eclib_rx, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	/* Reset of the whole system as init */
	status += (uint8_t)eclib_reset();

	/* Trig cold parameter initialization sequence */
	status += (uint8_t)eclib_cold_param_init();

	// TODO: retrieve mdm_manufacturer, mdm_model, mdm_revision, <mdm_imei, mdm_imsi>

	if (status > 0) {
		result = RESULT_KO;
	}

	return (result);
}
void eclib_register_iface(struct net_if *iface)
{
	eclib_data.iface = iface;
}

eclib_result_t eclib_reset()
{
	uint8_t status = 0;
	eclib_result_t result = RESULT_OK;

	/* GPIO initialization */
	status = (uint8_t)eclib_gpio_init();

	/* Reset ST87 module */
	status += (uint8_t)eclib_drive_reset_pin(RESET_PIN_PULSE);

	k_msleep(500); /* Wait after ST87 Hw reset prior to proceeding */

	if (status > 0) {
		result = RESULT_KO;
	}

	return (result);
}

int eclib_wakeup()
{
	int ret = 0;

	mdm_receiver_send(eclib_data.mctx, "\r", 1);
	if (eclib_data.sleep_wakeup_status == STATUS_SLEEP) {
		k_sem_reset(&eclib_data.response_sem);
		ret = k_sem_take(&eclib_data.response_sem, K_MSEC(MDM_AT_CMD_WAKEUP_TIMEOUT));
	}

	return ret;
}

int eclib_sleep()
{
	int ret = 0;

	if (eclib_data.sleep_wakeup_status != STATUS_SLEEP) {
		eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, "AT#SLEEPMODE");
	}

	return ret;
}

unsigned int eclib_send_sync_at(unsigned int timeout, const char *format, ...)
{
	int ret;
	uint8_t mdm_tx_buf[CONFIG_MODEM_ST87M01_MAX_TX_DATA_LENGTH];

	/* Parsing command */
	va_list args;
	va_start(args, format);
	uint32_t length_sent = vsprintf((char *)(mdm_tx_buf), (const char *)format, args);
	va_end(args);

	ret = eclib_wakeup();

	eclib_data.last_response_error = 0;
	LOG_DBG("OUT: [%s]", mdm_tx_buf);
	mdm_receiver_send(eclib_data.mctx, mdm_tx_buf, length_sent);
	mdm_receiver_send(eclib_data.mctx, "\r\n", 2);

	if (timeout == 0) {
		return length_sent + 2;
	}

	k_sem_reset(&eclib_data.response_sem);
	ret = k_sem_take(&eclib_data.response_sem, K_MSEC(timeout));

	if (ret == -EAGAIN) {
		return 0;
	} else {
		return length_sent + 2;
	}
}

eclib_result_t eclib_cold_param_init(void)
{
	eclib_result_t result = RESULT_OK;
	uint32_t tmp;

	if (eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, ST87EC_NVMRD_CMD) == 0) {
		LOG_ERR("ST87EC_NVMRD_CMD timeout!");
		return RESULT_KO;
	}

	if (eclib_data.last_response_error > 0) {
		return RESULT_KO;
	}

	if (eclib_data.cold_init_version == ST87EC_COLD_CONFIG_VERSION) {
		/* Cold condig already up-to-date */
		LOG_DBG("ST87M01 NVM up-to-date");
		return RESULT_OK;
	}
	LOG_DBG("ST87M01 NVM config version mismatch: %d, rewriting config...",
		eclib_data.cold_init_version);

	for (size_t k = 0; k < ST87EC_COLD_INIT_CMD_SIZE; k++) {
		if (eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, ST87EC_COLD_INIT_COMMANDS[k]) == 0) {
			LOG_ERR("ST87EC_NVMRD_CMD timeout!");
			return RESULT_KO;
		}

		if (eclib_data.last_response_error > 0) {
			return RESULT_KO;
		}

		k_msleep(50);
	}

	return (result);
}

eclib_result_t eclib_get_socket(struct net_context **context, enum net_ip_protocol ip_proto,
				sa_family_t family)
{
	if (eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, "AT#SOCKETCREATE?") == 0) {
		LOG_ERR("SOCKETCREATE reading timeout!");
		return RESULT_KO;
	}

	eclib_socket_t *sock = NULL;
	for (uint8_t i = 0; i < MDM_MAX_SOCKETS; i++) {
		if (eclib_data.sockets[i].context == NULL) {
			sock = &eclib_data.sockets[i];
			break;
		}
	}

	if (!sock) {
		return -ENOMEM;
	}

	(*context)->offload_context = sock;
	sock->id = -1;
	sock->ip_proto = ip_proto;
	sock->family = family;
	switch (ip_proto) {
	case IPPROTO_TCP:
		sock->type = TCP;
		break;
	case IPPROTO_UDP:
		sock->type = UDP;
		break;
	default:
		LOG_ERR("PROTOCOL NOT SUPPORTED!");
		return -ENOSYS;
	}
	sock->context = *context;
}

eclib_result_t eclib_create_socket(eclib_socket_t *socket)
{
	/* No socket created yet, create a new one */
	if (socket->id < 0) {
		int ip_mode = -1;
#if defined(CONFIG_NET_IPV6)
		if (socket->family == AF_INET6) {
			ip_mode = 1;
		} else
#endif
#if defined(CONFIG_NET_IPV4)
			if (socket->family == AF_INET) {
			ip_mode = 0;
		} else
#endif
		{
			LOG_ERR("Addr sa_family not supported: %d", socket->family);
			return -EINVAL;
		}

		switch (socket->type) {
		case UDP:
			if (eclib_send_sync_at(
				    MDM_AT_CMD_TIMEOUT, "AT#SOCKETCREATE=%d,%d,%s,,%d,%d,%d",
				    eclib_data.context_id, ip_mode, "UDP", SOCKET_SEND_TIMEOUT,
				    SOCKET_RECEIVE_TIMEOUT, SOCKET_FRAME_RECEIVED_URC) == 0) {
				LOG_ERR("SOCKETCREATE create timeout!");
				return RESULT_KO;
			}
			break;
		case TCP:
			if (eclib_send_sync_at(
				    MDM_AT_CMD_TIMEOUT, "AT#SOCKETCREATE=%d,%d,%s,,%d,%d,%d",
				    eclib_data.context_id, ip_mode, "TCP", SOCKET_SEND_TIMEOUT,
				    SOCKET_RECEIVE_TIMEOUT, SOCKET_FRAME_RECEIVED_URC) == 0) {
				LOG_ERR("SOCKETCREATE create timeout!");
				return RESULT_KO;
			}
			break;
		}

		// Socket creation successful
		socket->id = eclib_data.last_socket_id;
	}

	return RESULT_OK;
}

eclib_result_t eclib_recv_socket(eclib_socket_t *socket, net_context_recv_cb_t cb, void *user_data)
{
	socket->recv_cb = cb;
	socket->recv_user_data = user_data;

	return RESULT_OK;
}

int eclib_send_to_socket(eclib_socket_t *socket, const struct sockaddr *dst_addr,
			 struct net_pkt *pkt)
{
	if (socket->id < 0) {
		LOG_ERR("Socket does not have allocated ID");
		return -EINVAL;
	}

	int ret;
	size_t data_len = net_buf_frags_len(pkt->frags);
	/* No socket created yet, create a new one */
	switch (socket->type) {
	case UDP:
		struct sockaddr *addr = dst_addr;
		if (dst_addr == NULL) {
			addr = &socket->conn_addr;
		}

		int dst_port = -1;
#if defined(CONFIG_NET_IPV6)
		if (addr->sa_family == AF_INET6) {
			dst_port = ntohs(net_sin6(addr)->sin6_port);
		} else
#endif
#if defined(CONFIG_NET_IPV4)
			if (addr->sa_family == AF_INET) {
			dst_port = ntohs(net_sin(addr)->sin_port);
		} else
#endif
		{
			LOG_ERR("Addr sa_family not supported: %d", addr->sa_family);
			return -EINVAL;
		}

		eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, "AT#IPSENDUDP=%d,%d,%s,%d,%d,%d,%d",
				   eclib_data.context_id, socket->id, net_sprint_ip_addr(addr),
				   dst_port, 0, 1, data_len);
		break;
	case TCP:
		eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, "AT#IPSENDTCP=%d,%d,%d,%d",
				   eclib_data.context_id, socket->id, 1, data_len);
		break;
	}

	/* Loop through packet data and send */
	struct net_buf *frag = pkt->frags;
	while (frag) {
		mdm_receiver_send(eclib_data.mctx, frag->data, frag->len);
		frag = frag->frags;
	}

	k_sem_reset(&eclib_data.response_sem);
	ret = k_sem_take(&eclib_data.response_sem, K_MSEC(MDM_AT_CMD_TIMEOUT));

	if (ret == -EAGAIN) {
		return -EAGAIN;
	}

	return data_len;
}

eclib_result_t eclib_read_socket(eclib_socket_t *socket)
{
	if (eclib_send_sync_at(0, "AT#IPREAD=%d,%d", eclib_data.context_id, socket->id) == 0) {
		return RESULT_KO;
	}
	return RESULT_OK;
}

eclib_result_t eclib_close_socket(eclib_socket_t *socket)
{
	if (socket->id >= 0) {
		if (eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, "AT#SOCKETCLOSE=%d,%d",
				       eclib_data.context_id, socket->id) == 0) {
			return RESULT_KO;
		}
	}

	socket->id = -1;
	socket->context = NULL;

	eclib_sleep();

	return RESULT_OK;
}

/* Private functions --------------------------------------------------------*/

eclib_result_t eclib_gpio_init(void)
{
	eclib_result_t result = RESULT_OK;

	/* ST87EC USER CODE BEGIN  */
	if (!gpio_is_ready_dt(eclib_data.reset_gpio) || !gpio_is_ready_dt(eclib_data.ring_gpio)) {
		result = RESULT_KO;
	}

	/* Configure a GPO for ST87 Reset GPIO */
	if (gpio_pin_configure_dt(eclib_data.reset_gpio, GPIO_OUTPUT) < 0) {
		result = RESULT_KO;
	}

	/* Configure a GPI for ST87 ring pin, when ring pin rises call
	 * ST87EC_Lib_Hal_RingPinIsr */
	if (gpio_pin_configure_dt(eclib_data.ring_gpio, GPIO_INPUT) < 0) {
		result = RESULT_KO;
	} else {
		if (gpio_pin_interrupt_configure_dt(eclib_data.ring_gpio, GPIO_INT_EDGE_TO_ACTIVE) <
		    0) {
			result = RESULT_KO;
		} else {
			gpio_init_callback(&ring_gpio_callback_data, ring_ping_cb,
					   BIT(eclib_data.ring_gpio->pin));
			gpio_add_callback(eclib_data.ring_gpio->port, &ring_gpio_callback_data);
		}
	}

	return (result);
}

/**
 * @brief ST87MXX Reset pin management
 *
 * @param State: State to set on the Reset pin
 * @retval Function execution status
 */
eclib_result_t eclib_drive_reset_pin(eclib_reset_pin_t state)
{
	uint8_t status = 0;
	eclib_result_t result = RESULT_OK;

	switch (state) {
	case RESET_PIN_ON:
		status = gpio_pin_set_dt(eclib_data.reset_gpio, 1);
		break;

	case RESET_PIN_OFF:
		status = gpio_pin_set_dt(eclib_data.reset_gpio, 0);
		break;

	case RESET_PIN_PULSE:
		status = gpio_pin_set_dt(eclib_data.reset_gpio, 0);
		k_msleep(15);
		status += gpio_pin_set_dt(eclib_data.reset_gpio, 1);
		break;
	}

	if (status < 0) {
		return RESULT_OK;
	} else {
		return RESULT_KO;
	}
}

static inline struct net_buf *read_rx_allocator(k_timeout_t timeout, void *user_data)
{
	return net_buf_alloc((struct net_buf_pool *)user_data, timeout);
}

static void eclib_read_rx(struct net_buf **rx_buf)
{
	int ret;
	size_t bytes_read = 0, rx_len;
	uint8_t uart_rx_buf[CONFIG_MODEM_ST87M01_MAX_RX_DATA_LENGTH];

	while (true) {
		ret = mdm_receiver_recv(eclib_data.mctx, uart_rx_buf, sizeof(uart_rx_buf),
					&bytes_read);
		if (ret < 0 || bytes_read == 0) {
			/* mdm_receiver buffer is empty */
			break;
		}

		/* Allocate net_buf if not already done */
		if (!*rx_buf) {
			*rx_buf = net_buf_alloc(&mdm_recv_pool, BUF_ALLOC_TIMEOUT);
			if (!*rx_buf) {
				LOG_ERR("Can't allocate RX data! "
					"Skipping data!");
				break;
			}
		}

		rx_len = net_buf_append_bytes(*rx_buf, bytes_read, uart_rx_buf, BUF_ALLOC_TIMEOUT,
					      read_rx_allocator, &mdm_recv_pool);

		if (rx_len < bytes_read) {
			LOG_ERR("Data was lost! read %u of %zu!", rx_len, bytes_read);
		}
	}
}

static void eclib_rx()
{
	LOG_DBG("Start RX thread");
	struct net_buf *rx_buf = NULL;
	struct net_buf *frag = NULL;
	uint16_t offset, len;

	static const struct cmd_handler handlers[] = {
		/* GENERIC RESPONSES */
		CMD_HANDLER("OK", atok),
		CMD_HANDLER("ERROR", aterror),
		CMD_HANDLER("+CME ERROR", aterror),
		/* LOCAL STATUS */
		CMD_HANDLER("#SIMST", sim_status),
		CMD_HANDLER("+CSCON", connection_status),
		CMD_HANDLER("+CEREG", registration_status),
		CMD_HANDLER("#IPCFG", ip_config_status),
		CMD_HANDLER("#SLEEP", sleep),
		CMD_HANDLER("#WAKEUP", wakeup),
		/* CONFIG RESPONSES */
		CMD_HANDLER("#NVMRD", nvmread),
		/* SOCKET RESPONSES */
		CMD_HANDLER("#SOCKETCREATE", socket_create),
		CMD_HANDLER("#IPRECV", socket_iprecv),
		CMD_HANDLER("#IPREAD", socket_ipread),

#ifdef CONFIG_MODEM_ST87M01_RX_AT_FULL_LOG
		CMD_HANDLER("", fullmatch),
#endif
	};

	while (true) {
		/* wait for incoming data */
		(void)k_sem_take(&eclib_data.mctx->rx_sem, K_FOREVER);

		eclib_read_rx(&rx_buf);

		while (rx_buf) {
			net_buf_skipcrlf(&rx_buf);
			if (!rx_buf) {
				break;
			}

			frag = NULL;
			len = net_buf_findcrlf(rx_buf, &frag, &offset);
			if (!frag) {
				break;
			}

			/* look for matching data handlers */
			if (eclib_data.next_is_raw_cb != NULL) {
				/* locate next cr/lf */
				frag = NULL;
				len = net_buf_findcrlf(rx_buf, &frag, &offset);
				if (!frag) {
					break;
				}

				/* call handler */
				eclib_data.next_is_raw_cb(&rx_buf, len);

				frag = NULL;
				/* make sure buf still has data */
				if (!rx_buf) {
					break;
				}

				(void)net_buf_findcrlf(rx_buf, &frag, &offset);
			} else {
				for (int i = 0; i < ARRAY_SIZE(handlers); i++) {
					if (net_buf_ncmp(rx_buf, handlers[i].cmd,
							 handlers[i].cmd_len) == 0) {
						/* skip cmd_len */
						rx_buf = net_buf_skip(rx_buf, handlers[i].cmd_len);

						/* locate next cr/lf */
						frag = NULL;
						len = net_buf_findcrlf(rx_buf, &frag, &offset);
						if (!frag) {
							break;
						}

						/* call handler */
						if (handlers[i].func) {
							handlers[i].func(&rx_buf, len);
						}

						frag = NULL;
						/* make sure buf still has data */
						if (!rx_buf) {
							break;
						}

						/*
						 * We've handled the current line
						 * and need to exit the "search for
						 * handler loop".  Let's skip any
						 * "extra" data and look for the next
						 * CR/LF, leaving us ready for the
						 * next handler search.  Ignore the
						 * length returned.
						 */
						(void)net_buf_findcrlf(rx_buf, &frag, &offset);
						break;
					}
				}
			}

			if (frag && rx_buf) {
				/* clear out processed line (buffers) */
				while (frag && rx_buf != frag) {
					rx_buf = net_buf_frag_del(NULL, rx_buf);
				}

				net_buf_pull(rx_buf, offset);
			}
		}

		k_yield();
	}
}

static void ring_ping_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_DBG("RING CB");
	// TODO: implement RING call
}

/* AT CMD callback handler --------------------------------------------------------*/
static void on_cmd_atok(struct net_buf **buf, uint16_t len)
{
	k_sem_give(&eclib_data.response_sem);
}

static void on_cmd_aterror(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char error[len + 1];

	out_len = net_buf_linearize(error, len + 1, *buf, 2, len);
	error[out_len] = '\0';
	LOG_ERR("AT ERROR (%d) CODE: [%s]", len, error);
	eclib_data.last_response_error = 1;
	k_sem_give(&eclib_data.response_sem);
}

static void on_cmd_sim_status(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char simst[len];

	out_len = net_buf_linearize(simst, len, *buf, 2, len);

	if (simst[0] == '1') {
		eclib_data.sim_status = SIM_STATUS_SIM_VALID;
	} else if (simst[0] == '0') {
		eclib_data.sim_status = SIM_STATUS_SIM_INVALID;
	} else {
		eclib_data.sim_status = SIM_STATUS_UNKNOWN;
	}

	LOG_DBG("SIMST: %c", simst[0]);
}

static void on_cmd_connection_status(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char cscon[len];

	out_len = net_buf_linearize(cscon, len, *buf, 2, len);

	if (cscon[0] == '1') {
		eclib_data.connection_status = CONN_STATUS_CONNECTED;
	} else if (cscon[0] == '0') {
		eclib_data.connection_status = CONN_STATUS_IDLE;
	} else {
		eclib_data.connection_status = CONN_STATUS_UNKNOWN;
	}

	LOG_DBG("CSCON: %c", cscon[0]);
}

static void on_cmd_registration_status(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char cereg[len];

	out_len = net_buf_linearize(cereg, len, *buf, 2, len);

	/* 1 = registered, home network */
	/* 5 = registered, roaming      */
	if (cereg[0] == '1' || cereg[0] == '5') {
		eclib_data.registration_status = REGISTERED;
		eclib_send_sync_at(0, "AT#IPCFG?");
	} else {
		eclib_data.connection_status = NOT_REGISTERED;
	}

	LOG_DBG("CEREG: %c", cereg[0]);
}

static void on_cmd_ip_config_status(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char ipcfg[len];

	out_len = net_buf_linearize(ipcfg, len, *buf, 2, len);

	eclib_data.context_id = ipcfg[0] - CHAR_OFFSET;

	if (len == 7) {
		if (ipcfg[2] == '0') {
			eclib_data.ip_mode = IPV4_MODE;
		} else {
			eclib_data.ip_mode = IPV6_MODE;
		}
		if (ipcfg[4] == '0') {
			net_if_ipv4_addr_rm(eclib_data.iface, &eclib_data.ipv4_addr);
		} else if (ipcfg[4] == '2') {
			eclib_send_sync_at(0, "AT#IPCFG?");
		}
		LOG_DBG("IPCFG: %d, %d", eclib_data.context_id, eclib_data.ip_mode);
	} else {
		int ret = 0;

		if (eclib_data.ip_mode == IPV4_MODE) {
			net_if_ipv4_addr_rm(eclib_data.iface, &eclib_data.ipv4_addr);
			ret = net_addr_pton(AF_INET, ipcfg + 4, &eclib_data.ipv4_addr);

			net_if_ipv4_addr_add(eclib_data.iface, &eclib_data.ipv4_addr, NET_ADDR_DHCP,
					     0);
			// TODO: retrieve netmask + gateway using:
			// net_if_ipv4_set_netmask_by_addr(...), net_if_ipv4_set_gw(...)
		} else {
			// TODO: handle IPV6
		}

		LOG_DBG("IP: [%d] %s", len, ipcfg + 4);
	}
}

static void on_cmd_sleep(struct net_buf **buf, uint16_t len)
{
	LOG_DBG("SLEEP");
	eclib_data.sleep_wakeup_status = STATUS_SLEEP;
}
static void on_cmd_wakeup(struct net_buf **buf, uint16_t len)
{
	LOG_DBG("WAKEUP");
	eclib_data.sleep_wakeup_status = STATUS_WAKEUP;
	k_sem_give(&eclib_data.response_sem);
}

static void on_cmd_nvmread(struct net_buf **buf, uint16_t len)
{
	int tmp = 0;
	size_t out_len;
	char nvmrd[3];

	out_len = net_buf_linearize(nvmrd, sizeof(nvmrd), *buf, 2, len);
	nvmrd[out_len] = '\0';

	sscanf((const char *)nvmrd, "%x", (int *)&tmp);
	eclib_data.cold_init_version = (uint8_t)tmp;
}

static void on_cmd_socket_create(struct net_buf **buf, uint16_t len)
{
	if (len == 1) {
		for (uint8_t i = 0; i < MDM_MAX_SOCKETS; i++) {
			eclib_data.sockets[i].context = NULL;
			eclib_data.sockets[i].id = -1;
		}
	} else if (len == 3) {
		size_t out_len;
		char socket_id[3];

		out_len = net_buf_linearize(socket_id, sizeof(socket_id), *buf, 2, len);
		eclib_data.last_socket_id = socket_id[0] - CHAR_OFFSET;
		LOG_DBG("SOCKETCREATE: [%d]", eclib_data.last_socket_id);
	} else {
		// TODO: parse existing socket from ST87 to populate eclib_data.sockets array with
		// actual values
	}
}

static void on_cmd_socket_iprecv(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char socket_iprecv[3];

	out_len = net_buf_linearize(socket_iprecv, sizeof(socket_iprecv), *buf, 2, len);
	LOG_DBG("IPRECV: [%d]", socket_iprecv[2] - CHAR_OFFSET);
	for (uint8_t i = 0; i < MDM_MAX_SOCKETS; i++) {
		if (eclib_data.sockets[i].id == socket_iprecv[2] - CHAR_OFFSET &&
		    eclib_data.sockets[i].context) {
			eclib_read_socket(&eclib_data.sockets[i]);
			break;
		}
	}
}

static void on_cmd_socket_ipread_raw(struct net_buf **buf, uint16_t len)
{
	LOG_DBG("IPREAD RAW DATA");
	for (uint8_t i = 0; i < MDM_MAX_SOCKETS; i++) {
		if (eclib_data.sockets[i].id == eclib_data.last_socket_id &&
		    eclib_data.sockets[i].context) {
			struct net_pkt *pkt = net_pkt_rx_alloc_with_buffer(
				net_context_get_iface(eclib_data.sockets[i].context),
				eclib_data.last_ipread_size, eclib_data.sockets[i].family,
				eclib_data.sockets[i].ip_proto, BUF_ALLOC_TIMEOUT);
			if (!pkt) {
				LOG_ERR("Failed net_pkt_get_reserve_rx!");
				return;
			}
			net_pkt_set_context(pkt, eclib_data.sockets[i].context);

			for (size_t k = 0; k < eclib_data.last_ipread_size; k++) {
				char c = *(*buf)->data;

				if (net_pkt_write_u8(pkt, c)) {
					LOG_ERR("Unable to add data! Aborting!");
					net_pkt_unref(pkt);
					pkt = NULL;
					return;
				}

				/* pull data from buf and advance to the next frag if needed
				 */
				net_buf_pull_u8(*buf);
				if (!(*buf)->len) {
					*buf = net_buf_frag_del(NULL, *buf);
				}
			}

			/*net_pkt_set_overwrite(pkt, true);*/
			net_pkt_cursor_init(pkt);

			if (eclib_data.sockets[i].recv_cb) {
				eclib_data.sockets[i].recv_cb(eclib_data.sockets[i].context, pkt,
							      NULL, NULL, 0,
							      eclib_data.sockets[i].recv_user_data);
			} else {
				LOG_DBG("No callback ref for socket");
				net_pkt_unref(pkt);
			}
		}
	}

	eclib_data.next_is_raw_cb = NULL;
	eclib_data.last_socket_id = -1;
}
static void on_cmd_socket_ipread(struct net_buf **buf, uint16_t len)
{
	struct net_buf *frag = NULL;
	uint16_t offset;

	size_t out_len;
	char socket_iprecv[5];

	out_len = net_buf_linearize(socket_iprecv, sizeof(socket_iprecv), *buf, 2, len);
	for (uint8_t i = 0; i < MDM_MAX_SOCKETS; i++) {
		if (eclib_data.sockets[i].id == socket_iprecv[2] - CHAR_OFFSET &&
		    eclib_data.sockets[i].context) {
			eclib_data.last_ipread_size = socket_iprecv[4] - CHAR_OFFSET;
			eclib_data.last_socket_id = socket_iprecv[2] - CHAR_OFFSET;
			LOG_DBG("IPREAD: [%d] len = %d", eclib_data.last_socket_id,
				eclib_data.last_ipread_size);

			eclib_data.next_is_raw_cb = on_cmd_socket_ipread_raw;

			break;
		}
	}
}

#ifdef CONFIG_MODEM_ST87M01_RX_AT_FULL_LOG
static void on_cmd_fullmatch(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char str[len + 1];

	out_len = net_buf_linearize(str, len + 1, *buf, 0, len);
	str[out_len] = '\0';
	LOG_DBG("RAW: [%s]", str);
}
#endif

/* NET_BUF HELPERS --------------------------------------------------------*/
static uint8_t is_crlf(uint8_t c)
{
	if (c == '\n' || c == '\r') {
		return 1;
	} else {
		return 0;
	}
}

static void net_buf_skipcrlf(struct net_buf **buf)
{
	/* chop off any /n or /r */
	while (*buf && is_crlf(*(*buf)->data)) {
		net_buf_pull_u8(*buf);
		if (!(*buf)->len) {
			*buf = net_buf_frag_del(NULL, *buf);
		}
	}
}

static uint16_t net_buf_findcrlf(struct net_buf *buf, struct net_buf **frag, uint16_t *offset)
{
	uint16_t len = 0U, pos = 0U;

	while (buf && !is_crlf(*(buf->data + pos))) {
		if (pos + 1 >= buf->len) {
			len += buf->len;
			buf = buf->frags;
			pos = 0U;
		} else {
			pos++;
		}
	}

	if (buf && is_crlf(*(buf->data + pos))) {
		len += pos;
		*offset = pos;
		*frag = buf;
		return len;
	}

	return 0;
}

static int net_buf_ncmp(struct net_buf *buf, const uint8_t *s2, size_t n)
{
	struct net_buf *frag = buf;
	uint16_t offset = 0U;

	while ((n > 0) && (*(frag->data + offset) == *s2) && (*s2 != '\0')) {
		if (offset == frag->len) {
			if (!frag->frags) {
				break;
			}
			frag = frag->frags;
			offset = 0U;
		} else {
			offset++;
		}

		s2++;
		n--;
	}

	return (n == 0) ? 0 : (*(frag->data + offset) - *s2);
}

char *net_sprint_ip_addr(const struct sockaddr *addr)
{
	static char buf[NET_IPV6_ADDR_LEN];

#if defined(CONFIG_NET_IPV6)
	if (addr->sa_family == AF_INET6) {
		return net_addr_ntop(AF_INET6, &net_sin6(addr)->sin6_addr, buf, sizeof(buf));
	} else
#endif
#if defined(CONFIG_NET_IPV4)
		if (addr->sa_family == AF_INET) {
		return net_addr_ntop(AF_INET, &net_sin(addr)->sin_addr, buf, sizeof(buf));
	} else
#endif
	{
		LOG_ERR("Unknown IP address family:%d", addr->sa_family);
		return NULL;
	}
}
