#include "main.h"
#include "userconfig.h"
#include "version.h"
#include "inputpin.h"
#include "swd_pins.h"
#include "wdt.h"
#include "phaseout.h"
#include "sense.h"
#include "rc.h"
#include "led.h"
#include "crsf.h"
#include "hw_tests.h"
#include <math.h>

RcChannel* rc1 = NULL;
RcChannel* rc2 = NULL;
RcChannel* rc3 = NULL; // only used in direct-PWM mode

#if defined(DEVELOPMENT_BOARD)
Cereal_USART dbg_cer;
#endif

RcPulse_InputCap rc_pulse_1(IC_TIMER_REGISTER, INPUT_PIN_PORT, INPUT_PIN, IC_TIMER_CHANNEL);
RcPulse_GpioIsr  rc_pulse_2;
CrsfChannel      crsf_1;
CrsfChannel      crsf_2;
CrsfChannel      crsf_3; // only used in direct-PWM mode
Cereal_USART     main_cer;
#ifdef ENABLE_COMPILE_CLI
Cereal_TimerBitbang cli_cer;
#endif

#ifdef ENABLE_COMPILE_CLI
void cliboot_decide(void);
void cli_enter(void);
#endif

void direct_pwm(int32_t v1, int32_t v2, int32_t v3, uint32_t duty_max);
void tank_drive_arcade_mix(int throttle, int steering, int* out_left, int* out_right);

