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
#include "cmsis_os.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "task.h"
#include "queue.h"
#include "oled_user.h"
/* USER CODE END Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint8_t event;
  int people;
  uint8_t year, month, date;
  uint8_t hour, min, sec;
} LogItem_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_PEOPLE          5
#define EVENT_IN            1U
#define EVENT_OUT           2U
#define EVENT_DENIED        3U
#define SENSOR_IN           0U
#define SENSOR_OUT          1U
#define DISTANCE_THRESHOLD  20.0f     //超声波距离20cm
#define DEBOUNCE_MS         1000U

/* RTC 用户设定时间：烧录前在这里修改成你要显示的起始时间 */
#define RTC_SET_YEAR        2026U
#define RTC_SET_MONTH       6U
#define RTC_SET_DATE        16U
#define RTC_SET_HOUR        16U
#define RTC_SET_MINUTE      48U
#define RTC_SET_SECOND      0U
#define RTC_SET_WEEKDAY     RTC_WEEKDAY_TUESDAY

/*
 * 1：每次上电/复位都把 RTC 重新设置为上面的时间，适合你现在调试和演示前校准。
 * 0：只有备份寄存器无效时才设置 RTC，适合接了 VBAT 电池后长期走时。
 */
#define RTC_FORCE_SET_ON_BOOT  1U
#define RTC_BKP_MAGIC          0xA55AU

#define LED_ON()      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()     HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)

#define BEEP_ON()     HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET)
#define BEEP_OFF()    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

osThreadId InSensorTaskHandle;
osThreadId OutSensorTaskHandle;
osThreadId PeopleDisplayTaHandle;
osThreadId RTCTaskHandle;
osThreadId TFCardTaskHandle;
osMessageQId EventQueueHandle;
osMutexId peopleMutexHandle;
/* USER CODE BEGIN PV */
static int current_people = 0;
static uint8_t rtc_year  = (uint8_t)(RTC_SET_YEAR - 2000U);
static uint8_t rtc_month = (uint8_t)RTC_SET_MONTH;
static uint8_t rtc_date  = (uint8_t)RTC_SET_DATE;
static uint8_t rtc_hour  = (uint8_t)RTC_SET_HOUR;
static uint8_t rtc_min   = (uint8_t)RTC_SET_MINUTE;
static uint8_t rtc_sec   = (uint8_t)RTC_SET_SECOND;

static osMutexId measureMutexHandle;
static QueueHandle_t xLogQueue = NULL;

static volatile uint8_t ic_state[2] = {0};
static volatile uint16_t ic_rise[2] = {0};
static volatile float distance_cm[2] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
void StartInSensorTask(void const * argument);
void StartOutSensorTask(void const * argument);
void StartPeopleDisplayTask(void const * argument);
void StartRTCTask(void const * argument);
void StartTFCardTask(void const * argument);

/* USER CODE BEGIN PFP */
static float HCSR04_ReadCm(uint8_t sensor);
static void Buzzer_BeepShort(void);
static void Display_Refresh(void);
static void DoorLock_Update(void);
static void RTC_SetUserTime(void);
static void RTC_UpdateCachedTime(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void RTC_SetUserTime(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  sTime.Hours   = (uint8_t)RTC_SET_HOUR;
  sTime.Minutes = (uint8_t)RTC_SET_MINUTE;
  sTime.Seconds = (uint8_t)RTC_SET_SECOND;

  sDate.WeekDay = RTC_SET_WEEKDAY;
  sDate.Year    = (uint8_t)(RTC_SET_YEAR - 2000U);
  sDate.Month   = (uint8_t)RTC_SET_MONTH;
  sDate.Date    = (uint8_t)RTC_SET_DATE;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_MAGIC);
}

