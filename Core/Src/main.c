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

COM_InitTypeDef BspCOMInit;

ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc2;

FDCAN_HandleTypeDef hfdcan1;

osThreadId defaultTaskHandle;
osTimerId sendStateDisplayHandle;
osTimerId rearlightControlHandle;
osTimerId sendThrottleDisplayHandle;
osTimerId ledClusterHandle;
osTimerId relayReadHandle;
osTimerId chargeHandle;
/* USER CODE BEGIN PV */
// Race state
struct RaceState race_state;
enum MotoState moto_state = STATE_ERROR;
struct ChargerCommState charger_comm_state;
enum BMSCommState bms_comm_state = BMS_SLEEP;
enum MotoCharge moto_charge = STATE_PRECHARGE;
uint8_t button_moto = 0;  // 0b0000XYZT - X: ESDB_one, Y: ESDB_two, Z: TSMS, T: LVMS

FDCAN_ProtocolStatusTypeDef ps;
FDCAN_ErrorCountersTypeDef ec;

// Sensors
struct Throttle throttle_sensor;
struct battery bat;
// CAN
FDCAN_TxHeaderTypeDef tx_header;
can_message_eight tx_data;
can_message_four tx_data_four;
can_message_eight inverter_on_msg = { .int_val = 0x0101010101010101 };

FDCAN_RxHeaderTypeDef rx_header;
can_message_eight rx_data;

can_message_eight button_data_test;

// ADC
__IO uint8_t adc_complete_flag = 0;
uint16_t raw_adc_value;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC2_Init(void);
static void MX_FDCAN1_Init(void);
void StartDefaultTask(void const* argument);
void sendStateDisplayCallback(void const* argument);
void rearlightControlCallback(void const* argument);
void sendThrottleDisplayCallback(void const* argument);
void ledClusterCallback(void const* argument);
void relayReadCallback(void const* argument);
void CallbackCharge(void const* argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        // Retrieve Rx messages from RX FIFO0
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, rx_data.bytes) != HAL_OK) {
            // Reception Error
            Error_Handler();
        } else {
            // No error, process received payload
            switch (rx_header.Identifier) {
                // Inverter
                case 0x181:
                    break;
                case 0x281:
                    break;
                case 0x381:
                    break;
                case 0x481:
                    break;
                    // Display
                case 0x191:
                    // Read which button was pressed
                    button_data_test.int_val = rx_data.int_val;
                    handle_button_press(&race_state, rx_data.bytes[0]);  // we pass the adress of race_state
                    break;

                case 0x341: // BMS
                    for (int i = 0; i < 8; i++)
                        bat.raw[i] = rx_data.bytes[i];
                    convert_BMS_CAN(bat.raw, &bat);
                    break;
                case 0x441:
                    break;
            }
        }

        // Reactive receive notifications
        if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
            Error_Handler();
        }
    }
}

