#ifndef RETROLINK_JOYSTICK_H
#define RETROLINK_JOYSTICK_H

#include <stdint.h>

typedef uint8_t joystick_state_t;

enum {
    JOYSTICK_UP = 1u << 0,
    JOYSTICK_DOWN = 1u << 1,
    JOYSTICK_LEFT = 1u << 2,
    JOYSTICK_RIGHT = 1u << 3,
    JOYSTICK_BUTTON_A = 1u << 4,
    JOYSTICK_BUTTON_B = 1u << 5,
};

#endif