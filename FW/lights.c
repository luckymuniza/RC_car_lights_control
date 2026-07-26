//
// Created by all on 17. 12. 2025.
//

#include "lights.h"

#include <time.h>

#include "pico/stdlib.h"
#include "ibus.h"
#include "switch.h"
#include "rear_light.h"
#include "beacons.h"
#include "to_mfc.h"

#define LED_POWER_PIN 0
#define DAY_LIGHT_OUTPUT_PIN 27  //0 je stdout uart0
#define LOW_BEAM_OUT_PIN    1
#define HIGH_BEAM_OUT_PIN   2
#define FOG_LIGHT_OUT_PIN   3
#define POSITION_LIGHT_OUT_PIN    28
#define FRONT_RAMP_LIGHT_OUT_PIN    29
#define REAR_SEARCH_LIGHT_OUT_PIN 9
#define SIDE_LIGHT_1_OUT_PIN 11
#define SIDE_LIGHT_2_OUT_PIN 12

//pwm out

#define LIGHT_0_17_IN_PIN 17
#define LIGHT_1_15_IN_PIN 18
#define LIGHT_2_20_IN_PIN 19
#define LIGHT_3_13_IN_PIN 20

#define LIGHT_0_MASK (1<<LIGHT_0_17_IN_PIN)
#define LIGHT_1_MASK (1<<LIGHT_1_15_IN_PIN)
#define LIGHT_2_MASK (1<<LIGHT_2_20_IN_PIN)
#define LIGHT_3_MASK (1<<LIGHT_3_13_IN_PIN)

#define SIDE_LIGHT_BEACON_TIME_ON 100000
#define SIDE_LIGHT_BEACON_TIME_OFF 100000
#define SIDE_LIGHT_BEACON_TIME_PAUSE 200000

typedef enum {
    L_STATE_OFF,        //svtla vypnute, nedaju sa ani fog zapnut
    L_STATE_POSITION,   //poziscne + bocne
    L_STATE_LOW_BEAM,   //low beam
    L_STATE_HIGH_BEAM,  //high beam
    L_STATE_RAMP,       //rampa
    L_STATE_ON,         //koli side lights svetlam
    L_STATE_BEACON,      //bocne ako blikajuce
    L_STATE_CONFIG        //pre sidelight, blikaju ked je konfig beaconu
}lights_state;



static switch_state sw_state = SW_STATE_OFF;  //poloha switchu
static lights_state l_state = L_STATE_OFF;   //lights state
static lights_state s_state = L_STATE_OFF;  //bocne
static uint8_t side_beacon_cnt = 0;  //pre blikanie casovac
static uint32_t side_time_next;      //pre blikanie cas medzi urovnami
static bool mfc_light_is_on = false;
static uint8_t starting_seq = 0;

void side_light_1_on (void) {
    gpio_put(SIDE_LIGHT_1_OUT_PIN, 1);
}
void side_light_1_off (void) {
    gpio_put(SIDE_LIGHT_1_OUT_PIN, 0);
}
void side_light_2_on (void) {
    gpio_put(SIDE_LIGHT_2_OUT_PIN, 1);
}
void side_light_2_off (void) {
    gpio_put(SIDE_LIGHT_2_OUT_PIN, 0);
}

void day_light_on (void) {
    gpio_put(DAY_LIGHT_OUTPUT_PIN, 1);
}
void day_light_off (void) {
    gpio_put(DAY_LIGHT_OUTPUT_PIN, 0);
}

void low_beam_on (void) {
    gpio_put(LOW_BEAM_OUT_PIN, 1);
}
void low_beam_off (void) {
    gpio_put(LOW_BEAM_OUT_PIN, 0);
}

void high_beam_on (void) {
    gpio_put(HIGH_BEAM_OUT_PIN, 1);
}
void high_beam_off (void) {
    gpio_put(HIGH_BEAM_OUT_PIN, 0);
}

void fog_light_on (void) {
    gpio_put(FOG_LIGHT_OUT_PIN, 1);
}
void fog_light_off (void) {
    gpio_put(FOG_LIGHT_OUT_PIN, 0);
}

void position_light_on (void) {
    gpio_put(POSITION_LIGHT_OUT_PIN, 1);
}
void position_light_off (void) {
    gpio_put(POSITION_LIGHT_OUT_PIN, 0);
}

void front_ramp_light_on (void) {
    gpio_put(FRONT_RAMP_LIGHT_OUT_PIN, 1);
}
void front_ramp_light_off (void) {
    gpio_put(FRONT_RAMP_LIGHT_OUT_PIN, 0);
}

