/* Copyright (c) 2025 Nicolas THIERRY <nicolas.thierry@estaca.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ECLIB_AT_COMMAND_H
#define ECLIB_AT_COMMAND_H

#include "cold_config.h"

#define STR(VALUE) #VALUE

#define ST87EC_FORMAT_TEMPLIMIT_CMD(lOW, hIGH, eNABLE)                                             \
	"AT#TEMPLIMIT=-40,85," STR(lOW) "," STR(hIGH) ",0," STR(eNABLE)
#define ST87EC_TEMPLIMIT_CMD                                                                       \
	ST87EC_FORMAT_TEMPLIMIT_CMD(TEMP_LOW_SHUTDOWN, TEMP_HIGH_SHUTDONW, TEMP_SHUTDOWN)

#define ST87EC_FORMAT_VBATLIMIT_CMD(lOW, hIGH, eNABLE)                                             \
	"AT#VBATLIMIT=2200,3000," STR(lOW) "," STR(hIGH) "," STR(eNABLE) ",0,0"
#define ST87EC_VBATLIMIT_CMD                                                                       \
	ST87EC_FORMAT_VBATLIMIT_CMD(VBAT_LOW_SHUTDOWN, VBAT_HIGH_SHUTDOWN, VBAT_SHUTDOWN)

#define ST87EC_FORMAT_BANDSEL_CMD(bANDS) "AT#BANDSEL=" bANDS
#define ST87EC_BANDSEL_CMD               ST87EC_FORMAT_BANDSEL_CMD(BANDLIST)

#define ST87EC_FORMAT_SLEEPMODE_CMD(hOLD, aWAKE) "AT#SLEEPMODE=1," STR(hOLD) "," STR(aWAKE)
#define ST87EC_SLEEPMODE_CMD                     ST87EC_FORMAT_SLEEPMODE_CMD(HOLD_TIME, AWAKE_TIME)

#define ST87EC_FORMAT_BANDCFG_CMD(bAND, nMO1, nMO2, nMO3)                                          \
	"AT#BANDCFG=" bAND, "AT#BANDCFG=" nMO1, "AT#BANDCFG=" nMO2, "AT#BANDCFG=" nMO3
#define ST87EC_BANDCFG_CMD                                                                         \
	ST87EC_FORMAT_BANDCFG_CMD(BANDCFG, BANDCFG_NMO1, BANDCFG_NMO2, BANDCFG_NMO3)

#define ST87EC_FORMAT_CEDRXS_CMD(vAL) "AT+CEDRXS=1,5," STR(vAL)
#define ST87EC_CEDRX_CMD              ST87EC_FORMAT_CEDRXS_CMD(EDRX_VALUE)

#define ST87EC_FORMAT_PTW_CMD(vAL) "AT#PTW=" STR(vAL)
#define ST87EC_PTW_CMD             ST87EC_FORMAT_PTW_CMD(PTW_VALUE)

#define ST87EC_FORMAT_CPSMS_CMD(eNABLE, tAU, aCTIVE)                                               \
	"AT+CPSMS=" STR(eNABLE) ",,,\"" STR(tAU) "\",\"" STR(aCTIVE) "\""
#define ST87EC_CPSMS_CMD ST87EC_FORMAT_CPSMS_CMD(PSM_ENABLE, PERIODIC_TAU, ACTIVE_TIME)

#define ST87EC_FORMAT_RINGPIN_CMD(eNABLE, gPIO, pOL, dELAY)                                        \
	"AT#RINGPIN=" STR(eNABLE) "," STR(gPIO) "," STR(pOL) "," STR(dELAY)
#define ST87EC_RINGPIN_CMD                                                                         \
	ST87EC_FORMAT_RINGPIN_CMD(RING_PIN_ENABLE, RING_PIN_GPIO, RING_PIN_POLARITY, RING_PIN_DELAY)

#define ST87EC_FORMAT_NVMRD_CMD(pAGE, oFF) "AT#NVMRD=" STR(pAGE) "," STR(oFF) ",1"
#define ST87EC_NVMRD_CMD                                                                           \
	ST87EC_FORMAT_NVMRD_CMD(ST87EC_COLD_VERSION_NVM_PAGE, ST87EC_COLD_VERSION_NVM_OFFSET)

#define ST87EC_FORMAT_NVMWR_CMD(pAGE, oFF, vER) "AT#NVMWR=" STR(pAGE) "," STR(oFF) ",1," STR(vER)
#define ST87EC_NVMWR_CMD                                                                           \
	ST87EC_FORMAT_NVMWR_CMD(ST87EC_COLD_VERSION_NVM_PAGE, ST87EC_COLD_VERSION_NVM_OFFSET,      \
				ST87EC_COLD_CONFIG_VERSION)

#define ST87EC_FORMAT_IPPARAM_CMD(nBpACKETsENTvAL, dOMAINnAME)                                     \
	"AT#IPPARAMS=1,0,65535,60," STR(nBpACKETsENTvAL) "," STR(dOMAINnAME)
#define ST87EC_IPPARAM_CMD ST87EC_FORMAT_IPPARAM_CMD(NB_PACKET_SENT_ENABLE, DOMAIN_NAME)

/* Cold init sequence command list */
const char *ST87EC_COLD_INIT_COMMANDS[] = {
	/* Config for EC lib */
	"AT+CMEE=1",        /* Report Mobile Termination Error*/
	"AT+CEREG=5",       /* Registration status verbosity */
	"AT+CSCON=1",       /* Connection status verbosity */
	"AT#SLEEPIND=0x1F", /* Sleep indication verbosity */
	"AT#WDGMODE=0",     /* Watchdog setting */

	/* HW config */
	ST87EC_TEMPLIMIT_CMD, /* Limit of temperature setting */
	ST87EC_VBATLIMIT_CMD, /* Vbat limit setting */

	/* NBIOT */
	"AT+CFUN=0",                  /* Turn off modem before setting */
	ST87EC_BANDSEL_CMD,           /* Band selection list */
	ST87EC_BANDCFG_CMD,           /* Band configuration */
	"AT#SCAN=1,-104,1,360,1,360", /* Extended coverage enabled */
	ST87EC_CEDRX_CMD,             /* EDRX configuration */
	ST87EC_PTW_CMD,               /* Paging time window configuration */
	ST87EC_CPSMS_CMD,             /* Power saving setting */

	/* ST87 configuration */
	ST87EC_SLEEPMODE_CMD,  /* Sleep mode configuration */
	ST87EC_RINGPIN_CMD,    /* Ring pin configuration */
	"AT#WAKEUPEVENT=15,3", /* Wake up event configuration on UART */

	/* Connection */
	ST87EC_IPPARAM_CMD, /* IP configuration */

	/* Update NVM */
	ST87EC_NVMWR_CMD, /* Write Cold ini version in NVM */
	"AT#RESET=1"      /* Save changes in NVM */
};

#define ST87EC_COLD_INIT_CMD_SIZE (sizeof(ST87EC_COLD_INIT_COMMANDS) / 4)

#endif //#ECLIB_AT_COMMAND_H
