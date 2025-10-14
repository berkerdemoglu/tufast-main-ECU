/*
 * CAN_functions.c
 *
 *  Created on: Aug 31, 2025
 *      Author: jadlostan
 */

#include "CAN_functions.h"

void convert_float_display(can_message_four* msg_in, can_message_four* msg_out, int decimal_points) {
    // Used for MoTeC
    msg_out->int_val = (uint32_t) (msg_in->float_val * decimal_points);
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

    thr->throttle_value.float_val = 0.0f;  // init with 0 for safety
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
        th->throttle_value.float_val = output_value;
    } else {
        th->throttle_value.float_val = 0.0f;
    }
}

void check_moto_state(enum MotoState moto_state) {
    switch (moto_state) {
        case STATE_SAFE:
            HAL_GPIO_TogglePin(PORT_GREEN_LED, PIN_GREEN_LED);
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

void send_CAN_message(uint32_t address,
    can_message_eight* msg,
    FDCAN_TxHeaderTypeDef* tx_header,
    FDCAN_HandleTypeDef* hfdcan1) {
    // Update ID of the transmit header
    tx_header->Identifier = address;

    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan1, tx_header, msg->bytes) != HAL_OK) {
        __disable_irq();
        while (1) {
        }  // error Handler
    }
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_8);  //  light flashing to see if transmits
}
void send_CAN_message_four(uint32_t address,
    can_message_four* msg,
    FDCAN_TxHeaderTypeDef* tx_header,
    FDCAN_HandleTypeDef* hfdcan1) {
    // Update ID of the transmit header
    tx_header->Identifier = address;
    tx_header->DataLength = FDCAN_DLC_BYTES_4;

    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan1, tx_header, msg->bytes) != HAL_OK) {
        __disable_irq();
        while (1) {
        }  // error Handler
    }
    tx_header->DataLength = FDCAN_DLC_BYTES_8;
}
void send_turn_on_inverter(FDCAN_TxHeaderTypeDef* tx_header, FDCAN_HandleTypeDef* hfdcan1) {
    // Sends an ON message to the inverter
    can_message_eight inverter_on_msg = { .int_val = 0x0101010101010101 };

    send_CAN_message(0x201, &inverter_on_msg, tx_header, hfdcan1);
}

// BMS and Charger functions
void charger_comms_init(struct ChargerCommState* ccs) {
    ccs->state = CHARGER_OFF;
    ccs->flag_byte = 1;  // TODO: maybe init with 0?
}
void handle_charger_CAN(can_message_eight* tx_data,
    FDCAN_TxHeaderTypeDef* tx_header,
    struct ChargerCommState* charger_comm_state,
    FDCAN_HandleTypeDef* hfdcan1) {
    // Send a message to MoTeC that will be relayed to the charger
    switch (charger_comm_state->state) {
        case CHARGER_OFF:
            tx_data->second.int_val = 0x00000000;
            break;
        case CHARGER_ON:
            tx_data->second.int_val = 0x00010000;
            break;
        case CHARGER_VOUT_SET:
            // byte 3 - frame format, byte 2 - the value we want
            tx_data->second.int_val = 0x1B580020;
            break;
        case CHARGER_IOUT_SET:
            tx_data->second.int_val = 0x27100020;
            break;
        default:  // charger_comm_state == FAULT_STATUS
            // TODO
            break;
    }
    tx_data->first.int_val = charger_comm_state->flag_byte;

    // Update the flag byte
    if (charger_comm_state->flag_byte == 1) {
        charger_comm_state->flag_byte = 2;
    } else {
        charger_comm_state->flag_byte = 0;
    }

    // Send the message to the dashboard
    send_CAN_message(0x302, tx_data, tx_header, hfdcan1);
}

void handle_BMS_CAN(uint8_t value,
    can_message_eight* tx_data,
    FDCAN_TxHeaderTypeDef* tx_header,
    enum BMSCommState* bms_comm_state,
    FDCAN_HandleTypeDef* hfdcan1) {
    // TODO: this function needs more work!
    if (*bms_comm_state == BMS_SLEEP) {
        tx_data->bytes[0] = 0x20;
        tx_data->bytes[1] = 0;
        tx_data->bytes[2] = 0x10;
        tx_data->bytes[3] = 0x27;
        tx_data->bytes[4] = 0x20;
        tx_data->bytes[5] = 0;
        tx_data->bytes[6] = 0x10;
        tx_data->bytes[7] = 0x27;
    } else if (*bms_comm_state == BMS_ON) {
        tx_data->bytes[0] = 0x20;
        tx_data->bytes[1] = 0;
        tx_data->bytes[2] = 0x10;
        tx_data->bytes[3] = 0x27;
        tx_data->bytes[4] = 0x20;
        tx_data->bytes[5] = 0;
        tx_data->bytes[6] = 0x10;
        tx_data->bytes[7] = 0x27;
    }
    send_CAN_message(BMS_RXID, tx_data, tx_header, hfdcan1);
}

// Display transmission functions
void send_throttle_display(struct Throttle* th,
    FDCAN_TxHeaderTypeDef* tx_header,
    FDCAN_HandleTypeDef* hfdcan1,
    can_message_eight* tx_data) {
    // Send throttle in the first 4 bytes
    th->throttle_value.float_val *= 2;  // TODO: fix, this could be a problem!
    convert_float_display(&th->throttle_value, &tx_data->first, DECIMAL_POINT_2);

    tx_data->second.int_val = 0;
    send_CAN_message(0x102, tx_data, tx_header, hfdcan1);
}

void send_velocity_ref_inverter(struct Throttle* th,
    FDCAN_TxHeaderTypeDef* tx_header,
    FDCAN_HandleTypeDef* hfdcan1,
    can_message_eight* tx_data,
    struct Throttle* throttle_sensor) {
    // Check for safe throttle (and RPM) values
    if (throttle_sensor->throttle_value.float_val <= 100.0f) {
        tx_data->first.int_val = 0;
        tx_data->second.float_val = 1 * throttle_sensor->throttle_value.float_val;
        send_CAN_message(0x301, tx_data, tx_header, hfdcan1);

        send_turn_on_inverter(tx_header, hfdcan1);
    }
}