void rear_search_light_on (void) {
    gpio_put(REAR_SEARCH_LIGHT_OUT_PIN, 1);
}
void rear_search_light_off (void) {
    gpio_put(REAR_SEARCH_LIGHT_OUT_PIN, 0);
}

//okrem pinu led_power este aj hlavne svetla lebo tam to 5V nespina
void led_power_on (void) {
    gpio_put(LED_POWER_PIN, 1);
    if (l_state == L_STATE_LOW_BEAM) {
        low_beam_on();
    }
    else if (l_state == L_STATE_HIGH_BEAM) {
        high_beam_on();
        low_beam_on();
    }
    else if (l_state == L_STATE_RAMP) {
        high_beam_on();
        low_beam_on();
        front_ramp_light_on();
    }
}


void led_power_off (void) {
    gpio_put(LED_POWER_PIN, 0);
    if (l_state == L_STATE_LOW_BEAM) {
        low_beam_off();
    }
    else if (l_state == L_STATE_HIGH_BEAM) {
        high_beam_off();
        low_beam_off();
    }
    else if (l_state == L_STATE_RAMP) {
        high_beam_off();
        low_beam_off();
        front_ramp_light_off();
    }
}




void lights_init(void) {
    //init pins
    gpio_set_function_masked(
      (1<<DAY_LIGHT_OUTPUT_PIN)
      |(1<<LED_POWER_PIN)
      |(1<<LOW_BEAM_OUT_PIN)
      |(1<<HIGH_BEAM_OUT_PIN)
      |(1<<FOG_LIGHT_OUT_PIN)
      |(1<<POSITION_LIGHT_OUT_PIN)
      |(1<<FRONT_RAMP_LIGHT_OUT_PIN)
      |(1<<REAR_SEARCH_LIGHT_OUT_PIN)
      |(1<<SIDE_LIGHT_1_OUT_PIN)
      |(1<<SIDE_LIGHT_2_OUT_PIN)
      |(1<<LIGHT_0_17_IN_PIN)
      |(1<<LIGHT_1_15_IN_PIN)
      |(1<<LIGHT_2_20_IN_PIN)
      |(1<<LIGHT_3_13_IN_PIN),GPIO_FUNC_SIO);

    gpio_set_dir_in_masked(
        (1<<LIGHT_0_17_IN_PIN)
        |(1<<LIGHT_1_15_IN_PIN)
        |(1<<LIGHT_2_20_IN_PIN)
        |(1<<LIGHT_3_13_IN_PIN));
    gpio_set_dir_out_masked(
    (1<<LED_POWER_PIN)
        |(1<<DAY_LIGHT_OUTPUT_PIN)
        |(1<<LOW_BEAM_OUT_PIN)
        |(1<<HIGH_BEAM_OUT_PIN)
        |(1<<FOG_LIGHT_OUT_PIN)
        |(1<<POSITION_LIGHT_OUT_PIN)
        |(1<<FRONT_RAMP_LIGHT_OUT_PIN)
        |(1<<REAR_SEARCH_LIGHT_OUT_PIN)
        |(1<<SIDE_LIGHT_1_OUT_PIN)
        |(1<<SIDE_LIGHT_2_OUT_PIN));

    //pullups
    gpio_set_pulls(LIGHT_0_17_IN_PIN,true,false);
    gpio_set_pulls(LIGHT_1_15_IN_PIN,true,false);
    gpio_set_pulls(LIGHT_2_20_IN_PIN,true,false);
    gpio_set_pulls(LIGHT_3_13_IN_PIN,true,false);

    //side_light_1_off();
    //side_light_2_off();
    day_light_on ();
    //low_beam_on ();
    //high_beam_on ();
    //fog_light_on ();
    //position_light_on ();
    //front_ramp_light_on ();
    rear_search_light_off ();
    led_power_on ();

}

//striedavo dvojblik side_lights 1 a side_lights 2
void side_state_as_beacon (void) {
    uint32_t dt;
    switch (side_beacon_cnt){
            case 0:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_1_on();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_ON;
                    side_beacon_cnt++;
                }
                break;
            case 1:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_1_off();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_OFF;
                    side_beacon_cnt++;
                }
                break;
            case 2:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_1_on();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_ON;
                    side_beacon_cnt++;
                }
                break;
            case 3:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_1_off();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_PAUSE;
                    side_beacon_cnt++;
                }
                break;
            case 4:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_2_on();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_ON;
                    side_beacon_cnt++;
                }
                break;
            case 5:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_2_off();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_OFF;
                    side_beacon_cnt++;
                }
                break;
            case 6:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_2_on();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_ON;
                    side_beacon_cnt++;
                }
                break;
            case 7:
                dt = time_us_32() - side_time_next;
                if (((int32_t) dt) > 0) {
                    side_light_2_off();
                    side_time_next = time_us_32() + SIDE_LIGHT_BEACON_TIME_PAUSE;
                    side_beacon_cnt = 0;
                }
            break;
        }
}


