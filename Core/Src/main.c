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

/* USER CODE BEGIN PV */
// Race state
struct RaceState race_state;
enum MotoState moto_state = STATE_ERROR;
enum ChargerCommState charger_comm_state = CHARGER_ON;
enum BMSCommState bms_comm_state = BMS_SLEEP;
enum MotoCharge moto_charge = STATE_PRECHARGE;
struct ButonMoto buton_moto;
// Sensors
struct Throttle throttle_sensor;

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
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);  //  light flashing to see if receives

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
                case 0x301:
                    // Read which button was pressed
                    button_data_test.int_val = rx_data.int_val;
                    handle_button_press(&race_state, rx_data.bytes[0]);  // we pass the adress of race_state
                    break;
                    // BMS
                case 0x341:
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

// Race state

void handle_button_press(struct RaceState* rs, uint8_t button_index) {
    if (button_index == 1) {
        // Rain state update, green button
        // TODO: possibly replace with a simple bit inversion
        if (rs->rain_state == STATE_NO_RAIN) {
            rs->rain_state = STATE_RAIN;
        } else {  // rs->rain_state == STATE_RAIN
            rs->rain_state = STATE_NO_RAIN;
            // Turn off rearlight
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
        }

        // Send rain state update message to display
        send_rain_state_display(rs);
    } else {
        // Race mode update
        switch (button_index) {
            // TODO: Maybe use an enum for the button indices and names
            case 2:  // White
                if (rs->race_mode == MODE_GYMKHANA) {
                    rs->race_mode = MODE_RACE;
                } else {
                    rs->race_mode = MODE_GYMKHANA;
                }
                break;
            case 3:  // Black
                if (rs->race_mode == MODE_ECO) {
                    rs->race_mode = MODE_RACE;
                } else {
                    rs->race_mode = MODE_ECO;
                }
                break;
            case 4:  // Yellow
                if (rs->race_mode == MODE_SENSOR_READING) {
                    rs->race_mode = MODE_RACE;
                } else {
                    rs->race_mode = MODE_SENSOR_READING;
                }
                break;
            case 5:  // Blue
                if (rs->race_mode == MODE_PIT_LIMITER) {
                    rs->race_mode = MODE_RACE;
                } else {
                    rs->race_mode = MODE_PIT_LIMITER;
                }
                break;
        }

        // Send race mode update message to display
        send_race_mode_display(rs);
    }
}

// TODO: Update tx_header every time
void send_CAN_message(uint32_t address, can_message_eight* msg) {
    // Update ID of the transmit header
    tx_header.Identifier = address;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, msg->bytes) != HAL_OK) {
        Error_Handler();
    }
}
void send_CAN_message_four(uint32_t address, can_message_four* msg) {
    // Update ID of the transmit header
    tx_header.Identifier = address;
    tx_header.DataLength = FDCAN_DLC_BYTES_4;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, msg->bytes) != HAL_OK) {
        Error_Handler();
    }
    tx_header.DataLength = FDCAN_DLC_BYTES_8;
}

void send_turn_on_inverter(void) {
    // Sends an ON message to the inverter
    send_CAN_message(0x201, &inverter_on_msg);
}

void send_velocity_ref_inverter(struct Throttle* th) {
    // Check for safe throttle (and RPM) values
    if (throttle_sensor.throttle_value.float_val <= 100.0f) {
        tx_data.first.int_val = 0;
        tx_data.second.float_val = 1 * throttle_sensor.throttle_value.float_val;
        send_CAN_message(0x301, &tx_data);

        send_turn_on_inverter();
    }
}

// Display transmission functions
void send_throttle_display(struct Throttle* th) {
    // Send throttle in the first 4 bytes
    th->throttle_value.float_val *= 2;  // TODO: fix, this could be a problem!
    convert_float_display(&th->throttle_value, &tx_data.first, DECIMAL_POINT_2);

    tx_data.second.int_val = 0;
    send_CAN_message(0x102, &tx_data);
}

void send_race_mode_display(struct RaceState* rs) {
    tx_data.int_val = 0;  // reset transmit data
    tx_data.bytes[0] = rs->race_mode;
    send_CAN_message(0x202, &tx_data);
}

void send_rain_state_display(struct RaceState* rs) {
    tx_data.int_val = 0;  // reset transmit data
    tx_data.bytes[0] = rs->rain_state;
    send_CAN_message(0x302, &tx_data);
}

// TODO: send_state_mode_display

// ADC functions
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    adc_complete_flag = 1;
}