int main(void)
{
    mcu_init();

    #ifdef HW_TESTS
    hw_test();
    #endif

    #ifdef DEVELOPMENT_BOARD
    dbg_cer.init(CEREAL_ID_USART_DEBUG, DEBUG_BAUD, false, false);
    #endif

    led_init();

    pwm_init();
    pwm_all_flt();
    sense_init();
    dbg_printf("low level init done at %u\r\n", millis());

    eeprom_unlock_key = EEPROM_MAGIC;
    eeprom_load_or_default();
    eeprom_unlock_key = 0;

    load_runtime_configs();

    #ifdef ENABLE_COMPILE_CLI
    cliboot_decide();
    inp_pullDown();
    inp2_pullDown();
    swdpins_deinit();
    #if defined(DISABLE_LED) || defined(ENABLE_TONE)
    tone_stop();
    load_runtime_configs();
    #endif
    #endif

    eeprom_unlock_key = 0; // prevents any writes to EEPROM

    ledblink_disarmed();

    // setup inputs
    if (cfg.input_mode == INPUTMODE_RC || cfg.input_mode == INPUTMODE_RC_SWD)
    {
        rc_pulse_1.init();
        if (cfg.input_mode == INPUTMODE_RC) {
            rc_pulse_2.init(GPIOEXTI_TIMx, GPIOEXTI_GPIO, GPIOEXTI_Pin);
        }
        else if (cfg.input_mode == INPUTMODE_RC_SWD) {
            swclk_init(LL_GPIO_PULL_DOWN);
            rc_pulse_2.init(GPIOEXTI_TIMx, GPIOA, GPIO_PIN_SWCLK);
        }

        rc1 = &rc_pulse_1;
        rc2 = &rc_pulse_2;

        dbg_printf("input mode [%u] RC\r\n", cfg.input_mode);
    }
    else if (cfg.input_mode == INPUTMODE_CRSF || cfg.input_mode == INPUTMODE_CRSF_SWCLK)
    {
        #if defined(MAIN_SIGNAL_PA2)
        // for ESCs that use PA2 for input, we don't know if the user connected CRSF to PA2 or PB6
        // so we simply wait for one of the signals to go high
        // it is also possible that the CLI decision loop already detected a signal
        #if DEFAULT_INPUT_MODE == INPUTMODE_CRSF
        // if the hardware specified forces CRSF as default, it means there's no secondary signal, use PA2/USART2 as input
        if (crsf_inputGuess == 0) {
            crsf_inputGuess = 2;
        }
        #endif
        while (crsf_inputGuess == 0)
        {
            // unknown guess means wait for the one of the signals to become high, which means a USART is connected
            led_task(false);
            sense_task();
            current_limit_task(0, 0);
            if (inp_read() != 0) { // this is checking PA2/USART2
                crsf_inputGuess = 2;
                break;
            }
            if (inp2_read() != 0) { // this is checking PB6/USART1
                crsf_inputGuess = 1;
                break;
            }
        }
        #endif
        main_cer.init((cfg.input_mode == INPUTMODE_CRSF_SWCLK) ? CEREAL_ID_USART_SWCLK : CEREAL_ID_USART_CRSF
                        , (cfg.baud == 0) ? CRSF_BAUDRATE : cfg.baud
                        , false, true, true);
        crsf_1.init(&main_cer, cfg.channel_1);
        crsf_2.init(&main_cer, cfg.channel_2);
        rc1 = &crsf_1;
        rc2 = &crsf_2;
        if (cfg.voltage_split_mode == VSPLITMODE_DIRECTPWM) {
            crsf_3.init(&main_cer, cfg.channel_mode);
            rc3 = &crsf_3;
        }

        crsf_setMasterArmChan(cfg.channel_masterarm);

        dbg_printf("input mode [%u] CRSF\r\n", cfg.input_mode);
    }
    else
    {
        dbg_printf("input mode [%u] ERROR UNKNOWN\r\n", cfg.input_mode);

        #ifdef ENABLE_COMPILE_CLI
        ledblink_cli();
        cli_enter(); // this never returns
        #else
        while (true)
        {
            // do nothing forever due to unconfigured input
            led_task(false);
        }
        #endif
    }

    wdt_init();

    dbg_printf("init finished at %u\r\n", millis());

    bool armed_both = false;

    while (true) // main forever loop
    {
        //dbg_pintoggle(0); // for measuring loop time

        led_task(false);
        sense_task();
        battery_task();

        rc1->task();
        rc2->task();
        if (rc3 != NULL) { rc3->task(); }

        bool need_debug_print = false;
        #ifdef DEBUG_PRINT
        static uint32_t last_debug_time = 0;
        if ((millis() - last_debug_time) >= 200)
        {
            need_debug_print = true;
            last_debug_time = millis();
        }
        #endif

        int v1, v2, v3;
        int drv_throttle, drv_steering, drv_left, drv_right;

        if (cfg.tied == false)
        {
            if (armed_both == false) {
                if (rc1->is_armed() && rc2->is_armed()
                    && ((rc1->read() == 0 && rc2->read() == 0) || (cfg.channel_masterarm > 0))
                    ) { // require both to be simultaneously zero, or using master-arm feature
                    // check rc3 if direct-PWM mode
                    if (rc3 == NULL) {
                        armed_both = true;
                    }
                    else if (rc3->is_armed() && rc3->read() == 0) {
                        armed_both = true;
                    }
                }
            }
            else {
                if (rc1->is_armed() == false || rc2->is_armed() == false) { // disarm both on any disarm
                    armed_both = false;
                }
                // with CRSF, it is not possible for rc3 to be armed if rc1 or rc2 isn't armed
            }

            v1 = armed_both ? rc1->read() : 0;
            v2 = armed_both ? rc2->read() : 0;
            v3 = armed_both && rc3 != NULL ? rc3->read() : 0;

            if (cfg.tank_arcade_mix)
            {
                drv_throttle = v1;
                drv_steering = v2;
                tank_drive_arcade_mix(drv_throttle, drv_steering, &drv_left, &drv_right);
                v1 = drv_left;
                v2 = drv_right;
            }
        }
        else // controls are tied, take from one channel and apply to both
        {
            if (rc1->is_armed() != false) {
                v3 = v2 = v1 = rc1->read();
                armed_both = true;
            }
            else if (rc2->is_armed() != false) {
                v3 = v2 = v1 = rc2->read();
                armed_both = true;
            }
            else {
                v1 = v2 = v3 = 0;
                armed_both = false;
            }
        }
        if (cfg.chan_swap) {
            int vs = v1; v1 = v2; v2 = vs;
        }
        v1 *= cfg.flip_1 ? -1 : 1;
        v2 *= cfg.flip_2 ? -1 : 1;

        if (armed_both == false)
        {
            if (rc1->is_armed() || rc2->is_armed()) {
                ledblink_1armed();
            }
            else {
                ledblink_disarmed();
            }
            pwm_all_flt();
            if (need_debug_print) {
                dbg_printf("[%u] disarmed (%d %d), v=%lu   c=%u\r\n", millis(), rc1->read(), rc2->read(), sense_voltage, sense_current);
            }
            current_limit_task(0, 0);
            wdt_feed();
            continue; // do not execute the rest of the logic
        }
        else if (v1 == 0 && v2 == 0) {
            ledblink_armed_stopped();
        }
        else {
            ledblink_moving();
        }

        int32_t duty_max = cfg.pwm_period - cfg.pwm_deadtime;
        bool limit_reached = false;

        // impose temperature limiting if desired
        if (cfg.temperature_limit > 0 && sense_temperatureC > cfg.temperature_limit) {
            duty_max = fi_map(sense_temperatureC, cfg.temperature_limit, cfg.temperature_limit + TEMPERATURE_OVER, cfg.pwm_period / 2, 1, true);
            limit_reached = true;
        }

        // impose current limiting if desired
        if (cfg.current_limit > 0) {
            current_limit_task(v1, v2);
            if (duty_max > current_limit_duty) {
                limit_reached |= (current_limit_duty < (duty_max - (duty_max / 8)));
                duty_max = current_limit_duty;
            }
        }

        // impose voltage limit if desired
        if (voltage_limit > 0) {
            if (sense_voltage < voltage_limit) {
                int32_t lower_limit = ((int32_t)voltage_limit) - (cfg.lowbatt_stretch > UNDERVOLTAGE ? cfg.lowbatt_stretch : UNDERVOLTAGE);
                duty_max = fi_map(sense_voltage, lower_limit, voltage_limit, 0, duty_max, true);
                ledblink_lowbatt();
            }
        }

        if (limit_reached) {
            ledblink_currentlimit();
        }

        uint32_t duty_mid = (duty_max + 1) / 2; // this represents the center tap voltage if motors are going in opposite directions

        // determine the voltage boosting mode, either using a stick/switch value or using the mode set in EEPROM
        uint8_t boost_mode = ((cfg.channel_mode > 0) ?
                fi_map(crsf_readChan(cfg.channel_mode), CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_2000, 0, 29, true) / 10
                :
                cfg.voltage_split_mode
                ); // % VSPLITMODE_END;

        // set braking mode live from a switch on the transmitter
        if (cfg.channel_brake > 0) {
            pwm_set_braking(crsf_readChan(cfg.channel_brake) >= CRSF_CHANNEL_VALUE_MID);
        }

        int a1 = v1 >= 0 ? v1 : -v1;
        int a2 = v2 >= 0 ? v2 : -v2;
        int p1 = fi_map(a1, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true);
        int p2 = fi_map(a2, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true);
        int total_power = p1 + p2;
        bool use_half_voltage = false;

        if (cfg.voltage_split_mode == VSPLITMODE_DIRECTPWM)
        {
            // this needs to be the first if statement, so that it overrides everything
            direct_pwm(v1, v2, v3, duty_max);
        }
        else if (v1 == 0 && v2 == 0) // both stopped, at least one is armed, so braking will be active if needed
        {
            pwm_set_all_duty_remapped(0, 0, 0);
        }
        else if (boost_mode == VSPLITMODE_BOOST_ALWAYS)
        {
            if ((v1 > 0 && v2 >= 0) || (v1 >= 0 && v2 > 0)) // both forward
            {
                pwm_set_all_duty_remapped(0,
                    fi_map(v1, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true),
                    fi_map(v2, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true));
            }
            else if ((v1 < 0 && v2 <= 0) || (v1 <= 0 && v2 < 0)) // both reverse
            {
                pwm_set_all_duty_remapped(duty_max,
                    duty_max - fi_map(-v1, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true),
                    duty_max - fi_map(-v2, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true));
            }
            else // directions oppose
            {
                int tries = 2;
                while (total_power > duty_max && tries--) // not enough voltage available, slow down proportionally
                {
                    p1 = fi_map(p1, 0, total_power, 0, duty_max, true);
                    p2 = fi_map(p2, 0, total_power, 0, duty_max, true);
                    total_power = p1 + p2;
                }
                if (v1 > v2) {
                    pwm_set_all_duty_remapped(p2, total_power, 0);
                }
                else {
                    pwm_set_all_duty_remapped(p1, 0, total_power);
                }
            }
        }
        else if (boost_mode == VSPLITMODE_BOOST_FORWARD)
        {
            int px = fi_map(p1, 0, duty_max, 0, duty_mid, true);
            int py = fi_map(p2, 0, duty_max, 0, duty_mid, true);
            int pm = px < py ? px : py;

            if (v1 >= 0 && v2 >= 0) // both forward
            {
                pwm_set_all_duty_remapped(duty_mid - pm, duty_mid + px, duty_mid + py);
            }
            else if (v1 <= 0 && v2 <= 0) // both reverse
            {
                pwm_set_all_duty_remapped(duty_mid + pm, duty_mid - px, duty_mid - py);
            }
            else // directions oppose
            {
                use_half_voltage = true;
            }
        }
        else // half-voltage or unknown
        {
            use_half_voltage = true;
        }

        if (use_half_voltage) {
            pwm_set_all_duty_remapped(duty_mid,
                fi_map(v1, -THROTTLE_UNIT_RANGE, THROTTLE_UNIT_RANGE, 0, duty_max, true),
                fi_map(v2, -THROTTLE_UNIT_RANGE, THROTTLE_UNIT_RANGE, 0, duty_max, true));
        }

        if (need_debug_print) {
            dbg_printf("[%u], v=%lu   c=%u , ", millis(), sense_voltage, sense_current);
            if (cfg.tank_arcade_mix) {
                dbg_printf("%d, %d, %d, %d, ; ", drv_throttle, drv_steering, drv_left, drv_right);
            }
            pwm_debug_report();
            if (cfg.voltage_split_mode != VSPLITMODE_DIRECTPWM) {
                dbg_printf("; ");
                pwm_debug_report_drive();
            }
            if (cfg.current_limit > 0) {
                dbg_printf("; %u ", current_limit_duty);
            }
            dbg_printf("\r\n");
        }

        wdt_feed();
    }

    return 0;
}

