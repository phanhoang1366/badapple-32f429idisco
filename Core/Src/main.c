/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "stm32f429i_discovery_ts.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f4xx.h"

#include "config.h"
#include "frame_sizes.h"
#include "stm32f4xx_hal_dac.h"
#include "adpcm.h"
#include "video_seek.h"

static const
#include "video_data.h"

static const
#include "audio_data.h" // ADPCM, not used in this example

static const
#include "lyrics.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
  char *text;
} RectButton;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WIDTH 240 /* framebuffer width in pixel */
#define HEIGHT 320 /* framebuffer height in pixel */
#define BPP 4 /* bytes/pixel */

#define SCREEN_X_OFFSET 40 /* Shift video horizontally */
#define SCREEN_Y_OFFSET 40 /* Shift video vertically */

#define VIDEO_WIDTH 160
#define VIDEO_HEIGHT 120

#define LCD_WIDTH_PX 240
#define FONT_WIDTH_PX 8   // for 8pt font (usually 8x8 or 8x16)
#define MAX_CHARS_PER_LINE (LCD_WIDTH_PX / FONT_WIDTH_PX)

#define LYRICS_X_OFFSET 20
#define LYRICS_ROMAJI_Y_OFFSET (VIDEO_HEIGHT + SCREEN_Y_OFFSET + 10)
#define LYRICS_ENGLISH_Y_OFFSET (VIDEO_HEIGHT + SCREEN_Y_OFFSET + 50)

#define NO_EVENT 0xFF

#define DAC_MID 2048
#define BUFFER_SIZE 256

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac2;

TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
DMA2D_HandleTypeDef hdma2d;
LTDC_HandleTypeDef hltdc;
SDRAM_HandleTypeDef hsdram1;

static int LCD_LAYER_FRONT; // active display layer (front buffer)
static int LCD_LAYER_BACK;
static uint32_t* g_fb[2];

extern bool framebuffer_ready;

uint16_t frame_counter = 0;
uint32_t frame_offset = 0;
uint16_t num_frames = sizeof(frame_sizes) / sizeof(frame_sizes[0]);

uint8_t lyric_idx = 0;
uint8_t lyric_count = sizeof(lyric_events) / sizeof(lyric_events[0]);

static TS_StateTypeDef  TS_State;
extern volatile bool playing_state;

bool lyric_toggle = true;
// 2 variables for 2 different framebuffer lyric display states
uint8_t lyric_displayed[2] = {NO_EVENT, NO_EVENT};

uint16_t buffer_samples[BUFFER_SIZE];
unsigned int adpcm_data_index = 0;

/* Global ADPCM state buffer for seeking */
ADPCM_StateBuffer adpcm_buffer = {0};


/* Seeking variables */
static volatile bool is_seeking = false;
static int16_t seek_delta = 0;  // Accumulate seek steps //

