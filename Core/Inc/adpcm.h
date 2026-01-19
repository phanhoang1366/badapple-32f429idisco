/**
  ******************************************************************************
  * @file ADPCM/inc/adpcm.h 
  * @author  MCD Application Team
  * @version  V2.0.0
  * @date  04/27/2009
  * @brief  Header file for adpcm.c
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2009 STMicroelectronics</center></h2>
  */ 


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADPCM_H
#define __ADPCM_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "config.h"
#include <string.h>

/* ========== ADPCM STATE STRUCT ========== */
/* Exported types ------------------------------------------------------------*/
typedef struct {
  int16_t index;      /* Current quantizer step index (0-88) */
  int32_t predsample; /* Current predicted sample value */
} ADPCM_State;

/* ========== ADPCM STATE BUFFER ========== */
typedef struct {
  ADPCM_State states[ADPCM_SNAPSHOT_COUNT];
  uint16_t total_snapshots;
  uint32_t last_save_byte_count;
} ADPCM_StateBuffer;

/* ========== ERROR CODES ========== */
typedef enum {
  ADPCM_OK = 0,
  ADPCM_ERR_INVALID_INDEX = -1,
  ADPCM_ERR_BUFFER_OVERFLOW = -2,
  ADPCM_ERR_NULL_POINTER = -3,
  ADPCM_ERR_OUT_OF_BOUNDS = -4,
  ADPCM_ERR_INVALID_STATE = -5
} ADPCM_Error_t;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/* Original ADPCM encode/decode */
uint8_t ADPCM_Encode(int32_t sample);
int16_t ADPCM_Decode(uint8_t code);

/* State management */
void ADPCM_GetState(ADPCM_State *state);
ADPCM_Error_t ADPCM_SetState(const ADPCM_State *state);

/* Buffer management */
void ADPCM_InitBuffer(ADPCM_StateBuffer *buffer);
ADPCM_Error_t ADPCM_SaveSnapshot(ADPCM_StateBuffer *buffer, uint16_t snapshot_index);
ADPCM_Error_t ADPCM_RestoreSnapshot(ADPCM_StateBuffer *buffer, uint16_t snapshot_index);
uint16_t ADPCM_GetTotalSnapshots(const ADPCM_StateBuffer *buffer);

/* Validation */
ADPCM_Error_t ADPCM_ValidateState(const ADPCM_State *state);
ADPCM_Error_t ADPCM_ValidateSnapshotIndex(uint16_t index);

#endif /* __ADPCM_H*/
/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