//v mfc musi zapnut alebo vypnut svetla tiez,lebo ked vypne motor aby pipalo ze su zapnute svetla
//cize simuluje zapnutie svetiel kniplmo. zakazdym prepne jeden mod pokial neni stav ako pozaduje
void update_lights_in_mfc(void) {
    static uint32_t t = 0;
    uint32_t dt;

    //ked su zapnute l_val = 0
    if (starting_seq == 0) {
        if ((l_state == L_STATE_OFF && mfc_light_is_on) | (l_state != L_STATE_OFF && mfc_light_is_on == false)) {
            //svetla su vypnute ale v mfc svietia,
            //treba ich vypnut
            dt = time_us_32()-t;
            if((int32_t) dt > 0) {
                if (!switching_lights_in_mfc()) {
                    switch_lights_in_mfc();
                }
                t = time_us_32() + 500000;  //pockaj 0.5s a potom opat kontroluj
            }
        }
        else {
            t = time_us_32();
        }
    }
}

void detect_starting_seq(void) {
    static bool l_val_prev = 1;
    static uint64_t t_prev = 0;
    static uint64_t t_now = 0;
    static uint32_t t_starting;
    static uint16_t buff[64];
    static uint8_t idx_r = 0;
    static uint8_t idx_w = 0;
    static bool starting_seq_start = true;

    static bool led_power_is_on;
    uint64_t dt64;
    uint32_t dt;

    //sekvencia pri startovani - blika svetlo
    if (starting_seq) {
        if (starting_seq_start) {
            //na zaciatku start seq
            //load_buff
            t_starting = time_us_32() + (buff[idx_r] & ((1<<15)-1));
            //podla bitu 7 -> on/off
            if (buff[idx_r] & (1<<15)) {
                led_power_on();
                led_power_is_on = true;
            }
            else {
                led_power_off();
                led_power_is_on = false;
            }
            starting_seq_start = false;
            idx_r++;
            idx_r &= 63;
        }
        //este nejaky cas je v rezime starting seq, potom ho vypne
        else if (starting_seq == 2) {
            dt = time_us_32() - t_starting;
            if ((int32_t) dt >= 0) {
                starting_seq_start = true;
                starting_seq = 0;
            }

        }
        else {
            dt = time_us_32() - t_starting;
            if ((int32_t) dt >= 0) {
                if (led_power_is_on) {
                    led_power_off();
                    led_power_is_on = false;
                }
                else {
                    led_power_on();
                    led_power_is_on = true;
                }
                if (idx_r != idx_w) {
                    //load_buff
                    t_starting = time_us_32() + (buff[idx_r] & ((1<<15)-1));
                    if (buff[idx_r] & (1<<15)) {
                        led_power_on();
                        led_power_is_on = true;
                    }
                    else {
                        led_power_off();
                        led_power_is_on = false;
                    }
                    idx_r++;
                    idx_r &= 63;
                }
                else {
                    led_power_on();
                    starting_seq = 2;
                    t_starting = time_us_32() + 100000; // este 100ms pocka potom vypne
                }
            }
        }
    }

    bool l_val =  gpio_get(LIGHT_1_15_IN_PIN);
    if (l_val != l_val_prev) {
        t_now = time_us_64();
        dt64 = t_now - t_prev;
        t_prev = t_now;
        if (dt64 <= 25000) {
            //ak predtim bola 1-> vypnute
            if (l_val_prev) {
                //oznacit bit(7) na 0 ze vypnute
                buff[idx_w] = dt64;
            }
            else {
                //oznacit bit(7) na 1 ze zapnute
                buff[idx_w] = dt64 | (1 << 15);
            }
            idx_w++;
            idx_w &= 63;
           //starting detected
            starting_seq = 1;
        }
        else {
            //ak je ten cas medzi zenou dlhsi je to normalny stav
            mfc_light_is_on = !l_val;
        }
        l_val_prev = l_val;
    }
    else {
        mfc_light_is_on = !l_val;
    }
}