RectButton btns[5] = {
	{10, 250, 50, 30, "<<"},      // Seek backward //
	{65, 250, 50, 30, "Play"},
	{120, 250, 50, 30, ">>"},     // Seek forward
	{175, 250, 50, 30, "Lyric"},
	{230, 250, 10, 30, "M"}       // Mute (abbreviated)
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM6_Init(void);
static void MX_DAC_Init(void);
/* USER CODE BEGIN PFP */
void screen_flip_buffers(void);
uint8_t find_active_event(uint16_t frame_number);
void draw_video_frame(const uint8_t* frame_data);
void LCD_DisplayWrappedText(uint16_t x, uint16_t y, char *text);
void clear_lyric_area(void);
void save_adpcm_state_snapshot(void);
void seek_to_adpcm_state(uint16_t snapshot_index);
void handle_button_press(uint16_t x, uint16_t y);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief Save current ADPCM decoder state to snapshot array
  */
void save_adpcm_state_snapshot(void)
{
  uint16_t snapshot_idx = adpcm_data_index / ADPCM_STATE_SAVE_INTERVAL;
  
  ADPCM_SaveSnapshot(&adpcm_buffer, snapshot_idx);
}

/**
  * @brief Seek to ADPCM state at snapshot index
  * @param snapshot_index: Index into adpcm_buffer.states array (0-based)
  */
void seek_to_adpcm_state(uint16_t snapshot_index)
{
  ADPCM_RestoreSnapshot(&adpcm_buffer, snapshot_index);
}

/**
  * @brief Check if touch coordinates are within button bounds
  * @param x, y: Touch coordinates
  * @param btn: Button to check
  * @retval true if touch is within button
  */
bool is_touch_in_button(uint16_t x, uint16_t y, RectButton* btn)
{
  return (x >= btn->x && x < (btn->x + btn->width) &&
          y >= btn->y && y < (btn->y + btn->height));
}

/**
  * @brief Handle button press events
  * @param x, y: Touch coordinates
  */
void handle_button_press(uint16_t x, uint16_t y)
{
  // Check Seek Backward button (<<)
  if (is_touch_in_button(x, y, &btns[0])) {
    seek_backward(SEEK_STEP_FRAMES);
    playing_state = false;  // Pause when seeking
    return;
  }
  
  // Check Play button
  if (is_touch_in_button(x, y, &btns[1])) {
    playing_state = !playing_state;
    return;
  }
  
  // Check Seek Forward button (>>)
  if (is_touch_in_button(x, y, &btns[2])) {
    seek_forward(SEEK_STEP_FRAMES);
    playing_state = false;  // Pause when seeking
    return;
  }
  
  // Check Lyric toggle button
  if (is_touch_in_button(x, y, &btns[3])) {
    lyric_toggle = !lyric_toggle;
    if (!lyric_toggle) {
      clear_lyric_area();
      lyric_displayed[LCD_LAYER_BACK] = NO_EVENT;
    }
    return;
  }
  
  // Check Mute button (reserved for future use)
  if (is_touch_in_button(x, y, &btns[4])) {
    // TODO: Implement mute functionality
    return;
  }
}

/**
  * @brief Draw all UI buttons
  */
void draw_ui_buttons(void)
{
  BSP_LCD_SetTextColor(LCD_COLOR_GRAY);
  for (int i = 0; i < 5; i++) {
    BSP_LCD_DrawRect(btns[i].x, btns[i].y, btns[i].width, btns[i].height);
    BSP_LCD_DisplayStringAt(btns[i].x + 5, btns[i].y + 8, 
                           (uint8_t*)btns[i].text, LEFT_MODE);
  }
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
}

/**
  * @brief Calculate frame offset from frame counter
  * @param frame_num: Target frame number (0-based)
  * @retval Byte offset in video_data_bin
  */
uint32_t calculate_frame_offset(uint16_t frame_num)
{
  uint32_t offset = 0;
  for (uint16_t i = 0; i < frame_num && i < num_frames; i++) {
    offset += frame_sizes[i];
  }
  return offset;
}

/**
  * @brief Seek to specific frame and restore ADPCM state
  * @param target_frame: Target frame number (0-based)
  * @retval true if seek successful, false if out of bounds
  */
bool seek_to_frame(uint16_t target_frame)
{
  if (target_frame >= num_frames) {
    return false;
  }

  /* Delegate ADPCM state restoration to video_seek module */
  if (!video_seek_to_frame(target_frame, &adpcm_buffer)) {
    return false;
  }

  /* Update frame position */
  frame_counter = target_frame;
  frame_offset = calculate_frame_offset(target_frame);
  
  /* Reset lyric position */
  lyric_idx = 0;
  lyric_displayed[LCD_LAYER_BACK] = NO_EVENT;

  return true;
}

/**
  * @brief Seek forward by N frames
  */
void seek_forward(uint16_t frame_delta)
{
  uint16_t target = frame_counter + frame_delta;
  if (target >= num_frames) {
    target = num_frames - 1;
  }
  seek_to_frame(target);
}

/**
  * @brief Seek backward by N frames
  */
void seek_backward(uint16_t frame_delta)
{
  int16_t target = (int16_t)frame_counter - (int16_t)frame_delta;
  if (target < 0) {
    target = 0;
  }
  seek_to_frame((uint16_t)target);
}

/**
  * @brief Draw progress bar and time indicator
  * Shows current position and total duration
  */
void draw_progress_bar(void)
{
  uint16_t bar_y = 230;
  uint16_t bar_x = 10;
  uint16_t bar_width = 220;
  uint16_t bar_height = 10;
  
  /* Calculate progress percentage */
  uint16_t progress_width = (uint32_t)(frame_counter * bar_width) / num_frames;
  
  /* Draw background bar */
  BSP_LCD_SetTextColor(LCD_COLOR_GRAY);
  BSP_LCD_DrawRect(bar_x, bar_y, bar_width, bar_height);
  
  /* Draw progress fill */
  BSP_LCD_SetTextColor(LCD_COLOR_CYAN);
  if (progress_width > 0) {
    BSP_LCD_FillRect(bar_x + 1, bar_y + 1, progress_width - 1, bar_height - 2);
  }
  
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
}

/**
  * @brief Display time information
  * @param x, y: Position to display
  */
void draw_time_display(uint16_t x, uint16_t y)
{
  /* Estimate seconds based on frame number */
  /* Adjust 60 based on your actual frame rate */
  uint16_t current_seconds = frame_counter / 60;
  uint16_t total_seconds = num_frames / 60;
  
  char time_str[32];
  sprintf(time_str, "%02d:%02d / %02d:%02d", 
          current_seconds / 60, current_seconds % 60,
          total_seconds / 60, total_seconds % 60);
  
  BSP_LCD_SetTextColor(LCD_COLOR_YELLOW);
  BSP_LCD_DisplayStringAt(x, y, (uint8_t*)time_str, LEFT_MODE);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM6_Init();
  MX_DAC_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
      Error_Handler();
  }

  // DAC channel 1 fights with the VSYNC signal, so we use only channel 2 for audio output
  if (HAL_DAC_Start(&hdac, DAC_CHANNEL_2) != HAL_OK) {
      Error_Handler();
  }

  BSP_LED_Init(LED3);
  BSP_LED_Init(LED4);
  BSP_LED_On(LED4); /* RED LED active until init is complete */

  BSP_LCD_Init();
  LCD_LAYER_FRONT = 1;
  LCD_LAYER_BACK = 0;
  g_fb[0] = (uint32_t*)LCD_FRAME_BUFFER;
  g_fb[1] = (uint32_t*)(LCD_FRAME_BUFFER + WIDTH * HEIGHT * BPP);
  BSP_LCD_LayerDefaultInit(0, (uint32_t)g_fb[0]);
  BSP_LCD_LayerDefaultInit(1, (uint32_t)g_fb[1]);
  BSP_LCD_SetLayerVisible(0, DISABLE);
  BSP_LCD_SetLayerVisible(1, ENABLE);
  BSP_LCD_SelectLayer(LCD_LAYER_FRONT);
  BSP_LCD_Clear(LCD_COLOR_BLACK);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
  BSP_LCD_SetFont(&Font12);

  BSP_LCD_SelectLayer(LCD_LAYER_BACK);
  BSP_LCD_DisplayOn();
  BSP_LCD_Clear(LCD_COLOR_BLACK);
  BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_SetFont(&Font12);
  
  /* Draw UI buttons */
  draw_ui_buttons();

  /* ChromART (DMA2D) setup */
  /* ---------------------- */
  hdma2d.Init.Mode         = DMA2D_M2M; // convert 8bit palette colors to 32bit ARGB888
  hdma2d.Init.ColorMode    = DMA2D_ARGB8888; // destination color format
  hdma2d.Init.OutputOffset = 0;
  hdma2d.Instance = DMA2D;
  hdma2d.LayerCfg[LCD_LAYER_FRONT].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[LCD_LAYER_FRONT].InputAlpha = 0xFF; // N/A only for A8 or A4
  hdma2d.LayerCfg[LCD_LAYER_FRONT].InputColorMode = DMA2D_INPUT_ARGB8888; // source format
  hdma2d.LayerCfg[LCD_LAYER_FRONT].InputOffset = 0;
  HAL_DMA2D_Init(&hdma2d);
  HAL_DMA2D_ConfigLayer(&hdma2d, LCD_LAYER_FRONT);

  if (BSP_TS_Init(WIDTH, HEIGHT) != TS_OK)
  {
    Error_Handler();
  }

  // HAL_DAC_ConvCpltCallbackCh1(&hdac);
  HAL_DACEx_ConvHalfCpltCallbackCh2(&hdac);

  BSP_LED_Off(LED4); /* init complete, clear RED LED */

  /* Initialize ADPCM buffer */
  ADPCM_InitBuffer(&adpcm_buffer);

  int current_event = 0;

  // Make audio ready
  HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_2, (uint32_t*)buffer_samples, BUFFER_SIZE, DAC_ALIGN_12B_R);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (framebuffer_ready)
    {
        framebuffer_ready = false;

        BSP_TS_GetState(&TS_State);
        if (TS_State.TouchDetected) {
            BSP_LED_On(LED3);
            // Handle touch input
            handle_button_press(TS_State.TouchX, TS_State.TouchY);
        } else {
            BSP_LED_Off(LED3);
        }

        // Pause/play handling
        if (!playing_state) {
            continue;
        }

        const uint8_t* frame_data = &video_data_bin[frame_offset];
        draw_video_frame(frame_data);

        frame_offset += frame_sizes[frame_counter];
        frame_counter++;
        
        /* Save ADPCM state every ADPCM_STATE_SAVE_INTERVAL bytes */
        if (adpcm_data_index % ADPCM_STATE_SAVE_INTERVAL == 0) {
          save_adpcm_state_snapshot();
        }
        
        if (frame_counter >= num_frames)
        {
            frame_counter = 0;
            frame_offset = 0;
            lyric_idx = 0;
            adpcm_data_index = 0;
            adpcm_state_count = 0;
        }

        // Handle lyric display
        current_event = find_active_event(frame_counter);

        if (lyric_toggle && lyric_displayed[LCD_LAYER_BACK] != current_event) {
            // Clear lyric area
            clear_lyric_area();

            if (current_event != NO_EVENT) {
                // Display lyric
                LCD_DisplayWrappedText(LYRICS_X_OFFSET, LYRICS_ROMAJI_Y_OFFSET,
                                     (char*)lyrics_romaji[lyric_events[current_event].lyric_index]);
                LCD_DisplayWrappedText(LYRICS_X_OFFSET, LYRICS_ENGLISH_Y_OFFSET,
                                     (char*)lyrics_english[lyric_events[current_event].lyric_index]);
            }

            lyric_displayed[LCD_LAYER_BACK] = current_event;
        } else if (!lyric_toggle && lyric_displayed[LCD_LAYER_BACK] != NO_EVENT) {
            // Clear lyrics when toggle is off
            clear_lyric_area();
            lyric_displayed[LCD_LAYER_BACK] = NO_EVENT;
        }
        
        /* Draw progress bar and time display */
        draw_progress_bar();
        draw_time_display(10, 215);
        
        screen_flip_buffers(); /* swap backbuffer and frontbuffer */
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT2 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 179;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 124;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void screen_flip_buffers(void)
{
  // wait for VSYNC
    while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS)) {}

    BSP_LCD_SetLayerVisible(LCD_LAYER_FRONT, DISABLE);
    LCD_LAYER_BACK = LCD_LAYER_FRONT;
    LCD_LAYER_FRONT ^= 1;
    BSP_LCD_SetLayerVisible(LCD_LAYER_FRONT, ENABLE);
    BSP_LCD_SelectLayer(LCD_LAYER_BACK);
}