// ADC functions
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    adc_complete_flag = 1;
}

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

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC2_Init();
    MX_FDCAN1_Init();
    /* USER CODE BEGIN 2 */
    // Start ADC2
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*) &raw_adc_value, 1);

    // Start FDCAN1 and activate receive notifications
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
    }

    // Init structs
    race_state_init(&race_state);
    throttle_init(&throttle_sensor);
    charger_comms_init(&charger_comm_state);

    // Turn on the inverter
    send_turn_on_inverter(&tx_header, &hfdcan1);

    /* USER CODE END 2 */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* Create the timer(s) */
    /* definition and creation of sendStateDisplay */
    osTimerDef(sendStateDisplay, sendStateDisplayCallback);
    sendStateDisplayHandle = osTimerCreate(osTimer(sendStateDisplay), osTimerPeriodic, NULL);

    /* definition and creation of rearlightControl */
    osTimerDef(rearlightControl, rearlightControlCallback);
    rearlightControlHandle = osTimerCreate(osTimer(rearlightControl), osTimerPeriodic, NULL);

    /* definition and creation of sendThrottleDisplay */
    osTimerDef(sendThrottleDisplay, sendThrottleDisplayCallback);
    sendThrottleDisplayHandle = osTimerCreate(osTimer(sendThrottleDisplay), osTimerPeriodic, NULL);

    /* definition and creation of ledCluster */
    osTimerDef(ledCluster, ledClusterCallback);
    ledClusterHandle = osTimerCreate(osTimer(ledCluster), osTimerPeriodic, NULL);

    /* definition and creation of relayRead */
    osTimerDef(relayRead, relayReadCallback);
    relayReadHandle = osTimerCreate(osTimer(relayRead), osTimerPeriodic, NULL);

    /* definition and creation of charge */
    osTimerDef(charge, CallbackCharge);
    chargeHandle = osTimerCreate(osTimer(charge), osTimerPeriodic, NULL);

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    //osTimerStart(sendStateDisplayHandle, 50);
    osTimerStart(rearlightControlHandle, 200);
    //osTimerStart(sendThrottleDisplayHandle, 50);  // TODO: reduce this to 5-10 ms
    osTimerStart(ledClusterHandle, 1000);
    osTimerStart(relayReadHandle, 100);
    osTimerStart(chargeHandle, 100);
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* definition and creation of defaultTask */
    osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
    defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */

    /* Initialize leds */
    BSP_LED_Init(LED_GREEN);

    /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
    BspCOMInit.BaudRate = 115200;
    BspCOMInit.WordLength = COM_WORDLENGTH_8B;
    BspCOMInit.StopBits = COM_STOPBITS_1;
    BspCOMInit.Parity = COM_PARITY_NONE;
    BspCOMInit.HwFlowCtl = COM_HWCONTROL_NONE;
    if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE) {
        Error_Handler();
    }

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
//        if (adc_complete_flag) {
//            // Get throttle
//            convert_adc_throttle(&throttle_sensor, raw_adc_value);
//
//            // Reset ADC input
//            adc_complete_flag = 0;
//            HAL_ADC_Start_DMA(&hadc2, (uint32_t*) &raw_adc_value, 1);
//        }

        // TODO: uncomment the function calls below (?)
        // state of the motorcycle
        // fault_pin_service();
        // check_moto_state();
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
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    /** Configure the main internal regulator output voltage
     */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 8;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV8;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief ADC2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC2_Init(void) {

    /* USER CODE BEGIN ADC2_Init 0 */

    /* USER CODE END ADC2_Init 0 */

    ADC_ChannelConfTypeDef sConfig = { 0 };

    /* USER CODE BEGIN ADC2_Init 1 */

    /* USER CODE END ADC2_Init 1 */

    /** Common config
     */
    hadc2.Instance = ADC2;
    hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
    hadc2.Init.Resolution = ADC_RESOLUTION_12B;
    hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc2.Init.GainCompensation = 0;
    hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    hadc2.Init.LowPowerAutoWait = ENABLE;
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
    sConfig.Channel = ADC_CHANNEL_4;
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
    hfdcan1.Init.NominalPrescaler = 8;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 13;
    hfdcan1.Init.NominalTimeSeg2 = 2;
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
    tx_header.Identifier = 0x301;  // no need to init address yet
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;
    /* USER CODE END FDCAN1_Init 2 */

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

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, Normal_Pin | Charge_Led_Pin | Error_LED_Pin | Green_LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, Precharge_Pin | Debug_LED_Pin | GPIO_PIN_8, GPIO_PIN_RESET);

    /*Configure GPIO pins : ESDB2_Pin ESDB_Pin RELAY_CHARGER_Pin */
    GPIO_InitStruct.Pin = ESDB2_Pin | ESDB_Pin | RELAY_CHARGER_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : Normal_Pin Charge_Led_Pin Error_LED_Pin Green_LED_Pin */
    GPIO_InitStruct.Pin = Normal_Pin | Charge_Led_Pin | Error_LED_Pin | Green_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : Precharge_Pin Debug_LED_Pin PB8 */
    GPIO_InitStruct.Pin = Precharge_Pin | Debug_LED_Pin | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : Sensata_Aux_Pin */
    GPIO_InitStruct.Pin = Sensata_Aux_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(Sensata_Aux_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : LVMS_Pin TSMS_Pin */
    GPIO_InitStruct.Pin = LVMS_Pin | TSMS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const* argument) {
    /* USER CODE BEGIN 5 */
    /* Infinite loop */
    for (;;) {
        // TODO: Remove this CAN testing stuff below
//        HAL_FDCAN_GetProtocolStatus(&hfdcan1, &ps);
//        HAL_FDCAN_GetErrorCounters(&hfdcan1, &ec);

//        if (adc_complete_flag) {
//            // Get throttle
//            convert_adc_throttle(&throttle_sensor, raw_adc_value);
//
//            // Reset ADC input
//            adc_complete_flag = 0;
//            HAL_ADC_Start_DMA(&hadc2, (uint32_t*) &raw_adc_value, 1);
//        }
        osDelay(100);
    }
    /* USER CODE END 5 */
}

