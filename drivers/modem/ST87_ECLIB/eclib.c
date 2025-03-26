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
	struct mdm_receiver_context *mctx;
	struct gpio_dt_spec *reset_gpio;
	struct gpio_dt_spec *ring_gpio;

	struct k_sem response_sem;
	uint8_t last_response_error;

	uint8_t cold_init_version;
	eclib_sim_status_t sim_status;
	eclib_connection_status_t connection_status;
	eclib_registration_status_t registration_status;
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
static void on_cmd_nvmread(struct net_buf **buf, uint16_t len);
#ifdef CONFIG_MODEM_ST87M01_RX_AT_FULL_LOG
static void on_cmd_fullmatch(struct net_buf **buf, uint16_t len);
#endif

static uint8_t is_crlf(uint8_t c);
static void net_buf_skipcrlf(struct net_buf **buf);
static uint16_t net_buf_findcrlf(struct net_buf *buf, struct net_buf **frag, uint16_t *offset);
static int net_buf_ncmp(struct net_buf *buf, const uint8_t *s2, size_t n);

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

	/* Start RX thread */
	k_thread_create(&eclib_rx_thread, eclib_rx_stack, K_KERNEL_STACK_SIZEOF(eclib_rx_stack),
			eclib_rx, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	/* Reset of the whole system as init */
	status += (uint8_t)eclib_reset();

	/* Trig cold parameter initialization sequence */
	status += (uint8_t)eclib_cold_param_init();

	if (status > 0) {
		result = RESULT_KO;
	}

	return (result);
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

unsigned int eclib_send_sync_at(unsigned int timeout, const char *format, ...)
{
	int ret;

	/* Parsing command */
	va_list args;
	va_start(args, format);
	uint32_t length_sent = vsprintf((char *)(mdm_tx_buf), (const char *)format, args);
	va_end(args);

	eclib_data.last_response_error = 0;
	LOG_DBG("OUT: [%s]", mdm_tx_buf);
	mdm_receiver_send(eclib_data.mctx, mdm_tx_buf, length_sent);
	mdm_receiver_send(eclib_data.mctx, "\r\n", 2);

	if (timeout == 0) {
		return 0;
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

	// COLDINIT_STATE_INIT
	if (eclib_send_sync_at(MDM_AT_CMD_TIMEOUT, ST87EC_NVMRD_CMD) == 0) {
		LOG_ERR("ST87EC_NVMRD_CMD timeout!");
		return RESULT_KO;
	}

	if (eclib_data.last_response_error > 0) {
		return RESULT_KO;
	}

	// COLDINIT_STATE_CHECK_VERSION
	if (eclib_data.cold_init_version == ST87EC_COLD_CONFIG_VERSION) {
		/* Cold condig already up-to-date */
		LOG_INF("ST87M01 NVM up-to-date");
		return RESULT_OK;
	}
	LOG_INF("ST87M01 NVM config version mismatch: %d, rewriting config...",
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

	/* Configure a GPI for ST87 ring pin, when ring pin rises call ST87EC_Lib_Hal_RingPinIsr */
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
	uint8_t uart_rx_buf[MDM_RECV_BUF_SIZE];

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
		/* CONFIG RESPONSES */
		CMD_HANDLER("#NVMRD: ", nvmread),

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
			for (int i = 0; i < ARRAY_SIZE(handlers); i++) {
				if (net_buf_ncmp(rx_buf, handlers[i].cmd, handlers[i].cmd_len) ==
				    0) {
					/* found a matching handler */
					LOG_DBG("MATCH %s (len:%u)", handlers[i].cmd, len);

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

	LOG_INF("SIMST: %c", simst[0]);
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

	LOG_INF("CSCON: %c", cscon[0]);
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
	} else {
		eclib_data.connection_status = NOT_REGISTERED;
	}

	LOG_INF("CEREG: %c", cereg[0]);
}

static void on_cmd_nvmread(struct net_buf **buf, uint16_t len)
{
	int tmp = 0;
	size_t out_len;
	char nvmrd[3];

	out_len = net_buf_linearize(nvmrd, 3, *buf, 0, len);
	nvmrd[out_len] = '\0';

	sscanf((const char *)nvmrd, "%x", (int *)&tmp);
	eclib_data.cold_init_version = (uint8_t)tmp;
}

#ifdef CONFIG_MODEM_ST87M01_RX_AT_FULL_LOG
static void on_cmd_fullmatch(struct net_buf **buf, uint16_t len)
{
	size_t out_len;
	char str[len + 1];

	out_len = net_buf_linearize(str, len + 1, *buf, 0, len);
	str[out_len] = '\0';
	LOG_ERR("RAW: [%s]", str);
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
