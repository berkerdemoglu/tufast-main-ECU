/*
 * motostruct.c
 *
 *  Created on: Aug 31, 2025
 *      Author: jadlostan
 */
#include "motostruct.h"

void handle_moto_discharge(enum MotoState* moto_state) {
    if (HAL_GPIO_ReadPin(PORT_RELAY_STATE, PIN_RELAY_STATE) == GPIO_PIN_SET) {  // change the pins
        *moto_state = STATE_PRECHARGE;
    }
    if (HAL_GPIO_ReadPin(PORT_RELAY_STATE, PIN_RELAY_STATE) == GPIO_PIN_RESET) {  // change the pins
        *moto_state = STATE_NORMAL;
    }
}

void handle_moto_state(enum MotoState* moto_state) {
    if (HAL_GPIO_ReadPin(PORT_RELAY_STATE, PIN_RELAY_STATE) == GPIO_PIN_SET) {
        *moto_state = STATE_SAFE;
    }
    if (HAL_GPIO_ReadPin(PORT_RELAY_STATE, PIN_RELAY_STATE) == GPIO_PIN_RESET) {
        *moto_state = STATE_ENGAGED;

        if (HAL_GPIO_ReadPin(PORT_NOT_SAFE, PIN_NOT_SAFE) == GPIO_PIN_RESET) {

            *moto_state = STATE_NOT_SAFE;

        }
    }
}

void set_output_pins(GPIO_PinState o1, GPIO_PinState o2, GPIO_PinState o3, GPIO_PinState o4) {
    HAL_GPIO_WritePin(PORT_PRECHARGE, PIN_PRECHARGE, o1);
    HAL_GPIO_WritePin(PORT_GREEN_LED, PIN_GREEN_LED, o2);
    HAL_GPIO_WritePin(PORT_CHARGE, PIN_CHARGE, o3);
    HAL_GPIO_WritePin(PORT_ERROR, PIN_ERROR, o4);
}

void race_state_init(struct RaceState* rs) {
    rs->rain_state = STATE_NO_RAIN;
    rs->race_mode = MODE_RACE;
}

void buton_moto_init(struct ButonMoto* bm) {
    bm->ESDB_one = 0;
    bm->ESDB_two = 0;
    bm->TSMS = 0;
    bm->LVMS = 0;
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
        //    send_rain_state_display(rs, &tx_header, &hfdcan1);
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
        // send_race_mode_display(rs, &tx_header, &hfdcan1);
    }
}

// TODO: Update tx_header every time
