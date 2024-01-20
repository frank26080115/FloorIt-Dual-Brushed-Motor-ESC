#include "main.h"
#include "userconfig.h"
#include "inputpin.h"
#include "phaseout.h"
#include "sense.h"
#include "rc.h"
#include "led.h"
#include <math.h>

#ifdef STMICRO
#include "rc_stm32.h"
#include "cereal_timer.h"
#include "cereal_usart.h"
#endif

RcChannel* rc1 = NULL;
RcChannel* rc2 = NULL;

#if defined(STM32F051DISCO)
Cereal_USART* dbg_cer;
#endif

pidloop_t current_pid = {
    .Kp = 400,
    .Ki = 0,
    .Kd = 1000,
    .integral_limit = 20000,
    .output_limit = 100000
};
int16_t current_limit_val = 0;

#ifdef ENABLE_COMPILE_CLI
void boot_decide_cli(void);
void cli_enter(void);
#endif

int main(void)
{
    mcu_init();

    #if defined(STM32F051DISCO)
    dbg_cer = new Cereal_USART(3, CLI_BUFF_SIZE);
    dbg_cer->begin(CLI_BAUD, false, true, false);
    #endif

    led_init();
    inp_init();
    pwm_init();
    pwm_all_flt();

    eeprom_load_or_default();

    #ifdef ENABLE_COMPILE_CLI
    boot_decide_cli();
    #endif
    ledblink_disarmed();

    if (cfg.input_mode == INPUTMODE_RC)
    {
        rc1 = (RcChannel*)rc_makeInputCapture();
        rc2 = (RcChannel*)rc_makeGpioInput();
        // WARNING: this isn't very portable, but all of the ESCs so far only has one available input-capture pin
        // if there are two input capture pins, is it worth it to change the code to get slightly better jitter performance?
    }
    else if (cfg.input_mode == INPUTMODE_CRSF)
    {

    }

    while (true)
    {

    }

    return 0;
}

#ifdef ENABLE_COMPILE_CLI
void boot_decide_cli(void)
{
    ledblink_boot();

    if (inp_read() == 0) {
        while (inp_read() == 0) {
            led_task();
            if (millis() >= CLI_ENTER_LOW_CRITERIA) {
                inp_pullUp();
                ledblink_boot2();
                break;
            }
        }
        while (inp_read() == 0) {
            led_task();
        }
    }

    if (millis() < CLI_ENTER_LOW_CRITERIA) {
        // did not remain unplugged long enough
        // do not enter CLI
        return;
    }

    inp_pullDown();

    uint32_t t = millis();
    uint8_t low_pulse_cnt = 0;
    while (true) {
        led_task();
        // debounce low pulses from hot-plug
        if (inp_read() == 0) {
            // measure the low pulse
            uint32_t t2 = millis();
            while (inp_read() == 0) {
                led_task();
            }
            uint32_t t3 = millis();
            if ((t3 - t2 >= 5)) {
                // low pulse too long, it's probably a RC pulse
                // do not enter CLI
                return;
            }
            else {
                // low pulse very short
                low_pulse_cnt++;
                if (low_pulse_cnt >= 10) {
                    // too many of these low pulses
                    // do not enter CLI
                    return;
                }
            }
        }
        if ((millis() - t) >= CLI_ENTER_HIGH_CRITERIA) {
            // has been long enough
            ledblink_cli();
            cli_enter(); // this never returns
        }
    }
}
#endif

void current_limit_task()
{
    static uint32_t last_time = 0;
    if (cfg.current_limit <= 0) {
        current_limit_val = 0;
        return;
    }
    uint32_t now = millis();
    if (last_time == now) {
        return;
    }
    last_time = now;
    current_limit_val = (int16_t)lroundf((pid_calc(&current_pid, lroundf(sense_current), cfg.current_limit * 100) / 10000));
}