uint8_t find_active_event(uint16_t frame_number)
{
    while (lyric_idx < lyric_count &&
            frame_number >= lyric_events[lyric_idx].end_frame) {
        lyric_idx++;
    }

    if (lyric_idx < lyric_count &&
        frame_number >= lyric_events[lyric_idx].start_frame &&
        frame_number < lyric_events[lyric_idx].end_frame) {
            return lyric_idx;
    }

    return NO_EVENT;
}

void LCD_DisplayWrappedText(uint16_t x, uint16_t y, char *text)
{
    char line[64];
    int line_len = 0;
    int y_offset = 0;

    while (*text)
    {
        int word_len = 0;

        // Measure next word
        while (text[word_len] && text[word_len] != ' ')
            word_len++;

        // If word doesn't fit on current line, flush line
        if (line_len + word_len > MAX_CHARS_PER_LINE)
        {
            line[line_len] = '\0';
            BSP_LCD_DisplayStringAt(x, y + y_offset, (uint8_t*)line, LEFT_MODE);
            y_offset += BSP_LCD_GetFont()->Height;
            line_len = 0;
        }

        // Copy word
        for (int i = 0; i < word_len; i++)
            line[line_len++] = *text++;

        // Skip space
        if (*text == ' ')
        {
            line[line_len++] = ' ';
            text++;
        }
    }

    // Flush last line
    if (line_len > 0)
    {
        line[line_len] = '\0';
        BSP_LCD_DisplayStringAt(x, y + y_offset, (uint8_t*)line, LEFT_MODE);
    }
}

