#ifndef RETROLINK_MD_PAD_H
#define RETROLINK_MD_PAD_H

#include <stdint.h>

#include "retrolink/joystick.h"

typedef enum {
    MD_PAD_NONE,
    MD_PAD_3_BUTTON,
    MD_PAD_6_BUTTON,
} md_pad_type_t;

enum {
    MD_UP = 1u << 0,
    MD_DOWN = 1u << 1,
    MD_LEFT = 1u << 2,
    MD_RIGHT = 1u << 3,
    MD_B = 1u << 4,
    MD_C = 1u << 5,
    MD_A = 1u << 6,
    MD_START = 1u << 7,
    MD_Z = 1u << 8,
    MD_Y = 1u << 9,
    MD_X = 1u << 10,
    MD_MODE = 1u << 11,
};

typedef struct {
    md_pad_type_t type;
    uint16_t buttons;
} md_pad_state_t;

/* Active-low samples packed in D0..D5 order, independent of GPIO numbering. */
typedef struct {
    uint8_t idle_high;
    uint8_t first_low;
    uint8_t third_low;
    uint8_t third_high;
    uint8_t fourth_low;
} md_pad_samples_t;

md_pad_state_t md_pad_decode(md_pad_samples_t samples);
joystick_state_t md_pad_to_msx(md_pad_state_t state);

#endif
