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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;

FDCAN_HandleTypeDef hfdcan1;
FDCAN_HandleTypeDef hfdcan2;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
DMA_HandleTypeDef hdma_i2c2_rx;

SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_rx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim8;
DMA_HandleTypeDef hdma_tim2_ch3;

/* Definitions for Global_CAN_Send */
osThreadId_t Global_CAN_SendHandle;
const osThreadAttr_t Global_CAN_Send_attributes = {
    .name = "Global_CAN_Send", .priority = (osPriority_t)osPriorityHigh, .stack_size = 128 * 4};
/* Definitions for Sensor_MSG_1 */
osThreadId_t Sensor_MSG_1Handle;
const osThreadAttr_t Sensor_MSG_1_attributes = {
    .name = "Sensor_MSG_1", .priority = (osPriority_t)osPriorityNormal, .stack_size = 128 * 4};
/* Definitions for Sensor_MSG_2 */
osThreadId_t Sensor_MSG_2Handle;
const osThreadAttr_t Sensor_MSG_2_attributes = {
    .name = "Sensor_MSG_2", .priority = (osPriority_t)osPriorityNormal, .stack_size = 128 * 4};
/* Definitions for Temps_MSG */
osThreadId_t Temps_MSGHandle;
const osThreadAttr_t Temps_MSG_attributes = {
    .name = "Temps_MSG", .priority = (osPriority_t)osPriorityNormal, .stack_size = 128 * 4};
/* Definitions for Board_Diag_MSG */
osThreadId_t Board_Diag_MSGHandle;
const osThreadAttr_t Board_Diag_MSG_attributes = {
    .name = "Board_Diag_MSG", .priority = (osPriority_t)osPriorityNormal, .stack_size = 128 * 4};
/* Definitions for ADC1_Process */
osThreadId_t ADC1_ProcessHandle;
const osThreadAttr_t ADC1_Process_attributes = {
    .name = "ADC1_Process", .priority = (osPriority_t)osPriorityLow, .stack_size = 128 * 4};
/* Definitions for ADC2_Process */
osThreadId_t ADC2_ProcessHandle;
const osThreadAttr_t ADC2_Process_attributes = {
    .name = "ADC2_Process", .priority = (osPriority_t)osPriorityLow, .stack_size = 128 * 4};
/* Definitions for Process_WSS */
osThreadId_t Process_WSSHandle;
const osThreadAttr_t Process_WSS_attributes = {
    .name = "Process_WSS", .priority = (osPriority_t)osPriorityNormal, .stack_size = 128 * 4};
