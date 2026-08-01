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
#include <string.h>
#include "task.h"
#include "oled_user.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint8_t event;
  int32_t people;
  uint8_t year;
  uint8_t month;
  uint8_t date;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} LogItem_t;

typedef struct
{
  uint8_t year;
  uint8_t month;
  uint8_t date;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t weekday;
} RtcSnapshot_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_PEOPLE                 6

#define EVENT_IN                   1U
#define EVENT_OUT                  2U
#define EVENT_DENIED               3U

#define SENSOR_IN                  0U
#define SENSOR_OUT                 1U
#define SENSOR_ECHO_READY_FLAG     0x00000001U
#define DISTANCE_MIN_CM            2.0f
#define DISTANCE_THRESHOLD_CM      10.0f
#define SENSOR_SAMPLE_PERIOD_MS    120U
#define SENSOR_RELEASE_SAMPLES     2U
#define EVENT_COOLDOWN_MS          600U

/* Change these values before the first programming if another start time is needed. */
#define RTC_SET_YEAR               2026U
#define RTC_SET_MONTH              8U
#define RTC_SET_DATE               1U
#define RTC_SET_HOUR               11U
#define RTC_SET_MINUTE             50U
#define RTC_SET_SECOND             0U
#define RTC_SET_WEEKDAY            RTC_WEEKDAY_SATURDAY
/* Change this value after editing RTC_SET_* to apply the new time once. */
#define RTC_BKP_MAGIC              0xA55BU
#define RTC_BKP_DATE_REGISTER      RTC_BKP_DR2
#define RTC_BKP_WEEKDAY_REGISTER   RTC_BKP_DR3

#define LOG_BUFFER_LENGTH          16U
#define TF_RETRY_PERIOD_MS         5000U
/* Keep this at 0 until the TF module is connected. Change it to 1 to enable logging. */
#define TF_CARD_ENABLED            0U

#define LED_ON()    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()   HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)
#define BEEP_ON()   HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET)
#define BEEP_OFF()  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

