//
// Created by all on 12/30/2025.
//

#ifndef SWITCH_H
#define SWITCH_H

typedef enum {
    SW_STATE_OFF,
    SW_STATE_HIGH_ON,
    SW_STATE_LOW_ON,
    SW_STATE_WAIT_FOR_OFF, //caka ked sa uvolni tlacitko
    SW_STATE_WAIT_FOR_CONFIG //caka ci este moze byt

}switch_state;

#define SW_ON_HIGH_LEVEL 1800
#define SW_ON_LOW_LEVEL 1200

#define SW_SHORT_TIME 10000 //10ms
#define SW_LONG_TIME 1000000  //1s
#define SW_EXTRA_LONG_TIME 5000000  //5s pre config




#endif //SWITCH_H