/* Definitions for Global_CAN_Queue */
osMessageQueueId_t Global_CAN_QueueHandle;
const osMessageQueueAttr_t Global_CAN_Queue_attributes = {.name = "Global_CAN_Queue"};
/* Definitions for WSS_Queue */
osMessageQueueId_t WSS_QueueHandle;
const osMessageQueueAttr_t WSS_Queue_attributes = {.name = "WSS_Queue"};
/* Definitions for ADC1_key */
osMutexId_t ADC1_keyHandle;
const osMutexAttr_t ADC1_key_attributes = {.name = "ADC1_key"};
/* Definitions for ADC2_key */
osMutexId_t ADC2_keyHandle;
const osMutexAttr_t ADC2_key_attributes = {.name = "ADC2_key"};
/* Definitions for temp_data_key */
osMutexId_t temp_data_keyHandle;
const osMutexAttr_t temp_data_key_attributes = {.name = "temp_data_key"};
/* Definitions for error_info_key */
osMutexId_t error_info_keyHandle;
const osMutexAttr_t error_info_key_attributes = {.name = "error_info_key"};
/* Definitions for WSS_Key */
osMutexId_t WSS_KeyHandle;
const osMutexAttr_t WSS_Key_attributes = {.name = "WSS_Key"};
/* Definitions for board_mon_key */
osMutexId_t board_mon_keyHandle;
const osMutexAttr_t board_mon_key_attributes = {.name = "board_mon_key"};
/* Definitions for sem_MON_I2C */
osSemaphoreId_t sem_MON_I2CHandle;
const osSemaphoreAttr_t sem_MON_I2C_attributes = {.name = "sem_MON_I2C"};
/* Definitions for sem_WSS */
osSemaphoreId_t sem_WSSHandle;
const osSemaphoreAttr_t sem_WSS_attributes = {.name = "sem_WSS"};
/* Definitions for sem_ADC1 */
osSemaphoreId_t sem_ADC1Handle;
const osSemaphoreAttr_t sem_ADC1_attributes = {.name = "sem_ADC1"};
/* Definitions for wss_write_flag */
osEventFlagsId_t wss_write_flagHandle;
const osEventFlagsAttr_t wss_write_flag_attributes = {.name = "wss_write_flag"};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_FDCAN2_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM8_Init(void);
void Start_Global_CAN_Send(void* argument);
void Start_Sensor_MSG_1(void* argument);
void Start_Sensor_MSG_2(void* argument);
void Start_Temps_MSG(void* argument);
void Start_Board_Diag_MSG(void* argument);
void Start_ADC1_Process(void* argument);
void Start_ADC2_Process(void* argument);
void Start_Process_WSS(void* argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
Mutex_Struct_t mutex_struct;
Semaphore_Struct_t semaphore_struct;

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */
    adc1_info adc1_data = {0};
    adc2_info adc2_data = {0};
    mon_i2c_data board_mon_data = {0};
    wss = 0;
    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_FDCAN1_Init();
    MX_FDCAN2_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_SPI2_Init();
    MX_TIM2_Init();
    MX_TIM8_Init();
    /* USER CODE BEGIN 2 */
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&hadc1, uint32_t* pData, uint32_t Length)
        /* USER CODE END 2 */

        /* Init scheduler */
        osKernelInitialize();
    /* Create the mutex(es) */
    /* creation of ADC1_key */
    ADC1_keyHandle = osMutexNew(&ADC1_key_attributes);

    /* creation of ADC2_key */
    ADC2_keyHandle = osMutexNew(&ADC2_key_attributes);

    /* creation of temp_data_key */
    temp_data_keyHandle = osMutexNew(&temp_data_key_attributes);

    /* creation of error_info_key */
    error_info_keyHandle = osMutexNew(&error_info_key_attributes);

    /* creation of WSS_Key */
    WSS_KeyHandle = osMutexNew(&WSS_Key_attributes);

    /* creation of board_mon_key */
    board_mon_keyHandle = osMutexNew(&board_mon_key_attributes);

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    mutex_struct.error_info_key = error_info_keyHandle;
    mutex_struct.temp_data_key = temp_data_keyHandle;
    mutex_struct.adc1_key = ADC1_keyHandle;
    mutex_struct.adc2_key = ADC2_keyHandle;
    mutex_struct.board_mon_key = board_mon_keyHandle;

    /* USER CODE END RTOS_MUTEX */

    /* Create the semaphores(s) */
    /* creation of sem_MON_I2C */
    sem_MON_I2CHandle = osSemaphoreNew(1, 0, &sem_MON_I2C_attributes);

    /* creation of sem_WSS */
    sem_WSSHandle = osSemaphoreNew(1, 0, &sem_WSS_attributes);

    /* creation of sem_ADC1 */
    sem_ADC1Handle = osSemaphoreNew(1, 0, &sem_ADC1_attributes);

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    semaphore_struct.sem_ADC1 = sem_ADC1Handle;
    semaphore_struct.sem_MON_I2C = sem_MON_I2CHandle;
    semaphore_struct.sem_WSS = sem_WSSHandle;
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
    /* USER CODE END RTOS_TIMERS */

    /* Create the queue(s) */
    /* creation of Global_CAN_Queue */
    Global_CAN_QueueHandle = osMessageQueueNew(20, sizeof(uint16_t), &Global_CAN_Queue_attributes);

    /* creation of WSS_Queue */
    WSS_QueueHandle = osMessageQueueNew(1, sizeof(float), &WSS_Queue_attributes);

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    Local_Temp_CAN_QueueHandle =
        osMessageQueueNew(20, sizeof(uint8_t[8]), &Local_Temp_CAN_Queue_attributes);
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of Global_CAN_Send */
    Global_CAN_SendHandle = osThreadNew(Start_Global_CAN_Send, NULL, &Global_CAN_Send_attributes);

    /* creation of Sensor_MSG_1 */
    Sensor_MSG_1Handle = osThreadNew(Start_Sensor_MSG_1, NULL, &Sensor_MSG_1_attributes);

    /* creation of Sensor_MSG_2 */
    Sensor_MSG_2Handle = osThreadNew(Start_Sensor_MSG_2, NULL, &Sensor_MSG_2_attributes);

    /* creation of Temps_MSG */
    Temps_MSGHandle = osThreadNew(Start_Temps_MSG, NULL, &Temps_MSG_attributes);

    /* creation of Board_Diag_MSG */
    Board_Diag_MSGHandle = osThreadNew(Start_Board_Diag_MSG, NULL, &Board_Diag_MSG_attributes);

    /* creation of ADC1_Process */
    ADC1_ProcessHandle = osThreadNew(Start_ADC1_Process, NULL, &ADC1_Process_attributes);

    /* creation of ADC2_Process */
    ADC2_ProcessHandle = osThreadNew(Start_ADC2_Process, NULL, &ADC2_Process_attributes);

    /* creation of Process_WSS */
    Process_WSSHandle = osThreadNew(Start_Process_WSS, NULL, &Process_WSS_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */

    /* creation of wss_write_flag */
    wss_write_flagHandle = osEventFlagsNew(&wss_write_flag_attributes);

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 16;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void) {
    /* USER CODE BEGIN ADC1_Init 0 */

    /* USER CODE END ADC1_Init 0 */

    ADC_MultiModeTypeDef multimode = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC1_Init 1 */

    /* USER CODE END ADC1_Init 1 */

    /** Common config
     */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.GainCompensation = 0;
    hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 3;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    /** Configure the ADC multi-mode
     */
    multimode.Mode = ADC_MODE_INDEPENDENT;
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel = ADC_CHANNEL_5;
    sConfig.Rank = ADC_REGULAR_RANK_2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_3;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN ADC1_Init 2 */

    /* USER CODE END ADC1_Init 2 */
}

