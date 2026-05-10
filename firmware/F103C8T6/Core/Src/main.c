/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  ST_OFF_IDLE = 0,
  ST_OFF_LEVEL,
  ST_OFF_ANIM,
  ST_ON_IDLE,
  ST_ON_BLIP,
  ST_ON_LEVEL,
  ST_ON_ANIM,
} fsm_state_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BTN_PORT          GPIOB
#define BTN_PIN           GPIO_PIN_13
#define IND_LED_PORT      GPIOB
#define IND_LED_PIN       GPIO_PIN_12
#define BAT_OUT_PORT      GPIOB
#define BAT_OUT_PIN       GPIO_PIN_9

#define LED1_PORT         GPIOB
#define LED1_PIN          GPIO_PIN_10
#define LED2_PORT         GPIOA
#define LED2_PIN          GPIO_PIN_7
#define LED3_PORT         GPIOA
#define LED3_PIN          GPIO_PIN_4
#define LED4_PORT         GPIOA
#define LED4_PIN          GPIO_PIN_1

#define DEBOUNCE_MS       20U
#define WINDOW_MS         4000U
#define LONG_PRESS_MS     1200U
#define BLINK_HALF_MS     250U      /* 2Hz: 250ms on / 250ms off */
#define ON_BLIP_MS        200U      /* ON 短按瞬间全灭时长 */
#define ANIM_STEP_MS      300U      /* 2s 动画分 4 步 */

#define ADC_BUF_LEN       32U
#define V_REF_MV          3300U     /* ADC 满量程 */
#define V_BAT0_MV         2310U     /* 0% 对应分压 2.61V */
#define V_BAT100_MV       3000U     /* 100% 对应分压 3.30V */

#define LOOP_TICK_MS      5U        /* 主循环节流周期 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
/* ADC DMA 循环采样缓冲 */
static volatile uint16_t adc_buf[ADC_BUF_LEN];

/* 按键去抖与边沿事件 */
static uint8_t  btn_stable      = 0;          /* 去抖后稳定电平 0/1 */
static uint8_t  btn_raw_last    = 0;
static uint32_t btn_change_ms   = 0;
static uint8_t  btn_consume     = 0;          /* 切换后忽略当前按住 */
static uint8_t  ev_pressed      = 0;          /* 本轮按下边沿 */
static uint8_t  ev_released     = 0;          /* 本轮释放边沿 */

/* 状态机 */
static fsm_state_t fsm           = ST_OFF_IDLE;
static uint32_t window_start_ms  = 0;          /* LEVEL 窗口起点 */
static uint32_t anim_start_ms    = 0;          /* ANIM 起点 */
static uint32_t blip_start_ms    = 0;          /* ON_BLIP 起点 */