/* sendStateDisplayCallback function */
void sendStateDisplayCallback(void const* argument) {
    /* USER CODE BEGIN sendStateDisplayCallback */
    // Write state data to TX data
    tx_data.int_val = 0;

    tx_data.bytes[0] = race_state.race_mode;
    tx_data.bytes[1] = race_state.rain_state;
    tx_data.bytes[2] = moto_state;
    tx_data.bytes[3] = moto_charge;
    tx_data.bytes[4] = charger_comm_state.state;
    tx_data.bytes[5] = bms_comm_state;
    tx_data.bytes[6] = button_moto;
    tx_data.bytes[7] = 90;  // state of health, TODO

    // Send the message
    send_CAN_message(0x202, &tx_data, &tx_header, &hfdcan1);
    /* USER CODE END sendStateDisplayCallback */
}

/* rearlightControlCallback function */
void rearlightControlCallback(void const* argument) {
    /* USER CODE BEGIN rearlightControlCallback */
    if (race_state.rain_state == STATE_RAIN) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
    } else if (race_state.race_mode == MODE_RACE) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
    }

    check_moto_state(moto_state);
    /* USER CODE END rearlightControlCallback */
}

/* sendThrottleDisplayCallback function */
void sendThrottleDisplayCallback(void const* argument) {
    /* USER CODE BEGIN sendThrottleDisplayCallback */
//    tx_data.int_val = 0;
//    tx_data.second.float_val = throttle_sensor.throttle_value.float_val;
//    send_CAN_message(0x102, &tx_data, &tx_header, &hfdcan1);
    // TODO: Remove code below
    tx_data.int_val = 0x0000002000000030;
    send_CAN_message(0x711, &tx_data, &tx_header, &hfdcan1);
    /* USER CODE END sendThrottleDisplayCallback */
}

/* ledClusterCallback function */
void ledClusterCallback(void const* argument) {
    /* USER CODE BEGIN ledClusterCallback */
    check_moto_state_LED(&moto_state);
    // HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8); // change l’état de la pin // change to correct one
    /* USER CODE END ledClusterCallback */
}

/* relayReadCallback function */
void relayReadCallback(void const* argument) {
    /* USER CODE BEGIN relayReadCallback */
    readRelay(&moto_state);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);
    /* USER CODE END relayReadCallback */
}

/* CallbackCharge function */
void CallbackCharge(void const* argument) {
    /* USER CODE BEGIN CallbackCharge */
    if (moto_state == STATE_CHARGE) {
        handle_charger_CAN(&tx_data_four, &tx_header, &charger_comm_state, &hfdcan1, bat);
    }
    /* USER CODE END CallbackCharge */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM6) {
        HAL_IncTick();
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}
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

/**
 * @}
 */

/**
 * @}
 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }  // error Handler
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