void lights_service(void) {
    detect_starting_seq();
    update_lights_in_mfc();

    uint32_t dt;


   //           side lights as beacon
   //----------------------------------------------------------
    if (s_state == L_STATE_BEACON) {
        side_state_as_beacon();
    }

    //           side lights as config
    //----------------------------------------------------------
    //len blikaju side light 2

    if (s_state == L_STATE_CONFIG) {
        dt = time_us_32() - side_time_next;
        if (((int32_t) dt) > 0) {
            if (side_beacon_cnt == 0) {
                side_light_2_on();
                side_beacon_cnt = 1;
            }
            else {
                side_light_2_off();
                side_beacon_cnt = 0;
            }
            side_time_next = time_us_32() + 500000;
        }
    }

    //           side lights as config
    //----------------------------------------------------------
    //light switch state

    static uint32_t tn;
    uint16_t val = ibus_get_channel(IBUS_CHAN_LIGHTS_SW);
    switch (sw_state) {
        case SW_STATE_OFF:
            if (val > SW_ON_HIGH_LEVEL) {
                sw_state = SW_STATE_HIGH_ON;
                tn = time_us_32();
            }
            else if (val < SW_ON_LOW_LEVEL) {
                sw_state = SW_STATE_LOW_ON;
                front_ramp_light_on();
                //time_next = time_us_32();
            }
            break;
        case SW_STATE_HIGH_ON:
            dt = time_us_32() - tn;
            if ((int32_t) dt > SW_LONG_TIME) {
                //switch off
                sw_state = SW_STATE_WAIT_FOR_OFF;
                if (l_state == L_STATE_POSITION ) {
                    l_state = L_STATE_OFF;
                    fog_light_off();
                    fog_light_switched_off(); //aktualizovat fs_state v beacon
                    position_light_off();
                    if (s_state == L_STATE_ON) {
                        side_light_1_off();
                        side_light_2_off();
                        s_state = L_STATE_OFF;
                    }
                }
                else if (l_state == L_STATE_LOW_BEAM) {
                    l_state = L_STATE_POSITION;
                    low_beam_off();
                    rear_light_off();
                }
                else if (l_state == L_STATE_HIGH_BEAM) {
                    l_state = L_STATE_LOW_BEAM;
                    high_beam_off();
                    front_ramp_light_off();
                }
                else if (l_state == L_STATE_RAMP) {
                    l_state = L_STATE_HIGH_BEAM;
                    front_ramp_light_off();
                }
            }
            else if ((val > 1400) && (val < 1600)) {
                sw_state = SW_STATE_OFF;
                if ((int32_t) dt > SW_SHORT_TIME) {
                    //switch on
                    if (l_state == L_STATE_OFF ) {
                        l_state = L_STATE_POSITION;
                        position_light_on();
                        if (s_state != L_STATE_BEACON && s_state != L_STATE_CONFIG) {
                            side_light_1_on();
                            side_light_2_on();
                            s_state = L_STATE_ON;
                        }
                    }
                    else if (l_state == L_STATE_POSITION) {
                        l_state = L_STATE_LOW_BEAM;
                        low_beam_on();
                        rear_light_on();
                    }
                    else if (l_state == L_STATE_LOW_BEAM) {
                        l_state = L_STATE_HIGH_BEAM;
                        high_beam_on();
                    }
                    else if (l_state == L_STATE_HIGH_BEAM) {
                        l_state = L_STATE_RAMP;
                        front_ramp_light_on();
                    }
                }
            }
            break;
        case SW_STATE_LOW_ON:
            if ((val > 1400) && (val < 1600)) {
                sw_state = SW_STATE_OFF;
                if (l_state != L_STATE_RAMP) {
                    front_ramp_light_off();
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



//nastavi aby blikali bozne svetla
void set_side_lights_as_beacon (void) {
    s_state = L_STATE_BEACON;
    side_beacon_cnt = 0;
    side_time_next = time_us_32();
}

//zrusi iny stav  side lighta (as beacon, as config) a necha ich zapnute alebo vypnute podla toho ak su zapnute svrtla
void set_side_lights_as_side_lights(void) {
    if (l_state == L_STATE_OFF) {
        s_state = L_STATE_OFF;
        side_light_1_off();
        side_light_2_off();
    }
    else {
        s_state = L_STATE_ON;
        side_light_1_on();
        side_light_2_on();
    }
}

//side lights as conig
void set_side_lights_as_config(void) {
    s_state = L_STATE_CONFIG;
    side_beacon_cnt = 0;
    side_time_next = time_us_32();
}

//return true if lights are on
//minimalne pozicne musia svietit
bool lights_are_on(void) {
    return l_state != L_STATE_OFF;
}