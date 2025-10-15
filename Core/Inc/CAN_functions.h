/*
 * CAN_functions.h
 *
 *  Created on: Aug 31, 2025
 *      Author: jadlostan
 */

#include "stm32g4xx_hal.h"
#include "defines.h"
#include "motostruct.h"
#include "stm32g4xx_nucleo.h"
#include <stdio.h>
#include "cmsis_os.h"
#ifndef INC_CAN_FUNCTIONS_H_
#define INC_CAN_FUNCTIONS_H_

// Type definitions
typedef union {
    float float_val;
    uint32_t int_val;
    uint8_t bytes[4];
} can_message_four;

typedef union {
    uint64_t int_val;
    double double_val;
    struct {
        can_message_four first;
        can_message_four second;
    };
    uint8_t bytes[8];
} can_message_eight;

struct can_message_mail_obj {
    can_message_eight data;
    uint32_t id;
    uint32_t length;
};

// Throttle
#define THROTTLE_BUFFER_SIZE 32
struct Throttle {
    float adc_sum;
    float buffer[THROTTLE_BUFFER_SIZE];
    uint8_t buffer_index;

    can_message_four throttle_value;
    float hysteresis;
    float hysteresis_min;

    uint8_t throttle_activated;  // flag
};

void send_turn_on_inverter(FDCAN_TxHeaderTypeDef* tx_header, FDCAN_HandleTypeDef* hfdcan1);
void send_velocity_ref_inverter(struct Throttle* th, FDCAN_TxHeaderTypeDef* tx_header, FDCAN_HandleTypeDef* hfdcan1,
    can_message_eight* tx_data, struct Throttle* throttle);

// Display CAN transmit functions
void convert_float_display(can_message_four* msg_in, can_message_four* msg_out, int decimal_points);
void send_throttle_display(struct Throttle* th, FDCAN_TxHeaderTypeDef* tx_header, FDCAN_HandleTypeDef* hfdcan1,
    can_message_eight* tx_data);

// BMS, Charger, Output Pins related
void charger_comms_init(struct ChargerCommState* ccs);
void handle_charger_CAN(can_message_eight* tx_data,
    FDCAN_TxHeaderTypeDef* tx_header,
    struct ChargerCommState* charger_comm_state,
    FDCAN_HandleTypeDef* hfdcan1);
void handle_BMS_CAN(uint8_t value,
    can_message_eight* tx_data,
    FDCAN_TxHeaderTypeDef* tx_header,
    enum BMSCommState* bms_comm_state,
    FDCAN_HandleTypeDef* hfdcan1);

// Throttle functions
void throttle_init(struct Throttle* thr);
void convert_adc_throttle(struct Throttle* th, uint16_t raw_adc_value);

void check_moto_state(enum MotoState moto_state);
void send_CAN_message(uint32_t address, can_message_eight* msg, FDCAN_TxHeaderTypeDef* tx_header,
    FDCAN_HandleTypeDef* hfdcan1);
void send_CAN_message_four(uint32_t address, can_message_four* msg, FDCAN_TxHeaderTypeDef* tx_header,
    FDCAN_HandleTypeDef* hfdcan1);
void convert_BMS_CAN(uint8_t receive_BMS[8], battery* bat);

#endif /* INC_CAN_FUNCTIONS_H_ */
