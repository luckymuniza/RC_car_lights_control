//
// Created by all on 16. 12. 2025.
//

#include "beacons.h"
#include <time.h>
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "ibus.h"
#include "lights.h"
#include "switch.h"


#define BEACON_SMALL_OUT_PWM_PIN 14  //slice 7 A
#define BEACON_BIG_OUT_PWM_PIN 15    //slice 7 B

#define PWM_OFF_LEVEL 1000
#define PWM_ON_LEVEL 2000

#define BEACON_SMALL_SWITCHING_PAUSE 50000 //[us] 50ms cas medzi zmenou pulsu on-off
#define BEACON_BIG_INITIALIZING_PAUSE 2500000 //[us]  1s ked sa posle prikaz na vypnutie po resete
#define BEACON_BIG_CONFIG_CHANGE_PAUSE 200000 //pausa pri zmene modu on- off


//beacons status
typedef enum {
    B_STATE_INITIALIZING,  //po resete sa musia povypinat
    B_STATE_OFF,
    B_STATE_ON,
    B_STATE_SWITCHING_OFF,  //sb sa musi vypinaty postupne cez vsetky mody
    B_STATE_SWITCHING_ON,
    B_STATE_CONFIG,
    B_STATE_CHANGE_MODE,
    B_STATE_CHANGE_BRIGHTNESS
}beacon_state;

//fog a search light status
typedef enum {
    FS_STATE_OFF,  //fog a search off
    FS_STATE_ON,   //fog a search on
    FS_STATE_F_ON, //only fog on
    FS_STATE_S_ON  //only search on
}fog_search_state;


static uint pwm_slice; //beacon small and big
static uint pwm_chan_num_beaconsmall;
static uint pwm_chan_num_beaconbig;

static switch_state sw_state = SW_STATE_OFF;
static beacon_state bb_state = B_STATE_OFF;
static beacon_state sb_state = B_STATE_OFF;
static fog_search_state fs_state = FS_STATE_OFF;


//musi vypnut a zapnut - jden cyklus
//fcia sa vola na nieklko x v cykle
void beacon_small_on(void) {
    static bool first_time = true;
    static uint32_t t;
    if (first_time) {
        sb_state = B_STATE_SWITCHING_ON;
        pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconsmall, PWM_OFF_LEVEL);
        first_time = false;
        t =  time_us_32() + BEACON_SMALL_SWITCHING_PAUSE;
    }
    else {
        uint32_t dt = time_us_32() - t;
        if ((int32_t) dt >= 0) {
            pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconsmall, PWM_ON_LEVEL);
            first_time = true;
            sb_state = B_STATE_ON;
        }
    }
}


//musi vypnut - zapnut ->>zmena jedneho modu - modov je 4, precyklit cez kazdy a potom je uz vypnuty
//zapina sa na prvy mod
//fcia sa vola na nieklko x v cykle
void beacon_small_off(void) {
    static uint8_t cnt = 0;
    static uint32_t t;
    static bool next_on;
    if (cnt == 0) {
        cnt = 1;
        sb_state = B_STATE_SWITCHING_OFF;
        pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconsmall, PWM_OFF_LEVEL);
        t = time_us_32() + BEACON_SMALL_SWITCHING_PAUSE;
        next_on = true;
    }
    else {
        uint32_t dt = time_us_32() - t;
        if((int32_t) dt >= 0) {
            if(next_on) {
                pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconsmall, PWM_ON_LEVEL);
                t = time_us_32() + BEACON_SMALL_SWITCHING_PAUSE;
                next_on = false;
            }
            else {
                pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconsmall, PWM_OFF_LEVEL);
                t = time_us_32() + BEACON_SMALL_SWITCHING_PAUSE;
                next_on = true;

            }
            cnt++;
            if (cnt == 8) {
                cnt = 0;
                sb_state = B_STATE_OFF;
            }
        }
    }
}


//najprv ho akoze zapne a po BEACON_BIG_INITIALIZING_PAUSE vypne
//fcia sa vola na nieklko x v cykle
void beacon_big_initializing(void) {
    static bool first_time = true;
    static uint32_t t;
    if (first_time) {
        bb_state = B_STATE_INITIALIZING;
        pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_ON_LEVEL);
        first_time = false;
        t =  time_us_32() + BEACON_BIG_INITIALIZING_PAUSE;
    }
    else {
        uint32_t dt = time_us_32() - t;
        if ((int32_t) dt >= 0) {
            pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_OFF_LEVEL);
            first_time = true;
            bb_state = B_STATE_OFF;
        }
    }
}

void beacon_big_on(void) {
    pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_ON_LEVEL);
    bb_state = B_STATE_ON;
}

void beacon_big_off(void) {
    pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_OFF_LEVEL);
    bb_state = B_STATE_OFF;
}