void clear_lyric_area(void)
{
    // Clear the lyric display area
    uint16_t rect_x = LYRICS_X_OFFSET;
    uint16_t rect_y = LYRICS_ROMAJI_Y_OFFSET;
    uint16_t rect_width = LCD_WIDTH_PX - LYRICS_X_OFFSET;
    uint16_t rect_height = 6 * BSP_LCD_GetFont()->Height + 10; // enough for 3 lines
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_FillRect(rect_x, rect_y, rect_width, rect_height);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
}

/**
  * @brief  Draw a video frame from RLE encoded data
  * @param  frame_data: Pointer to RLE encoded frame data
  * @retval None
  */
void draw_video_kframe(const uint8_t* frame_data)
{
  for(int y = 0; y < VIDEO_HEIGHT; y++) {
    int x = 0;
    while(1) {
      uint8_t r = *frame_data++;
      if (r == 0) break; // end of line

      uint8_t color = r >> 7;
      uint8_t run = r & 0x7F;
      uint32_t pix = color ? 0xFF000000u : 0xFFFFFFFFu ; // ARGB8888 (opaque white / black)
      for(int i = 0; i < run && x < VIDEO_WIDTH; i++, x++) {
        g_fb[LCD_LAYER_BACK][(y + SCREEN_Y_OFFSET) * WIDTH + (x + SCREEN_X_OFFSET)] = pix;
      }
    }
  }
}

