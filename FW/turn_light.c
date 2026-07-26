//
// Created by all on 26. 11. 2025.
//

#include "turn_light.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"



#define TURN_L_FRONT_OUT_PIN 8   //predne smerovky pre kratky pulz
#define TURN_R_FRONT_OUT_PIN 23   //predne smerovky pre kratky pulz
#define TURN_R_PWM_OUT_PIN   6          //slice 3 A
#define TURN_L_PWM_OUT_PIN   7          //slice 3 B

#define TURN_LIGHT_R_21_IN_PIN 16
#define TURN_LIGHT_L_22_IN_PIN 22

#define TURN_LIGHT_R_MASK (1<<TURN_LIGHT_R_21_IN_PIN)
#define TURN_LIGHT_L_MASK (1<<TURN_LIGHT_L_22_IN_PIN)

#define TURN_LIGHT_PWM_INCREMENT 100
#define TURN_LIGHT_PWM_DECREMENT 50

#define TURN_LIGHT_INC_TIME_US 10000 //us
#define TURN_LIGHT_DEC_TIME_US 7500 //us
#define TURN_LIGHT_HOLD_TIME_US 100000 //us
#define TURN_LIGHT_FRONT_ON_TIME_USEC 100000
#define TURN_LIGHT_TOP_VALUE 2000

static uint pwm_slice_num_turn_light;  //smerovka PWM slice
static uint pwm_chan_num_turn_light_L;
static uint pwm_chan_num_turn_light_R;



static uint32_t turn_light_prev_state = TURN_LIGHT_L_MASK | TURN_LIGHT_R_MASK;  //L=1, R=1 - vypnute
static volatile bool turn_light_L_is_on = false;   //ze su smerovky zapnute
static volatile bool turn_light_R_is_on = false;   //ze su smerovky zapnute
static uint32_t f_turn_light_L_time;
static uint32_t f_turn_light_R_time;


//nastavit jas smeroviek 0...2000
void turn_light_L_set (uint16_t val) {
    pwm_set_chan_level(pwm_slice_num_turn_light, pwm_chan_num_turn_light_L, val);
}
void turn_light_R_set (uint16_t val) {
    pwm_set_chan_level(pwm_slice_num_turn_light, pwm_chan_num_turn_light_R, val);
}

void f_turn_light_L_on (void) {
    gpio_put(TURN_L_FRONT_OUT_PIN,true);
}
void f_turn_light_L_off (void) {
    gpio_put(TURN_L_FRONT_OUT_PIN,false);
}
void f_turn_light_R_on (void) {
    gpio_put(TURN_R_FRONT_OUT_PIN,true);
}
void f_turn_light_R_off (void) {
    gpio_put(TURN_R_FRONT_OUT_PIN,false);
}



void turn_light_init (void) {
    //init pins
    gpio_set_function_masked(
            (1<<TURN_L_FRONT_OUT_PIN)
            |(1<<TURN_R_FRONT_OUT_PIN)
            |(1<<TURN_LIGHT_R_21_IN_PIN)
            |(1<<TURN_LIGHT_L_22_IN_PIN)
            ,GPIO_FUNC_SIO);

    gpio_set_dir_in_masked(
        (1<<TURN_LIGHT_R_21_IN_PIN)
        |(1<<TURN_LIGHT_L_22_IN_PIN));

    gpio_set_dir_out_masked(
    (1<<TURN_L_FRONT_OUT_PIN)
        |(1<<TURN_R_FRONT_OUT_PIN));

    //enable pullups on turn_lights, lights inputs
    gpio_set_pulls(TURN_LIGHT_L_22_IN_PIN,true,false);
    gpio_set_pulls(TURN_LIGHT_R_21_IN_PIN,true,false);

    gpio_set_function_masked((1<<TURN_L_PWM_OUT_PIN) | (1<<TURN_R_PWM_OUT_PIN), GPIO_FUNC_PWM);

    pwm_slice_num_turn_light = pwm_gpio_to_slice_num(TURN_L_PWM_OUT_PIN); //L a R maju tu istu slice
    pwm_chan_num_turn_light_L = pwm_gpio_to_channel(TURN_L_PWM_OUT_PIN);
    pwm_chan_num_turn_light_R = pwm_gpio_to_channel(TURN_R_PWM_OUT_PIN);

    //smerovky
    //pre PWM 2000Hz: f = fsys/((1999+1) x clkdiv) = 125000000/((1999+1) x 31.25) = 2000
    //perioda = 0.5ms -> 2000
    //rozsah 0 - 0.5ms -> 0 - 2000
    //31.25 = 31 + 4/16   8bit int  + 4bit frac
    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac4(&c,31,4);
    pwm_config_set_clkdiv_mode(&c, PWM_DIV_FREE_RUNNING);
    pwm_config_set_wrap(&c, 1999);
    pwm_config_set_output_polarity(&c,false,false);
    pwm_init(pwm_slice_num_turn_light, &c, true);
    turn_light_L_set(0);
    turn_light_R_set(0);
    f_turn_light_L_off();
    f_turn_light_R_off();

}

