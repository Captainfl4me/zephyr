/**
  ******************************************************************************
  * @file    st87ec_sequence_wmbus.c
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
#include <string.h>
#include <stdio.h>

#include "../inc/st87ec_sequence_wmbus.h"

#include "../inc/st87ec_wrapper.h"
#include "../inc/st87ec_engine.h"

//
/* Exported functions --------------------------------------------------------*/

/* Constants -----------------------------------------------------------------*/
#define _RET_OK     (0)
#define _RET_ERR1   (1)
#define _RET_ERR2   (2)
#define _RET_ERR3   (3)

#define _CMD_INIT_TIMEOUT           (10000)
#define _CMD_SLEEP_TIMEOUT          (5000)
#define _CMD_DEF_TIMEOUT            (1000)
#define _CMD_WAKEUP_FORCED_TIMEOUT  (2000)

#define _CMD_SEND_TIMEOUT_ASYNCH  (1000)
#define _CMD_SEND_TIMEOUT_SYNCH   ((WMBUS_T_NOM + 10)*1000)
#define _CMD_WAKEUP_TIMEOUT       ((WMBUS_T_NOM + 10)*1000)

/**
 *  PRIVATE REGISTER
 *  @{
 */
#define _REG_DL_FW_VERSION                (0x1000)      /**< r  U32 0  0x0:0xFFFFFFFF */
#define _REG_DL_TX_PACKET_COUNT           (0x1002)      /**< rw U32 0  0x0:0xFFFFFFFF (w only works with 0: reset counter) */
#define _REG_DL_TX_REFUSED_PACKET_COUNT   (0x1003)      /**< rw U32 0  0x0:0xFFFFFFFF (w only works with 0: reset counter) */
#define _REG_DL_T_NOM                     (0x1004)      /**< rw U32 20 16:900 */

#define _REG_PHY_TX_POWER                 (0x2000)      /**< rw S08 0  -40:14 */
#define _REG_PHY_DEV_TYPE                 (0x2001)      /**< rw U08 2  0:2 -> only mode 0 (aka METER) is supported */
#define _REG_PHY_ENCODER_STATUS           (0x2002)      /**< Reserved */
#define _REG_PHY_FRAME_FORMAT             (0x2003)      /**< rw U08 0  0:1 */
#define _REG_PHY_FSK_DEVIATION_OFFSET     (0x2004)      /**< Reserved */
#define _REG_PHY_CENTER_FREQUENCY_OFFSET  (0x2005)      /**< Reserved */
#define _REG_PHY_HEADER_LENGTH            (0x2006)      /**< rw U16 0x0013 0x0000:0xFFFF */
#define _REG_PHY_POSTAMBLE_LENGTH         (0x2007)      /**< rw U08 U08 0x01 0x01:0x04 */
#define _REG_PHY_TX_CONTINUOUS_TEST_MODE  (0x2008)      /**< Reserved */
#define _REG_PHY_MODE                     (0x200C)      /**< rw U08 0x03 0:9 */
/** @} */

typedef enum
{
  _CFG_PHY_MODE            = 0,
  _CFG_PHY_TX_POWER         ,

  _CFG_PHY_DEV_TYPE         ,
  _CFG_PHY_FRAME_FORMAT     ,
  _CFG_PHY_POSTAMPLE_LENGTH ,

  _CFG_DL_T_NOM,
  _CFG_PHY_HEADER_LENGTH,

  _CFG_ENTRIES
}_cfg_idx_t;

typedef  struct {
  uint32_t addr;
  enum
  {
    _U8,
    _S8,
    _U16,
  }type;
  union {
    uint8_t  u8;
    int8_t   s8;
    uint16_t u16;
  };
}_cfg_t;

typedef enum
{
  _ERR_INIT_VAL,
  _ERR_INIT_RSP,
  _ERR_INIT_TMO,
  _ERR_CFG_TYPE,
  _ERR_CFG_RSP,
  _ERR_CFG_TMO,
  _ERR_SEND_RSP,
  _ERR_SEND_TMO,
  _ERR_SEND_LEN,
  _ERR_SLEEP_RSP,
  _ERR_SLEEP_TMO,
  _ERR_WAKEUP_TMO,
  _ERR_DEINIT_RSP,
  _ERR_DEINIT_TMO,
  _ERR_TIMER_START,
}_error_t;