/* 主循环节流 */
static uint32_t last_tick_ms     = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static void     update_button(uint32_t now);
static uint16_t adc_average_mv(void);
static int16_t  battery_percent(void);
static void     compute_level_pattern(uint8_t pat[4]);
static void     apply_leds(const uint8_t pat[4], uint8_t blink_on);
static void     render_off_anim(uint32_t elapsed, uint8_t pat[4]);
static void     render_on_anim(uint32_t elapsed, uint8_t pat[4]);
static void     fsm_step(uint32_t now);
static void     render_leds(uint32_t now);
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
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_LEN);

  /* 启动即关闭电池输出，确保上电安全 */
  HAL_GPIO_WritePin(BAT_OUT_PORT, BAT_OUT_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(IND_LED_PORT, IND_LED_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED4_PORT, LED4_PIN, GPIO_PIN_RESET);

  fsm           = ST_OFF_IDLE;
  last_tick_ms  = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_tick_ms) >= LOOP_TICK_MS)
    {
      last_tick_ms = now;
      update_button(now);
      fsm_step(now);
      render_leds(now);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 80-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 10000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA4 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB12 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB13 PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ------------------------------------------------------------------
 * 按键去抖与边沿检测：每个主循环 tick 调用一次
 * ------------------------------------------------------------------ */
static void update_button(uint32_t now)
{
  ev_pressed  = 0;
  ev_released = 0;

  uint8_t raw = (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_SET) ? 1U : 0U;

  if (raw != btn_raw_last)
  {
    btn_raw_last  = raw;
    btn_change_ms = now;
  }

  if (raw != btn_stable && (uint32_t)(now - btn_change_ms) >= DEBOUNCE_MS)
  {
    btn_stable = raw;
    if (raw)
    {
      ev_pressed = 1;
    }
    else
    {
      ev_released = 1;
    }
  }
}

/* ------------------------------------------------------------------
 * ADC 取平均并换算成 mV
 * ------------------------------------------------------------------ */
static uint16_t adc_average_mv(void)
{
  uint32_t sum = 0;
  for (uint32_t i = 0; i < ADC_BUF_LEN; ++i)
  {
    sum += adc_buf[i];
  }
  uint32_t avg = sum / ADC_BUF_LEN;        /* 0..4095 */
  return (uint16_t)((avg * V_REF_MV) / 4095U);
}

/* ------------------------------------------------------------------
 * 电压百分比：2.61V=0%，3.30V=100%，线性
 * ------------------------------------------------------------------ */
static int16_t battery_percent(void)
{
  uint16_t mv = adc_average_mv();
  if (mv <= V_BAT0_MV)
  {
    return 0;
  }
  if (mv >= V_BAT100_MV)
  {
    return 100;
  }
  return (int16_t)(((uint32_t)(mv - V_BAT0_MV) * 100U) / (V_BAT100_MV - V_BAT0_MV));
}

/* ------------------------------------------------------------------
 * 把百分比映射为 4 路 LED 模式（0=灭 1=亮 2=2Hz 闪）
 *
 *   每 12.5% 一个分段（band n = (2*p)/25, n in 0..7）：
 *     n=0  [0,  12.5)  -> blink LED1
 *     n=1  [12.5,25)   -> solid LED1
 *     n=2  [25,  37.5) -> solid LED1, blink LED2
 *     ...
 *     n=7  [87.5,100)  -> all solid
 *   p == 0   单独处理：全灭
 *   p == 100 单独处理：全亮
 * ------------------------------------------------------------------ */
static void compute_level_pattern(uint8_t pat[4])
{
  pat[0] = pat[1] = pat[2] = pat[3] = 0;

  int16_t p = battery_percent();
  if (p <= 0)
  {
    return;
  }
  if (p >= 100)
  {
    pat[0] = pat[1] = pat[2] = pat[3] = 1;
    return;
  }

  int n          = (2 * p) / 25;          /* 0..7 */
  int full_solid = (n + 1) / 2;           /* 0..4 */
  int blink_at   = ((n & 1) == 0) ? 1 : 0;

  for (int i = 0; i < full_solid && i < 4; ++i)
  {
    pat[i] = 1;
  }
  if (blink_at && full_solid < 4)
  {
    pat[full_solid] = 2;
  }
}

/* ------------------------------------------------------------------
 * 把 pattern 写到 GPIO；blink_on 为闪烁全局相位（0/1）
 * ------------------------------------------------------------------ */
static void apply_leds(const uint8_t pat[4], uint8_t blink_on)
{
  GPIO_PinState s[4];
  for (int i = 0; i < 4; ++i)
  {
    if (pat[i] == 0U)
    {
      s[i] = GPIO_PIN_RESET;
    }
    else if (pat[i] == 1U)
    {
      s[i] = GPIO_PIN_SET;
    }
    else
    {
      s[i] = blink_on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }
  }
  HAL_GPIO_WritePin(LED1_PORT, LED1_PIN, s[0]);
  HAL_GPIO_WritePin(LED2_PORT, LED2_PIN, s[1]);
  HAL_GPIO_WritePin(LED3_PORT, LED3_PIN, s[2]);
  HAL_GPIO_WritePin(LED4_PORT, LED4_PIN, s[3]);
}

/* ------------------------------------------------------------------
 * OFF -> ON 动画（LED1~4 先全灭再依次点亮）
 * ------------------------------------------------------------------ */
static void render_off_anim(uint32_t elapsed, uint8_t pat[4])
{
  pat[0] = pat[1] = pat[2] = pat[3] = 0;
  if (elapsed >= ANIM_STEP_MS * 4U)
  {
    pat[0] = pat[1] = pat[2] = pat[3] = 1;
  }
  else if (elapsed >= ANIM_STEP_MS * 3U)
  {
    pat[0] = pat[1] = pat[2] = 1;
  }
  else if (elapsed >= ANIM_STEP_MS * 2U)
  {
    pat[0] = pat[1] = 1;
  }
  else if (elapsed >= ANIM_STEP_MS)
  {
    pat[0] = 1;
  }
}

/* ------------------------------------------------------------------
 * ON -> OFF 动画（LED4~1 先全亮再依次熄灭）
 * ------------------------------------------------------------------ */
static void render_on_anim(uint32_t elapsed, uint8_t pat[4])
{
  pat[0] = pat[1] = pat[2] = pat[3] = 1;
  if (elapsed >= ANIM_STEP_MS * 4U)
  {
    pat[0] = pat[1] = pat[2] = pat[3] = 0;
  }
  else if (elapsed >= ANIM_STEP_MS * 3U)
  {
    pat[1] = pat[2] = pat[3] = 0;
    pat[0] = 1;
  }
  else if (elapsed >= ANIM_STEP_MS * 2U)
  {
    pat[2] = pat[3] = 0;
    pat[0] = pat[1] = 1;
  }
  else if (elapsed >= ANIM_STEP_MS)
  {
    pat[3] = 0;
    pat[0] = pat[1] = pat[2] = 1;
  }
}

/* ------------------------------------------------------------------
 * 状态机推进：消费 ev_pressed / ev_released
 * ------------------------------------------------------------------ */
static void fsm_step(uint32_t now)
{
  /* 切换瞬间按键仍按住：吃掉这一次释放，不当作新一次短按 */
  if (ev_released && btn_consume)
  {
    btn_consume  = 0;
    ev_released  = 0;
  }

  switch (fsm)
  {
    case ST_OFF_IDLE:
      /* 必须先释放（短按）才进入 LEVEL：单纯按住不算 */
      if (ev_released)
      {
        fsm             = ST_OFF_LEVEL;
        window_start_ms = now;
      }
      break;

    case ST_OFF_LEVEL:
      if (ev_pressed)
      {
        fsm           = ST_OFF_ANIM;
        anim_start_ms = now;
      }
      else if ((uint32_t)(now - window_start_ms) >= WINDOW_MS)
      {
        fsm = ST_OFF_IDLE;
      }
      break;

    case ST_OFF_ANIM:
      if (btn_stable && (uint32_t)(now - anim_start_ms) >= LONG_PRESS_MS)
      {
        HAL_GPIO_WritePin(BAT_OUT_PORT, BAT_OUT_PIN, GPIO_PIN_SET);
        fsm         = ST_ON_IDLE;
        btn_consume = 1;        /* 等用户释放，期间不再触发短按 */
      }
      else if (ev_released)
      {
        /* 未到 2s 提前释放：回到 LEVEL，沿用原窗口剩余时间 */
        fsm = ST_OFF_LEVEL;
      }
      break;

    case ST_ON_IDLE:
      if (ev_released)
      {
        fsm            = ST_ON_BLIP;
        blip_start_ms  = now;
      }
      break;

    case ST_ON_BLIP:
      if ((uint32_t)(now - blip_start_ms) >= ON_BLIP_MS)
      {
        fsm             = ST_ON_LEVEL;
        window_start_ms = now;
      }
      break;

    case ST_ON_LEVEL:
      if (ev_pressed)
      {
        fsm           = ST_ON_ANIM;
        anim_start_ms = now;
      }
      else if ((uint32_t)(now - window_start_ms) >= WINDOW_MS)
      {
        fsm = ST_ON_IDLE;
      }
      break;

    case ST_ON_ANIM:
      if (btn_stable && (uint32_t)(now - anim_start_ms) >= LONG_PRESS_MS)
      {
        HAL_GPIO_WritePin(BAT_OUT_PORT, BAT_OUT_PIN, GPIO_PIN_RESET);
        fsm         = ST_OFF_IDLE;
        btn_consume = 1;
      }
      else if (ev_released)
      {
        fsm = ST_ON_LEVEL;
      }
      break;

    default:
      fsm = ST_OFF_IDLE;
      break;
  }

  ev_pressed  = 0;
  ev_released = 0;
}

/* ------------------------------------------------------------------
 * 根据当前状态刷新所有 LED（PB12 + LED1~4）
 * ------------------------------------------------------------------ */
static void render_leds(uint32_t now)
{
  uint8_t pat[4]   = {0, 0, 0, 0};
  uint8_t blink_on = (((now / BLINK_HALF_MS) & 1U) == 0U) ? 1U : 0U;
  uint8_t indicator;

  switch (fsm)
  {
    case ST_OFF_IDLE:
      /* 平时熄灭；按下时随之点亮，提供按键反馈 */
      indicator = btn_stable;
      break;

    case ST_OFF_LEVEL:
      indicator = 1;
      compute_level_pattern(pat);
      break;

    case ST_OFF_ANIM:
      indicator = 1;
      render_off_anim((uint32_t)(now - anim_start_ms), pat);
      break;

    case ST_ON_IDLE:
      indicator = 1;
      compute_level_pattern(pat);
      break;

    case ST_ON_BLIP:
      indicator = 1;
      /* pat 保持全 0，达到“短按熄灭所有 LED 再恢复”的瞬间效果 */
      break;

    case ST_ON_LEVEL:
      indicator = 1;
      compute_level_pattern(pat);
      break;

    case ST_ON_ANIM:
      indicator = 1;
      render_on_anim((uint32_t)(now - anim_start_ms), pat);
      break;

    default:
      indicator = 0;
      break;
  }

  HAL_GPIO_WritePin(IND_LED_PORT, IND_LED_PIN,
                    indicator ? GPIO_PIN_SET : GPIO_PIN_RESET);
  apply_leds(pat, blink_on);
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

#ifdef  USE_FULL_ASSERT
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
