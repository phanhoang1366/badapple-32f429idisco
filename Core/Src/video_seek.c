/**
  ******************************************************************************
  * @file video_seek.c
  * @brief Video seeking implementation
  * @author STM32 Embedded Developer
  * @date January 2026
  ******************************************************************************
  */

#include "video_seek.h"

/* External reference to global ADPCM buffer (defined in main.c or adpcm.c) */
extern ADPCM_StateBuffer adpcm_buffer;

/* ========== UTILITY FUNCTIONS ========== */

/**
  * @brief Convert frame number to time in milliseconds
  * @param frame_num: frame number
  * @retval uint32_t: time in milliseconds
  * @note: time_ms = frame_num * (1000 / VIDEO_FPS)
  */
uint32_t Video_FrameToTimeMs(uint32_t frame_num)
{
  return (frame_num * 1000) / VIDEO_FPS;
}

/**
  * @brief Convert time in milliseconds to frame number
  * @param time_ms: time in milliseconds
  * @retval uint32_t: frame number
  * @note: frame = time_ms * (VIDEO_FPS / 1000)
  */
uint32_t Video_TimeToFrame(uint32_t time_ms)
{
  return (time_ms * VIDEO_FPS) / 1000;
}

/**
  * @brief Convert frame number to audio snapshot index
  * @param frame_num: frame number
  * @retval uint16_t: snapshot index
  * @note: Snapshots are saved every VIDEO_FPS frames (every 1 second)
  */
uint16_t Video_FrameToAudioSnapshot(uint32_t frame_num)
{
  return frame_num / VIDEO_FPS;
}

/**
  * @brief Validate seek target is within bounds
  * @param target_frame: target frame number
  * @retval Seek_Error_t: SEEK_OK or SEEK_ERR_OUT_OF_RANGE
  */
Seek_Error_t Video_ValidateFrameNumber(uint32_t target_frame)
{
  if (target_frame >= VIDEO_TOTAL_FRAMES) {
    DBG_SEEK("ERROR: Frame %lu out of range (max %u)", 
             target_frame, VIDEO_TOTAL_FRAMES - 1);
    return SEEK_ERR_OUT_OF_RANGE;
  }
  return SEEK_OK;
}

/* ========== MAIN SEEK FUNCTIONS ========== */

/**
  * @brief Seek to specific frame
  * @param target_frame: target frame number (0 to VIDEO_TOTAL_FRAMES-1)
  * @param info: pointer to SeekInfo_t to store seek information
  * @retval Seek_Error_t: SEEK_OK or error code
  */
Seek_Error_t Video_SeekToFrame(uint32_t target_frame, SeekInfo_t *info)
{
  if (info == NULL) {
    DBG_SEEK("ERROR: NULL SeekInfo pointer");
    return SEEK_ERR_NULL_POINTER;
  }
  
  /* Validate target frame */
  Seek_Error_t err = Video_ValidateFrameNumber(target_frame);
  if (err != SEEK_OK) {
    return err;
  }
  
  /* Calculate audio snapshot index */
  uint16_t snapshot_idx = Video_FrameToAudioSnapshot(target_frame);
  
  /* Restore audio state from snapshot */
  ADPCM_Error_t adpcm_err = ADPCM_RestoreSnapshot(&adpcm_buffer, snapshot_idx);
  if (adpcm_err != ADPCM_OK) {
    DBG_SEEK("ERROR: Failed to restore audio snapshot %d (error %d)", 
             snapshot_idx, adpcm_err);
    return SEEK_ERR_AUDIO_FAILED;
  }
  
  /* Fill seek info structure */
  info->frame_number = target_frame;
  info->byte_offset = (target_frame * AUDIO_BYTES_PER_FRAME);
  info->audio_snapshot_index = snapshot_idx;
  info->timestamp_ms = Video_FrameToTimeMs(target_frame);
  info->is_key_frame = (target_frame % VIDEO_FPS == 0);  /* First frame of each second */
  
  DBG_SEEK("Seek successful: frame=%lu, time=%lums, snapshot=%u",
           target_frame, info->timestamp_ms, snapshot_idx);
  
  return SEEK_OK;
}

/**
  * @brief Seek to specific time
  * @param time_ms: target time in milliseconds
  * @param info: pointer to SeekInfo_t to store seek information
  * @retval Seek_Error_t: SEEK_OK or error code
  */
Seek_Error_t Video_SeekToTime(uint32_t time_ms, SeekInfo_t *info)
{
  if (info == NULL) {
    DBG_SEEK("ERROR: NULL SeekInfo pointer");
    return SEEK_ERR_NULL_POINTER;
  }
  
  uint32_t target_frame = Video_TimeToFrame(time_ms);
  
  DBG_SEEK("Converting time %lums to frame %lu", time_ms, target_frame);
  
  return Video_SeekToFrame(target_frame, info);
}

/**
  * @brief Seek relative to current frame
  * @param frame_delta: frame offset (positive = forward, negative = backward)
  * @param current_frame: current frame number for reference
  * @param info: pointer to SeekInfo_t to store seek information
  * @retval Seek_Error_t: SEEK_OK or error code
  */
Seek_Error_t Video_SeekRelative(int32_t frame_delta, uint32_t current_frame, SeekInfo_t *info)
{
  if (info == NULL) {
    DBG_SEEK("ERROR: NULL SeekInfo pointer");
    return SEEK_ERR_NULL_POINTER;
  }
  
  /* Calculate target frame */
  int32_t target_frame = (int32_t)current_frame + frame_delta;
  
  /* Clamp to valid range */
  if (target_frame < 0) {
    target_frame = 0;
    DBG_SEEK("Seek would go before start, clamping to frame 0");
  } else if (target_frame >= (int32_t)VIDEO_TOTAL_FRAMES) {
    target_frame = VIDEO_TOTAL_FRAMES - 1;
    DBG_SEEK("Seek would go past end, clamping to frame %u", VIDEO_TOTAL_FRAMES - 1);
  }
  
  DBG_SEEK("Relative seek: current=%lu, delta=%ld, target=%ld", 
           current_frame, frame_delta, target_frame);
  
  return Video_SeekToFrame((uint32_t)target_frame, info);
}

/************************** (C) STM32 Developer *****END OF FILE****/
