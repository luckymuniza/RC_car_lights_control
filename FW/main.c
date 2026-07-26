
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include <stdlib.h>
#include <time.h>

#include "beacons.h"
#include "ibus.h"
#include "lights.h"
#include"rear_light.h"
#include "turn_light.h"
#include "to_mfc.h"
#include "aux1.h"

#define DBG


#define AUX_2_OUT_PIN 5     //pwm slice 2


#define LED_PIN 25




int main(void) {
    //stdout_uart_init();
//    stdio_usb_init();
    //printf("Starting...\n");
    gpio_set_function_masked((1<<LED_PIN),GPIO_FUNC_SIO);

    //pre AUX
    gpio_set_function_masked(
        (1<<AUX_2_OUT_PIN)|(1<<AUX_2_OUT_PIN),GPIO_FUNC_SIO);

    gpio_set_dir_out_masked((1<<LED_PIN) | (1<<AUX_2_OUT_PIN));

    gpio_put(LED_PIN,1);
    busy_wait_ms(500);
    gpio_put(LED_PIN,0);
    busy_wait_ms(500);
    gpio_put(LED_PIN,1);
    busy_wait_ms(500);
    gpio_put(LED_PIN,0);



    //init iBUS uart
    ibus_init();
    while (!ibus_data_valid()) {
        ibus_service();
    }
    rear_light_init();

    beacon_init();
    to_mfc_init();
    lights_init();
    turn_light_init();
    aux_1_init();





    while(1) {
        gpio_xor_mask(1<<AUX_2_OUT_PIN);
        ibus_service();

        lights_service();
        //****************************************
        //              zadne svetla
        //****************************************
        rear_light_service();

        //****************************************
        //              smerovky
        //****************************************
        turn_light_service();

        //beacon
        beacon_service();

        //to mfc
        to_mfc_service();

        //aux2
        aux_1_service();

    } //while 1

} //main
