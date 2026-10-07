#include "retrolink/msx_mouse.h"

#include "retrolink/hid_mouse.h"

#ifdef RETROLINK_ON_DEVICE
#include "pico.h"
#define MSX_MOUSE_RAM_FUNC(name) __not_in_flash_func(name)
#else
#define MSX_MOUSE_RAM_FUNC(name) name
#endif

#define MSX_MOUSE_HID_DELTA_LIMIT (INT32_C(1) << 24)

static inline int32_t clamp(int32_t value, int32_t minimum, int32_t maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static inline int32_t floor_divide(int32_t value, int32_t divisor)
{
    int32_t quotient = value / divisor;
    if (value % divisor != 0 && value < 0) --quotient;
    return quotient;
}

void MSX_MOUSE_RAM_FUNC(msx_mouse_reset)(msx_mouse_t *mouse, bool strobe, uint32_t now_us, uint32_t timeout_us)
{
    /* Field-by-field so no flash-resident memset is emitted for the core 1 caller. */
    mouse->last_edge_us = now_us;
    mouse->timeout_us = timeout_us;
    mouse->pending_x = 0;
    mouse->pending_y = 0;
    mouse->x = 0;
    mouse->y = 0;
    mouse->phase = MSX_MOUSE_PHASE_Y_LOW_2;
    mouse->strobe = strobe;
}

void MSX_MOUSE_RAM_FUNC(msx_mouse_add_motion)(msx_mouse_t *mouse, int32_t msx_dx, int32_t msx_dy)
{
    msx_dx = clamp(msx_dx, -MSX_MOUSE_PENDING_LIMIT, MSX_MOUSE_PENDING_LIMIT);
    msx_dy = clamp(msx_dy, -MSX_MOUSE_PENDING_LIMIT, MSX_MOUSE_PENDING_LIMIT);
    mouse->pending_x = clamp(mouse->pending_x + msx_dx, -MSX_MOUSE_PENDING_LIMIT, MSX_MOUSE_PENDING_LIMIT);
    mouse->pending_y = clamp(mouse->pending_y + msx_dy, -MSX_MOUSE_PENDING_LIMIT, MSX_MOUSE_PENDING_LIMIT);
}

void MSX_MOUSE_RAM_FUNC(msx_mouse_strobe)(msx_mouse_t *mouse, bool strobe, uint32_t now_us)
{
    if (strobe == mouse->strobe) return;
    mouse->strobe = strobe;
    if ((uint32_t)(now_us - mouse->last_edge_us) > mouse->timeout_us) {
        mouse->phase = MSX_MOUSE_PHASE_Y_LOW_2;
    }
    mouse->last_edge_us = now_us;

    switch (mouse->phase) {
    case MSX_MOUSE_PHASE_X_HIGH_1:
    case MSX_MOUSE_PHASE_Y_HIGH_1:
    case MSX_MOUSE_PHASE_X_HIGH_2:
    case MSX_MOUSE_PHASE_Y_HIGH_2:
        if (!strobe) ++mouse->phase;
        break;
    case MSX_MOUSE_PHASE_X_LOW_1:
    case MSX_MOUSE_PHASE_X_LOW_2:
        if (strobe) ++mouse->phase;
        break;
    case MSX_MOUSE_PHASE_Y_LOW_1:
        if (strobe) {
            mouse->phase = MSX_MOUSE_PHASE_X_HIGH_2;
            mouse->x = 0;
            mouse->y = 0;
        }
        break;
    default:
        if (strobe) {
            mouse->phase = MSX_MOUSE_PHASE_X_HIGH_1;
            mouse->x = (int8_t)clamp(mouse->pending_x, -127, 127);
            mouse->y = (int8_t)clamp(mouse->pending_y, -127, 127);
            mouse->pending_x -= mouse->x;
            mouse->pending_y -= mouse->y;
        }
        break;
    }
}

uint8_t MSX_MOUSE_RAM_FUNC(msx_mouse_nibble)(const msx_mouse_t *mouse)
{
    uint8_t x = (uint8_t)mouse->x;
    uint8_t y = (uint8_t)mouse->y;
    switch (mouse->phase) {
    case MSX_MOUSE_PHASE_X_HIGH_1:
    case MSX_MOUSE_PHASE_X_HIGH_2: return (uint8_t)(x >> 4);
    case MSX_MOUSE_PHASE_X_LOW_1:
    case MSX_MOUSE_PHASE_X_LOW_2: return (uint8_t)(x & 0x0fu);
    case MSX_MOUSE_PHASE_Y_HIGH_1:
    case MSX_MOUSE_PHASE_Y_HIGH_2: return (uint8_t)(y >> 4);
    default: return (uint8_t)(y & 0x0fu);
    }
}

joystick_state_t MSX_MOUSE_RAM_FUNC(msx_mouse_lines)(const msx_mouse_t *mouse)
{
    /* Nibble bits 0-3 appear on DB9 pins 1-4; a 0 bit is an asserted (low) line. */
    return (joystick_state_t)(~msx_mouse_nibble(mouse) & (JOYSTICK_UP | JOYSTICK_DOWN | JOYSTICK_LEFT | JOYSTICK_RIGHT));
}

void msx_mouse_scale(msx_mouse_scaler_t *scaler, int32_t hid_dx, int32_t hid_dy, int32_t divisor,
                     int32_t *msx_dx, int32_t *msx_dy)
{
    if (divisor < 1) divisor = 1;
    int32_t x = clamp(hid_dx, -MSX_MOUSE_HID_DELTA_LIMIT, MSX_MOUSE_HID_DELTA_LIMIT) + scaler->remainder_x;
    int32_t y = clamp(hid_dy, -MSX_MOUSE_HID_DELTA_LIMIT, MSX_MOUSE_HID_DELTA_LIMIT) + scaler->remainder_y;
    int32_t quotient_x = floor_divide(x, divisor);
    int32_t quotient_y = floor_divide(y, divisor);
    scaler->remainder_x = x - quotient_x * divisor;
    scaler->remainder_y = y - quotient_y * divisor;
    *msx_dx = -quotient_x;
    *msx_dy = -quotient_y;
}

joystick_state_t msx_mouse_joystick_directions(int32_t msx_dx, int32_t msx_dy, int32_t threshold)
{
    int64_t abs_x = msx_dx < 0 ? -(int64_t)msx_dx : msx_dx;
    int64_t abs_y = msx_dy < 0 ? -(int64_t)msx_dy : msx_dy;
    if (abs_x < threshold && abs_y < threshold) return 0;

    /* 5/12 approximates tan(pi/8), splitting movement into eight directions. */
    joystick_state_t state = 0;
    if (12 * abs_x > 5 * abs_y) state |= msx_dx > 0 ? JOYSTICK_LEFT : JOYSTICK_RIGHT;
    if (12 * abs_y > 5 * abs_x) state |= msx_dy > 0 ? JOYSTICK_UP : JOYSTICK_DOWN;
    return state;
}

joystick_state_t msx_mouse_buttons(uint8_t hid_buttons)
{
    joystick_state_t state = 0;
    if ((hid_buttons & HID_MOUSE_BUTTON_LEFT) != 0) state |= JOYSTICK_BUTTON_A;
    if ((hid_buttons & HID_MOUSE_BUTTON_RIGHT) != 0) state |= JOYSTICK_BUTTON_B;
    return state;
}
