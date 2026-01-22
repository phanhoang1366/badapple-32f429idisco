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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "stm32f429i_discovery_ts.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f4xx.h"

#include "frame_sizes.h"
#include "stm32f4xx_hal_dac.h"

static const
#include "video_data.h"

static const
#include "audio_data.h"

static const
#include "lyrics.h"

#include "adpcm_states.h"
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

UART_HandleTypeDef huart1;

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

RectButton btns[3] = {
	{10, 250, 60, 30, "Play"},
	{80, 250, 60, 30, "Lyric"},
	{150, 250, 60, 30, "Mute"}
};

uint32_t last_ms = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM6_Init(void);
static void MX_DAC_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void screen_flip_buffers(void);
uint8_t find_active_event(uint16_t frame_number);
void draw_video_frame(const uint8_t* frame_data);
void LCD_DisplayWrappedText(uint16_t x, uint16_t y, char *text);
void clear_lyric_area(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
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
  last_ms = HAL_GetTick();

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
        if (frame_counter >= num_frames)
        {
            // frame_counter = 0;
            // frame_offset = 0;
            // lyric_idx = 0;
            // Let's not loop for this demo
            playing_state = false;
            continue;
        }

        // Handle lyric display
        current_event = find_active_event(frame_counter);

        if (lyric_displayed[LCD_LAYER_BACK] != current_event) {
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
        }
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
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pins : PE2 PE3 PE4 PE5 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

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

    if (lyric_toggle) {
        return NO_EVENT;
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

void fill_blank_range(int start, int end)
{
    for (int i = start; i < end; i++)
    {
        buffer_samples[i] = DAC_MID;
    }
}

extern int16_t a_index;
extern int32_t predsample;

void print_debugging_info() 
{
  uint32_t current_ms = HAL_GetTick();
  if (current_ms - last_ms >= 1000)
  {
    char msg[100];
    int len = snprintf(msg, sizeof(msg), "ADPCM index: %d, PredSample: %ld\r\n", a_index, predsample);
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, len, HAL_MAX_DELAY);
    last_ms = current_ms;
  }
}

void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef *hdac)
{
  if (!playing_state) fill_blank_range(0, BUFFER_SIZE);
  else 
  {
    fill_range(0, BUFFER_SIZE);
    // print_debugging_info();
  }
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
