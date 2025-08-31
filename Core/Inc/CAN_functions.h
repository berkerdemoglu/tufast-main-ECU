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
#ifndef INC_CAN_FUNCTIONS_H_
#define INC_CAN_FUNCTIONS_H_

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

void send_can_message_four(uint32_t address, can_message_four* msg);
void send_can_message_eight(uint32_t address, can_message_eight* msg);
void send_turn_on_inverter(void);
void send_velocity_ref_inverter(struct Throttle* th);

// Display CAN transmit functions
void convert_float_display(can_message_four* msg_in, can_message_four* msg_out, int decimal_points);
void send_throttle_display(struct Throttle* th);
void send_race_mode_display(struct RaceState* rs);
void send_rain_state_display(struct RaceState* rs);

// BMS, Charger, Output Pins related
void handle_BMS_CAN(void);
void handle_charger_CAN(uint8_t value, can_message_four* tx_data_four, FDCAN_TxHeaderTypeDef* tx_header,
        enum ChargerCommState* charger_comm_state);

// Throttle functions
void throttle_init(struct Throttle* thr);
void convert_adc_throttle(struct Throttle* th, uint16_t raw_adc_value);

#endif /* INC_CAN_FUNCTIONS_H_ */