/**
 * @brief ADC2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC2_Init(void) {
    /* USER CODE BEGIN ADC2_Init 0 */

    /* USER CODE END ADC2_Init 0 */

    ADC_ChannelConfTypeDef sConfig = {0};

    /* USER CODE BEGIN ADC2_Init 1 */

    /* USER CODE END ADC2_Init 1 */

    /** Common config
     */
    hadc2.Instance = ADC2;
    hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc2.Init.Resolution = ADC_RESOLUTION_12B;
    hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc2.Init.GainCompensation = 0;
    hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc2.Init.LowPowerAutoWait = DISABLE;
    hadc2.Init.ContinuousConvMode = DISABLE;
    hadc2.Init.NbrOfConversion = 1;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc2.Init.DMAContinuousRequests = DISABLE;
    hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc2.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hadc2) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Regular Channel
     */
    sConfig.Channel = ADC_CHANNEL_12;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN ADC2_Init 2 */

    /* USER CODE END ADC2_Init 2 */
}

/**
 * @brief FDCAN1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_FDCAN1_Init(void) {
    /* USER CODE BEGIN FDCAN1_Init 0 */

    /* USER CODE END FDCAN1_Init 0 */

    /* USER CODE BEGIN FDCAN1_Init 1 */

    /* USER CODE END FDCAN1_Init 1 */
    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = DISABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 16;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 1;
    hfdcan1.Init.NominalTimeSeg2 = 1;
    hfdcan1.Init.DataPrescaler = 1;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 1;
    hfdcan1.Init.DataTimeSeg2 = 1;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 0;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN FDCAN1_Init 2 */
    FDCAN_FilterTypeDef sFilterConfig;

    sFilterConfig.IdType = FDCAN_STANDARD_ID;  // 11 Bit Standard Length
    sFilterConfig.FilterIndex = 0;             // One filter so set to 0
    sFilterConfig.FilterType =
        FDCAN_FILTER_MASK;  // Standard Filter Type, FilterID1 = filter, FilterID2 = mask
    sFilterConfig.FilterID1 = 0x000;
    sFilterConfig.FilterID2 = 0x000;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
        /* Filter Configuration Error */
        Error_Handler();
    }
    /* USER CODE END FDCAN1_Init 2 */
}

