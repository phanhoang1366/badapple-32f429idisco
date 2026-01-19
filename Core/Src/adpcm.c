/**
  ******************************************************************************
  * @file ADPCM/src/adpcm.c 
  * @author  MCD Application Team
  * @version  V2.0.0
  * @date  04/27/2009
  * @brief  This is ADPCM decoder/encoder souce file
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


/* Includes ------------------------------------------------------------------*/
#include "adpcm.h"
#include "config.h"

/* Private define ------------------------------------------------------------*/
/* Quantizer step size lookup table */
const uint16_t StepSizeTable[89]={7,8,9,10,11,12,13,14,16,17,
                            19,21,23,25,28,31,34,37,41,45,
                            50,55,60,66,73,80,88,97,107,118,
                            130,143,157,173,190,209,230,253,279,307,
                            337,371,408,449,494,544,598,658,724,796,
                            876,963,1060,1166,1282,1411,1552,1707,1878,2066,
                            2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,
                            5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
                            15289,16818,18500,20350,22385,24623,27086,29794,32767};
/* Table of index changes */
const int8_t IndexTable[16]={0xff,0xff,0xff,0xff,2,4,6,8,0xff,0xff,0xff,0xff,2,4,6,8};

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static int16_t  adpcm_index = 0;
static int32_t adpcm_predsample = 0;
static ADPCM_StateBuffer adpcm_buffer = {0};

