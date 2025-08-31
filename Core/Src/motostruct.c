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
