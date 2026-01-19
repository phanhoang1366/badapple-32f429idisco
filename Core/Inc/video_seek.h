/**
  ******************************************************************************
  * @file video_seek.h
  * @brief Video seeking and synchronization module
  * @author STM32 Embedded Developer
  * @date January 2026
  ******************************************************************************
  */

#ifndef __VIDEO_SEEK_H
#define __VIDEO_SEEK_H

#include "config.h"
#include "adpcm.h"

/* ========== SEEK INFO STRUCTURE ========== */
typedef struct {
  uint32_t frame_number;
  uint32_t byte_offset;
  uint16_t audio_snapshot_index;
  uint32_t timestamp_ms;
  uint8_t is_key_frame;  /* For future: support I-frame seeks */
} SeekInfo_t;

/* ========== SEEK ERROR CODES ========== */
typedef enum {
  SEEK_OK = 0,
  SEEK_ERR_OUT_OF_RANGE = -1,
  SEEK_ERR_INVALID_FRAME = -2,
  SEEK_ERR_AUDIO_FAILED = -3,
  SEEK_ERR_NULL_POINTER = -4
} Seek_Error_t;

/* ========== FUNCTION PROTOTYPES ========== */

/**
  * @brief Seek to specific frame
  * @param target_frame: target frame number (0 to VIDEO_TOTAL_FRAMES-1)
  * @param info: pointer to SeekInfo_t to store seek information
  * @retval Seek_Error_t: SEEK_OK or error code
  */
Seek_Error_t Video_SeekToFrame(uint32_t target_frame, SeekInfo_t *info);

/**
  * @brief Seek to specific time
  * @param time_ms: target time in milliseconds
  * @param info: pointer to SeekInfo_t to store seek information
  * @retval Seek_Error_t: SEEK_OK or error code
  */
Seek_Error_t Video_SeekToTime(uint32_t time_ms, SeekInfo_t *info);

/**
  * @brief Seek relative to current frame
  * @param frame_delta: frame offset (positive = forward, negative = backward)
  * @param current_frame: current frame number for reference
  * @param info: pointer to SeekInfo_t to store seek information
  * @retval Seek_Error_t: SEEK_OK or error code
  */
Seek_Error_t Video_SeekRelative(int32_t frame_delta, uint32_t current_frame, SeekInfo_t *info);

/* ========== UTILITY FUNCTIONS ========== */

/**
  * @brief Convert frame number to time in milliseconds
  * @param frame_num: frame number
  * @retval uint32_t: time in milliseconds
  */
uint32_t Video_FrameToTimeMs(uint32_t frame_num);

/**
  * @brief Convert time in milliseconds to frame number
  * @param time_ms: time in milliseconds
  * @retval uint32_t: frame number
  */
uint32_t Video_TimeToFrame(uint32_t time_ms);

/**
  * @brief Convert frame number to audio snapshot index
  * @param frame_num: frame number
  * @retval uint16_t: snapshot index
  */
uint16_t Video_FrameToAudioSnapshot(uint32_t frame_num);

/**
  * @brief Validate seek target is within bounds
  * @param target_frame: target frame number
  * @retval Seek_Error_t: SEEK_OK or SEEK_ERR_OUT_OF_RANGE
  */
Seek_Error_t Video_ValidateFrameNumber(uint32_t target_frame);

#endif /* __VIDEO_SEEK_H */
/************************** (C) STM32 Developer *****END OF FILE****/