void beacon_big_change_mode(void) {
    static bool first_time = true;
    static uint32_t t;
    if (first_time) {
        bb_state = B_STATE_CHANGE_MODE;
        pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_OFF_LEVEL);
        first_time = false;
        t =  time_us_32() + BEACON_BIG_CONFIG_CHANGE_PAUSE;
    }
    else {
        uint32_t dt = time_us_32() - t;
        if ((int32_t) dt >= 0) {
            pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_ON_LEVEL);
            first_time = true;
            bb_state = B_STATE_CONFIG;
        }
    }
}

void beacon_big_change_brightness(void) {
    static uint8_t cnt = 0;
    static uint32_t t;
    if (cnt == 0) {
        bb_state = B_STATE_CHANGE_BRIGHTNESS;
        pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_OFF_LEVEL);
        cnt = 1;
        t =  time_us_32() + 1000000; //sekundu pocka po vypnuti
    }
    else if (cnt == 1) {
        uint32_t dt = time_us_32() - t;
        if ((int32_t) dt >= 0) {
            pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_ON_LEVEL);
            cnt = 2;
            t =  time_us_32() + BEACON_BIG_CONFIG_CHANGE_PAUSE; // pocka po zapnutui chvilku
        }
    }
    else if (cnt == 2) {
        uint32_t dt = time_us_32() - t;
        if ((int32_t) dt >= 0) {
            pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_OFF_LEVEL);
            cnt = 3;
            t =  time_us_32() + 1000000; //sekundu pocka po vypnuti
        }
    }
    else if (cnt == 3) {
        uint32_t dt = time_us_32() - t;
        if ((int32_t) dt >= 0) {
            pwm_set_chan_level(pwm_slice, pwm_chan_num_beaconbig, PWM_ON_LEVEL);
            cnt = 0;
            bb_state = B_STATE_CONFIG;
        }
    }
}

void beacon_init(void) {

    //init PWM pins
    gpio_set_function_masked(    (1<<BEACON_SMALL_OUT_PWM_PIN)
        |(1<<BEACON_BIG_OUT_PWM_PIN), GPIO_FUNC_PWM);

    pwm_slice = pwm_gpio_to_slice_num(BEACON_SMALL_OUT_PWM_PIN);  //beacon small a big pouzivaju jednu slice

    pwm_chan_num_beaconsmall = pwm_gpio_to_channel(BEACON_SMALL_OUT_PWM_PIN);
    pwm_chan_num_beaconbig = pwm_gpio_to_channel(BEACON_BIG_OUT_PWM_PIN);

    pwm_config c = pwm_get_default_config();
    // BEACONS ovladanie ako servo, 2 kanaly , jedna slice
    //pre PWM 50Hz: f = fsys/((19999+1) x clkdiv) = 125000000/((19999+1) x 125)
    //perioda = 20ms -> 20000
    //rozsah 1 - 2ms -> 1000 - 2000
    pwm_config_set_clkdiv_int(&c, 125);
    pwm_config_set_clkdiv_mode(&c, PWM_DIV_FREE_RUNNING);
    pwm_config_set_wrap(&c, 19999);
    pwm_config_set_output_polarity(&c,false,false);
    pwm_init(pwm_slice, &c, true);
    beacon_small_off();
    beacon_big_initializing();

}

void beacon_config (void) {

    if (bb_state == B_STATE_CHANGE_MODE) {
        beacon_big_change_mode();
    }
    if (bb_state == B_STATE_CHANGE_BRIGHTNESS) {
        beacon_big_change_brightness();
    }

    static uint32_t tn;
    uint32_t dt;
    //switch state
    uint16_t val = ibus_get_channel(IBUS_CHAN_BEACON_SW);
    switch (sw_state) {
        case SW_STATE_OFF:
            if (val > SW_ON_HIGH_LEVEL) {
                sw_state = SW_STATE_HIGH_ON;
                tn = time_us_32();
            }
            else if (val < SW_ON_LOW_LEVEL) {
                sw_state = SW_STATE_LOW_ON;
                tn = time_us_32();
            }
        break;
        case SW_STATE_HIGH_ON:
            //ak sa podrzi extra dlho skonci config - bocne svetla zhasnu
            dt = time_us_32() - tn;
            if ((int32_t) dt >= SW_EXTRA_LONG_TIME) {
                //switch off
                beacon_big_off();
                set_side_lights_as_side_lights();
                sw_state = SW_STATE_WAIT_FOR_OFF;
            }
            else if ((val > 1400) && (val < 1600)) {
                sw_state = SW_STATE_OFF;
                if ((int32_t) dt >= SW_SHORT_TIME) {
                    //switch next mode
                    if (bb_state == B_STATE_CONFIG) {
                        beacon_big_change_mode();
                    }
                }
            }
        break;

        case SW_STATE_LOW_ON:

            if ((val > 1400) && (val < 1600)) {
                dt = time_us_32() - tn;
                sw_state = SW_STATE_OFF;
                if ((int32_t) dt >= SW_SHORT_TIME) {
                //switch on next mode - jas
                    if (bb_state == B_STATE_CONFIG) {
                        beacon_big_change_brightness();
                    }

                }

            }
        break;

        case SW_STATE_WAIT_FOR_OFF:
            if ((val > 1400) && (val < 1600)) {
                sw_state = SW_STATE_OFF;
            }
        break;
    }
}



