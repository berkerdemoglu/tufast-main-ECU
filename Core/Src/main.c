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

uint32_t ICValue = 0;
uint32_t Frequency = 0;
float Duty = 0;

COM_InitTypeDef BspCOMInit;

FDCAN_HandleTypeDef hfdcan1;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
// Race state
struct RaceState race_state;
MotoState moto_state = STATE_PRECHARGE;

// Sensors
struct Throttle throttle_sensor;
struct SteeringAngle steering_sensor;

// CAN
FDCAN_TxHeaderTypeDef tx_header;
can_message_eight tx_data;
can_message_eight inverter_on_msg = { .sensor_int = 0x0101010101010101 };

FDCAN_RxHeaderTypeDef rx_header;
can_message_eight rx_data;

can_message_eight button_data_test;

// ADC
__IO uint8_t adc_complete_flag = 0;
uint16_t raw_adc_values[2];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim) {
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)  // If the interrupt is triggered by channel 1
            {
        // Read the IC value
        ICValue = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        if (ICValue != 0) {
            // calculate the Duty Cycle
            Duty = (HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2) * 100) / ICValue;

            Frequency = 90000000 / ICValue;
        }
    }
}

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
                    button_data_test.sensor_int = rx_data.sensor_int;
                    handle_button_press(&race_state, rx_data.bytes[0]);
                    break;
            }
        }

        // Reactive receive notifications
        if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
            Error_Handler();
        }
    }
}

// error detection

void fault_pin_service() {
    if (HAL_GPIO_ReadPin(PORT_RELAY_STATE, PIN_RELAY_STATE) == GPIO_PIN_SET) {

        moto_state = STATE_ERROR;
    }
    if (HAL_GPIO_ReadPin(PORT_RELAY_STATE, PIN_RELAY_STATE) == GPIO_PIN_RESET) {
        if (moto_state == STATE_ERROR) {

            moto_state = STATE_NORMAL;
        }
    }
}

//set output

static inline void set_all(GPIO_PinState o1, GPIO_PinState o2, GPIO_PinState o3, GPIO_PinState o4) {
    HAL_GPIO_WritePin(PORT_PRECHARGE, PIN_PRECHARGE, o1);
    HAL_GPIO_WritePin(PORT_NORMAL, PIN_NORMAL, o2);
    HAL_GPIO_WritePin(PORT_CHARGE, PIN_CHARGE, o3);
    HAL_GPIO_WritePin(PORT_ERROR, PIN_ERROR, o4);
}

// Race state
void race_state_init(struct RaceState* rs) {
    rs->rain_state = STATE_NO_RAIN;
    rs->race_mode = MODE_RACE;
}

//check moto_state
void check_moto_state(uint8_t toggle_precharge) {
    switch (moto_state) {
        case STATE_PRECHARGE:
            // Actions spécifiques au précharge
            if (toggle_precharge > 200) {
                HAL_GPIO_TogglePin(PORT_NORMAL, PIN_NORMAL);

            }

            break;

        case STATE_NORMAL:
            // Actions spécifiques au mode normal
            set_all(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);

            break;

        case STATE_CHARGE:
            // Actions spécifiques à la charge
            set_all(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);

            break;

        case STATE_ERROR:
            // Actions en cas d'erreur
            set_all(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);

            break;
    }
}

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

void convert_float_display(can_message_four* msg_in, can_message_four* msg_out, int decimal_points) {
    // Used for MoTeC
    msg_out->sensor_int = (uint32_t) (msg_in->sensor_float * decimal_points);
}

void send_CAN_message(uint16_t address, can_message_eight* msg) {
    // Update ID of the transmit header
    tx_header.Identifier = address;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, msg->bytes) != HAL_OK) {
        Error_Handler();
    }
}

void send_turn_on_inverter(void) {
    // Sends a ON message to the inverter
    send_CAN_message(0x201, &inverter_on_msg);
}

void send_velocity_ref_inverter(struct Throttle* th) {
    // Check for safe throttle (and RPM) values
    if (throttle_sensor.throttle_value.sensor_float <= 100.0f) {
        tx_data.first.sensor_int = 0;
        tx_data.second.sensor_float = 1 * throttle_sensor.throttle_value.sensor_float;
        send_CAN_message(0x301, &tx_data);

        send_turn_on_inverter();
    }
}

// Display transmission functions
void send_throttle_steering_display(struct Throttle* th, struct SteeringAngle* sa) {
    // Send throttle in the first 4 bytes
    th->throttle_value.sensor_float *= 2;  // TODO: fix, this could be a problem!
    convert_float_display(&th->throttle_value, &tx_data.first, DECIMAL_POINT_2);

    // Send steering angle in the last 4 bytes
    // todo: REMOVE THE IF STATEMENTS HERE LATER, THIS IS JUST FOR ROLLOUT
    if (sa->steering_value.sensor_float < 0) {
        if (sa->steering_value.sensor_float < -24.0f) {
            sa->steering_value.sensor_float = 24.0f;
        } else {
            sa->steering_value.sensor_float = -sa->steering_value.sensor_float;
        }
    }
    convert_float_display(&sa->steering_value, &tx_data.second, DECIMAL_POINT_2);

    send_CAN_message(0x102, &tx_data);
}

void send_race_mode_display(struct RaceState* rs) {
    tx_data.sensor_int = 0;  // reset transmit data
    tx_data.bytes[0] = rs->race_mode;
    send_CAN_message(0x202, &tx_data);
}

void send_rain_state_display(struct RaceState* rs) {
    tx_data.sensor_int = 0;  // reset transmit data
    tx_data.bytes[0] = rs->rain_state;
    send_CAN_message(0x302, &tx_data);
}

