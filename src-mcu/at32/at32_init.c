#include "at32.h"
#include "systick.h"
#include "stm32_at32_compat.h"

void SystemClock_Config(void);
void system_clock_config(void);
extern void pwm_all_low(void);

void mcu_init(void)
{
    system_clock_config();

    #if defined(MCU_AT421)
    crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK,    TRUE);
    crm_periph_clock_enable(CRM_SCFG_PERIPH_CLOCK,   TRUE);
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK,  TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK,  TRUE);
    crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK,   TRUE);
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_TMR1_PERIPH_CLOCK,   TRUE);
    crm_periph_clock_enable(CRM_TMR3_PERIPH_CLOCK,   TRUE);
    crm_periph_clock_enable(CRM_TMR6_PERIPH_CLOCK,   TRUE);
    crm_periph_clock_enable(CRM_TMR15_PERIPH_CLOCK,  TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK,   TRUE);
    #endif

    pwm_all_low();

    systick_init();
    __enable_irq();
    // vector table offset is taken care of by SystemInit() in system_at32f421.c
}

void gpio_mode_QUICK(gpio_type* gpio_periph, uint32_t mode, uint32_t pull_up_down, uint32_t pin)
{
    //#if defined(MCU_AT421)
    ////gpio_periph->cfgr = (((((gpio_periph->cfgr))) & (~(((pin * pin) * (0x3UL << (0U)))))) | (((pin * pin) * mode)));
    //LL_GPIO_SetPinMode(gpio_periph, pin, mode);
    //LL_GPIO_SetPinPull(gpio_periph, pin, pull_up_down);
    //#elif defined(MCU_AT415)
    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode = mode;
    gpio_init_struct.gpio_pins = pin;
    gpio_init_struct.gpio_pull = pull_up_down;
    gpio_init(gpio_periph, &gpio_init_struct);
    //#endif
}
