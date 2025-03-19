/**
  ******************************************************************************
  * @file    st87ec_sequence_gnss.c
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

#include <string.h>
#include <stdio.h>
#include "../inc/st87ec_sequence_gnss.h"
#include "../inc/st87ec_wrapper.h"

/* Exported functions --------------------------------------------------------*/

/* Constants -----------------------------------------------------------------*/
#define ST87EC_FORMAT_GNSS_INIT_CMD(CONST)  "AT#GNSSINIT=1," STR(CONST)
#define ST87EC_GNSS_INIT_CMD  ST87EC_FORMAT_GNSS_INIT_CMD(GNSS_CONSTELLATION_ID)

#if GNSS_FORMAT_TYPE == 0 /* ST format */
#define ST87EC_FORMAT_GNSS_FIX_CMD(POS,ACC,SV,ORIENT)  "AT#GNSSFIX=1,1,0," STR(POS) STR(ACC) STR(SV) STR(ORIENT)
#define ST87EC_GNSS_FIX_CMD  ST87EC_FORMAT_GNSS_FIX_CMD(GNSS_FORMAT_ST_POSITION,GNSS_FORMAT_ST_ACCURACY,GNSS_FORMAT_ST_SATELLITES,GNSS_FORMAT_ST_ORIENTATION)
#elif GNSS_FORMAT_TYPE == 1 /* NMEA format */
#define ST87EC_FORMAT_GNSS_FIX_CMD(GGA,GSA,GSV,GLL,RMC,VTG)  "AT#GNSSFIX=1,1,1," STR(GGA) STR(GSA) STR(GSV) STR(GLL) STR(RMC) STR(VTG)
#define ST87EC_GNSS_FIX_CMD  ST87EC_FORMAT_GNSS_FIX_CMD(GNSS_NMEA_GPGGA,GNSS_NMEA_GPGSA,GNSS_NMEA_GPGSV,GNSS_NMEA_GPGLL,GNSS_NMEA_GPRMC,GNSS_NMEA_GPVTG)
#endif /* GNSS_FORMAT_TYPE */

#define ST87EC_GNSS_DEINIT_CMD  "AT#GNSSDEINIT"
#define ST87EC_GNSS_READY            ('2')
#define ST87EC_GNSS_VALID_FIX        ('0')

/* Global variables ----------------------------------------------------------*/


