#pragma once

#ifndef PWM_ENABLE_BRIDGE

#define PHASE_GPIO_PORT_LOW_A    PHASE_A_GPIO_PORT_LOW
#define PHASE_GPIO_PORT_HIGH_A   PHASE_A_GPIO_PORT_HIGH
#define PHASE_GPIO_LOW_A         PHASE_A_GPIO_LOW
#define PHASE_GPIO_HIGH_A        PHASE_A_GPIO_HIGH
#define PHASE_GPIO_PORT_LOW_B    PHASE_B_GPIO_PORT_LOW
#define PHASE_GPIO_PORT_HIGH_B   PHASE_B_GPIO_PORT_HIGH
#define PHASE_GPIO_LOW_B         PHASE_B_GPIO_LOW
#define PHASE_GPIO_HIGH_B        PHASE_B_GPIO_HIGH
#define PHASE_GPIO_PORT_LOW_C    PHASE_C_GPIO_PORT_LOW
#define PHASE_GPIO_PORT_HIGH_C   PHASE_C_GPIO_PORT_HIGH
#define PHASE_GPIO_LOW_C         PHASE_C_GPIO_LOW
#define PHASE_GPIO_HIGH_C        PHASE_C_GPIO_HIGH

#else

#define PHASE_GPIO_PORT_ENABLE_A    PHASE_A_GPIO_PORT_ENABLE
#define PHASE_GPIO_PORT_ENABLE_B    PHASE_B_GPIO_PORT_ENABLE
#define PHASE_GPIO_PORT_ENABLE_C    PHASE_B_GPIO_PORT_ENABLE
#define PHASE_GPIO_PORT_PWM_A       PHASE_A_GPIO_PORT_PWM
#define PHASE_GPIO_PORT_PWM_B       PHASE_B_GPIO_PORT_PWM
#define PHASE_GPIO_PORT_PWM_C       PHASE_B_GPIO_PORT_PWM
#define PHASE_GPIO_ENABLE_A         PHASE_A_GPIO_ENABLE
#define PHASE_GPIO_ENABLE_B         PHASE_B_GPIO_ENABLE
#define PHASE_GPIO_ENABLE_C         PHASE_B_GPIO_ENABLE
#define PHASE_GPIO_PWM_A            PHASE_A_GPIO_PWM
#define PHASE_GPIO_PWM_B            PHASE_B_GPIO_PWM
#define PHASE_GPIO_PWM_C            PHASE_B_GPIO_PWM

#endif