void copy_video_frame()
{
  for(int y = 0; y < VIDEO_HEIGHT; y++) {
    for(int x = 0; x < VIDEO_WIDTH; x++) {
      g_fb[LCD_LAYER_BACK][(y + SCREEN_Y_OFFSET) * WIDTH + (x + SCREEN_X_OFFSET)] =
        g_fb[LCD_LAYER_FRONT][(y + SCREEN_Y_OFFSET) * WIDTH + (x + SCREEN_X_OFFSET)];
    }
  }
}

void draw_video_iframe(const uint8_t* frame_data)
{
  copy_video_frame();
  uint8_t bitmap[15];
  memcpy(bitmap, frame_data, 15);
  frame_data += 15;
  for(int y = 0; y < VIDEO_HEIGHT; y++) {
    if (bitmap[y >> 3] & (1 << (7 - (y & 7)))) {
      int x = 0;
      while(1) {
        uint8_t r = *frame_data++;
        if (r == 0) break; // end of line

        uint8_t color = r >> 7;
        uint8_t run = r & 0x7F;
        uint32_t pix = color ? 0xFF000000u : 0xFFFFFFFFu ; // ARGB8888 (opaque white / black)
        for(int i = 0; i < run && x < VIDEO_WIDTH; i++, x++) {
          g_fb[LCD_LAYER_BACK][(y + SCREEN_Y_OFFSET) * WIDTH + (x + SCREEN_X_OFFSET)] = pix;
        }
      }
    }
  }
}

void draw_video_frame(const uint8_t* frame_data)
{
  // Detect frame type
  if (frame_data[0] == 0x00) {
    // Keyframe
    draw_video_kframe(frame_data + 1);
  } else if (frame_data[0] == 0x01) {
    // Delta frame - not implemented
    draw_video_iframe(frame_data + 1);
  } else if (frame_data[0] == 0x02) {
    // Duplicate frame
    copy_video_frame();
  }
}

void fill_range(int start, int end)
{
    for (int i = start; i < end; i += 2)
    {
        if (adpcm_data_index < sizeof(adpcm_data))
        {
            uint8_t b = adpcm_data[adpcm_data_index++];
            int16_t s1 = ADPCM_Decode((b >> 4) & 0x0F);
            int16_t s2 = ADPCM_Decode(b & 0x0F);
            buffer_samples[i]     = ((uint16_t)(32768 + s1)) >> 4;
            buffer_samples[i + 1] = ((uint16_t)(32768 + s2)) >> 4;
        }
        else
        {
            buffer_samples[i]     = DAC_MID;
            buffer_samples[i + 1] = DAC_MID;
        }
    }
}

void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef *hdac)
{
    fill_range(0, BUFFER_SIZE);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