/**
* @brief Sequence for the GNSS Get Fix request
*
* @retval Function execution status
*/
ST87EC_Lib_Result_t ST87EC_SequenceGNSS_GetFix(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  /* Check timer expiration */
  if ((EcLibVars.SequenceGnss.GetFix.FsmState != GETFIX_STATE_INIT)
      && (ST87EC_Wrapper_GetTimerStatus(EcLibVars.SequenceGnss.GetFix.TimerId) == TIMER_STATUS_ELAPSED))
  {
    EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_ERROR;
  }

  switch (EcLibVars.SequenceGnss.GetFix.FsmState)
  {
    case GETFIX_STATE_INIT:
      /* Start the timeout */
      EcLibVars.SequenceGnss.GetFix.TimerId = ST87EC_Wrapper_StartTimer(EcLibVars.SequenceGnss.GetFix.Params.TimeoutMs);
      if (EcLibVars.SequenceGnss.GetFix.TimerId == ST87EC_TIMER_ERROR)
      {
        /* Error occurred */
        EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_ERROR;
      }
      else
      {
        /* Indicate response from cmd to be sent is awaited */
        EcLibVars.RspInfo[GNSS_INIT_INDEX].Validity = RSP_AWAITED;
        /* Send the GNSS Init AT cmd */
        ST87EC_Wrapper_SendCmd(ST87EC_GNSS_INIT_CMD);

        EcLibVars.SequenceGnss.GetFix.Params.InitDone = 1;
        EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_INIT_WAIT_FOR_RSP;
      }
      break;

    case GETFIX_STATE_INIT_WAIT_FOR_RSP:
      /* Check if awaited response is received */
      if (EcLibVars.RspInfo[GNSS_INIT_INDEX].Validity == RSP_RECEIVED)
      {
        /* Check if the module is ready for fix*/
        if (EcLibVars.RspData[GNSS_INIT_TAG_SIZE+1] == ST87EC_GNSS_READY)
        {
          EcLibVars.RspInfo[GNSS_INIT_INDEX].Validity = RSP_NONE;
          EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_FIX_SEND_AT_CMD;
        }
      }
      break;

    case GETFIX_STATE_FIX_SEND_AT_CMD:
      /* Indicate response from cmd to be sent is awaited */
#if GNSS_FORMAT_TYPE == 0 /* ST format */
      EcLibVars.RspInfo[GNSS_FIX_INDEX].Validity = RSP_AWAITED;
#elif GNSS_FORMAT_TYPE == 1 /* NMEA format */
      for (uint16_t index=GNSS_GGA_INDEX; index<=GNSS_VTG_INDEX; index++)
      {
        EcLibVars.RspInfo[index].Validity = RSP_AWAITED;
      }
#endif
      /* Send the GNSS fix AT cmd */
      ST87EC_Wrapper_SendCmd(ST87EC_GNSS_FIX_CMD);
      EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_FIX_WAIT_FOR_RSP;
      break;

#if GNSS_FORMAT_TYPE == 0 /* ST format */
    case GETFIX_STATE_FIX_WAIT_FOR_RSP:
      /* Check if awaited response is received */
      if (EcLibVars.RspInfo[GNSS_FIX_INDEX].Validity == RSP_RECEIVED)
      {
        /* Awaited response is received, call the customer callback function with received data in parameter ( +tag_length to remove the "#GNSSFIX: " ) */
        EcLibVars.SequenceGnss.GetFix.Params.pGetFixCallbackFunc(&EcLibVars.RspData[GNSS_FIX_TAG_SIZE]);
        EcLibVars.RspInfo[GNSS_FIX_INDEX].Validity = RSP_AWAITED;

        if (EcLibVars.RspData[GNSS_FIX_TAG_SIZE] == ST87EC_GNSS_VALID_FIX)
        {
          EcLibVars.SequenceGnss.GetFix.Params.CountPos++;
          /* Stop reception if counter reaches NbPosition value */
          if (EcLibVars.SequenceGnss.GetFix.Params.CountPos >=  EcLibVars.SequenceGnss.GetFix.Params.NbPosition)
          {
            /* Stop the timer */
            result = ST87EC_Wrapper_StopTimer(EcLibVars.SequenceGnss.GetFix.TimerId);

            EcLibVars.RspInfo[GNSS_FIX_INDEX].Validity = RSP_NONE;
            EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_DEINIT_SEND_AT_CMD;
          }
        }
      }
      break;
#elif GNSS_FORMAT_TYPE == 1 /* NMEA format */
    case GETFIX_STATE_FIX_WAIT_FOR_RSP:
      /* Check if awaited response is received */
      for (uint16_t index=GNSS_GGA_INDEX; index<=GNSS_VTG_INDEX; index++)
      {
        if (EcLibVars.RspInfo[index].Validity == RSP_RECEIVED)
        {
          /* Awaited response is received, call the customer callback function with received data in parameter */
          EcLibVars.SequenceGnss.GetFix.Params.pGetFixCallbackFunc(EcLibVars.RspData);
          EcLibVars.RspInfo[index].Validity = RSP_AWAITED;

          if (index == GNSS_GLL_INDEX)
          {
            EcLibVars.SequenceGnss.GetFix.Params.CountPos++;
            /* Stop reception if counter reaches NbPosition value */
            if (EcLibVars.SequenceGnss.GetFix.Params.CountPos >=  EcLibVars.SequenceGnss.GetFix.Params.NbPosition)
            {
              /* Stop the timer */
              result = ST87EC_Wrapper_StopTimer(EcLibVars.SequenceGnss.GetFix.TimerId);
              EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_DEINIT_SEND_AT_CMD;
              EcLibVars.RspInfo[index].Validity = RSP_RECEIVED;
            }
          }
        }
      }
      break;
#endif /* GNSS_FORMAT_TYPE  */
    case GETFIX_STATE_DEINIT_SEND_AT_CMD:

      /* Send cmd AT GNSS to disable GNSS */
      ST87EC_Wrapper_SendCmd("AT#GNSSDEINIT");

      /* Update the sequence status in the EC lib status array and FSM */
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_INIT;
      break;

    default:
    case GETFIX_STATE_ERROR:
      /* Stop the timer */
      ST87EC_Wrapper_StopTimer(EcLibVars.SequenceGnss.GetFix.TimerId);

      /* Call the customer callback with a NULL pointer */
      EcLibVars.SequenceGnss.GetFix.Params.pGetFixCallbackFunc(NULL);

      if(EcLibVars.SequenceGnss.GetFix.Params.InitDone == 1)
      {
        /* Send cmd AT GNSS to disable GNSS */
        ST87EC_Wrapper_SendCmd("AT#GNSSDEINIT");
#if GNSS_FORMAT_TYPE == 0 /* ST format */
        EcLibVars.RspInfo[GNSS_FIX_INDEX].Validity = RSP_NONE;
#elif GNSS_FORMAT_TYPE == 1 /* NMEA format */
        for (uint16_t index=GNSS_GGA_INDEX; index<=GNSS_VTG_INDEX; index++)
        {
          EcLibVars.RspInfo[index].Validity = RSP_NONE;
        }
#endif
      }

      /* Stop the sequence */
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      EcLibVars.SequenceGnss.GetFix.FsmState = GETFIX_STATE_INIT;

      result = RESULT_KO;
      break;

  }
  return (result);
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

