/**
  ******************************************************************************
  * @file    st87ec_sequence_sys.c
  * @author  APMS Application Team
  * @brief   EC System sequences functions
  *
  @verbatim
  @endverbatim
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics International N.V.
  * All rights reserved.
  *
  ******************************************************************************
  */

#include <stdlib.h>
#include <stdio.h>
#include "../inc/st87ec_lib.h"
#include "../inc/st87ec_sequence_sys.h"
#include "../inc/st87ec_wrapper.h"
#include "../inc/st87ec_engine.h"
#include "../../Cust/inc/st87ec_cold_config.h"
#include "../../Cust/inc/st87ec_lib_hal.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ST87EC_Sequence_Sys);

/* Exported functions --------------------------------------------------------*/

/* Constants -----------------------------------------------------------------*/
#define ST87EC_COLD_INIT_TIMEOUT       (10000)
#define ST87EC_COLD_VERSION_INDEX      (8)
#define ST87EC_COLD_VERSION_NVM_PAGE   5
#define ST87EC_COLD_VERSION_NVM_OFFSET 12

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

#define ST87EC_START_BOOT_RSP_TIMER                                                                \
	if (len_sent > 0) {                                                                        \
		/* Start waiting for AT response timer (and launching UART Boot Handshake to come  \
		 * if no Rsp...) */                                                                \
		EcLibVars.BootIf.AtRspWaitingTimerId = ST87EC_Wrapper_StartTimer(300);             \
		if (EcLibVars.BootIf.AtRspWaitingTimerId == ST87EC_TIMER_ERROR) {                  \
			EcLibVars.BootIf.SendAtToBootStep = LOADER_AT_STEP_NONE;                   \
			len_sent = 0; /* Raise error */                                            \
		} else {                                                                           \
			EcLibVars.BootIf.SendAtToBootStep = LOADER_AT_WAIT_FOR_AT_RSP;             \
		}                                                                                  \
	}

#define ST87EC_SEND_BOOT_CMD(...)                                                                  \
	len_sent = ST87EC_Wrapper_SendCmdToBoot(__VA_ARGS__);                                      \
	EcLibVars.BootIf.HandshakeOkTrialsCnt = 0;                                                 \
	ST87EC_START_BOOT_RSP_TIMER

#define ST87EC_RESEND_BOOT_CMD                                                                     \
	len_sent = ST87EC_Wrapper_SendByte(NULL, 0);                                               \
	ST87EC_START_BOOT_RSP_TIMER

/* Private types -------------------------------------------------------------*/
typedef struct {
	uint8_t NbCmd;        /**< Number of AT commands to send for upload of the given binary */
	char *CmdStrArray[3]; /**< Array of AT command strings to be sent for upload of the given
				 binary */
} ST87EC_SequenceSys_SpecificAtCmds_t;

/* Private variables ---------------------------------------------------------*/

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

/* Loader sequence core specific command list */
const ST87EC_SequenceSys_SpecificAtCmds_t ST87EC_SequenceSys_SpecificAtCmdTable[] = {
	{1, {"AT", "", ""}},                                     /* ST87_SEC_BINARY   */
	{3, {"AT", "AT#LDCORE=0,1", "AT#NVMSV=2"}},              /* ST87_ALP_BINARY   */
	{2, {"AT#NVMWR=1,332,4,0x330000001", "AT#NVMSV=2", ""}}, /* ST87_MODEM_BINARY */
	{1, {"AT", "", ""}},                                     /* ST87_GNSS_BINARY  */
	{3, {"AT", "AT#LDCORE=0,1", "AT#NVMSV=2"}}               /* ST87_CAP_BINARY   */
};

/* Internal functions --------------------------------------------------------*/

