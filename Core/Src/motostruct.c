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

            *moto_state = STATE_ERROR;

        }
    }
}

void set_output_pins(GPIO_PinState o1, GPIO_PinState o2, GPIO_PinState o3, GPIO_PinState o4) {
    HAL_GPIO_WritePin(PORT_PRECHARGE, PIN_PRECHARGE, o1);
    HAL_GPIO_WritePin(PORT_GREEN_LED, PIN_GREEN_LED, o2);
    HAL_GPIO_WritePin(Charge_Led_GPIO_Port, Charge_Led_Pin, o3);
    HAL_GPIO_WritePin(Error_LED_GPIO_Port, Error_LED_Pin, o4);
}

void race_state_init(struct RaceState* rs) {
    rs->rain_state = STATE_NO_RAIN;
    rs->race_mode = MODE_RACE;
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

void update_led_cluster(enum MotoState moto_state) {
    switch (moto_state) {
        case STATE_SAFE:
            uint8_t flipped_state = GPIO_PIN_SET - HAL_GPIO_ReadPin(PORT_GREEN_LED, PIN_GREEN_LED);
            set_output_pins(GPIO_PIN_RESET, flipped_state, GPIO_PIN_RESET, GPIO_PIN_RESET);
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

void readRelay(enum MotoState* moto_state) {
    uint8_t imd = HAL_GPIO_ReadPin(IMD_GPIO_Port, IMD_Pin);
    uint8_t tsms = HAL_GPIO_ReadPin(TSMS_GPIO_Port, TSMS_Pin);
    uint8_t charger = HAL_GPIO_ReadPin(RELAY_CHARGER_GPIO_Port, RELAY_CHARGER_Pin);

    if (imd == GPIO_PIN_RESET) {
        // Action 1 : not safe, error
        *moto_state = STATE_ERROR;
    } else if (tsms == GPIO_PIN_RESET) {
        // safe
        *moto_state = STATE_SAFE;
    } else if (charger == GPIO_PIN_SET) {
        // Charge
        *moto_state = STATE_CHARGE;
    } else {
        // engaged
        *moto_state = STATE_ENGAGED;
    }
//    *moto_state = STATE_ERROR;  // todo remove
}
// TODO: Update tx_header every time
