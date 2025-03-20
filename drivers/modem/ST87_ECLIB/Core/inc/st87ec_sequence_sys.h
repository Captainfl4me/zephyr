/**
  ******************************************************************************
  * @file    st87ec_sequence_sys.h
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
#ifndef ST87EC_SEQUENCE_SYS_H
#define ST87EC_SEQUENCE_SYS_H

#include "st87ec_engine.h"

/* Private constants and macros-----------------------------------------------*/



/* Types ---------------------------------------------------------------------*/




/* Global variables ----------------------------------------------------------*/


/* Exported functions --------------------------------------------------------*/
ST87EC_Lib_Result_t ST87EC_SequenceSys_GetTime(void);
ST87EC_Lib_Result_t ST87EC_SequenceSys_ColdParamInit(void);

#endif /* ST87EC_SEQUENCE_SYS_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
