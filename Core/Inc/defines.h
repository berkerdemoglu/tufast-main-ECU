/*
 * defines.h
 *
 *  Created on: Aug 30, 2025
 *      Author: jadlostan
 */

#ifndef INC_DEFINES_H_
#define INC_DEFINES_H_

#define CHARGER_RXID        0x000C0100
#define CHARGER_TXID        0x000C0000
#define BMS_RXID            0x232

#define PORT_RELAY_STATE    GPIOA
#define PIN_RELAY_STATE     GPIO_PIN_8

#define PORT_ESDB   GPIOA
#define PIN_ESDB    GPIO_PIN_1

#define PORT_NOT_SAFE   GPIOA
#define PIN_NOT_SAFE    GPIO_PIN_10

#define PORT_PRECHARGE      GPIOB
#define PIN_PRECHARGE       GPIO_PIN_0 // not connected to LED

#define PORT_NORMAL         GPIOA
#define PIN_NORMAL          GPIO_PIN_4

#define PORT_CHARGE         GPIOA
#define PIN_CHARGE          GPIO_PIN_5

#define PORT_ERROR          GPIOA
#define PIN_ERROR           GPIO_PIN_6

#define PORT_GREEN_LED      GPIOA
#define PIN_GREEN_LED       GPIO_PIN_9

#define PORT_TSMS      GPIOB
#define PIN_TSMS       GPIO_PIN_5

#define PORT_LVMS      GPIOB
#define PIN_LVMS       GPIO_PIN_4

#define PORT_ESDB      GPIOA
#define PIN_ESDB       GPIO_PIN_1

#define PORT_ESDB_TWO      GPIOA
#define PIN_ESDB_TWO       GPIO_PIN_0

// The macros below are to be used in the float convert function for the display
#define DECIMAL_POINT_0 1
#define DECIMAL_POINT_1 10
#define DECIMAL_POINT_2 100

#endif /* INC_DEFINES_H_ */
