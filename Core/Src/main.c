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
#include <stdbool.h>
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

I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
// Race state
// CAN
FDCAN_TxHeaderTypeDef tx_header;
can_message_eight tx_data;
can_message_four tx_data_four;
can_message_eight inverter_on_msg = { .sensor_int = 0x0101010101010101 };

FDCAN_RxHeaderTypeDef rx_header;
can_message_eight rx_data;

can_message_eight button_data_test;

// ADC
__IO uint8_t adc_complete_flag = 0;
uint16_t raw_adc_values[4];

//Accelerometer Declaration
struct Accelerometer accelerometer;
//Brake Temperature Declaration
float SteeringAngle,BrakeTemperature,PressureValue,LinearPotentiometerValue;
//Steering Angle
struct SteeringAngle steering_sensor;





/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);

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
                    //button_data_test.sensor_int = rx_data.sensor_int;
                    //handle_button_press(&race_state, rx_data.bytes[0]); todo: remove the comments
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

//set output

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
    ;
}

//ADC CallBack
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
	adc_complete_flag=1;
}





// Accelerometer Functions
void accelerometer_init(struct Accelerometer* accelerometer) {
    accelerometer->device_config[0] = 0x18;
    accelerometer->device_config[1] = 2;

    accelerometer->lin_acc_config[0] = 0x10;
    accelerometer->lin_acc_config[1] = 0xAA;

    accelerometer->ang_vel_config[0] = 0x11;
    accelerometer->ang_vel_config[1] = 0xB8;

    accelerometer->lin_acc_out_address = 0x28;
    accelerometer->ang_vel_out_address = 0x22;
}

void convert_acceleration(const uint8_t* data, float* ax, float* ay, float* az) {
    int16_t raw_x = (int16_t) ((data[1] << 8) | data[0]);
    int16_t raw_y = (int16_t) ((data[3] << 8) | data[2]);
    int16_t raw_z = (int16_t) ((data[5] << 8) | data[4]);

    *ax = (raw_x / 65535.0 * 8);
    *ay = (raw_y / 65535.0 * 8);
    *az = (raw_z / 65535.0 * 8);
}

void convert_angular_velocity(const uint8_t* data, float* ax, float* ay, float* az) {
    int16_t raw_x = (int16_t) ((data[1] << 8) | data[0]);
    int16_t raw_y = (int16_t) ((data[3] << 8) | data[2]);
    int16_t raw_z = (int16_t) ((data[5] << 8) | data[4]);

    *ax = (raw_x / 65535.0 * 1000 - 0.06);
    *ay = (raw_y / 65535.0 * 1000 + 0.24);
    *az = (raw_z / 65535.0 * 1000 + 0.03);
}


// Steering Angle Functions
void steering_angle_init(struct SteeringAngle* sa) {
    sa->adc_sum = 0;
    sa->buffer_index = 0;

    // Init buffer with zeroes
    // maybe this can also be done at initialization? TODO
    for (int i = 0; i < STEERING_BUFFER_SIZE; i++) {
        sa->buffer[i] = 0;
    }

    sa->steering_value.float_val = 0.0f;  // init with 0 for safety
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
    sa->steering_value.float_val = sa->adc_sum / 32.0f;
}



//Steering Angle Start
float SteeringAngleADC(uint16_t rawValue){
    float Steering=(float)(rawValue-3200)/4095*110;
    return Steering;
}

//Steering Angle End



//Brake Temperature Sensor - Start
float BrakeTemperatureADC(uint16_t rawValue){
	float Value;

	Value = 565.0/4095.0*(float)rawValue;
	return Value;
}
//Brake Temperature Sensor - End

//Pressure Sensor - Start
float PressureSensorADC(uint16_t rawValue){
	float Value;

	Value = 39.5/2916*(float)rawValue-1217.0/81.0;
	return Value;
}
//Pressure Sensor -End

//Linear Potentiometer - Start

float LinPotentiometer(uint16_t rawValue){

	float volt = 3.3f * ((float) rawValue) / 4096.0f;
	float calc = ((float) volt - 0.42f) * 100.0f / 1.65f;

	return calc;
}
//Linear Potentiometer - End

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
  MX_FDCAN1_Init();
  MX_I2C1_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
    // Start ADC2
    // Start FDCAN1
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        /* Notification Error */
        Error_Handler();
    }

    // Init race state
    // Init sensor structs
    //Accelerometer Init and I2C Init
        accelerometer_init(&accelerometer);

        HAL_I2C_Master_Transmit(&hi2c1, 0xd6, accelerometer.device_config, 2, 1000);

        HAL_I2C_Master_Transmit(&hi2c1, 0xd6, accelerometer.lin_acc_config, 2, 1000);
        HAL_I2C_Master_Transmit(&hi2c1, 0xd6, accelerometer.ang_vel_config, 2, 1000);

        HAL_ADC_Start_DMA(&hadc2, (uint32_t *)raw_adc_values,4);


  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {

        uint32_t sensor_value = 0;

        tx_data_four.sensor_int = sensor_value;
//        send_CAN_message_four(CHARGER_RXID, &tx_data_four);


        if (adc_complete_flag) {

        	BrakeTemperature=BrakeTemperatureADC(raw_adc_values[0]);
			SteeringAngle=SteeringAngleADC(raw_adc_values[1]);
			PressureValue=PressureSensorADC(raw_adc_values[2]);
			LinearPotentiometerValue=LinPotentiometer(raw_adc_values[3]);

        	adc_complete_flag=0;
        	HAL_ADC_Start_DMA(&hadc2, (uint32_t *)raw_adc_values,4);

        		}



        //Accelerometer I2C Start
        HAL_I2C_Master_Transmit(&hi2c1, 0xd6, &(accelerometer.lin_acc_out_address), 1, 1000);
        HAL_I2C_Master_Receive(&hi2c1, 0xd7, accelerometer.lin_acc_out_data, 6, 1000);
        HAL_Delay(10);
        HAL_I2C_Master_Transmit(&hi2c1, 0xd6, &(accelerometer.ang_vel_out_address), 1, 1000);
        HAL_I2C_Master_Receive(&hi2c1, 0xd7, accelerometer.ang_vel_out_data, 6, 1000);

        convert_acceleration(accelerometer.lin_acc_out_data, &(accelerometer.lin_acc_x), &(accelerometer.lin_acc_y), &(accelerometer.lin_acc_z));
        convert_angular_velocity(accelerometer.ang_vel_out_data, &(accelerometer.ang_vel_x), &(accelerometer.ang_vel_y), &(accelerometer.ang_vel_z));
        HAL_Delay(10);
        //Accelerometer I2C End






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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

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
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 4;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = DISABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_17;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
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
static void MX_FDCAN1_Init(void)
{

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
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
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
  hi2c1.Init.Timing = 0x10B17DB5;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

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
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

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
void Error_Handler(void)
{
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