/* Definitions for InSensorTask */
osThreadId_t InSensorTaskHandle;
const osThreadAttr_t InSensorTask_attributes = {
  .name = "InSensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for OutSensorTask */
osThreadId_t OutSensorTaskHandle;
const osThreadAttr_t OutSensorTask_attributes = {
  .name = "OutSensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for PeopleDisplayTa */
osThreadId_t PeopleDisplayTaHandle;
const osThreadAttr_t PeopleDisplayTa_attributes = {
  .name = "PeopleDisplayTa",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for RTCTask */
osThreadId_t RTCTaskHandle;
const osThreadAttr_t RTCTask_attributes = {
  .name = "RTCTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for TFCardTask */
osThreadId_t TFCardTaskHandle;
const osThreadAttr_t TFCardTask_attributes = {
  .name = "TFCardTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for EventQueue */
osMessageQueueId_t EventQueueHandle;
const osMessageQueueAttr_t EventQueue_attributes = {
  .name = "EventQueue"
};
/* Definitions for peopleMutex */
osMutexId_t peopleMutexHandle;
const osMutexAttr_t peopleMutex_attributes = {
  .name = "peopleMutex"
};
/* Definitions for measureMutex */
osMutexId_t measureMutexHandle;
const osMutexAttr_t measureMutex_attributes = {
  .name = "measureMutex"
};
/* USER CODE BEGIN PV */
static int32_t current_people = 0;

static volatile uint8_t ic_state[2] = {0U, 0U};
static volatile uint16_t ic_rise[2] = {0U, 0U};
static volatile float distance_cm[2] = {-1.0f, -1.0f};

static RtcSnapshot_t rtc_snapshot = {
  (uint8_t)(RTC_SET_YEAR - 2000U),
  (uint8_t)RTC_SET_MONTH,
  (uint8_t)RTC_SET_DATE,
  (uint8_t)RTC_SET_HOUR,
  (uint8_t)RTC_SET_MINUTE,
  (uint8_t)RTC_SET_SECOND,
  (uint8_t)RTC_SET_WEEKDAY
};
static volatile uint32_t rtc_fat_timestamp = 0U;

#if TF_CARD_ENABLED != 0U
static LogItem_t log_buffer[LOG_BUFFER_LENGTH];
static uint8_t log_head = 0U;
static uint8_t log_tail = 0U;
static uint8_t log_count = 0U;
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C2_Init(void);
void StartInSensorTask(void *argument);
void StartOutSensorTask(void *argument);
void StartPeopleDisplayTask(void *argument);
void StartRTCTask(void *argument);
void StartTFCardTask(void *argument);

/* USER CODE BEGIN PFP */
static void RTC_SetUserTime(void);
static void RTC_SaveDateToBackup(const RTC_DateTypeDef *date);
static uint8_t RTC_RestoreDateFromBackup(void);
static void RTC_UpdateCachedTime(void);
static void RTC_GetCachedTime(RtcSnapshot_t *snapshot);
static uint32_t RTC_PackFatTime(const RtcSnapshot_t *snapshot);
static const char *RTC_GetWeekdayText(uint8_t weekday);
#if TF_CARD_ENABLED != 0U
static void LogBuffer_Push(const LogItem_t *item);
static uint8_t LogBuffer_Pop(LogItem_t *item);
static char *Text_Append(char *destination, const char *source);
static char *Text_AppendPeople(char *destination, int32_t people);
static uint16_t FormatLogLine(char *line, const RtcSnapshot_t *rtc,
                              const char *event_text, int32_t people);
#endif
static char *Text_AppendTwoDigits(char *destination, uint8_t value);
static void Buzzer_BeepShort(void);
static void DoorLock_Update(int32_t people);
static void Display_Refresh(void);
static void HCSR04_Trig(uint8_t sensor);
static float HCSR04_ReadCm(uint8_t sensor);
static void ProcessSensorSample(uint8_t sensor, uint32_t event,
                                uint8_t *occupied, uint8_t *release_count,
                                uint32_t *last_event_tick);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t RTC_PackFatTime(const RtcSnapshot_t *snapshot)
{
  uint32_t full_year = 2000U + snapshot->year;

  if (full_year < 1980U)
  {
    full_year = 1980U;
  }
  if (full_year > 2107U)
  {
    full_year = 2107U;
  }

  return ((full_year - 1980U) << 25)
       | ((uint32_t)snapshot->month << 21)
       | ((uint32_t)snapshot->date << 16)
       | ((uint32_t)snapshot->hour << 11)
       | ((uint32_t)snapshot->minute << 5)
       | ((uint32_t)snapshot->second >> 1);
}

static void RTC_SaveDateToBackup(const RTC_DateTypeDef *date)
{
  uint32_t packed_date = ((uint32_t)date->Year << 9)
                       | ((uint32_t)date->Month << 5)
                       | (uint32_t)date->Date;

  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DATE_REGISTER) != packed_date)
  {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DATE_REGISTER, packed_date);
  }
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_WEEKDAY_REGISTER) != date->WeekDay)
  {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_WEEKDAY_REGISTER, date->WeekDay);
  }
}

static uint8_t RTC_RestoreDateFromBackup(void)
{
  uint32_t packed_date = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DATE_REGISTER);
  uint8_t year = (uint8_t)((packed_date >> 9) & 0x7FU);
  uint8_t month = (uint8_t)((packed_date >> 5) & 0x0FU);
  uint8_t date = (uint8_t)(packed_date & 0x1FU);
  uint8_t weekday = (uint8_t)HAL_RTCEx_BKUPRead(&hrtc,
                                                RTC_BKP_WEEKDAY_REGISTER);

  if ((year > 99U) || (month < 1U) || (month > 12U) ||
      (date < 1U) || (date > 31U) ||
      (weekday < RTC_WEEKDAY_MONDAY) ||
      (weekday > RTC_WEEKDAY_SUNDAY))
  {
    return 0U;
  }

  hrtc.DateToUpdate.Year = year;
  hrtc.DateToUpdate.Month = month;
  hrtc.DateToUpdate.Date = date;
  hrtc.DateToUpdate.WeekDay = weekday;
  return 1U;
}

