//
// Created by all on 16. 12. 2025.
//

#ifndef IBUS_H
#define IBUS_H


#include <stdint.h>
#include <stdbool.h>


typedef enum
{
    IBUS_CHAN_DIR = (1-1),
    IBUS_CHAN_LIGHTS_MFC = (2-1),
    IBUS_CHAN_THROTTLE = (3-1),
    IBUS_CHAN_THROTTLE_X = (4-1),
    IBUS_CHAN_MODE_SW = (5-1),
    IBUS_CHAN_BEACON_SW = (6-1),
    IBUS_CHAN_GEAR_SW = (7-1),
    IBUS_CHAN_LIGHTS_SW = (8-1),
    IBUS_CHAN_POT_A = (9-1),
    IBUS_CHAN_POT_B = (10-1)
} ibus_channel;

void ibus_init(void);
void ibus_service(void);
uint16_t ibus_get_channel (ibus_channel chan);
bool ibus_data_valid (void);


#endif //IBUS_H

