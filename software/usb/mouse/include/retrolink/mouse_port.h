#ifndef RETROLINK_MOUSE_PORT_H
#define RETROLINK_MOUSE_PORT_H

#include <stdint.h>

#include "retrolink/joystick.h"

typedef enum {
    MOUSE_PORT_MODE_RELEASED = 0,
    MOUSE_PORT_MODE_MOUSE,
    MOUSE_PORT_MODE_JOYSTICK,
} mouse_port_mode_t;

/* Releases the DB9 outputs and starts the core 1 strobe/output loop. */
void mouse_port_init(void);

/* Changing the mode restarts the mouse sequence and discards pending movement. */
void mouse_port_set_mode(mouse_port_mode_t mode);
mouse_port_mode_t mouse_port_mode(void);

/* JOYSTICK_BUTTON_A/B bits; applied in mouse and joystick modes. */
void mouse_port_set_buttons(joystick_state_t buttons);

/* Movement in MSX sign (positive = left/up), consumed by the mouse protocol. */
void mouse_port_add_motion(int32_t msx_dx, int32_t msx_dy);

/* JOYSTICK_UP/DOWN/LEFT/RIGHT bits used in joystick mode. */
void mouse_port_set_directions(joystick_state_t directions);

#endif
