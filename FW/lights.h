//
// Created by all on 17. 12. 2025.
//

#ifndef LIGHTS_H
#define LIGHTS_H

#include "pico/stdlib.h"

void side_light_1_on (void);
void side_light_1_off (void);
void side_light_2_on (void);
void side_light_2_off (void);
void day_light_on (void);
void day_light_off (void);
void low_beam_on (void);
void low_beam_off (void);
void high_beam_on (void);
void high_beam_off (void);
void fog_light_on (void);
void fog_light_off (void);
void position_light_on (void);
void position_light_off (void);
void front_ramp_light_on (void);
void front_ramp_light_off (void);
void rear_search_light_on (void);
void rear_search_light_off (void);
void set_side_lights_as_beacon (void);
void set_side_lights_as_side_lights (void);
void set_side_lights_as_config(void);

void lights_init(void);
void lights_service(void);
bool lights_are_on(void);




#endif //LIGHTS_H
