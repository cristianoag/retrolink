#ifndef RETROLINK_MSX_MOUSE_H
#define RETROLINK_MSX_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "retrolink/joystick.h"

/*
 * MSX mouse protocol: every DB9 pin 8 (strobe) edge selects the next nibble on pins 1-4,
 * in the order X bits 7-4, X bits 3-0, Y bits 7-4, Y bits 3-0. The MSX BIOS reads two
 * such sequences per scan; the second (alternate) sequence reports zero so the BIOS
 * identifies a mouse rather than a trackball. Positive X means left, positive Y means up.
 */
typedef enum {
    MSX_MOUSE_PHASE_X_HIGH_1 = 0,
    MSX_MOUSE_PHASE_X_LOW_1,
    MSX_MOUSE_PHASE_Y_HIGH_1,
    MSX_MOUSE_PHASE_Y_LOW_1,
    MSX_MOUSE_PHASE_X_HIGH_2,
    MSX_MOUSE_PHASE_X_LOW_2,
    MSX_MOUSE_PHASE_Y_HIGH_2,
    MSX_MOUSE_PHASE_Y_LOW_2,
} msx_mouse_phase_t;

#define MSX_MOUSE_PENDING_LIMIT 1024

typedef struct {
    uint32_t last_edge_us;
    uint32_t timeout_us;
    int32_t pending_x;
    int32_t pending_y;
    int8_t x;
    int8_t y;
    uint8_t phase;
    bool strobe;
} msx_mouse_t;

typedef struct {
    int32_t remainder_x;
    int32_t remainder_y;
} msx_mouse_scaler_t;

void msx_mouse_reset(msx_mouse_t *mouse, bool strobe, uint32_t now_us, uint32_t timeout_us);

/* Adds movement already converted to MSX sign; the backlog is limited to +/-MSX_MOUSE_PENDING_LIMIT. */
void msx_mouse_add_motion(msx_mouse_t *mouse, int32_t msx_dx, int32_t msx_dy);

/* Feed the sampled pin 8 level; only level changes advance the sequence. */
void msx_mouse_strobe(msx_mouse_t *mouse, bool strobe, uint32_t now_us);

uint8_t msx_mouse_nibble(const msx_mouse_t *mouse);

/* Converts the current nibble to asserted (low) JOYSTICK_UP/DOWN/LEFT/RIGHT lines for pins 1-4. */
joystick_state_t msx_mouse_lines(const msx_mouse_t *mouse);

/* Divides USB counts by divisor, keeping the remainder, and converts to MSX sign. */
void msx_mouse_scale(msx_mouse_scaler_t *scaler, int32_t hid_dx, int32_t hid_dy, int32_t divisor,
                     int32_t *msx_dx, int32_t *msx_dy);

/* Joystick emulation directions for accumulated MSX-sign movement. */
joystick_state_t msx_mouse_joystick_directions(int32_t msx_dx, int32_t msx_dy, int32_t threshold);

/* Maps HID left/right buttons to MSX trigger A/B. */
joystick_state_t msx_mouse_buttons(uint8_t hid_buttons);

#endif