void turn_light_service (void) {
    uint32_t dt;
    static uint32_t ftL;
    static uint32_t ftR;
    static uint32_t tL;
    static uint32_t tR;
    static uint32_t valR;
    static uint32_t valL;
    static bool upL;
    static bool upR;

    //detekuje hranu a 1->0 potom necha zapnute urcity cas smerovky



    uint32_t turnlight_new_state = gpio_get_all();
    turnlight_new_state &= TURN_LIGHT_L_MASK | TURN_LIGHT_R_MASK;
     uint32_t tmp = turn_light_prev_state ^ turnlight_new_state;  //ak je zmena = tmp = 1
    tmp &= turn_light_prev_state;    //predchadzajuci stav ak je 1 tak toto je pozadadovana hrana 1->0
    turn_light_prev_state = turnlight_new_state;


    if (!turn_light_L_is_on && (tmp & TURN_LIGHT_L_MASK)) {
        //turnlight_L switched on
        turn_light_L_is_on = true;
        ftL = time_us_32() + TURN_LIGHT_FRONT_ON_TIME_USEC ;
        f_turn_light_L_on();

        valL = TURN_LIGHT_PWM_INCREMENT;
        turn_light_L_set(valL);
        tL = time_us_32() + TURN_LIGHT_INC_TIME_US;
        upL = true;
    }
    if (!turn_light_R_is_on && (tmp & TURN_LIGHT_R_MASK)) {
        //turnlight_R switched on
        turn_light_R_is_on = true;
        ftR = time_us_32() + TURN_LIGHT_FRONT_ON_TIME_USEC ;
        f_turn_light_R_on();

        valR = TURN_LIGHT_PWM_INCREMENT;
        turn_light_R_set(valR);
        tR = time_us_32() + TURN_LIGHT_INC_TIME_US;
        upR = true;
    }



    if (turn_light_L_is_on) {
        dt = (time_us_32() - ftL);
        if ((int32_t) dt >= 0) {
            f_turn_light_L_off();
        }

        dt = (time_us_32() - tL);
        if ((int32_t) dt >= 0) {
            if (upL) {
                valL += TURN_LIGHT_PWM_INCREMENT;
                turn_light_L_set(valL);
                if (valL == TURN_LIGHT_TOP_VALUE) {
                    tL += TURN_LIGHT_HOLD_TIME_US;
                    upL = false;
                }
                else {
                    tL += TURN_LIGHT_INC_TIME_US;
                }
            }
            else {
                valL -= TURN_LIGHT_PWM_DECREMENT;
                turn_light_L_set(valL);
                if (valL == 0) {
                    turn_light_L_is_on = false;
                }
                else {
                    tL += TURN_LIGHT_DEC_TIME_US;
                }
            }
        }
    }


    if (turn_light_R_is_on) {
        dt = (time_us_32() - ftR);
        if ((int32_t) dt >= 0) {
            f_turn_light_R_off();
        }

        dt = (time_us_32() - tR);
        if ((int32_t) dt >= 0) {
            if (upR) {
                valR += TURN_LIGHT_PWM_INCREMENT;
                turn_light_R_set(valR);
                if (valR == TURN_LIGHT_TOP_VALUE) {
                    tR += TURN_LIGHT_HOLD_TIME_US;
                    upR = false;
                }
                else {
                    tR += TURN_LIGHT_INC_TIME_US;
                }
            }
            else {
                valR -= TURN_LIGHT_PWM_DECREMENT;
                turn_light_R_set(valR);
                if (valR == 0) {
                    turn_light_R_is_on = false;
                }
                else {
                    tR += TURN_LIGHT_DEC_TIME_US;
                }
            }
        }
    }

}