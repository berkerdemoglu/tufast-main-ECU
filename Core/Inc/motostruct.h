/*
 * motostruct.h
 *
 *  Created on: Aug 30, 2025
 *      Author: jadlostan
 */

#ifndef INC_MOTOSTRUCT_H_
#define INC_MOTOSTRUCT_H_
#include "stm32g4xx_hal.h"
#include "stm32g4xx_nucleo.h"
#include <stdio.h>

#include <stdbool.h>
#include "defines.h"

// The actual state of the motorcycle showed by the LED
enum MotoState {
    STATE_SAFE = 0,
    STATE_ENGAGED = 1,
    STATE_CHARGE = 2,
    STATE_ERROR = 3
};

enum MotoCharge {
    STATE_PRECHARGE = 1,
    STATE_NORMAL = 2,
};

// Communication state with the charger
enum ChargerState {
    CHARGER_ON = 1,
    CHARGER_VOUT_SET = 2,
    CHARGER_IOUT_SET = 3,
    CHARGER_FAULT_STATUS = 4,
    CHARGER_OFF = 5
};

struct ChargerCommState {
    enum ChargerState state;
    uint8_t flag_byte;
};

// Communication state with the BMS
enum BMSCommState {
    BMS_ON = 1,
    BMS_SLEEP = 2,
    BMS_VOLTAGE = 3,
    BMS_CURRENT = 4
};

// Racing-related modes and states
enum RaceMode {
    MODE_PIT_LIMITER = 1,
    MODE_RACE = 2,
    MODE_ECO = 3,
    MODE_SENSOR_READING = 4,
    MODE_GYMKHANA = 5
};

enum RainState {
    STATE_NO_RAIN = 0,
    STATE_RAIN = 1
};

struct RaceState {
    enum RainState rain_state;
    enum RaceMode race_mode;
};

typedef struct battery {
    uint16_t voltage;
    uint16_t current;
    uint16_t capacity;
    uint16_t soc;
    uint8_t raw[8];
} battery;  // TODO: Rename this struct and remove typedef

typedef union {
    uint8_t bytes[8];
    struct {
        uint16_t max_battery_voltage;
        uint16_t max_battery_charge_current;
        uint16_t min_battery_voltage;
        uint16_t max_battery_discharge_current;
    };
    uint64_t int_val;
} BMSMaxMinInfo;

struct BMSErrorState {
    uint8_t stop_charge_flag;
    uint8_t stop_discharge_flag;
    uint8_t primary_bmu_failure_status;
};

void handle_moto_discharge(enum MotoState* moto_state);
void handle_moto_state(enum MotoState* moto_state);
void set_output_pins(GPIO_PinState o1, GPIO_PinState o2, GPIO_PinState o3, GPIO_PinState o4);
void race_state_init(struct RaceState* rs);
void handle_button_press(struct RaceState* rs, uint8_t button_index);
void update_led_cluster(enum MotoState moto_state);
void readRelay(enum MotoState* moto_state);
#endif /* INC_MOTOSTRUCT_H_ */
