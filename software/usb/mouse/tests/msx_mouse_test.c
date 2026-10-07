#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "retrolink/hid_mouse.h"
#include "retrolink/msx_mouse.h"

#define TIMEOUT_US 1500u

static uint32_t now_us;

static uint8_t strobe_and_read(msx_mouse_t *mouse, bool level)
{
    now_us += 50u;
    msx_mouse_strobe(mouse, level, now_us);
    return msx_mouse_nibble(mouse);
}

/* Performs one BIOS-style read: pin 8 high, low, high, low, returning X and Y. */
static void read_sequence(msx_mouse_t *mouse, int8_t *x, int8_t *y)
{
    uint8_t x_high = strobe_and_read(mouse, true);
    uint8_t x_low = strobe_and_read(mouse, false);
    uint8_t y_high = strobe_and_read(mouse, true);
    uint8_t y_low = strobe_and_read(mouse, false);
    *x = (int8_t)(uint8_t)((x_high << 4) | x_low);
    *y = (int8_t)(uint8_t)((y_high << 4) | y_low);
}

static void next_scan(void)
{
    now_us += 20000u;
}

static void test_sequence_and_alternate_cycle(void)
{
    msx_mouse_t mouse;
    now_us = 1000u;
    msx_mouse_reset(&mouse, false, now_us, TIMEOUT_US);
    assert(msx_mouse_nibble(&mouse) == 0);
    assert(msx_mouse_lines(&mouse) == (JOYSTICK_UP | JOYSTICK_DOWN | JOYSTICK_LEFT | JOYSTICK_RIGHT));

    msx_mouse_add_motion(&mouse, 0x35, -0x12);
    next_scan();
    int8_t x;
    int8_t y;
    read_sequence(&mouse, &x, &y);
    assert(x == 0x35 && y == -0x12);
    read_sequence(&mouse, &x, &y);
    assert(x == 0 && y == 0);

    next_scan();
    read_sequence(&mouse, &x, &y);
    assert(x == 0 && y == 0);
}

static void test_nibble_lines(void)
{
    msx_mouse_t mouse;
    now_us = 0;
    msx_mouse_reset(&mouse, false, now_us, TIMEOUT_US);
    msx_mouse_add_motion(&mouse, 0x5a, 0);
    next_scan();
    assert(strobe_and_read(&mouse, true) == 0x5);
    /* 0101: pins 1 and 3 high (released), pins 2 and 4 low (asserted). */
    assert(msx_mouse_lines(&mouse) == (JOYSTICK_DOWN | JOYSTICK_RIGHT));
    assert(strobe_and_read(&mouse, false) == 0xa);
    assert(msx_mouse_lines(&mouse) == (JOYSTICK_UP | JOYSTICK_LEFT));
}

static void test_clamp_and_carry(void)
{
    msx_mouse_t mouse;
    now_us = 0;
    msx_mouse_reset(&mouse, false, now_us, TIMEOUT_US);
    msx_mouse_add_motion(&mouse, 300, -200);
    int8_t x;
    int8_t y;
    next_scan();
    read_sequence(&mouse, &x, &y);
    assert(x == 127 && y == -127);
    read_sequence(&mouse, &x, &y);
    next_scan();
    read_sequence(&mouse, &x, &y);
    assert(x == 127 && y == -73);
    next_scan();
    read_sequence(&mouse, &x, &y);
    assert(x == 46 && y == 0);

    msx_mouse_add_motion(&mouse, 5000, -5000);
    assert(mouse.pending_x == MSX_MOUSE_PENDING_LIMIT && mouse.pending_y == -MSX_MOUSE_PENDING_LIMIT);
}