/**
 * @brief Sequence Cold parameter initialization
 *
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_SequenceSys_ColdParamInit(void)
{
	ST87EC_Lib_Result_t result = RESULT_OK;
	uint32_t tmp;

	/* Check timer expiration */
	if ((EcLibVars.SequenceSys.ColdInit.FsmState != COLDINIT_STATE_INIT) &&
	    (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceSys.ColdInit.TimerId) ==
	     TIMER_STATUS_ELAPSED)) {
		EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_ERROR;
	}

	switch (EcLibVars.SequenceSys.ColdInit.FsmState) {
	case COLDINIT_STATE_INIT:
		/* Start the timeout */
		EcLibVars.SequenceSys.ColdInit.TimerId =
			ST87EC_Wrapper_StartTimer(ST87EC_COLD_INIT_TIMEOUT);

		if (EcLibVars.SequenceSys.ColdInit.TimerId == ST87EC_TIMER_ERROR) {
			/* Error occurred */
			EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_CHECK_VERSION;
		} else {
			/* Read the NVM version in CUST NVMto check if cold init is necessary */
			ST87EC_Wrapper_SendCmd(ST87EC_NVMRD_CMD);
			EcLibVars.RspInfo[NVMREAD_INDEX].Validity = RSP_AWAITED;
			EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_CHECK_VERSION;
		}
		break;

	case COLDINIT_STATE_CHECK_VERSION:
		/* Version check ==  ST87EC_COLD_CONFIG_VERSION */
		if (EcLibVars.RspInfo[NVMREAD_INDEX].Validity == RSP_RECEIVED) {
			EcLibVars.RspInfo[NVMREAD_INDEX].Validity = RSP_NONE;
			sscanf((const char *)&EcLibVars.RspData[ST87EC_COLD_VERSION_INDEX], "%x",
			       (int *)&tmp);
			EcLibVars.ColdInitVersion = (uint8_t)tmp;
			if (EcLibVars.ColdInitVersion != ST87EC_COLD_CONFIG_VERSION) {
				EcLibVars.SequenceSys.ColdInit.FsmState =
					COLDINIT_STATE_SEND_COMMANDS;
			} else {
				EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_END;
			}
		}
		break;

	case COLDINIT_STATE_SEND_COMMANDS:
		if (EcLibVars.SequenceSys.ColdInit.SequenceIndex < ST87EC_COLD_INIT_CMD_SIZE) {
			/* Wait to purge previous "OK" response */
			ST87EC_Wrapper_Delay(100);
			EcLibVars.RspReceived = SPECIFIC_RSP_NONE;

			EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_SEND_COMMANDS;
			ST87EC_Wrapper_SendCmd(
				ST87EC_COLD_INIT_COMMANDS[EcLibVars.SequenceSys.ColdInit
								  .SequenceIndex]);
			EcLibVars.SequenceSys.ColdInit.SequenceIndex++;
			EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_WAIT_OK;
		} else /* Last init command sent */
		{
			EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_END;
		}
		break;

	case COLDINIT_STATE_WAIT_OK:
		/* Wait for OK or error */
		if (EcLibVars.RspReceived != SPECIFIC_RSP_NONE) {
			if (EcLibVars.RspReceived == SPECIFIC_RSP_OK) {
				/* Send next init command */
				EcLibVars.SequenceSys.ColdInit.FsmState =
					COLDINIT_STATE_SEND_COMMANDS;
				EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
			}
			if (EcLibVars.RspReceived == SPECIFIC_RSP_CME) {
				/* Error received */
				EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_ERROR;
				EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
			}
		}
		break;

	default:
	case COLDINIT_STATE_ERROR:
		result = RESULT_KO;

	case COLDINIT_STATE_END:
		/* Stop the timer */
		ST87EC_Wrapper_StopTimer(EcLibVars.SequenceSys.ColdInit.TimerId);

		/* Stop the sequence */
		EcLibVars.OnGoingSequence = SEQUENCE_NONE;
		EcLibVars.SequenceSys.ColdInit.FsmState = COLDINIT_STATE_INIT;
		EcLibVars.SequenceSys.ColdInit.SequenceIndex = 0;
		break;
	}
	return (result);
}

/**
 * @brief Sequence for the Get Time request
 *
 * @retval Function execution status
 */
ST87EC_Lib_Result_t ST87EC_SequenceSys_GetTime(void)
{
	ST87EC_Lib_Result_t result = RESULT_OK;

	/* Check timer expiration */
	if ((EcLibVars.SequenceSys.GetTime.FsmState != GETTIME_STATE_INIT) &&
	    (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceSys.GetTime.TimerId) ==
	     TIMER_STATUS_ELAPSED)) {
		EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_ERROR;
	}

	switch (EcLibVars.SequenceSys.GetTime.FsmState) {
	case GETTIME_STATE_INIT:
		/* Start the timeout */
		EcLibVars.SequenceSys.GetTime.TimerId =
			ST87EC_Wrapper_StartTimer(EcLibVars.SequenceSys.GetTime.Params.TimeoutMs);

		if (EcLibVars.SequenceSys.GetTime.TimerId == ST87EC_TIMER_ERROR) {
			/* Error occurred */
			EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_ERROR;
		} else {
			EcLibVars.RspInfo[CCLK_INDEX].Validity =
				RSP_AWAITED; /* Indicate response from cmd to be sent is awaited */
			EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
			ST87EC_Wrapper_SendCmd("AT+CCLK?");

			EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_WAIT_FOR_RSP;
		}
		break;

	case GETTIME_STATE_WAIT_FOR_RSP:
		/* Check if awaited response is received */
		if (EcLibVars.RspReceived == SPECIFIC_RSP_CME) {
			EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_ERROR;
		} else if (EcLibVars.RspInfo[CCLK_INDEX].Validity == RSP_RECEIVED) {
			/* Stop the timer */
			result = ST87EC_Wrapper_StopTimer(EcLibVars.SequenceSys.GetTime.TimerId);

			/* Awaited response is received, call the customer callback function with
			 * received data in parameter (+CCLK_TAG_SIZE to avoid the "+CCLK: " tag) */
			EcLibVars.SequenceSys.GetTime.Params.pGetTimeCallbackFunc(
				&EcLibVars.RspData[CCLK_TAG_SIZE]);

			EcLibVars.RspInfo[CCLK_INDEX].Validity = RSP_NONE;

			/* Update the sequence status in the EC lib status array and FSM */
			EcLibVars.OnGoingSequence = SEQUENCE_NONE;
			EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_INIT;
		} else {
		}

		EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
		break;

	default:
	case GETTIME_STATE_ERROR:
		/* Stop the timer */
		ST87EC_Wrapper_StopTimer(EcLibVars.SequenceSys.GetTime.TimerId);
		/* Call the customer callback with a NULL pointer */
		EcLibVars.SequenceSys.GetTime.Params.pGetTimeCallbackFunc(NULL);

		/* Stop the sequence */
		EcLibVars.OnGoingSequence = SEQUENCE_NONE;
		EcLibVars.SequenceSys.GetTime.FsmState = GETTIME_STATE_INIT;

		result = RESULT_KO;
		break;
	}
	return (result);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