// Throttle functions
void throttle_init(struct Throttle* thr) {
    thr->adc_sum = 0;
    thr->buffer_index = 0;
    thr->hysteresis = 2.0f;
    thr->hysteresis_min = 5.0f;
    thr->throttle_activated = 0;
    // Init buffer with zeroes
    // maybe this can also be done at initialization
    for (int i = 0; i < THROTTLE_BUFFER_SIZE; i++) {
        thr->buffer[i] = 0;
    }

    thr->throttle_value.sensor_float = 0.0f;  // init with 0 for safety
}

void convert_adc_throttle(struct Throttle* th, uint16_t adc_value) {
    // Calibration
    float volt = 3.3f * ((float) adc_value) / 4096.0f;  // TODO: we should always get 0 ?
    float calc = ((float) volt - 0.42f) * 100.0f / 1.65f;

    th->adc_sum -= th->buffer[th->buffer_index];

    // Add new sample
    th->buffer[th->buffer_index] = calc;
    th->adc_sum += calc;

    // Increment index
    th->buffer_index++;
    if (th->buffer_index >= THROTTLE_BUFFER_SIZE) {
        th->buffer_index = 0;
    }

    float output_value = th->adc_sum / THROTTLE_BUFFER_SIZE;

    if (output_value > 100.0f) {
        output_value = 100.0f;
    }

    // Hysteresis -- TODO: This could be cleaned up?
    if (output_value > th->hysteresis_min) {
        th->throttle_activated = 1;
    }
    if (th->throttle_activated == 1 && output_value < (th->hysteresis_min - th->hysteresis)) {
        th->throttle_activated = 0;
    }

    // Write output value
    if (th->throttle_activated == 1) {
        th->throttle_value.sensor_float = output_value;
    } else {
        th->throttle_value.sensor_float = 0.0f;
    }
}

// Steering angle functions
void steering_angle_init(struct SteeringAngle* sa) {
    sa->adc_sum = 0;
    sa->buffer_index = 0;

    // Init buffer with zeroes
    // maybe this can also be done at initialization
    for (int i = 0; i < THROTTLE_BUFFER_SIZE; i++) {
        sa->buffer[i] = 0;
    }

    sa->steering_value.sensor_float = 0.0f;  // init with 0 for safety
}

void steering_angle_avg(struct SteeringAngle* sa, float steering_value) {
    sa->adc_sum -= sa->buffer[sa->buffer_index];

    // Add new sample
    sa->buffer[sa->buffer_index] = steering_value;
    sa->adc_sum += steering_value;

    // Increment index
    sa->buffer_index++;
    if (sa->buffer_index >= 32) {
        sa->buffer_index = 0;
    }

    // Write average value
    sa->steering_value.sensor_float = sa->adc_sum / 32.0f;
}

// ADC functions
//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
//   adc_complete_flag = 1;
//}

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
    MX_FDCAN1_Init();
    MX_TIM1_Init();
    /* USER CODE BEGIN 2 */
    // Start ADC2
    //HAL_ADC_Start_DMA(&hadc2, (uint32_t*) raw_adc_values, 2);
    // Start FDCAN1
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        /* Notification Error */
        Error_Handler();
    }

    // Init race state
    race_state_init(&race_state);
    // Init sensor structs
    throttle_init(&throttle_sensor);
    steering_angle_init(&steering_sensor);

    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);   // main channel
    // HAL_TIM_IC_Start(&htim2, TIM_CHANNEL_2);   // indirect channel
    /* USER CODE END 2 */

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

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    uint32_t time_last_3000ms = HAL_GetTick();
    uint32_t time_last_200ms = HAL_GetTick();
    uint32_t time_now;
    while (1) {
        time_now = HAL_GetTick();

        if (time_now - time_last_3000ms > 3000) {
            moto_state = STATE_NORMAL;
            time_last_3000ms = -3000; // todo: change the code to be compliant with the rules
        }
        uint8_t toggle_precharge = time_now - time_last_200ms;
        fault_pin_service();
        check_moto_state(toggle_precharge);

    }
    // Turn on the inverter

    int time_sum = 0;
    while (time_sum < 5000) {
        send_turn_on_inverter();

        // CAN messages at 50 ms interval
        time_sum += 50;
        HAL_Delay(50);
    }

    // Send Race Mode and Rain State
    send_race_mode_display(&race_state);
    send_rain_state_display(&race_state);

    uint32_t time_last_5ms = HAL_GetTick();
    uint32_t time_last_50ms = HAL_GetTick();
    // uint32_t time_last_200ms = HAL_GetTick(); attention!

    //uint32_t time_now; ///// attention!
    while (1) {

        /* USER CODE END WHILE */
        HAL_Delay(1000);
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
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

    /* USER CODE BEGIN TIM1_Init 0 */

    /* USER CODE END TIM1_Init 0 */

    TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
    TIM_SlaveConfigTypeDef sSlaveConfig = { 0 };
    TIM_IC_InitTypeDef sConfigIC = { 0 };
    TIM_MasterConfigTypeDef sMasterConfig = { 0 };

    /* USER CODE BEGIN TIM1_Init 1 */

    /* USER CODE END TIM1_Init 1 */
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 65535;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_IC_Init(&htim1) != HAL_OK) {
        Error_Handler();
    }
    sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
    sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
    sSlaveConfig.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sSlaveConfig.TriggerPrescaler = TIM_ICPSC_DIV1;
    sSlaveConfig.TriggerFilter = 0;
    if (HAL_TIM_SlaveConfigSynchro(&htim1, &sSlaveConfig) != HAL_OK) {
        Error_Handler();
    }
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
    sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM1_Init 2 */

    /* USER CODE END TIM1_Init 2 */

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
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

    /*Configure GPIO pin : PB3 */
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
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