void tank_drive_arcade_mix(int throttle, int steering, int* out_left, int* out_right)
{
    int left  = throttle + steering;
    int right = throttle - steering;
    left  = left  >  THROTTLE_UNIT_RANGE ?  THROTTLE_UNIT_RANGE : left;
    right = right >  THROTTLE_UNIT_RANGE ?  THROTTLE_UNIT_RANGE : right;
    left  = left  < -THROTTLE_UNIT_RANGE ? -THROTTLE_UNIT_RANGE : left;
    right = right < -THROTTLE_UNIT_RANGE ? -THROTTLE_UNIT_RANGE : right;
    *out_left  = left;
    *out_right = right;
}

void direct_pwm(int32_t v1, int32_t v2, int32_t v3, uint32_t duty_max)
{
    v1 = fi_map(v1 < 0 ? -v1 : v1, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true);
    v2 = fi_map(v2 < 0 ? -v2 : v2, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true);
    v3 = fi_map(v3 < 0 ? -v3 : v3, 0, THROTTLE_UNIT_RANGE, 0, duty_max, true);

    #define DIRPWM_TEMPLATE(lt, val)    do {                   \
    if (cfg.dirpwm_chancfg_ ##lt == DIRPWM_PUSHPULL) {         \
        pwm_setPWM_CP_ ##lt ();                                \
    }                                                          \
    else if (cfg.dirpwm_chancfg_ ##lt == DIRPWM_HIGHONLY) {    \
        pwm_setPWM_HI_ ##lt ();                                \
    }                                                          \
    else if (cfg.dirpwm_chancfg_ ##lt == DIRPWM_OPENDRAIN) {   \
        pwm_setPWM_OD_ ##lt ();                                \
        val = cfg.pwm_period - val;                            \
    }                                                          \
    else {                                                     \
        pwm_setFlt_ ##lt ();                                   \
        val = 0;                                               \
    }                                                          \
    pwm_setDuty_ ##lt (val);                                   \
    } while (0)                                                \

    DIRPWM_TEMPLATE(A, v1);
    DIRPWM_TEMPLATE(B, v2);
    DIRPWM_TEMPLATE(C, v3);
}

#include "release_checks.h"
