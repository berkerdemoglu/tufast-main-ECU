/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"
#include "stm32g4xx_nucleo.h"
#include <stdio.h>

#include "defines.h"
#include "motostruct.h"
#include "CAN_functions.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/*
 * Checks if there is an error with the relay pin (?, TODO: rewrite)
 */
void fault_pin_service(void);

/*
 * Sets the pin state (SET or RESET) for the output pins.
 */

void check_moto_state(uint8_t precharge_time_delta);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ESDB2_Pin GPIO_PIN_0
#define ESDB2_GPIO_Port GPIOA
#define ESDB_Pin GPIO_PIN_1
#define ESDB_GPIO_Port GPIOA
#define Normal_Pin GPIO_PIN_4
#define Normal_GPIO_Port GPIOA
#define Charge_Led_Pin GPIO_PIN_5
#define Charge_Led_GPIO_Port GPIOA
#define Error_LED_Pin GPIO_PIN_6
#define Error_LED_GPIO_Port GPIOA
#define Throttle_Pin GPIO_PIN_7
#define Throttle_GPIO_Port GPIOA
#define Precharge_Pin GPIO_PIN_0
#define Precharge_GPIO_Port GPIOB
#define Sensata_Aux_Pin GPIO_PIN_8
#define Sensata_Aux_GPIO_Port GPIOA
#define Green_LED_Pin GPIO_PIN_9
#define Green_LED_GPIO_Port GPIOA
#define Not_safe_Pin GPIO_PIN_10
#define Not_safe_GPIO_Port GPIOA
#define Debug_LED_Pin GPIO_PIN_3
#define Debug_LED_GPIO_Port GPIOB
#define LVMS_Pin GPIO_PIN_4
#define LVMS_GPIO_Port GPIOB
#define TSMS_Pin GPIO_PIN_5
#define TSMS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
