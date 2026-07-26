//
// Created by all on 16. 12. 2025.
//

#include "to_mfc.h"

#include <time.h>

#include "ibus.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"


#define MFC_CHANEL_PWM_OUT_PIN 13     //slice 6 B

#define SWITCH_LIGHTS_LEVEL 1000
#define SWITCH_LIGHTS_TIME_HOLD_US 100000 //150ms

static uint pwm_chan_num_mfc;
static uint pwm_slice_num_mfc;  //RX chanel
static bool switch_lights = false;
static bool aux_1_active = false;

void to_mfc_init(void) {
    gpio_set_function_masked((1<<MFC_CHANEL_PWM_OUT_PIN), GPIO_FUNC_PWM);
    pwm_slice_num_mfc = pwm_gpio_to_slice_num(MFC_CHANEL_PWM_OUT_PIN);

    pwm_chan_num_mfc = pwm_gpio_to_channel(MFC_CHANEL_PWM_OUT_PIN);

    //to iste ale RX chanel ten kanal co riadi svetla
    pwm_config c = pwm_get_default_config();
    // ovladanie ako servo, 1 kanaly , jedna slice
    //pre PWM 50Hz: f = fsys/((19999+1) x clkdiv) = 125000000/((19999+1) x 125)
    //perioda = 20ms -> 20000
    //rozsah 1 - 2ms -> 1000 - 2000
    pwm_config_set_clkdiv_int(&c, 125);
    pwm_config_set_clkdiv_mode(&c, PWM_DIV_FREE_RUNNING);
    pwm_config_set_wrap(&c, 19999);
    pwm_config_set_output_polarity(&c,false,false);
    pwm_init(pwm_slice_num_mfc, &c, true);
    pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, 1500); //stred

}


void to_mfc_service (void) {
    static uint8_t switch_lights_seq = 0;
    static uint32_t t;
    uint32_t dt;

    if (aux_1_active) {
        pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, 1500);
        return;
    }

    if (switch_lights) {
        if (switch_lights_seq == 0) {
            pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, 1500);
            t = time_us_32() + SWITCH_LIGHTS_TIME_HOLD_US;
            switch_lights_seq = 1;
        }
        else if (switch_lights_seq == 1) {
            dt = time_us_32() - t;
            if ((int32_t) dt >=0 ) {
                pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, SWITCH_LIGHTS_LEVEL);
                t = time_us_32() + SWITCH_LIGHTS_TIME_HOLD_US;
                switch_lights_seq = 2;
            }
        }
        else if (switch_lights_seq == 2) {
            dt = time_us_32() - t;
            if ((int32_t) dt >=0 ) {
                pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, 1500);
                t = time_us_32() + SWITCH_LIGHTS_TIME_HOLD_US;
                switch_lights_seq = 3;
            }
        }
        else if (switch_lights_seq == 3) {
            dt = time_us_32() - t;
            if ((int32_t) dt >=0 ) {
                uint16_t val = ibus_get_channel(IBUS_CHAN_LIGHTS_MFC);
                pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, val);
                switch_lights_seq = 0;
                switch_lights = false;
            }
        }

    }
    else{
        uint16_t val = ibus_get_channel(IBUS_CHAN_LIGHTS_MFC);
        pwm_set_chan_level(pwm_slice_num_mfc, pwm_chan_num_mfc, val);
    }
}

void switch_lights_in_mfc(void) {
    switch_lights = true;
}

bool switching_lights_in_mfc (void) {
    return switch_lights;
}

void set_aux_1(bool v) {
    aux_1_active = v;
}