static void RTC_SetUserTime(void)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  time.Hours = (uint8_t)RTC_SET_HOUR;
  time.Minutes = (uint8_t)RTC_SET_MINUTE;
  time.Seconds = (uint8_t)RTC_SET_SECOND;

  date.WeekDay = RTC_SET_WEEKDAY;
  date.Year = (uint8_t)(RTC_SET_YEAR - 2000U);
  date.Month = (uint8_t)RTC_SET_MONTH;
  date.Date = (uint8_t)RTC_SET_DATE;

  if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

  RTC_SaveDateToBackup(&date);
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_MAGIC);
}

static void RTC_UpdateCachedTime(void)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  RtcSnapshot_t next;

  if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK)
  {
    return;
  }
  /* On STM32F1 the date must be read after the time to unlock the shadow registers. */
  if (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK)
  {
    return;
  }

  next.year = date.Year;
  next.month = date.Month;
  next.date = date.Date;
  next.hour = time.Hours;
  next.minute = time.Minutes;
  next.second = time.Seconds;
  next.weekday = date.WeekDay;

  RTC_SaveDateToBackup(&date);

  taskENTER_CRITICAL();
  rtc_snapshot = next;
  rtc_fat_timestamp = RTC_PackFatTime(&next);
  taskEXIT_CRITICAL();
}

static void RTC_GetCachedTime(RtcSnapshot_t *snapshot)
{
  taskENTER_CRITICAL();
  *snapshot = rtc_snapshot;
  taskEXIT_CRITICAL();
}

static const char *RTC_GetWeekdayText(uint8_t weekday)
{
  switch (weekday)
  {
    case RTC_WEEKDAY_MONDAY:    return "MON";
    case RTC_WEEKDAY_TUESDAY:   return "TUE";
    case RTC_WEEKDAY_WEDNESDAY: return "WED";
    case RTC_WEEKDAY_THURSDAY:  return "THU";
    case RTC_WEEKDAY_FRIDAY:    return "FRI";
    case RTC_WEEKDAY_SATURDAY:  return "SAT";
    case RTC_WEEKDAY_SUNDAY:    return "SUN";
    default:                    return "---";
  }
}

uint32_t App_GetFatTimestamp(void)
{
  return rtc_fat_timestamp;
}

#if TF_CARD_ENABLED != 0U
static void LogBuffer_Push(const LogItem_t *item)
{
  taskENTER_CRITICAL();
  if (log_count >= LOG_BUFFER_LENGTH)
  {
    log_tail = (uint8_t)((log_tail + 1U) % LOG_BUFFER_LENGTH);
    log_count--;
  }
  log_buffer[log_head] = *item;
  log_head = (uint8_t)((log_head + 1U) % LOG_BUFFER_LENGTH);
  log_count++;
  taskEXIT_CRITICAL();
}

static uint8_t LogBuffer_Pop(LogItem_t *item)
{
  uint8_t available = 0U;

  taskENTER_CRITICAL();
  if (log_count > 0U)
  {
    *item = log_buffer[log_tail];
    log_tail = (uint8_t)((log_tail + 1U) % LOG_BUFFER_LENGTH);
    log_count--;
    available = 1U;
  }
  taskEXIT_CRITICAL();

  return available;
}

static char *Text_Append(char *destination, const char *source)
{
  while (*source != '\0')
  {
    *destination++ = *source++;
  }
  return destination;
}
#endif

static char *Text_AppendTwoDigits(char *destination, uint8_t value)
{
  *destination++ = (char)('0' + ((value / 10U) % 10U));
  *destination++ = (char)('0' + (value % 10U));
  return destination;
}