/**
 * @brief FDCAN2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_FDCAN2_Init(void) {
    /* USER CODE BEGIN FDCAN2_Init 0 */

    /* USER CODE END FDCAN2_Init 0 */

    /* USER CODE BEGIN FDCAN2_Init 1 */

    /* USER CODE END FDCAN2_Init 1 */
    hfdcan2.Instance = FDCAN2;
    hfdcan2.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan2.Init.AutoRetransmission = DISABLE;
    hfdcan2.Init.TransmitPause = DISABLE;
    hfdcan2.Init.ProtocolException = DISABLE;
    hfdcan2.Init.NominalPrescaler = 16;
    hfdcan2.Init.NominalSyncJumpWidth = 1;
    hfdcan2.Init.NominalTimeSeg1 = 1;
    hfdcan2.Init.NominalTimeSeg2 = 1;
    hfdcan2.Init.DataPrescaler = 1;
    hfdcan2.Init.DataSyncJumpWidth = 1;
    hfdcan2.Init.DataTimeSeg1 = 1;
    hfdcan2.Init.DataTimeSeg2 = 1;
    hfdcan2.Init.StdFiltersNbr = 0;
    hfdcan2.Init.ExtFiltersNbr = 0;
    hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN FDCAN2_Init 2 */
    sFilterConfig.IdType = FDCAN_STANDARD_ID;  // 11 Bit Standard Length
    sFilterConfig.FilterIndex = 0;             // One filter so set to 0
    sFilterConfig.FilterType =
        FDCAN_FILTER_MASK;  // Standard Filter Type, FilterID1 = filter, FilterID2 = mask
    sFilterConfig.FilterID1 = 0x000;
    sFilterConfig.FilterID2 = 0x000;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    /* USER CODE END FDCAN2_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {
    /* USER CODE BEGIN I2C1_Init 0 */

    /* USER CODE END I2C1_Init 0 */

    /* USER CODE BEGIN I2C1_Init 1 */

    /* USER CODE END I2C1_Init 1 */
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x10B17DB5;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C1_Init 2 */

    /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void) {
    /* USER CODE BEGIN I2C2_Init 0 */

    /* USER CODE END I2C2_Init 0 */

    /* USER CODE BEGIN I2C2_Init 1 */

    /* USER CODE END I2C2_Init 1 */
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x10B17DB5;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }

    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C2_Init 2 */

    /* USER CODE END I2C2_Init 2 */
}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {
    /* USER CODE BEGIN SPI2_Init 0 */

    /* USER CODE END SPI2_Init 0 */

    /* USER CODE BEGIN SPI2_Init 1 */

    /* USER CODE END SPI2_Init 1 */
    /* SPI2 parameter configuration*/
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_4BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 7;
    hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    if (HAL_SPI_Init(&hspi2) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN SPI2_Init 2 */

    /* USER CODE END SPI2_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {
    /* USER CODE BEGIN TIM2_Init 0 */

    /* USER CODE END TIM2_Init 0 */

    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};

    /* USER CODE BEGIN TIM2_Init 1 */

    /* USER CODE END TIM2_Init 1 */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 64 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 4294967295;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim2) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 8;
    if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM2_Init 2 */

    /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief TIM8 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM8_Init(void) {
    /* USER CODE BEGIN TIM8_Init 0 */

    /* USER CODE END TIM8_Init 0 */

    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* USER CODE BEGIN TIM8_Init 1 */

    /* USER CODE END TIM8_Init 1 */
    htim8.Instance = TIM8;
    htim8.Init.Prescaler = 640 - 1;
    htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim8.Init.Period = 999;
    htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim8.Init.RepetitionCounter = 0;
    htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim8) != HAL_OK) {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM8_Init 2 */

    /* USER CODE END TIM8_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {
    /* DMA controller clock enable */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel1_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    /* DMA1_Channel2_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    /* DMA1_Channel3_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
    /* DMA1_Channel4_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    /* DMA1_Channel5_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | Indicator_LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : ALERT_Pin */
    GPIO_InitStruct.Pin = ALERT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ALERT_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : PA4 */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : PB0 PB1 Indicator_LED_Pin */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | Indicator_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_Start_Global_CAN_Send */
/**
 * @brief  Function implementing the Global_CAN_Send thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_Global_CAN_Send */
void Start_Global_CAN_Send(void* argument) {
    /* USER CODE BEGIN 5 */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        osDelayUntil(curr_tick + GLOBAL_CAN_SEND_TIME);
        curr_tick += GLOBAL_CAN_SEND_TIME;
    }
    /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_Start_Sensor_MSG_1 */
/**
 * @brief Function implementing the Sensor_MSG_1 thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_Sensor_MSG_1 */
void Start_Sensor_MSG_1(void* argument) {
    /* USER CODE BEGIN Start_Sensor_MSG_1 */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        osDelayUntil(curr_tick + SENSOR_MSG_1_TIME);
        curr_tick += SENSOR_MSG_1_TIME;
    }
    /* USER CODE END Start_Sensor_MSG_1 */
}

