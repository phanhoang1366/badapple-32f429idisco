/**
  ******************************************************************************
  * @file config.h
  * @brief Centralized configuration for multimedia engine
  * @author STM32 Embedded Developer
  * @date January 2026
  ******************************************************************************
  */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>

/* ========== VIDEO CONFIGURATION ========== */
#define VIDEO_WIDTH                240
#define VIDEO_HEIGHT               320
#define VIDEO_FPS                  60          /* Frame per second */
#define VIDEO_TOTAL_FRAMES         18000       /* Total frames (300 seconds at 60fps) */
#define VIDEO_DURATION_SECONDS     (VIDEO_TOTAL_FRAMES / VIDEO_FPS)

/* ========== AUDIO CONFIGURATION ========== */
#define AUDIO_SAMPLE_RATE          4000        /* Hz */
#define AUDIO_BITS_PER_SAMPLE      4           /* ADPCM 4-bit */
#define AUDIO_BYTES_PER_FRAME      ((AUDIO_SAMPLE_RATE / VIDEO_FPS) * AUDIO_BITS_PER_SAMPLE / 8)
#define AUDIO_FRAMES_PER_SECOND    VIDEO_FPS
#define ADPCM_STATE_SAVE_INTERVAL  (AUDIO_BYTES_PER_FRAME * AUDIO_FRAMES_PER_SECOND)  /* Save per 1s */

/* ========== MEMORY MANAGEMENT ========== */
#define ADPCM_SNAPSHOT_COUNT       (VIDEO_DURATION_SECONDS + 1)  /* +1 for safety */
#define ADPCM_SNAPSHOT_SIZE        sizeof(ADPCM_State)
#define TOTAL_ADPCM_BUFFER_SIZE    (ADPCM_SNAPSHOT_COUNT * ADPCM_SNAPSHOT_SIZE)

/* Compile-time assertions */
#if TOTAL_ADPCM_BUFFER_SIZE > 50000
    #error "ADPCM buffer exceeds 50KB! Check memory constraints."
#endif

#if VIDEO_FPS <= 0
    #error "VIDEO_FPS must be positive!"
#endif

#if VIDEO_TOTAL_FRAMES <= 0
    #error "VIDEO_TOTAL_FRAMES must be positive!"
#endif

/* ========== SEEK CONFIGURATION ========== */
#define SEEK_STEP_SECONDS          5           /* Seek ±5 seconds */
#define SEEK_STEP_FRAMES           (SEEK_STEP_SECONDS * VIDEO_FPS)

/* ========== DEBUG CONFIGURATION ========== */
#define DEBUG_ENABLED              1
#define DEBUG_SEEK                 1
#define DEBUG_AUDIO                1
#define DEBUG_SYNC                 1
#define DEBUG_MEMORY               1

#if DEBUG_ENABLED
    #define DBG(fmt, ...) \
        do { \
            printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__); \
        } while(0)
#else
    #define DBG(fmt, ...)
#endif

#if DEBUG_SEEK
    #define DBG_SEEK(fmt, ...) DBG("[SEEK] " fmt, ##__VA_ARGS__)
#else
    #define DBG_SEEK(fmt, ...)
#endif

#if DEBUG_AUDIO
    #define DBG_AUDIO(fmt, ...) DBG("[AUDIO] " fmt, ##__VA_ARGS__)
#else
    #define DBG_AUDIO(fmt, ...)
#endif

#if DEBUG_SYNC
    #define DBG_SYNC(fmt, ...) DBG("[SYNC] " fmt, ##__VA_ARGS__)
#else
    #define DBG_SYNC(fmt, ...)
#endif

#if DEBUG_MEMORY
    #define DBG_MEM(fmt, ...) DBG("[MEM] " fmt, ##__VA_ARGS__)
#else
    #define DBG_MEM(fmt, ...)
#endif

#endif /* __CONFIG_H */
/************************** (C) STM32 Developer *****END OF FILE****/