// BMS and Charger functions
void handle_charger_CAN(uint8_t value, can_message_four* tx_data_four, FDCAN_TxHeaderTypeDef* tx_header,
        enum ChargerCommState* charger_comm_state) {
    if (*charger_comm_state == CHARGER_OFF) {
        tx_data_four->bytes[0] = 0;
        tx_data_four->bytes[1] = 0;
        tx_data_four->bytes[2] = 0;
        tx_data_four->bytes[3] = 0;
    } else if (*charger_comm_state == CHARGER_ON) {
        tx_data_four->bytes[0] = 0;
        tx_data_four->bytes[1] = 0;
        tx_data_four->bytes[2] = 1;
        tx_data_four->bytes[3] = 0;
    } else if (*charger_comm_state == CHARGER_VOUT_SET) {
        tx_data_four->bytes[0] = 0x20;
        tx_data_four->bytes[1] = 0;
        tx_data_four->bytes[2] = 0x58;  // TODO: change the value to the one we need
        tx_data_four->bytes[3] = 0x1B; // frame format!
    } else if (*charger_comm_state == CHARGER_IOUT_SET) {
        tx_data_four->bytes[0] = 0x20;
        tx_data_four->bytes[1] = 0;
        tx_data_four->bytes[2] = 0x10;
        tx_data_four->bytes[3] = 0x27;
    } else {  // charger_comm_state == FAULT_STATUS
        // TODO: different action here
    }
    tx_header->IdType = FDCAN_EXTENDED_ID;
    send_CAN_message_four(CHARGER_RXID, tx_data_four);
    tx_header->IdType = FDCAN_STANDARD_ID;
}

void handle_BMS_CAN(void) {
    if (bms_comm_state == BMS_SLEEP) {
        tx_data.bytes[0] = 0x20;
        tx_data.bytes[1] = 0;
        tx_data.bytes[2] = 0x10;
        tx_data.bytes[3] = 0x27;
        tx_data.bytes[4] = 0x20;
        tx_data.bytes[5] = 0;
        tx_data.bytes[6] = 0x10;
        tx_data.bytes[7] = 0x27;
    } else if (bms_comm_state == BMS_ON) {
        tx_data.bytes[0] = 0x20;
        tx_data.bytes[1] = 0;
        tx_data.bytes[2] = 0x10;
        tx_data.bytes[3] = 0x27;
        tx_data.bytes[4] = 0x20;
        tx_data.bytes[5] = 0;
        tx_data.bytes[6] = 0x10;
        tx_data.bytes[7] = 0x27;
    }
    // TODO: this function probably needs more work!
    send_CAN_message(BMS_RXID, &tx_data);
}

void check_moto_state(uint8_t safe_time_delta) {
    switch (moto_state) {
        case STATE_SAFE:
            if (safe_time_delta > 200) {  // toggle every 200 ms
                HAL_GPIO_TogglePin(PORT_GREEN_LED, PIN_GREEN_LED);
            }
            break;
        case STATE_ENGAGED:
            set_output_pins(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
            break;
        case STATE_CHARGE:
            set_output_pins(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
            break;
        case STATE_ERROR:
            set_output_pins(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
            break;
    }
}
;

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

    // Init race state
    race_state_init(&race_state);
    buton_moto_init(&buton_moto);
    // Init sensor structs
    throttle_init(&throttle_sensor);
    /* USER CODE END 2 */

    /* Initialize leds */
    BSP_LED_Init(LED_GREEN);

    /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    // TODO: Maybe move up the part before loop to USER CODE 2
    // Turn on the inverter
    // TODO: Send only once (?) no Hal delay! just send the On once or in the while loop
    int time_sum = 0;
    while (time_sum < 5000) {
        send_turn_on_inverter();

        // CAN messages at 50 ms interval
        time_sum += 50;
        HAL_Delay(50);
    }

    // Send initial Race Mode and Rain State to display
    send_race_mode_display(&race_state);
    send_rain_state_display(&race_state);

    // Timers
    uint32_t time_last_5ms = HAL_GetTick();
    uint32_t time_last_50ms = HAL_GetTick();
    uint32_t time_last_200ms = HAL_GetTick();
    uint32_t time_now;

    /************************************************
     * SECTION : While loop
     ***********************************************/
    while (1) {
        time_now = HAL_GetTick();

        // Display
        if (time_now - time_last_5ms > 5) {
            send_throttle_display(&throttle_sensor);
            time_last_5ms = time_now;  // update last time
        }

        // Inverter
        if (time_now - time_last_50ms > 50) {
            send_velocity_ref_inverter(&throttle_sensor);
            time_last_50ms = time_now;  // update last time
        }

        // Rearlight
        if (time_now - time_last_200ms > 200) {
            if (race_state.rain_state == STATE_RAIN) {
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
            } else if (race_state.race_mode == MODE_RACE) {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
            } else {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
            }

            check_moto_state(time_now - time_last_200ms);

            time_last_200ms = time_now;  // update last time
        } // todo: Clean it and make a function

        // Other tasks
        if (adc_complete_flag) {
            // Get throttle
            convert_adc_throttle(&throttle_sensor, raw_adc_value);

            // Reset ADC input
            adc_complete_flag = 0;
            HAL_ADC_Start_DMA(&hadc2, (uint32_t*) &raw_adc_value, 1);
        } //todo: Clean it and make a function

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
    hfdcan1.Init.NominalPrescaler = 16;
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
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
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
    HAL_GPIO_WritePin(GPIOB, Precharge_Pin | Debug_LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pins : ESDB2_Pin ESDB_Pin Not_safe_Pin */
    GPIO_InitStruct.Pin = ESDB2_Pin | ESDB_Pin | Not_safe_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : Normal_Pin Charge_Led_Pin Error_LED_Pin Green_LED_Pin */
    GPIO_InitStruct.Pin = Normal_Pin | Charge_Led_Pin | Error_LED_Pin | Green_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : Precharge_Pin Debug_LED_Pin */
    GPIO_InitStruct.Pin = Precharge_Pin | Debug_LED_Pin;
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
