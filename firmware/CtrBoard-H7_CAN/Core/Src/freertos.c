/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ws2812.h"
#include "motor.h"
#include "fdcan.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId LiveTestHandle;
uint32_t LiveTestBuffer[ 128 ];
osStaticThreadDef_t LiveTestControlBlock;
osThreadId ControlTaskHandle;
uint32_t ControlTaskBuffer[ 256 ];
osStaticThreadDef_t ControlTaskControlBlock;
osSemaphoreId MotorInitDone_SemHandle;
osStaticSemaphoreDef_t MotorInitDoneControlBlock;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void LiveTest_Entry(void const * argument);
void ControlTask_Entry(void const * argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* GetTimerTaskMemory prototype (linked to static allocation support) */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
  /* place for user code */
}
/* USER CODE END GET_TIMER_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of MotorInitDone_Sem */
  osSemaphoreStaticDef(MotorInitDone_Sem, &MotorInitDoneControlBlock);
  MotorInitDone_SemHandle = osSemaphoreCreate(osSemaphore(MotorInitDone_Sem), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of LiveTest */
  osThreadStaticDef(LiveTest, LiveTest_Entry, osPriorityNormal, 0, 128, LiveTestBuffer, &LiveTestControlBlock);
  LiveTestHandle = osThreadCreate(osThread(LiveTest), NULL);

  /* definition and creation of ControlTask */
  osThreadStaticDef(ControlTask, ControlTask_Entry, osPriorityHigh, 0, 256, ControlTaskBuffer, &ControlTaskControlBlock);
  ControlTaskHandle = osThreadCreate(osThread(ControlTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_LiveTest_Entry */
/**
* @brief Function implementing the LiveTest thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LiveTest_Entry */
__weak void LiveTest_Entry(void const * argument)
{
  /* USER CODE BEGIN LiveTest_Entry */
	uint8_t live_tic = 0;
  if (MotorInitDone_SemHandle != NULL)
  {
    if (osSemaphoreWait(MotorInitDone_SemHandle, osWaitForever) == osOK)
    {
      /* Semaphore obtained, proceed with the task */
      for(;;)
      {
        WS2812_Ctrl(0, live_tic*255, 0);
        live_tic=!live_tic;
        osDelay(300);
      }
    }
  }
  /* Infinite loop */
  for(;;)
  {    
    osDelay(1);
  }
  /* USER CODE END LiveTest_Entry */
}

/* USER CODE BEGIN Header_ControlTask_Entry */
/**
* @brief Function implementing the ControlTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ControlTask_Entry */
__weak void ControlTask_Entry(void const * argument)
{
  /* USER CODE BEGIN ControlTask_Entry */
	WS2812_Ctrl(255, 0, 0);
	osDelay(1000);
  uint8_t ret;
  const float home_dir[4] = {1.0f, -1.0f, -1.0f, 1.0f}; // 每个电机归零方向: +1/-1
  const float home_offset_deg[4] = {-190, 192, 192, -192}; // 每个电机二次记零前旋转角度N(度)
  const float pos_step_abs = 0.012f; // 1rad/s * 1ms
  const float tor_th = 0.1f;
  const float pos_resp_vel = 500.0f;
  const uint16_t settle_ms = 1000; // 二次记零前到位等待时间
	
  ret = dm_motor_multi_home_and_rezero(&hfdcan2, 4, home_dir, home_offset_deg, pos_step_abs, tor_th, pos_resp_vel, settle_ms);
  if (ret != 0){
    while(1){
      osDelay(1);
    }
  }

  osSemaphoreRelease(MotorInitDone_SemHandle);
	WS2812_Ctrl(0, 255, 0);
  osDelay(10);

  /* Infinite loop */
  for(;;)
  {
		osDelay(1);
  }
  /* USER CODE END ControlTask_Entry */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
