/**
  ******************************************************************************
  * @file    st87ec_sequence_nbiot.h
  * @author  APMS Application Team
  * @brief   System sequences header file
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
#ifndef ST87EC_SEQUENCE_NBIOT_H
#define ST87EC_SEQUENCE_NBIOT_H

#include "st87ec_engine.h"

/* Private constants and macros-----------------------------------------------*/


/* Types ---------------------------------------------------------------------*/


/* Exported functions --------------------------------------------------------*/
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_MqttPublish(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_UdpTcp(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapOpen(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapTransmit(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapReceive(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_CoapClose(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_HttpOpen(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_HttpTransfer(void);
ST87EC_Lib_Result_t ST87EC_SequenceNBIOT_HttpClose(void);

#endif /* ST87EC_SEQUENCE_NBIOT_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
