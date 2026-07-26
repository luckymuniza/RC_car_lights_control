//
// Created by all on 16. 12. 2025.
//

#include "rear_light.h"

#include <stdio.h>

#include "stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"


#define REAR_LIGHT_OUT_PWM_PIN 10    //slice 5 A
#define REAR_LIGHT_19_IN_PIN 26  //ADC0

//vstupne urovne signalu pre zadne svetlo
//#define REAR_LIGHT_OFF_LEVEL 3000
//#define REAR_LIGHT_ON_LEVEL 1500
#define REAR_LIGHT_BRAKE_LEVEL 2000    //1600 brzdy,   2250 svetla

#define REAR_LIGHT_OFF_PWM_LEVEL 0 //0-2000
#define REAR_LIGHT_ON_PWM_LEVEL 500 //0-2000
#define REAR_LIGHT_BRAKE_PWM_LEVEL 2000 //0-2000

uint pwm_slice_num_rearlight;  //slice
uint pwm_chan_num_rearlight;  //channel A/B

bool rear_light_is_on = false;
bool brake_light_is_on = false;

static inline void my_adc_start_conversion(void) {
    hw_set_bits(&adc_hw->cs, ADC_CS_START_ONCE_BITS);
}


//zapnut,vypny, brzdy zadne svetlo.
inline void rear_light_on (void){
    if (brake_light_is_on == false) {
        pwm_set_chan_level(pwm_slice_num_rearlight, pwm_chan_num_rearlight, REAR_LIGHT_ON_PWM_LEVEL); //vypnute
    }
    rear_light_is_on = true;
}
inline void rear_light_off (void){
    if (brake_light_is_on == false) {
        pwm_set_chan_level(pwm_slice_num_rearlight, pwm_chan_num_rearlight, REAR_LIGHT_OFF_PWM_LEVEL); //vypnute
    }
    rear_light_is_on = false;
}
static void rear_light_brake_on (void){
    pwm_set_chan_level(pwm_slice_num_rearlight, pwm_chan_num_rearlight, REAR_LIGHT_BRAKE_PWM_LEVEL); //vypnute
}




//plavajuci priemer /32
static inline uint32_t my_adc_get_data(void) {
    static uint16_t buf[32] = {0};
    static uint8_t idx = 0; //kde sa ma prave zapisat
    static uint32_t sum = 0;

    if (adc_hw->cs & ADC_CS_READY_BITS) {
        sum -= buf[idx];
        buf[idx] = (uint16_t ) adc_hw->result;;
        sum += buf[idx++];
        if (idx == 32) {idx =0;}
        my_adc_start_conversion();
    }
    return sum >> 5;
}


void rear_light_init(void) {
    //ADC init for rearlight
    //init PWM pins
    gpio_set_function_masked(
        (1<<REAR_LIGHT_OUT_PWM_PIN), GPIO_FUNC_PWM);
    pwm_slice_num_rearlight = pwm_gpio_to_slice_num(REAR_LIGHT_OUT_PWM_PIN);
    pwm_chan_num_rearlight = pwm_gpio_to_channel(REAR_LIGHT_OUT_PWM_PIN);
    pwm_config c = pwm_get_default_config();
    //zadne a brzdove
    //pre PWM 500Hz: f = fsys/((1999+1) x clkdiv) = 125000000/((1999+1) x 125)
    //perioda = 2ms -> 2000
    //rozsah 0 - 2ms -> 0 - 2000
    pwm_config_set_clkdiv_int(&c, 125);
    pwm_config_set_clkdiv_mode(&c, PWM_DIV_FREE_RUNNING);
    pwm_config_set_wrap(&c, 1999);
    pwm_config_set_output_polarity(&c,false,false);
    pwm_init(pwm_slice_num_rearlight, &c, true);
    rear_light_off(); //vypnute

    adc_init();
    adc_gpio_init(REAR_LIGHT_19_IN_PIN);
    adc_select_input(0);
    my_adc_start_conversion();

}
//len zapne brzdy ak su
void rear_light_service(void) {
    uint32_t tmp = my_adc_get_data();
    //printf("Rear Light: %u\n", tmp);
    if (tmp < REAR_LIGHT_BRAKE_LEVEL) {
        //brzdy on
        rear_light_brake_on();
        brake_light_is_on = true;
    }
    else {
        brake_light_is_on = false;
        if (rear_light_is_on) {
            //zapni svetla
            rear_light_on();
        }
        else {
            rear_light_off();
        }
    }
}


/*
void rear_light_service(void) {
    uint32_t tmp = my_adc_get_data();
    if (tmp > REAR_LIGHT_OFF_LEVEL) {
        //zhasni svetla
        rearlight_off();
    }
    else if ((tmp < REAR_LIGHT_ON_LEVEL) && (tmp > REAR_LIGHT_BRAKE_LEVEL)) {
        //zapni svetla
        rearlight_on();
    }
    else if (tmp < REAR_LIGHT_BRAKE_LEVEL){
        //zapni brzdov
        rearlight_brake_on();
    }
}
*/
