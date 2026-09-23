#include "retrolink/md_pad.h"

md_pad_state_t md_pad_decode(md_pad_samples_t samples)
{
    md_pad_state_t state = {MD_PAD_NONE, 0};
    if ((samples.first_low & 0x0cu) != 0) {
        return state;
    }

    if ((samples.third_low & 0x0fu) == 0 && (samples.fourth_low & 0x0fu) == 0x0fu) {
        state.type = MD_PAD_6_BUTTON;
    } else if ((samples.third_low & 0x0cu) == 0 && (samples.fourth_low & 0x0cu) == 0) {
        state.type = MD_PAD_3_BUTTON;
    } else {
        return state;
    }

    state.buttons = (uint16_t)(~samples.idle_high & 0x3fu);
    if ((samples.first_low & 0x10u) == 0) state.buttons |= MD_A;
    if ((samples.first_low & 0x20u) == 0) state.buttons |= MD_START;
    if (state.type == MD_PAD_6_BUTTON) {
        state.buttons |= (uint16_t)((~samples.third_high & 0x0fu) << 8);
    }
    return state;
}

joystick_state_t md_pad_to_msx(md_pad_state_t state)
{
    if (state.type == MD_PAD_NONE) {
        return 0;
    }

    joystick_state_t result = 0;
    if (state.buttons & MD_UP) result |= JOYSTICK_UP;
    if (state.buttons & MD_DOWN) result |= JOYSTICK_DOWN;
    if (state.buttons & MD_LEFT) result |= JOYSTICK_LEFT;
    if (state.buttons & MD_RIGHT) result |= JOYSTICK_RIGHT;
    if (state.buttons & MD_B) result |= JOYSTICK_BUTTON_A;
    if (state.buttons & MD_C) result |= JOYSTICK_BUTTON_B;

    if ((result & (JOYSTICK_UP | JOYSTICK_DOWN)) == (JOYSTICK_UP | JOYSTICK_DOWN)) {
        result &= (joystick_state_t)~(JOYSTICK_UP | JOYSTICK_DOWN);
    }
    if ((result & (JOYSTICK_LEFT | JOYSTICK_RIGHT)) == (JOYSTICK_LEFT | JOYSTICK_RIGHT)) {
        result &= (joystick_state_t)~(JOYSTICK_LEFT | JOYSTICK_RIGHT);
    }
    return result;
}