static void test_timeout_resynchronizes(void)
{
    msx_mouse_t mouse;
    now_us = UINT32_MAX - 30u;
    msx_mouse_reset(&mouse, false, now_us, TIMEOUT_US);
    msx_mouse_add_motion(&mouse, 9, 3);
    /* An interrupted read: only X high/low were clocked. */
    strobe_and_read(&mouse, true);
    strobe_and_read(&mouse, false);
    assert(mouse.phase == MSX_MOUSE_PHASE_X_LOW_1);
    next_scan();
    msx_mouse_add_motion(&mouse, 4, 0);
    int8_t x;
    int8_t y;
    read_sequence(&mouse, &x, &y);
    assert(x == 4 && y == 0);

    /* A falling edge after a long gap falls back to Y low; the next rising edge latches new movement. */
    msx_mouse_add_motion(&mouse, -2, 1);
    next_scan();
    strobe_and_read(&mouse, true);
    next_scan();
    assert(strobe_and_read(&mouse, false) == 0x1);
    assert(mouse.phase == MSX_MOUSE_PHASE_Y_LOW_2);
    msx_mouse_add_motion(&mouse, 3, -4);
    read_sequence(&mouse, &x, &y);
    assert(x == 3 && y == -4);
}

static void test_repeated_level_is_ignored(void)
{
    msx_mouse_t mouse;
    now_us = 0;
    msx_mouse_reset(&mouse, false, now_us, TIMEOUT_US);
    msx_mouse_add_motion(&mouse, 0x21, 0);
    next_scan();
    assert(strobe_and_read(&mouse, true) == 0x2);
    assert(strobe_and_read(&mouse, true) == 0x2);
    assert(mouse.phase == MSX_MOUSE_PHASE_X_HIGH_1);
}

static void test_scale(void)
{
    msx_mouse_scaler_t scaler = {0};
    int32_t x;
    int32_t y;
    msx_mouse_scale(&scaler, 5, -5, 2, &x, &y);
    assert(x == -2 && y == 3);
    msx_mouse_scale(&scaler, 1, -1, 2, &x, &y);
    assert(x == -1 && y == 0);
    assert(scaler.remainder_x == 0 && scaler.remainder_y == 0);
    msx_mouse_scale(&scaler, 7, 9, 1, &x, &y);
    assert(x == -7 && y == -9);
    msx_mouse_scaler_t zero = {0};
    msx_mouse_scale(&zero, 3, 0, 0, &x, &y);
    assert(x == -3 && y == 0);
    msx_mouse_scale(&zero, INT32_MAX, INT32_MIN, 1, &x, &y);
    assert(x == -(INT32_C(1) << 24) && y == (INT32_C(1) << 24));
}

static void test_joystick_emulation(void)
{
    assert(msx_mouse_joystick_directions(1, -1, 2) == 0);
    assert(msx_mouse_joystick_directions(5, 0, 2) == JOYSTICK_LEFT);
    assert(msx_mouse_joystick_directions(-5, 0, 2) == JOYSTICK_RIGHT);
    assert(msx_mouse_joystick_directions(0, 5, 2) == JOYSTICK_UP);
    assert(msx_mouse_joystick_directions(0, -5, 2) == JOYSTICK_DOWN);
    assert(msx_mouse_joystick_directions(6, 6, 2) == (JOYSTICK_LEFT | JOYSTICK_UP));
    assert(msx_mouse_joystick_directions(-6, -6, 2) == (JOYSTICK_RIGHT | JOYSTICK_DOWN));
    assert(msx_mouse_joystick_directions(12, 4, 2) == JOYSTICK_LEFT);
    assert(msx_mouse_joystick_directions(-4, 12, 2) == JOYSTICK_UP);
    assert(msx_mouse_joystick_directions(INT32_MIN, 0, 2) == JOYSTICK_RIGHT);

    assert(msx_mouse_buttons(0) == 0);
    assert(msx_mouse_buttons(HID_MOUSE_BUTTON_LEFT) == JOYSTICK_BUTTON_A);
    assert(msx_mouse_buttons(HID_MOUSE_BUTTON_RIGHT) == JOYSTICK_BUTTON_B);
    assert(msx_mouse_buttons(HID_MOUSE_BUTTON_MIDDLE) == 0);
    assert(msx_mouse_buttons(0x07) == (JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B));
}

int main(void)
{
    test_sequence_and_alternate_cycle();
    test_nibble_lines();
    test_clamp_and_carry();
    test_timeout_resynchronizes();
    test_repeated_level_is_ignored();
    test_scale();
    test_joystick_emulation();
    puts("MSX mouse protocol tests passed");
    return 0;
}
