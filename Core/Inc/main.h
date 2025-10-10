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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#define CHARGER_RXID  0x000C0100
#define CHARGER_TXID  0x000C0000

#define PORT_RELAY_STATE  GPIOA
#define PIN_RELAY_STATE  GPIO_PIN_8

#define PORT_PRECHARGE  GPIOB
#define PIN_PRECHARGE  GPIO_PIN_0 // not connected to LED

#define PORT_NORMAL  GPIOA
#define PIN_NORMAL   GPIO_PIN_4

#define PORT_CHARGE  GPIOA
#define PIN_CHARGE   GPIO_PIN_5

#define PORT_ERROR  GPIOA
#define PIN_ERROR   GPIO_PIN_6

// --- Define IDs for A2C2 ---
#define A2C2_ACC_X_ID          0x000C0B00
#define A2C2_ACC_Y_ID          0x000C0C00
#define A2C2_ACC_Z_ID          0x000C0D00
#define A2C2_ANGV_X_ID         0x000C0E00
#define A2C2_ANGV_Y_ID         0x000C0F00
#define A2C2_ANGV_Z_ID         0x000C1000
#define A2C2_BRAKE_TEMP_ID     0x000C1100
#define A2C2_PRESSURE2_ID      0x000C1200
#define A2C2_PRESSURE1_ID      0x000C1300
#define A2C2_POTENTIOMETER_ID  0x000C1400

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef union {
    float sensor_float;
    uint32_t sensor_int;
    uint8_t bytes[4];
} can_message_four;

typedef union {
    uint64_t sensor_int;  // we might not need this
    double sensor_double;  // we might not need this
    struct {
        can_message_four first;
        can_message_four second;
    };
    uint8_t bytes[8];
} can_message_eight;

typedef union {
    float float_val;
    uint32_t int_val;
    uint8_t bytes[4];
} bytes_four;

// motorcycle State

// Race modes

typedef enum {
    ON = 1,
    VOUT_SET = 2,
    IOUT_SET = 3,
    FAULT_STATUS = 4,
    OFF = 5
} ChargerCom;



//Sensors Begin

//Accelerometer Begin
struct Accelerometer {
    uint8_t lin_acc_out_address;
    uint8_t lin_acc_config[2];  // CTRL_1
    uint8_t lin_acc_out_data[6];
    float lin_acc_x;
    float lin_acc_y;
    float lin_acc_z;

    uint8_t ang_vel_out_address;
    uint8_t ang_vel_config[2];  // CTRL_2
    uint8_t ang_vel_out_data[6];
    float ang_vel_x;
    float ang_vel_y;
    float ang_vel_z;

    uint8_t device_config[2];  // CTRL_9
};
//Accelerometer End

//Steering Angle Start
#define STEERING_BUFFER_SIZE 32
struct SteeringAngle {
    bytes_four steering_value;
    float steering_output;

    float adc_sum;
    float buffer[STEERING_BUFFER_SIZE];
    uint8_t buffer_index;
};
void steering_angle_init(struct SteeringAngle* sa);
void steering_angle_avg(struct SteeringAngle* sa, float value);
//Steering Angle End


//Sensors End

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

void convert_float_display(can_message_four* msg_in, can_message_four* msg_out, int decimal_points);

void send_CAN_message(uint32_t address, can_message_eight* msg);
void send_CAN_message_four(uint32_t address, can_message_four* msg);

void steering_angle_init(struct SteeringAngle* sa);
void steering_angle_avg(struct SteeringAngle* sa, float value);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Linear_Potentiometer_Pin GPIO_PIN_0
#define Linear_Potentiometer_GPIO_Port GPIOA
#define Pressure_1_Pin GPIO_PIN_1
#define Pressure_1_GPIO_Port GPIOA
#define Brake_Temperature_Pin GPIO_PIN_4
#define Brake_Temperature_GPIO_Port GPIOA
#define Pressure_2_Pin GPIO_PIN_7
#define Pressure_2_GPIO_Port GPIOA
#define I2C_SCL_ACCELEROMETER_Pin GPIO_PIN_15
#define I2C_SCL_ACCELEROMETER_GPIO_Port GPIOA
#define I2C_SDA_ACCELEROMETER_Pin GPIO_PIN_7
#define I2C_SDA_ACCELEROMETER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
// Aswin throttle values (?)
#define SPEED_REFERENCE 1500.0f  // TODO: remove
#define MAX_RPM 1500.0f

// The macros below are to be used in the convert function
#define DECIMAL_POINT_0 1
#define DECIMAL_POINT_1 10
#define DECIMAL_POINT_2 100
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
