#ifndef RETROLINK_MSX_PORT_H
#define RETROLINK_MSX_PORT_H

#include <stdbool.h>

#include "retrolink/joystick.h"

void msx_port_init(void);
void msx_port_set_state(joystick_state_t state);
void msx_port_set_trigger_a(bool pressed);
bool msx_port_trigger_a_is_pressed(void);

#endif