/* Private function prototypes -----------------------------------------------*/
static void disable_interrupts(void);
static void enable_interrupts(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief Disable interrupts (thread-safe operation)
  * @retval None
  */
static void disable_interrupts(void)
{
  __disable_irq();
}

/**
  * @brief Enable interrupts
  * @retval None
  */
static void enable_interrupts(void)
{
  __enable_irq();
}

/* ========== VALIDATION FUNCTIONS ========== */

/**
  * @brief Validate ADPCM state values
  * @param state: pointer to ADPCM_State
  * @retval ADPCM_Error_t: ADPCM_OK or error code
  */
ADPCM_Error_t ADPCM_ValidateState(const ADPCM_State *state)
{
  if (state == NULL) {
    DBG_AUDIO("ERROR: NULL state pointer");
    return ADPCM_ERR_NULL_POINTER;
  }
  
  if (state->index < 0 || state->index > 88) {
    DBG_AUDIO("ERROR: Invalid index %d (must be 0-88)", state->index);
    return ADPCM_ERR_INVALID_INDEX;
  }
  
  /* Predicted sample should be within int16 range */
  if (state->predsample > 32767 || state->predsample < -32768) {
    DBG_AUDIO("ERROR: Invalid predsample %ld", state->predsample);
    return ADPCM_ERR_OUT_OF_BOUNDS;
  }
  
  return ADPCM_OK;
}

/**
  * @brief Validate snapshot index
  * @param index: snapshot index
  * @retval ADPCM_Error_t: ADPCM_OK or error code
  */
ADPCM_Error_t ADPCM_ValidateSnapshotIndex(uint16_t index)
{
  if (index >= ADPCM_SNAPSHOT_COUNT) {
    DBG_AUDIO("ERROR: Snapshot index %d exceeds max %d", index, ADPCM_SNAPSHOT_COUNT - 1);
    return ADPCM_ERR_OUT_OF_BOUNDS;
  }
  return ADPCM_OK;
}

/* ========== BUFFER MANAGEMENT FUNCTIONS ========== */

/**
  * @brief Initialize ADPCM state buffer
  * @param buffer: pointer to ADPCM_StateBuffer
  * @retval None
  */
void ADPCM_InitBuffer(ADPCM_StateBuffer *buffer)
{
  if (buffer == NULL) return;
  
  memset(buffer->states, 0, sizeof(buffer->states));
  buffer->total_snapshots = 0;
  buffer->last_save_byte_count = 0;
  
  DBG_MEM("ADPCM Buffer initialized (max %d snapshots, %u bytes)", 
          ADPCM_SNAPSHOT_COUNT, TOTAL_ADPCM_BUFFER_SIZE);
}

/**
  * @brief Save ADPCM state snapshot
  * @param buffer: pointer to ADPCM_StateBuffer
  * @param snapshot_index: index to save
  * @retval ADPCM_Error_t
  */
ADPCM_Error_t ADPCM_SaveSnapshot(ADPCM_StateBuffer *buffer, uint16_t snapshot_index)
{
  ADPCM_Error_t err = ADPCM_ValidateSnapshotIndex(snapshot_index);
  if (err != ADPCM_OK) return err;
  
  if (buffer == NULL) return ADPCM_ERR_NULL_POINTER;
  
  ADPCM_State current_state;
  ADPCM_GetState(&current_state);
  
  /* Thread-safe save */
  disable_interrupts();
  buffer->states[snapshot_index] = current_state;
  if (snapshot_index >= buffer->total_snapshots) {
    buffer->total_snapshots = snapshot_index + 1;
  }
  enable_interrupts();
  
  DBG_AUDIO("Saved snapshot [%d]: index=%d, predsample=%ld", 
            snapshot_index, current_state.index, current_state.predsample);
  
  return ADPCM_OK;
}

/**
  * @brief Restore ADPCM state from snapshot
  * @param buffer: pointer to ADPCM_StateBuffer
  * @param snapshot_index: index to restore
  * @retval ADPCM_Error_t
  */
ADPCM_Error_t ADPCM_RestoreSnapshot(ADPCM_StateBuffer *buffer, uint16_t snapshot_index)
{
  ADPCM_Error_t err = ADPCM_ValidateSnapshotIndex(snapshot_index);
  if (err != ADPCM_OK) return err;
  
  if (buffer == NULL) return ADPCM_ERR_NULL_POINTER;
  
  if (snapshot_index >= buffer->total_snapshots) {
    DBG_AUDIO("ERROR: Snapshot %d not yet saved (max saved: %d)", 
              snapshot_index, buffer->total_snapshots - 1);
    return ADPCM_ERR_OUT_OF_BOUNDS;
  }
  
  /* Thread-safe restore */
  disable_interrupts();
  err = ADPCM_SetState(&buffer->states[snapshot_index]);
  enable_interrupts();
  
  if (err == ADPCM_OK) {
    DBG_AUDIO("Restored snapshot [%d]: index=%d, predsample=%ld", 
              snapshot_index, 
              buffer->states[snapshot_index].index,
              buffer->states[snapshot_index].predsample);
  }
  
  return err;
}

/**
  * @brief Get total snapshots saved
  * @param buffer: pointer to ADPCM_StateBuffer
  * @retval uint16_t: number of snapshots
  */
uint16_t ADPCM_GetTotalSnapshots(const ADPCM_StateBuffer *buffer)
{
  if (buffer == NULL) return 0;
  return buffer->total_snapshots;
}


/**
  * @brief  ADPCM_Encode.
  * @param sample: a 16-bit PCM sample
  * @retval : a 4-bit ADPCM sample
  */
uint8_t ADPCM_Encode(int32_t sample)
{
  uint8_t code=0;
  uint16_t tmpstep=0;
  int32_t diff=0;
  int32_t diffq=0;
  uint16_t step=0;
  
  step = StepSizeTable[adpcm_index];

  /* 2. compute diff and record sign and absolut value */
  diff = sample-adpcm_predsample;
  if (diff < 0)  
  {
    code=8;
    diff = -diff;
  }    
  
  /* 3. quantize the diff into ADPCM code */
  /* 4. inverse quantize the code into a predicted diff */
  tmpstep = step;
  diffq = (step >> 3);

  if (diff >= tmpstep)
  {
    code |= 0x04;
    diff -= tmpstep;
    diffq += step;
  }
  
  tmpstep = tmpstep >> 1;

  if (diff >= tmpstep)
  {
    code |= 0x02;
    diff -= tmpstep;
    diffq+=(step >> 1);
  }
  
  tmpstep = tmpstep >> 1;
  
  if (diff >= tmpstep)
  {
    code |=0x01;
    diffq+=(step >> 2);
  }
  
  /* 5. fixed predictor to get new predicted sample*/
  if (code & 8)
  {
    adpcm_predsample -= diffq;
  }
  else
  {
    adpcm_predsample += diffq;
  }  

  /* check for overflow*/
  if (adpcm_predsample > 32767)
  {
    adpcm_predsample = 32767;
  }
  else if (adpcm_predsample < -32768)
  {
    adpcm_predsample = -32768;
  }
  
  /* 6. find new stepsize index */
  adpcm_index += IndexTable[code];
  /* check for overflow*/
  if (adpcm_index <0)
  {
    adpcm_index = 0;
  }
  else if (adpcm_index > 88)
  {
    adpcm_index = 88;
  }
  
  /* 8. return new ADPCM code*/
  return (code & 0x0f);
}



/**
  * @brief  ADPCM_Decode.
  * @param code: a byte containing a 4-bit ADPCM sample. 
  * @retval : 16-bit ADPCM sample
  */
int16_t ADPCM_Decode(uint8_t code)
{
  uint16_t step=0;
  int32_t diffq=0;
  
  step = StepSizeTable[adpcm_index];

  /* 2. inverse code into diff */
  diffq = step>> 3;
  if (code&4)
  {
    diffq += step;
  }
  
  if (code&2)
  {
    diffq += step>>1;
  }
  
  if (code&1)
  {
    diffq += step>>2;
  }

  /* 3. add diff to predicted sample*/
  if (code&8)
  {
    adpcm_predsample -= diffq;
  }
  else
  {
    adpcm_predsample += diffq;
  }
  
  /* check for overflow*/
  if (adpcm_predsample > 32767)
  {
    adpcm_predsample = 32767;
  }
  else if (adpcm_predsample < -32768)
  {
    adpcm_predsample = -32768;
  }

  /* 4. find new quantizer step size */
  adpcm_index += IndexTable [code];
  /* check for overflow*/
  if (adpcm_index < 0)
  {
    adpcm_index = 0;
  }
  if (adpcm_index > 88)
  {
    adpcm_index = 88;
  }
  
  /* 5. save predict sample and index for next iteration */
  /* done! static variables */
  
  /* 6. return new speech sample*/
  return ((int16_t)adpcm_predsample);
}

/**
  * @brief  ADPCM_GetState - Get current decoder state
  * @param state: pointer to ADPCM_State struct to store state
  * @retval None
  */
void ADPCM_GetState(ADPCM_State *state)
{
  // These are accessed from ADPCM_Decode, but we need to make them accessible
  // This is a helper to save state before seek
  extern int16_t adpcm_index;
  extern int32_t adpcm_predsample;
  
  state->index = adpcm_index;
  state->predsample = adpcm_predsample;
}

/**
  * @brief  ADPCM_SetState - Restore decoder state
  * @param state: pointer to ADPCM_State struct with saved state
  * @retval None
  */
void ADPCM_SetState(const ADPCM_State *state)
{
  extern int16_t adpcm_index;
  extern int32_t adpcm_predsample;
  
  adpcm_index = state->index;
  adpcm_predsample = state->predsample;
}

/**
  * @}
  */ 

/**
  * @brief  ADPCM_GetState - Get current decoder state
  * @param state: pointer to ADPCM_State struct to store state
  * @retval None
  */
void ADPCM_GetState(ADPCM_State *state)
{
  if (state == NULL) return;
  
  state->index = adpcm_index;
  state->predsample = adpcm_predsample;
}

/**
  * @brief  ADPCM_SetState - Restore decoder state
  * @param state: pointer to ADPCM_State struct with saved state
  * @retval ADPCM_Error_t: ADPCM_OK or error code
  */
ADPCM_Error_t ADPCM_SetState(const ADPCM_State *state)
{
  ADPCM_Error_t err = ADPCM_ValidateState(state);
  if (err != ADPCM_OK) return err;
  
  adpcm_index = state->index;
  adpcm_predsample = state->predsample;
  
  return ADPCM_OK;
}


/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