void beacon_service(void) {

    static uint32_t tn;
    if (bb_state == B_STATE_CONFIG || bb_state == B_STATE_CHANGE_MODE || bb_state == B_STATE_CHANGE_BRIGHTNESS) {
        beacon_config();
        return;
    }

    uint32_t dt;
    if (bb_state == B_STATE_INITIALIZING) {
        beacon_big_initializing();
    }
    if (sb_state == B_STATE_SWITCHING_OFF) {
        beacon_small_off();
    }
    if (sb_state == B_STATE_SWITCHING_ON) {
        beacon_small_on();
    }
    //switch state
    uint16_t val = ibus_get_channel(IBUS_CHAN_BEACON_SW);
    switch (sw_state) {
        case SW_STATE_OFF:
            if (val > SW_ON_HIGH_LEVEL) {
                sw_state = SW_STATE_HIGH_ON;
                tn = time_us_32();
            }
            else if (val < SW_ON_LOW_LEVEL) {
                sw_state = SW_STATE_LOW_ON;
                tn = time_us_32();
            }
        break;
        case SW_STATE_HIGH_ON:
            //ak je zopnuty dlhsie ako SW_LONG_TIME vypne
            dt = time_us_32() - tn;
            if ((int32_t) dt >= SW_LONG_TIME) {
                //switch off
                if (bb_state == B_STATE_ON) {
                    beacon_big_off();
                }
                if (sb_state == B_STATE_ON) {
                    beacon_small_off();
                }
                set_side_lights_as_side_lights();
                sw_state = SW_STATE_WAIT_FOR_CONFIG;
            }
            else if ((val > 1400) && (val < 1600)) {
                sw_state = SW_STATE_OFF;
                if ((int32_t) dt >= SW_SHORT_TIME) {
                    //switch on
                    if (bb_state == B_STATE_OFF && sb_state == B_STATE_OFF) {
                        beacon_big_on();
                        beacon_small_on();
                        set_side_lights_as_beacon();
                    }
                }
            }
        break;
        case SW_STATE_LOW_ON:
            dt = time_us_32() - tn;
        if ((int32_t) dt >= SW_LONG_TIME) {
            //switch off vzdy oba fog aj search
            rear_search_light_off();
            fog_light_off();
            sw_state = SW_STATE_WAIT_FOR_OFF;
            fs_state = FS_STATE_OFF;
        }
        else if ((val > 1400) && (val < 1600)) {
            sw_state = SW_STATE_OFF;
            if ((int32_t) dt >= SW_SHORT_TIME) {
                //switch on
                if (fs_state == FS_STATE_OFF) {
                    rear_search_light_on();
                    fs_state = FS_STATE_S_ON;
                }
                else if (fs_state == FS_STATE_S_ON) {
                    //zapne hmlove len ked su zapnute svetla
                    if (lights_are_on()) {
                        rear_search_light_off();
                        fog_light_on();
                        fs_state = FS_STATE_F_ON;
                    }
                }
                else if (fs_state == FS_STATE_F_ON) {
                    if (lights_are_on()) {
                        rear_search_light_on();
                        fog_light_on();
                        fs_state = FS_STATE_F_ON;
                    }
                }
            }
        }
        break;
        case SW_STATE_WAIT_FOR_OFF:
            if ((val > 1400) && (val < 1600)) {
                sw_state = SW_STATE_OFF;
            }
        break;
        case SW_STATE_WAIT_FOR_CONFIG:
            //ak sa podrzi extra dlho prepne sa do configu - bocne svetla blkaju
            dt = time_us_32() - tn;
            if ((int32_t) dt >= SW_EXTRA_LONG_TIME) {
                //switch off
                if (bb_state == B_STATE_OFF) {
                    beacon_big_on();
                    set_side_lights_as_config();
                    bb_state = B_STATE_CONFIG;
                    sw_state = SW_STATE_WAIT_FOR_OFF;
                }
            }
            else if ((val > 1400) && (val < 1600)) {
                    sw_state = SW_STATE_OFF;
                }
        break;
    }
}





//zavola sa ked sa svetla vypnu a ma sa vypnut aj foglight
void fog_light_switched_off(void) {
    if (fs_state == FS_STATE_F_ON) {
        fs_state = FS_STATE_OFF;
    }
    else if (fs_state == FS_STATE_ON) {
        fs_state = FS_STATE_S_ON;
    }
}