/* Private variables ----------------------------------------------------------*/
/* Private functions declaration ----------------------------------------------*/
static uint8_t _SendDataFrame(ST87EC_Lib_WmbusTransfer_t * pCtx);

/* Private variables ----------------------------------------------------------*/
static const _cfg_t  _CFG_TAB [_CFG_ENTRIES] = {
    [_CFG_PHY_MODE]            = {   .addr = _REG_PHY_MODE           , .type = _U8,  .u8 = (uint8_t) WMBUS_PHY_MODE           },
    [_CFG_PHY_DEV_TYPE]         = {   .addr = _REG_PHY_DEV_TYPE        , .type = _U8,  .u8 = (uint8_t) WMBUS_DEV_TYPE           },
    [_CFG_PHY_FRAME_FORMAT]     = {   .addr = _REG_PHY_FRAME_FORMAT    , .type = _U8,  .u8 = (uint8_t) WMBUS_FRAME_FORMAT       },
    [_CFG_PHY_POSTAMPLE_LENGTH] = {   .addr = _REG_PHY_POSTAMBLE_LENGTH, .type = _U8,  .u8 = (uint8_t) WMBUS_POSTAMPLE_LENGTH   },
    [_CFG_PHY_TX_POWER]         = {   .addr = _REG_PHY_TX_POWER        , .type = _S8,  .s8 = (int8_t) WMBUS_TX_POWER            },
    [_CFG_DL_T_NOM]             = {   .addr = _REG_DL_T_NOM            , .type = _U16, .u16 = (uint16_t) WMBUS_T_NOM            },
    [_CFG_PHY_HEADER_LENGTH]    = {   .addr = _REG_PHY_HEADER_LENGTH   , .type = _U16, .u16 = (uint16_t) WMBUS_HEADER_LENGTH },
};

/* Inline functions ----------------------------------------------------------*/

static inline void _ErrorHandler(ST87EC_Lib_WmbusTransfer_t * pCtx, _error_t ErrId, int32_t Value) {
  pCtx->FsmState = WMBUS_TRANSFER_STATE_ERROR;
  pCtx->LastError.id = ErrId;
  pCtx->LastError.value = Value;
}

#define _SendCmd(P_CTX, ...) do { \
  EcLibVars.RspReceived = SPECIFIC_RSP_NONE; \
  ST87EC_Wrapper_SendCmd(__VA_ARGS__); \
}while(0)


static inline ST87EC_Lib_SpecificResponse_t _CheckAnswer(ST87EC_Lib_WmbusTransfer_t * pCtx) {
  ST87EC_Lib_SpecificResponse_t ret = EcLibVars.RspReceived;

  if (SPECIFIC_RSP_NONE != EcLibVars.RspReceived) {
    EcLibVars.RspReceived = SPECIFIC_RSP_NONE;
  }

  return ret;
}

static inline void _StartResp(uint32_t Idx) {
  ST87EC_Lib_RspInfo_t * pRspInfo = &EcLibVars.RspInfo[Idx];

  pRspInfo->Validity = RSP_AWAITED;
}

static inline ST87EC_Lib_RspValidity_t _CheckResp(uint32_t Idx) {
  ST87EC_Lib_RspInfo_t * pRspInfo = &EcLibVars.RspInfo[Idx];
  ST87EC_Lib_RspValidity_t ret = pRspInfo->Validity;

  if (RSP_RECEIVED == pRspInfo->Validity) {
    pRspInfo->Validity = RSP_NONE;
  }

  return ret;
}

static inline uint8_t _TimerStart(ST87EC_Lib_WmbusTransfer_t * pCtx, uint32_t Timeout){
  uint32_t timerId = ST87EC_Wrapper_StartTimer(Timeout);
  uint8_t result = _RET_OK;

  if (ST87EC_TIMER_ERROR == timerId) {
    result = _RET_ERR1;
  }
  else {
    /*
    if (timerId != pCtx->TimerId)
    {
      //warning: previous timer has not been release, so there's the risk of get out of timers
    }
    */
    pCtx->TimerId = timerId;
  }

  return result;
}

