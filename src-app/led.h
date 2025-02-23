#pragma once

#include "mcu.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef USE_LED_STRIP
#include "ws2812.h"
#endif

#include "tone.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STMICRO)
#define LED_PORT_TYPE GPIO_TypeDef
#elif defined(ARTERY)
#define LED_PORT_TYPE gpio_type
#endif

void led_init(void); // call to initialize IO and state machine
void led_task(bool); // call continously to perform blinking

extern void ledhw_init(void); // MCU specific implementation
extern void led_set(LED_PORT_TYPE* GPIOx, uint32_t Pin, bool ison, bool opendrain);

void led_blink_set(uint8_t x);
void led_state_set(bool x);

void ledblink_cliwait(void);
void ledblink_cliplug(void);
void ledblink_disarmed(void);
void ledblink_1armed(void);
void ledblink_armed_stopped(void);
void ledblink_moving(void);
void ledblink_cli(void);
void ledblink_currentlimit(void);
void ledblink_lowbatt(void);

#ifdef __cplusplus
}
#endif
