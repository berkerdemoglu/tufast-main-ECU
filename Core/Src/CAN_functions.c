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