static inline void _ForceWakeup(ST87EC_Lib_WmbusTransfer_t * pCtx) {
  /* If user request a zero size "Last Packet", then exit sleep mode immediately */
  _SendCmd(pCtx, "AT");
}

/* Public functions ----------------------------------------------------------*/

ST87EC_Lib_Result_t ST87EC_SequenceWmbus_Transfer(void)
{
  ST87EC_Lib_Result_t result = RESULT_OK;

  ST87EC_Lib_WmbusTransfer_t * pCtx = &EcLibVars.SequenceWmbus.Transfer;

  switch(pCtx->FsmState)
  {
    case WMBUS_TRANSFER_STATE_INIT:
    {
      if (STATUS_WAKEUP == EcLibVars.ModuleStatus.SleepWakeupstatus)
      {
        pCtx->CmdIdx = 0;

        if (_RET_OK == _TimerStart(pCtx, _CMD_INIT_TIMEOUT))
        {
          _StartResp(WMBUS_INIT_INDEX);
          _SendCmd(pCtx, "AT#WMBUS_INIT");

          pCtx->FsmState = WMBUS_TRANSFER_STATE_INIT_OK;
        }
        else
        {
          _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
        }
      }
      else
      {
        if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
        {
          _ForceWakeup(pCtx);

          pCtx->FsmState = WMBUS_TRANSFER_STATE_WAKEUP_INIT;
        }
        else
        {
          _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_WAKEUP_INIT:
    {
      if (STATUS_WAKEUP == EcLibVars.ModuleStatus.SleepWakeupstatus)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        pCtx->FsmState = WMBUS_TRANSFER_STATE_INIT;
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
          {
            _ForceWakeup(pCtx);
          }
          else
          {
            _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
          }
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_INIT_OK:
    {
      ST87EC_Lib_SpecificResponse_t answer = _CheckAnswer(pCtx);

      if (SPECIFIC_RSP_NONE != answer)
      {
        if (SPECIFIC_RSP_OK == answer)
        {
          pCtx->FsmState = WMBUS_TRANSFER_STATE_INIT_URC;
        }
        else
        {
          result = RESULT_KO;
          _ErrorHandler(pCtx, _ERR_INIT_RSP, (int32_t) answer);
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_INIT_TMO, 0);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_INIT_URC:
    {
      ST87EC_Lib_RspValidity_t ret = _CheckResp(WMBUS_INIT_INDEX);

      if (RSP_RECEIVED == ret)
      {
        const size_t param_offset = strlen("#WMBUS_INIT:");
        char ret = EcLibVars.RspData[param_offset];

        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        if ('1' == ret)
        {
          pCtx->CmdIdx = 0;
          pCtx->FsmState = WMBUS_TRANSFER_STATE_CFG;
        }
        else
        {
          result = RESULT_KO;
          _ErrorHandler(pCtx, _ERR_INIT_VAL, (int32_t) ret);
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_INIT_TMO, 1);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_CFG:
    {
      const _cfg_t * pCfg = &_CFG_TAB[pCtx->CmdIdx];

      if (_RET_OK == _TimerStart(pCtx,_CMD_DEF_TIMEOUT))
      {
        pCtx->FsmState = WMBUS_TRANSFER_STATE_CFG_OK;

        if (_U8 == pCfg->type)
        {
          _SendCmd(pCtx,"AT#WMBUS_SET=0x%X,%u", pCfg->addr, pCfg->u8);
        }
        else if (_S8 == pCfg->type)
        {
          _SendCmd(pCtx,"AT#WMBUS_SET=0x%X,%d", pCfg->addr, pCfg->s8);
        }
        else if (_U16 == pCfg->type)
        {
          _SendCmd(pCtx,"AT#WMBUS_SET=0x%X,%u", pCfg->addr, pCfg->u16);
        }
        else
        {
          result = RESULT_BAD_PARAM; /* should not happen */

          _ErrorHandler(pCtx, _ERR_CFG_TYPE, (int32_t)pCfg->type);
        }
      }
      else
      {
        _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_CFG_OK:
    {
      ST87EC_Lib_SpecificResponse_t answer = _CheckAnswer(pCtx);

      if (SPECIFIC_RSP_NONE != answer)
      {
        if (SPECIFIC_RSP_OK == answer)
        {
          ST87EC_Wrapper_StopTimer(pCtx->TimerId);

          pCtx->CmdIdx++;

          if (pCtx->CmdIdx < _CFG_ENTRIES)
          {
            pCtx->FsmState = WMBUS_TRANSFER_STATE_CFG;
          }
          else
          {
            pCtx->FsmState = WMBUS_TRANSFER_STATE_READY;
          }
        }
        else
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_CFG_RSP, (int32_t) answer);
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_CFG_TMO, 0);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_READY:
    {
      if (pCtx->DataLen > 0)
      {
        pCtx->FsmState = WMBUS_TRANSFER_STATE_SEND;
      }
      else
      {
        if (pCtx->LastPacket == LAST_PKT_TRUE)
        {
          pCtx->FsmState = WMBUS_TRANSFER_STATE_DEINIT;
        }
        else
        {
          /* No data to send? Notify User that it can send a new request */
          if (pCtx->Callback)
            pCtx->Callback(WMBUS_NTY_Ready, pCtx->UsrPtr);

          pCtx->FsmState = WMBUS_TRANSFER_STATE_SLEEP;
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_SEND:
    {
      uint8_t ret = _RET_OK;

      _StartResp(WMBUS_SENT_INDEX);
      ret = _SendDataFrame(pCtx);

      if (_RET_OK != ret)
      {
        result = RESULT_KO;

        _ErrorHandler(pCtx, _ERR_SEND_LEN, (int32_t) ret);
      }
      else
      {
        pCtx->FsmState = WMBUS_TRANSFER_STATE_SEND_OK;
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_SEND_OK:
    {
      ST87EC_Lib_SpecificResponse_t answer = _CheckAnswer(pCtx);

      if (SPECIFIC_RSP_NONE != answer)
      {
        if (SPECIFIC_RSP_OK == answer)
        {
          pCtx->FsmState = WMBUS_TRANSFER_STATE_SEND_URC;
        }
        else
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_SEND_RSP, (int32_t) answer);
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_SEND_TMO, 0);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_SEND_URC:
    {
      ST87EC_Lib_RspValidity_t ret = _CheckResp(WMBUS_SENT_INDEX);

      if (RSP_RECEIVED == ret)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        /* Reset data length in order to not trigger another send */
        pCtx->DataLen = 0;
        pCtx->NAcc++;

        if (LAST_PKT_TRUE == pCtx->LastPacket)
        {
          pCtx->FsmState = WMBUS_TRANSFER_STATE_DEINIT;
        }
        else
        {
          /* Data has been sent:  Notify User that it can send a new request */
          if (pCtx->Callback)
            pCtx->Callback(WMBUS_NTY_Sent, pCtx->UsrPtr);

          pCtx->FsmState = WMBUS_TRANSFER_STATE_SLEEP;
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_SEND_TMO, 1);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_SLEEP:
    {
      if (_RET_OK == _TimerStart(pCtx,_CMD_SLEEP_TIMEOUT))
      {
        if (pCtx->Asynch)
        {
          /* Sleep for the maximum  value allowed */
          _SendCmd(pCtx,"AT#WMBUS_SLEEP_START=%u", WMBUS_ASYNCH_SLEEP_DURATION_MS);
        }
        else
        {

          _SendCmd(pCtx,"AT#WMBUS_SLEEP_START=%u", WMBUS_SLEEP_DURATION_MS);
        }

        pCtx->FsmState = WMBUS_TRANSFER_STATE_SLEEP_OK;
      }
      else
      {
        _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_SLEEP_OK:
    {
      ST87EC_Lib_SpecificResponse_t answer = _CheckAnswer(pCtx);

      if (SPECIFIC_RSP_NONE != answer)
      {
        if (SPECIFIC_RSP_OK == answer)
        {
          pCtx->FsmState = WMBUS_TRANSFER_STATE_SLEEP_URC;
        }
        else
        {
          _ErrorHandler(pCtx, _ERR_SLEEP_RSP, (int32_t) answer);
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_SLEEP_TMO, 0);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_SLEEP_URC:
    {
      if (STATUS_SLEEP == EcLibVars.ModuleStatus.SleepWakeupstatus)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        if (!pCtx->Asynch)
        {
          if (_RET_OK == _TimerStart(pCtx,_CMD_WAKEUP_TIMEOUT))
          {
            pCtx->FsmState = WMBUS_TRANSFER_STATE_WAKEUP_URC;
          }
          else
          {
            _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
          }
        }
        else
        {
          pCtx->FsmState = WMBUS_TRANSFER_STATE_WAKEUP_URC;
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_SLEEP_TMO, 1);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_WAKEUP_URC:
    {
      if (STATUS_WAKEUP == EcLibVars.ModuleStatus.SleepWakeupstatus)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        pCtx->FsmState = WMBUS_TRANSFER_STATE_READY;
      }
      else if ((0 == pCtx->DataLen) && (pCtx->LastPacket == LAST_PKT_TRUE))
      {
        if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
        {
          _ForceWakeup(pCtx);

          pCtx->FsmState = WMBUS_TRANSFER_STATE_WAKEUP_DEINIT;
        }
        else
        {
          _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
        }
      }
      else if ((pCtx->Asynch) && (pCtx->DataLen > 0))
      {
        if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
        {
          _ForceWakeup(pCtx);

          pCtx->FsmState = WMBUS_TRANSFER_STATE_WAKEUP_READY;
        }
        else
        {
          _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
        }
      }
      else
      {
        ST87EC_Wrapper_TimerStatus_t timerStatus = ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId);

        if (TIMER_STATUS_ELAPSED == timerStatus)
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_WAKEUP_TMO, 0);
        }
        else if (TIMER_STATUS_IDLE == timerStatus)
        {
          /* If wakeup timeout timer is not running, it means that asynch transfer has been done,
           * so if user request a synch transfer, wakeup must be performed
           */
          if ((!pCtx->Asynch) && (pCtx->DataLen > 0))
          {
            if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
            {
              _ForceWakeup(pCtx);

              pCtx->FsmState = WMBUS_TRANSFER_STATE_WAKEUP_READY;
            }
            else
            {
              _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
            }
          }
        }
        else
        {
          ;
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_WAKEUP_READY:
    {
      if (STATUS_WAKEUP == EcLibVars.ModuleStatus.SleepWakeupstatus)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        pCtx->FsmState = WMBUS_TRANSFER_STATE_READY;
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
          {
            _ForceWakeup(pCtx);
          }
          else
          {
            _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
          }
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_WAKEUP_DEINIT:
    {
      if (STATUS_WAKEUP == EcLibVars.ModuleStatus.SleepWakeupstatus)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);

        pCtx->FsmState = WMBUS_TRANSFER_STATE_DEINIT;
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          if (_RET_OK == _TimerStart(pCtx, _CMD_WAKEUP_FORCED_TIMEOUT))
          {
            _ForceWakeup(pCtx);
          }
          else
          {
            _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
          }
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_DEINIT:
    {
      if (_RET_OK == _TimerStart(pCtx, _CMD_DEF_TIMEOUT))
      {
        _SendCmd(pCtx,"AT#WMBUS_DEINIT");

        pCtx->FsmState = WMBUS_TRANSFER_STATE_DEINIT_OK;
      }
      else
      {
        _ErrorHandler(pCtx, _ERR_TIMER_START, __LINE__);
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_DEINIT_OK:
    {
      ST87EC_Lib_SpecificResponse_t answer = _CheckAnswer(pCtx);

      if (SPECIFIC_RSP_NONE != answer)
      {
        if (SPECIFIC_RSP_OK == answer)
        {
          ST87EC_Wrapper_StopTimer(pCtx->TimerId);

          if (pCtx->Callback)
            pCtx->Callback(WMBUS_NTY_Done, pCtx->UsrPtr);

          pCtx->FsmState = WMBUS_TRANSFER_STATE_END;
        }
        else
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_DEINIT_RSP, (int32_t) answer);
        }
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          result = RESULT_KO;

          _ErrorHandler(pCtx, _ERR_DEINIT_TMO, 0);
        }
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_ERROR:
    {
      (void) _TimerStart(pCtx, _CMD_DEF_TIMEOUT);

      _SendCmd(pCtx,"AT#WMBUS_DEINIT");

      pCtx->FsmState = WMBUS_TRANSFER_STATE_ERROR_OK;

      break;
    }

    case WMBUS_TRANSFER_STATE_ERROR_OK:
    {
      uint8_t endCondition = 0;

      ST87EC_Lib_SpecificResponse_t answer = _CheckAnswer(pCtx);

      if (SPECIFIC_RSP_NONE != answer)
      {
        ST87EC_Wrapper_StopTimer(pCtx->TimerId);
        endCondition = 1;
      }
      else
      {
        if (TIMER_STATUS_ELAPSED == ST87EC_Wrapper_GetTimerStatus(pCtx->TimerId))
        {
          endCondition = 1;

          result = RESULT_KO;
        }
      }

      if (endCondition)
      {
        if (pCtx->Callback)
          pCtx->Callback(WMBUS_NTY_Fail, pCtx->UsrPtr);

        pCtx->FsmState = WMBUS_TRANSFER_STATE_END;
      }

      break;
    }

    case WMBUS_TRANSFER_STATE_END:
      /* Handled outside switch */
      break;

    default:
      EcLibVars.ModuleStatus.TransferOnGoing = 0;
      EcLibVars.OnGoingSequence = SEQUENCE_NONE;
      result = RESULT_KO;
      break;
  }

  if (WMBUS_TRANSFER_STATE_END == pCtx->FsmState)
  {
    EcLibVars.ModuleStatus.TransferOnGoing = 0;
    EcLibVars.OnGoingSequence = SEQUENCE_NONE;
  }

  return (result);
}

/* Private functions definition -----------------------------------------------*/

static uint8_t _SendDataFrame(ST87EC_Lib_WmbusTransfer_t * pCtx)
{
  const uint32_t MAX_LEN = strlen("AT#WMBUS_TX_DATA=0,1,X,") + 2U*WMBUS_TX_BUFFER_SIZE + 1U;

  uint8_t buffer[MAX_LEN];
  uint32_t size = 0U;
  uint8_t retval = _RET_OK;

  const uint8_t * pFrame = pCtx->DataBuffer;
  uint16_t LenFrame = pCtx->DataLen;

  if (LenFrame > 0)
  {
    /* Parsing command */
    if (pCtx->Asynch)
    {
      if (_RET_OK == _TimerStart(pCtx, _CMD_SEND_TIMEOUT_ASYNCH))
      {
        size += snprintf((char *)(&buffer[size]), MAX_LEN - size, "AT#WMBUS_TX_DATA=0,0,%u,", pCtx->NAcc);
      }
      else
      {
        retval = _RET_ERR3;
      }
    }
    else
    {
      if (_RET_OK == _TimerStart(pCtx, _CMD_SEND_TIMEOUT_SYNCH))
      {
        size += snprintf((char *)(&buffer[size]), MAX_LEN - size, "AT#WMBUS_TX_DATA=0,1,%u,", pCtx->NAcc);
      }
      else
      {
        retval = _RET_ERR3;
      }
    }

    if (_RET_OK == retval)
    {
      while(LenFrame > 0 && (size <= (MAX_LEN - 1)))
      {
        size += snprintf((char *)(&buffer[size]), MAX_LEN - size, "%02X", *pFrame);
        LenFrame--;
        pFrame++;
      }

      if (0 == LenFrame)
      {
        uint32_t length_sent = 0U;

        EcLibVars.RspReceived = SPECIFIC_RSP_NONE;

        /* Adding closing CR/LF */
        buffer[size++] = '\r';
        length_sent = ST87EC_Wrapper_SendByte(buffer, size);

        if (length_sent != size)
        {
          retval = _RET_ERR1;
        }
      }
      else
      {
        retval = _RET_ERR2; /* Warning: maximum length reached! */
      }
    }
  }

  return(retval);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

