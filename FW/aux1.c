//
// Created by all on 12/31/2025.
//

#include "aux1.h"
#include <time.h>

#include "ibus.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "to_mfc.h"


#define AUX_1_PWM_OUT_PIN 4     //pwm slice 2

static uint pwm_chan_num_aux_1;
static uint pwm_slice_num_aux_1;  //RX chanel


void aux_1_init(void) {
    gpio_set_function_masked((1<<AUX_1_PWM_OUT_PIN), GPIO_FUNC_PWM);
    pwm_slice_num_aux_1 = pwm_gpio_to_slice_num(AUX_1_PWM_OUT_PIN);

    pwm_chan_num_aux_1 = pwm_gpio_to_channel(AUX_1_PWM_OUT_PIN);

    //to iste ale RX chanel ten kanal co riadi svetla
    pwm_config c = pwm_get_default_config();
    // ovladanie ako servo, 1 kanaly , jedna slice
    //pre PWM 50Hz: f = fsys/((19999+1) x clkdiv) = 125000000/((19999+1) x 125)
    //perioda = 20ms -> 20000
    //rozsah 1 - 2ms -> 1000 - 2000
    pwm_config_set_clkdiv_int(&c, 125);
    pwm_config_set_clkdiv_mode(&c, PWM_DIV_FREE_RUNNING);
    pwm_config_set_wrap(&c, 19999);
    pwm_config_set_output_polarity(&c,true,false);
    pwm_init(pwm_slice_num_aux_1, &c, true);
    pwm_set_chan_level(pwm_slice_num_aux_1, pwm_chan_num_aux_1, 1500); //stred

}
//Ked je prepnuty prepinac posiela kanal 2 do zeriavu
void aux_1_service(void) {
    uint16_t mode_sw = ibus_get_channel(IBUS_CHAN_MODE_SW);
    if (mode_sw >= 1800) {
        set_aux_1(true);
        uint16_t val = ibus_get_channel(IBUS_CHAN_LIGHTS_MFC);
        pwm_set_chan_level(pwm_slice_num_aux_1, pwm_chan_num_aux_1, val); //stred
    }
    else {
        set_aux_1(false);
        pwm_set_chan_level(pwm_slice_num_aux_1, pwm_chan_num_aux_1, 1500); //stred
    }
}