static void RTC_UpdateCachedTime(void)
{
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;

  HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
  /* STM32F1 读取 RTC 时间后必须再读取日期，时间寄存器才会正常释放/刷新 */
  HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

  rtc_hour  = t.Hours;
  rtc_min   = t.Minutes;
  rtc_sec   = t.Seconds;
  rtc_year  = d.Year;
  rtc_month = d.Month;
  rtc_date  = d.Date;
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
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(TF_CS_GPIO_Port, TF_CS_Pin, GPIO_PIN_SET);

/* 先把 LED 和蜂鸣器关掉 */
	HAL_GPIO_WritePin(TF_CS_GPIO_Port, TF_CS_Pin, GPIO_PIN_SET);
	LED_OFF();
	BEEP_OFF();
	OLED_Init();
//  Display_Refresh();
  /* USER CODE END 2 */

  /* Create the mutex(es) */
  /* definition and creation of peopleMutex */
  osMutexDef(peopleMutex);
  peopleMutexHandle = osMutexCreate(osMutex(peopleMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  osMutexDef(measureMutex);
  measureMutexHandle = osMutexCreate(osMutex(measureMutex));
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of EventQueue */
  osMessageQDef(EventQueue, 10, 4);
  EventQueueHandle = osMessageCreate(osMessageQ(EventQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  xLogQueue = xQueueCreate(20, sizeof(LogItem_t));
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of InSensorTask */
  osThreadDef(InSensorTask, StartInSensorTask, osPriorityAboveNormal, 0, 256);
  InSensorTaskHandle = osThreadCreate(osThread(InSensorTask), NULL);

  /* definition and creation of OutSensorTask */
  osThreadDef(OutSensorTask, StartOutSensorTask, osPriorityAboveNormal, 0, 256);
  OutSensorTaskHandle = osThreadCreate(osThread(OutSensorTask), NULL);

  /* definition and creation of PeopleDisplayTa */
  osThreadDef(PeopleDisplayTa, StartPeopleDisplayTask, osPriorityNormal, 0, 256);
  PeopleDisplayTaHandle = osThreadCreate(osThread(PeopleDisplayTa), NULL);

  /* definition and creation of RTCTask */
  osThreadDef(RTCTask, StartRTCTask, osPriorityLow, 0, 128);
  RTCTaskHandle = osThreadCreate(osThread(RTCTask), NULL);

  /* definition and creation of TFCardTask */
  osThreadDef(TFCardTask, StartTFCardTask, osPriorityBelowNormal, 0, 1024);
  TFCardTaskHandle = osThreadCreate(osThread(TFCardTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /* USER CODE BEGIN RTC_Init 2 */
#if RTC_FORCE_SET_ON_BOOT
  RTC_SetUserTime();
#else
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != RTC_BKP_MAGIC)
  {
    RTC_SetUserTime();
  }
#endif
  RTC_UpdateCachedTime();
  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 72-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_Pin|BEEP_Pin|TF_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TR1_Pin|TR2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_Pin BEEP_Pin */
  GPIO_InitStruct.Pin = LED_Pin|BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : TF_CS_Pin */
  GPIO_InitStruct.Pin = TF_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(TF_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TR1_Pin TR2_Pin */
  GPIO_InitStruct.Pin = TR1_Pin|TR2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void Buzzer_BeepShort(void)
{
  BEEP_ON();
  osDelay(80);
  BEEP_OFF();
}

static void DoorLock_Update(void)
{
  if (current_people >= MAX_PEOPLE)
	{ LED_ON();  }   // 人数满，门禁锁止，LED亮
  else
  {  LED_OFF();  }  // 人数没满，LED灭
}

static void Display_Refresh(void)
{
  char line[24];
  int people_copy;

  osMutexWait(peopleMutexHandle, osWaitForever);
  people_copy = current_people;
  osMutexRelease(peopleMutexHandle);

  OLED_Clear();



	sprintf(line, "People: %d / Max: %d", people_copy, MAX_PEOPLE);
	OLED_ShowString(10, 0, line);

  /* Second line: time */
  sprintf(line, "%02d:%02d:%02d", rtc_hour, rtc_min, rtc_sec);
  OLED_ShowString2x(15, 30, line);

  OLED_Refresh();
}

static void HCSR04_Trig(uint8_t sensor)
{
  GPIO_TypeDef *port = (sensor == SENSOR_IN) ? TR1_GPIO_Port : TR2_GPIO_Port;
  uint16_t pin = (sensor == SENSOR_IN) ? TR1_Pin : TR2_Pin;

  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  taskENTER_CRITICAL();
  for (volatile uint32_t i = 0; i < 80; i++);
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
  for (volatile uint32_t i = 0; i < 900; i++); /* about 10 us at 72 MHz, rough delay */
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  taskEXIT_CRITICAL();
}

static float HCSR04_ReadCm(uint8_t sensor)
{
  uint32_t channel = (sensor == SENSOR_IN) ? TIM_CHANNEL_1 : TIM_CHANNEL_2;
  float ret = -1.0f;

  osMutexWait(measureMutexHandle, osWaitForever);

  while (ulTaskNotifyTake(pdTRUE, 0) != 0) {}

  ic_state[sensor] = 0;
  distance_cm[sensor] = -1.0f;
  __HAL_TIM_SET_COUNTER(&htim3, 0);
  __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, channel, TIM_INPUTCHANNELPOLARITY_RISING);
  HAL_TIM_IC_Start_IT(&htim3, channel);

  HCSR04_Trig(sensor);

  if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60)) > 0) {
    ret = distance_cm[sensor];
  }

  HAL_TIM_IC_Stop_IT(&htim3, channel);
  osMutexRelease(measureMutexHandle);
  return ret;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (htim->Instance != TIM3) return;

  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
    if (ic_state[SENSOR_IN] == 0) {
      ic_rise[SENSOR_IN] = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
      ic_state[SENSOR_IN] = 1;
      __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);
    } else {
      uint16_t fall = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
      uint16_t diff = (fall >= ic_rise[SENSOR_IN]) ? (fall - ic_rise[SENSOR_IN]) : (65535 - ic_rise[SENSOR_IN] + fall + 1);
      distance_cm[SENSOR_IN] = diff * 0.0343f / 2.0f;
      ic_state[SENSOR_IN] = 0;
      __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
      vTaskNotifyGiveFromISR((TaskHandle_t)InSensorTaskHandle, &xHigherPriorityTaskWoken);
    }
  }

  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
    if (ic_state[SENSOR_OUT] == 0) {
      ic_rise[SENSOR_OUT] = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
      ic_state[SENSOR_OUT] = 1;
      __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
    } else {
      uint16_t fall = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
      uint16_t diff = (fall >= ic_rise[SENSOR_OUT]) ? (fall - ic_rise[SENSOR_OUT]) : (65535 - ic_rise[SENSOR_OUT] + fall + 1);
      distance_cm[SENSOR_OUT] = diff * 0.0343f / 2.0f;
      ic_state[SENSOR_OUT] = 0;
      __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
      vTaskNotifyGiveFromISR((TaskHandle_t)OutSensorTaskHandle, &xHigherPriorityTaskWoken);
    }
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartInSensorTask */
/**
  * @brief  Function implementing the InSensorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartInSensorTask */
void StartInSensorTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  uint32_t last_valid_tick = 0;
  for(;;)
  {
    float d = HCSR04_ReadCm(SENSOR_IN);
    uint32_t now = osKernelSysTick();

    if (d > 2.0f && d < DISTANCE_THRESHOLD && (now - last_valid_tick) >= DEBOUNCE_MS) {
      last_valid_tick = now;

      osMutexWait(peopleMutexHandle, osWaitForever);
      if (current_people >= MAX_PEOPLE) {
        DoorLock_Update();
        osMutexRelease(peopleMutexHandle);
        Buzzer_BeepShort();
      } else {
        osMutexRelease(peopleMutexHandle);
        osMessagePut(EventQueueHandle, EVENT_IN, 0);
      }
    }
    osDelay(120);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartOutSensorTask */
/**
* @brief Function implementing the OutSensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartOutSensorTask */
void StartOutSensorTask(void const * argument)
{
  /* USER CODE BEGIN StartOutSensorTask */
  uint32_t last_valid_tick = 0;
  for(;;)
  {
    float d = HCSR04_ReadCm(SENSOR_OUT);
    uint32_t now = osKernelSysTick();

    if (d > 2.0f && d < DISTANCE_THRESHOLD && (now - last_valid_tick) >= DEBOUNCE_MS) {
      last_valid_tick = now;
      osMessagePut(EventQueueHandle, EVENT_OUT, 0);
    }
    osDelay(120);
  }
  /* USER CODE END StartOutSensorTask */
}

/* USER CODE BEGIN Header_StartPeopleDisplayTask */
/**
* @brief Function implementing the PeopleDisplayTa thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPeopleDisplayTask */
void StartPeopleDisplayTask(void const * argument)
{
  /* USER CODE BEGIN StartPeopleDisplayTask */
  osEvent evt;
  LogItem_t log;

  for(;;)
  {
    evt = osMessageGet(EventQueueHandle, 200);
    if (evt.status == osEventMessage) {
      uint32_t type = evt.value.v;
      int people_after;

      osMutexWait(peopleMutexHandle, osWaitForever);
      if (type == EVENT_IN) {
        if (current_people < MAX_PEOPLE) current_people++;
      } else if (type == EVENT_OUT) {
        if (current_people > 0) current_people--;
      }
      people_after = current_people;
      DoorLock_Update();
      osMutexRelease(peopleMutexHandle);

      log.event = (uint8_t)type;
      log.people = people_after;
      log.year = rtc_year;
      log.month = rtc_month;
      log.date = rtc_date;
      log.hour = rtc_hour;
      log.min = rtc_min;
      log.sec = rtc_sec;
      if (xLogQueue != NULL) xQueueSend(xLogQueue, &log, 0);
    }

    Display_Refresh();
  }
  /* USER CODE END StartPeopleDisplayTask */
}

/* USER CODE BEGIN Header_StartRTCTask */
/**
* @brief Function implementing the RTCTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartRTCTask */
void StartRTCTask(void const * argument)
{
  /* USER CODE BEGIN StartRTCTask */
  for(;;)
  {
    RTC_UpdateCachedTime();
    osDelay(1000);
  }
  /* USER CODE END StartRTCTask */
}

/* USER CODE BEGIN Header_StartTFCardTask */
/**
* @brief Function implementing the TFCardTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTFCardTask */
void StartTFCardTask(void const * argument)
{
  /* USER CODE BEGIN StartTFCardTask */
  FRESULT res;
  UINT bw;
  char line[128];

  /* 等待 SPI、RTC、OLED 等外设和其他任务稳定 */
  osDelay(1000);

  /* 上电先尝试创建 boot.txt。只要 SD 卡能写，这个文件一定会出现，方便判断 SD 是否正常。 */
  res = f_mount(&USERFatFS, USERPath, 1);
  if (res == FR_OK)
  {
    res = f_open(&USERFile, "boot.txt", FA_OPEN_ALWAYS | FA_WRITE);
    if (res == FR_OK)
    {
      f_lseek(&USERFile, f_size(&USERFile));
      sprintf(line, "System boot, SD OK, %02d:%02d:%02d\r\n", rtc_hour, rtc_min, rtc_sec);
      f_write(&USERFile, line, strlen(line), &bw);
      f_sync(&USERFile);
      f_close(&USERFile);
    }
  }

  for(;;)
  {
    LogItem_t item;
    int people_copy;

    /* 每 5 秒采集/保存一次当前人数和时间，即使没有进出事件也会写入。 */
    osDelay(5000);
    RTC_UpdateCachedTime();

    res = f_mount(&USERFatFS, USERPath, 1);
    if (res == FR_OK)
    {
      res = f_open(&USERFile, "log.csv", FA_OPEN_ALWAYS | FA_WRITE);
      if (res == FR_OK)
      {
        f_lseek(&USERFile, f_size(&USERFile));

        /* 先把这 5 秒内的 IN/OUT 事件全部写进去 */
        while (xLogQueue != NULL && xQueueReceive(xLogQueue, &item, 0) == pdTRUE)
        {
          const char *etype = (item.event == EVENT_IN) ? "IN" :
                              (item.event == EVENT_OUT) ? "OUT" : "DENIED";
          sprintf(line, "20%02d-%02d-%02d %02d:%02d:%02d, %s, People=%d\r\n",
                  item.year, item.month, item.date,
                  item.hour, item.min, item.sec,
                  etype, item.people);
          f_write(&USERFile, line, strlen(line), &bw);
        }

        /* 再每 5 秒固定记录一次当前状态，便于确认 TF 卡确实在工作 */
        osMutexWait(peopleMutexHandle, osWaitForever);
        people_copy = current_people;
        osMutexRelease(peopleMutexHandle);

        sprintf(line, "20%02d-%02d-%02d %02d:%02d:%02d, STAT, People=%d\r\n",
                rtc_year, rtc_month, rtc_date,
                rtc_hour, rtc_min, rtc_sec,
                people_copy);
        f_write(&USERFile, line, strlen(line), &bw);

        /* 立即同步到卡里，避免断电或拔卡时数据还在缓存里 */
        f_sync(&USERFile);
        f_close(&USERFile);
      }
    }
  }
  /* USER CODE END StartTFCardTask */
}

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
