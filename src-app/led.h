#pragma once

#include "mcu.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef USE_LED_STRIP
#include "ws2812.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef STMICRO
#define LED_PORT_TYPE GPIO_TypeDef
#endif

void led_init(void); // call to initialize IO and state machine
void led_task(void); // call continously to perform blinking

extern void ledhw_init(void); // MCU specific implementation
extern void led_set(LED_PORT_TYPE* GPIOx, uint32_t Pin, bool ison, bool opendrain);

void ledblink_boot(void);
void ledblink_boot2(void);
void ledblink_disarmed(void);
void ledblink_armed_stopped(void);
void ledblink_moving(void);
void ledblink_cli(void);

#ifdef __cplusplus
}
#endif