/* USER CODE BEGIN Header_Start_Sensor_MSG_2 */
/**
 * @brief Function implementing the Sensor_MSG_2 thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_Sensor_MSG_2 */
void Start_Sensor_MSG_2(void* argument) {
    /* USER CODE BEGIN Start_Sensor_MSG_2 */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        osDelayUntil(curr_tick + SENSOR_MSG_2_TIME);
        curr_tick += SENSOR_MSG_2_TIME;
    }
    /* USER CODE END Start_Sensor_MSG_2 */
}

/* USER CODE BEGIN Header_Start_Temps_MSG */
/**
 * @brief Function implementing the Temps_MSG thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_Temps_MSG */
void Start_Temps_MSG(void* argument) {
    /* USER CODE BEGIN Start_Temps_MSG */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        osDelayUntil(curr_tick + TEMPS_MSG_TIME);
        curr_tick += TEMPS_MSG_TIME;
    }
    /* USER CODE END Start_Temps_MSG */
}

/* USER CODE BEGIN Header_Start_Board_Diag_MSG */
/**
 * @brief Function implementing the Board_Diag_MSG thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_Board_Diag_MSG */
void Start_Board_Diag_MSG(void* argument) {
    /* USER CODE BEGIN Start_Board_Diag_MSG */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        osDelayUntil(curr_tick + BOARD_DIAG_MSG_TIME);
        curr_tick += BOARD_DIAG_MSG_TIME;
    }
    /* USER CODE END Start_Board_Diag_MSG */
}

/* USER CODE BEGIN Header_Start_ADC1_Process */
/**
 * @brief Function implementing the ADC1_Process thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_ADC1_Process */
void Start_ADC1_Process(void* argument) {
    /* USER CODE BEGIN Start_ADC1_Process */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        if (osSemaphoreAcquire(semaphore_struct.sem_ADC1, 10) != osOK) {
            continue;
        }
        adc1_info local_adc1;
        convert_adc1_to_real(&local_adc1);
        copyWithMutex(&mutex_struct.adc1_key, dest_ptr, src_ptr)
            osDelayUntil(curr_tick + ADC1_PROCESS_TIME);
        curr_tick += ADC1_PROCESS_TIME;
    }
    /* USER CODE END Start_ADC1_Process */
}

/* USER CODE BEGIN Header_Start_ADC2_Process */
/**
 * @brief Function implementing the ADC2_Process thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_ADC2_Process */
void Start_ADC2_Process(void* argument) {
    /* USER CODE BEGIN Start_ADC2_Process */
    uint32_t curr_tick = osKernelGetTickCount();
    /* Infinite loop */
    for (;;) {
        osDelayUntil(curr_tick + ADC2_PROCESS_TIME);
        curr_tick += ADC2_PROCESS_TIME;
    }
    /* USER CODE END Start_ADC2_Process */
}

/* USER CODE BEGIN Header_Start_Process_WSS */
/**
 * @brief Function implementing the Process_WSS thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Start_Process_WSS */
void Start_Process_WSS(void* argument) {
    /* USER CODE BEGIN Start_Process_WSS */
    uint32_t flags;
    /* Infinite loop */
    for (;;) {
        flags = osThreadFlagsWait(0x0001, osFlagsWaitAny, osWaitForever);
        if (flags & 0x0001) {
            float temp_wss;
            osMessageQueueGet(WSS_QueueHandle, &temp_wss, QUEUE_NORM_PRIO, 0);
            copyWithMutex(WSS_KeyHandle, &wss, &temp_wss);

            uint32_t curr_tick = osKernelGetTickCount();
            osDelayUntil(curr_tick + WSS_PROCESS_TIME);
            curr_tick += WSS_PROCESS_TIME;
        }
    }
    /* USER CODE END Start_Process_WSS */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM1) {
        HAL_IncTick();
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
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
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