#if TF_CARD_ENABLED != 0U
static char *Text_AppendPeople(char *destination, int32_t people)
{
  if (people < 0)
  {
    *destination++ = '-';
    people = -people;
  }

  if (people >= 10)
  {
    *destination++ = (char)('0' + ((people / 10) % 10));
  }
  *destination++ = (char)('0' + (people % 10));
  return destination;
}

static uint16_t FormatLogLine(char *line, const RtcSnapshot_t *rtc,
                              const char *event_text, int32_t people)
{
  char *position = line;

  *position++ = '2';
  *position++ = '0';
  position = Text_AppendTwoDigits(position, rtc->year);
  *position++ = '-';
  position = Text_AppendTwoDigits(position, rtc->month);
  *position++ = '-';
  position = Text_AppendTwoDigits(position, rtc->date);
  *position++ = ' ';
  position = Text_AppendTwoDigits(position, rtc->hour);
  *position++ = ':';
  position = Text_AppendTwoDigits(position, rtc->minute);
  *position++ = ':';
  position = Text_AppendTwoDigits(position, rtc->second);
  *position++ = ',';
  position = Text_Append(position, event_text);
  *position++ = ',';
  position = Text_AppendPeople(position, people);
  *position++ = '\r';
  *position++ = '\n';
  *position = '\0';

  return (uint16_t)(position - line);
}
#endif
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
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_I2C2_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  LED_OFF();
  BEEP_OFF();
  OLED_Init();
/* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of peopleMutex */
  peopleMutexHandle = osMutexNew(&peopleMutex_attributes);

  /* creation of measureMutex */
  measureMutexHandle = osMutexNew(&measureMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of EventQueue */
  EventQueueHandle = osMessageQueueNew (10, sizeof(uint32_t), &EventQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of InSensorTask */
  InSensorTaskHandle = osThreadNew(StartInSensorTask, NULL, &InSensorTask_attributes);

  /* creation of OutSensorTask */
  OutSensorTaskHandle = osThreadNew(StartOutSensorTask, NULL, &OutSensorTask_attributes);

  /* creation of PeopleDisplayTa */
  PeopleDisplayTaHandle = osThreadNew(StartPeopleDisplayTask, NULL, &PeopleDisplayTa_attributes);

  /* creation of RTCTask */
  RTCTaskHandle = osThreadNew(StartRTCTask, NULL, &RTCTask_attributes);

  /* creation of TFCardTask */
  TFCardTaskHandle = osThreadNew(StartTFCardTask, NULL, &TFCardTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  if ((peopleMutexHandle == NULL) || (measureMutexHandle == NULL) ||
      (EventQueueHandle == NULL) || (InSensorTaskHandle == NULL) ||
      (OutSensorTaskHandle == NULL) || (PeopleDisplayTaHandle == NULL) ||
      (RTCTaskHandle == NULL) || (TFCardTaskHandle == NULL))
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

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
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

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

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

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
  if ((HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != RTC_BKP_MAGIC) ||
      (RTC_RestoreDateFromBackup() == 0U))
  {
/* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_JANUARY;
  DateToUpdate.Date = 0x1;
  DateToUpdate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */
    RTC_SetUserTime();
  }
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
  HAL_GPIO_WritePin(GPIOA, LED_Pin|TF_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);

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
  osDelay(80U);
  BEEP_OFF();
}

static void DoorLock_Update(int32_t people)
{
  if (people >= MAX_PEOPLE)
  {
    LED_ON();
  }
  else
  {
    LED_OFF();
  }
}

static void Display_Refresh(void)
{
  char people_line[12] = "People: 0/5";
  char time_line[9];
  char date_line[11];
  int32_t people;
  RtcSnapshot_t rtc;

  if (osMutexAcquire(peopleMutexHandle, osWaitForever) != osOK)
  {
    return;
  }
  people = current_people;
  (void)osMutexRelease(peopleMutexHandle);
  RTC_GetCachedTime(&rtc);

  OLED_Clear();
  people_line[8] = (char)('0' + (people % 10));
  people_line[10] = (char)('0' + MAX_PEOPLE);
  OLED_ShowString(9U, 0U, people_line);

  (void)Text_AppendTwoDigits(&time_line[0], rtc.hour);
  time_line[2] = ':';
  (void)Text_AppendTwoDigits(&time_line[3], rtc.minute);
  time_line[5] = ':';
  (void)Text_AppendTwoDigits(&time_line[6], rtc.second);
  time_line[8] = '\0';
  OLED_ShowString2x(18U, 30U, time_line);

  date_line[0] = '2';
  date_line[1] = '0';
  (void)Text_AppendTwoDigits(&date_line[2], rtc.year);
  date_line[4] = '/';
  (void)Text_AppendTwoDigits(&date_line[5], rtc.month);
  date_line[7] = '/';
  (void)Text_AppendTwoDigits(&date_line[8], rtc.date);
  date_line[10] = '\0';
  OLED_ShowString(0U, 7U, date_line);
  OLED_ShowString(72U, 7U, RTC_GetWeekdayText(rtc.weekday));

  OLED_Refresh();
}

static void HCSR04_Trig(uint8_t sensor)
{
  GPIO_TypeDef *port = (sensor == SENSOR_IN) ? TR1_GPIO_Port : TR2_GPIO_Port;
  uint16_t pin = (sensor == SENSOR_IN) ? TR1_Pin : TR2_Pin;
  volatile uint32_t delay_count;

  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  taskENTER_CRITICAL();
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
  for (delay_count = 0U; delay_count < 900U; delay_count++)
  {
    __NOP();
  }
  HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
  taskEXIT_CRITICAL();
}

static float HCSR04_ReadCm(uint8_t sensor)
{
  uint32_t channel = (sensor == SENSOR_IN) ? TIM_CHANNEL_1 : TIM_CHANNEL_2;
  uint32_t flags;
  float result = -1.0f;

  if (osMutexAcquire(measureMutexHandle, osWaitForever) != osOK)
  {
    return -1.0f;
  }

  (void)osThreadFlagsClear(SENSOR_ECHO_READY_FLAG);
  ic_state[sensor] = 0U;
  distance_cm[sensor] = -1.0f;
  __HAL_TIM_SET_COUNTER(&htim3, 0U);
  __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, channel,
                                TIM_INPUTCHANNELPOLARITY_RISING);

  if (HAL_TIM_IC_Start_IT(&htim3, channel) == HAL_OK)
  {
    HCSR04_Trig(sensor);
    flags = osThreadFlagsWait(SENSOR_ECHO_READY_FLAG, osFlagsWaitAny, 60U);
    if ((flags & SENSOR_ECHO_READY_FLAG) != 0U)
    {
      result = distance_cm[sensor];
    }
    (void)HAL_TIM_IC_Stop_IT(&htim3, channel);
  }

  (void)osMutexRelease(measureMutexHandle);
  return result;
}

static void ProcessSensorSample(uint8_t sensor, uint32_t event,
                                uint8_t *occupied, uint8_t *release_count,
                                uint32_t *last_event_tick)
{
  float distance = HCSR04_ReadCm(sensor);
  uint32_t now = osKernelGetTickCount();
  uint32_t cooldown_ticks = (EVENT_COOLDOWN_MS * osKernelGetTickFreq()) / 1000U;
  uint8_t is_near = ((distance >= DISTANCE_MIN_CM) &&
                     (distance < DISTANCE_THRESHOLD_CM)) ? 1U : 0U;

  if (is_near != 0U)
  {
    *release_count = 0U;
    if ((*occupied == 0U) && ((now - *last_event_tick) >= cooldown_ticks))
    {
      uint32_t event_to_send = event;

      *occupied = 1U;
      *last_event_tick = now;

      if (event == EVENT_IN)
      {
        int32_t people;

        if (osMutexAcquire(peopleMutexHandle, osWaitForever) != osOK)
        {
          return;
        }
        people = current_people;
        (void)osMutexRelease(peopleMutexHandle);

        if (people >= MAX_PEOPLE)
        {
          event_to_send = EVENT_DENIED;
          DoorLock_Update(people);
          Buzzer_BeepShort();
        }
      }

      (void)osMessageQueuePut(EventQueueHandle, &event_to_send, 0U, 0U);
    }
  }
  else if (*occupied != 0U)
  {
    (*release_count)++;
    if (*release_count >= SENSOR_RELEASE_SAMPLES)
    {
      *occupied = 0U;
      *release_count = 0U;
    }
  }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  uint8_t sensor;
  uint32_t channel;
  osThreadId_t target_task;
  uint16_t falling;
  uint16_t difference;

  if (htim->Instance != TIM3)
  {
    return;
  }

  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
  {
    sensor = SENSOR_IN;
    channel = TIM_CHANNEL_1;
    target_task = InSensorTaskHandle;
  }
  else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
  {
    sensor = SENSOR_OUT;
    channel = TIM_CHANNEL_2;
    target_task = OutSensorTaskHandle;
  }
  else
  {
    return;
  }

  if (ic_state[sensor] == 0U)
  {
    ic_rise[sensor] = (uint16_t)HAL_TIM_ReadCapturedValue(htim, channel);
    ic_state[sensor] = 1U;
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, channel,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
  }
  else
  {
    falling = (uint16_t)HAL_TIM_ReadCapturedValue(htim, channel);
    difference = (uint16_t)(falling - ic_rise[sensor]);
    distance_cm[sensor] = ((float)difference * 0.0343f) / 2.0f;
    ic_state[sensor] = 0U;
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, channel,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
    if (target_task != NULL)
    {
      (void)osThreadFlagsSet(target_task, SENSOR_ECHO_READY_FLAG);
    }
  }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartInSensorTask */
/**
  * @brief  Function implementing the InSensorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartInSensorTask */
void StartInSensorTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  uint8_t occupied = 0U;
  uint8_t release_count = 0U;
  uint32_t last_event_tick = 0U;

  /* Infinite loop */
  for(;;)
  {
    ProcessSensorSample(SENSOR_IN, EVENT_IN, &occupied,
                        &release_count, &last_event_tick);
    osDelay(SENSOR_SAMPLE_PERIOD_MS);
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
void StartOutSensorTask(void *argument)
{
  /* USER CODE BEGIN StartOutSensorTask */
  uint8_t occupied = 0U;
  uint8_t release_count = 0U;
  uint32_t last_event_tick = 0U;

  /* Infinite loop */
  for(;;)
  {
    ProcessSensorSample(SENSOR_OUT, EVENT_OUT, &occupied,
                        &release_count, &last_event_tick);
    osDelay(SENSOR_SAMPLE_PERIOD_MS);
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
void StartPeopleDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartPeopleDisplayTask */
  uint32_t event;
#if TF_CARD_ENABLED != 0U
  LogItem_t log;
#endif

  /* Infinite loop */
  for(;;)
  {
    if (osMessageQueueGet(EventQueueHandle, &event, NULL, 200U) == osOK)
    {
      int32_t people;
#if TF_CARD_ENABLED != 0U
      RtcSnapshot_t rtc;
#endif

      if (osMutexAcquire(peopleMutexHandle, osWaitForever) == osOK)
      {
        if ((event == EVENT_IN) && (current_people < MAX_PEOPLE))
        {
          current_people++;
        }
        else if ((event == EVENT_OUT) && (current_people > 0))
        {
          current_people--;
        }
        people = current_people;
        DoorLock_Update(people);
        (void)osMutexRelease(peopleMutexHandle);

#if TF_CARD_ENABLED != 0U
        RTC_GetCachedTime(&rtc);
        log.event = (uint8_t)event;
        log.people = people;
        log.year = rtc.year;
        log.month = rtc.month;
        log.date = rtc.date;
        log.hour = rtc.hour;
        log.minute = rtc.minute;
        log.second = rtc.second;
        LogBuffer_Push(&log);
#endif
      }
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
void StartRTCTask(void *argument)
{
  /* USER CODE BEGIN StartRTCTask */
  /* Infinite loop */
  for(;;)
  {
    RTC_UpdateCachedTime();
    osDelay(1000U);
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
void StartTFCardTask(void *argument)
{
  /* USER CODE BEGIN StartTFCardTask */
#if TF_CARD_ENABLED == 0U
  for (;;)
  {
    osDelay(1000U);
  }
#else
  FRESULT result;
  UINT bytes_written;
  uint16_t line_length;
  char line[128];
  uint8_t boot_recorded = 0U;

  osDelay(1500U);

  /* Infinite loop */
  for(;;)
  {
    result = f_mount(&USERFatFS, USERPath, 1U);
    if (result == FR_OK)
    {
      if (boot_recorded == 0U)
      {
        RtcSnapshot_t rtc;
        int32_t people;

        RTC_GetCachedTime(&rtc);
        if (osMutexAcquire(peopleMutexHandle, osWaitForever) == osOK)
        {
          people = current_people;
          (void)osMutexRelease(peopleMutexHandle);
        }
        else
        {
          people = 0;
        }
        result = f_open(&USERFile, "boot.txt", FA_OPEN_ALWAYS | FA_WRITE);
        if (result == FR_OK)
        {
          (void)f_lseek(&USERFile, f_size(&USERFile));
          line_length = FormatLogLine(line, &rtc, "BOOT", people);
          result = f_write(&USERFile, line, line_length, &bytes_written);
          if (result == FR_OK)
          {
            result = f_sync(&USERFile);
          }
          (void)f_close(&USERFile);
          if (result == FR_OK)
          {
            boot_recorded = 1U;
          }
        }
      }

      result = f_open(&USERFile, "log.csv", FA_OPEN_ALWAYS | FA_WRITE);
      if (result == FR_OK)
      {
        LogItem_t item;
        int32_t people;
        RtcSnapshot_t rtc;

        if (f_size(&USERFile) == 0U)
        {
          const char *header = "time,event,people\r\n";
          result = f_write(&USERFile, header, strlen(header), &bytes_written);
        }
        if (result == FR_OK)
        {
          result = f_lseek(&USERFile, f_size(&USERFile));
        }

        while ((result == FR_OK) && (LogBuffer_Pop(&item) != 0U))
        {
          const char *event_text = (item.event == EVENT_IN) ? "IN" :
                                   (item.event == EVENT_OUT) ? "OUT" : "DENIED";
          RtcSnapshot_t item_time;

          item_time.year = item.year;
          item_time.month = item.month;
          item_time.date = item.date;
          item_time.hour = item.hour;
          item_time.minute = item.minute;
          item_time.second = item.second;
          line_length = FormatLogLine(line, &item_time, event_text, item.people);
          result = f_write(&USERFile, line, line_length, &bytes_written);
        }

        if ((result == FR_OK) &&
            (osMutexAcquire(peopleMutexHandle, osWaitForever) == osOK))
        {
          people = current_people;
          (void)osMutexRelease(peopleMutexHandle);
          RTC_GetCachedTime(&rtc);
          line_length = FormatLogLine(line, &rtc, "STAT", people);
          result = f_write(&USERFile, line, line_length, &bytes_written);
        }

        if (result == FR_OK)
        {
          (void)f_sync(&USERFile);
        }
        (void)f_close(&USERFile);
      }
    }

    /* With no TF module present this task simply retries every five seconds. */
    osDelay(TF_RETRY_PERIOD_MS);
  }
#endif
  /* USER CODE END StartTFCardTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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
