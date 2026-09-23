#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "retrolink/md_pad.h"

static md_pad_samples_t make_samples(uint16_t buttons, bool six_button)
{
    uint8_t low = 0x33u;
    low &= (uint8_t)~(buttons & 0x03u);
    if (buttons & MD_A) low &= (uint8_t)~0x10u;
    if (buttons & MD_START) low &= (uint8_t)~0x20u;
    md_pad_samples_t samples = {
        .idle_high = (uint8_t)(~buttons & 0x3fu),
        .first_low = low,
        .third_low = six_button ? (uint8_t)(low & 0x30u) : low,
        .third_high = six_button ? (uint8_t)(0x30u | (~(buttons >> 8) & 0x0fu)) :
                                  (uint8_t)(~buttons & 0x3fu),
        .fourth_low = six_button ? (uint8_t)(low | 0x0fu) : low,
    };
    return samples;
}

static void expect_invalid(md_pad_samples_t samples)
{
    md_pad_state_t state = md_pad_decode(samples);
    assert(state.type == MD_PAD_NONE);
    assert(state.buttons == 0);
    assert(md_pad_to_msx(state) == 0);
}

int main(void)
{
    for (unsigned six_button = 0; six_button <= 1; ++six_button) {
        uint16_t limit = six_button ? 4096u : 256u;
        for (uint16_t buttons = 0; buttons < limit; ++buttons) {
            md_pad_state_t state = md_pad_decode(make_samples(buttons, six_button != 0));
            assert(state.type == (six_button ? MD_PAD_6_BUTTON : MD_PAD_3_BUTTON));
            assert(state.buttons == buttons);
            joystick_state_t expected = (joystick_state_t)(buttons & 0x3fu);
            if ((buttons & 3u) == 3u) expected &= (joystick_state_t)~3u;
            if ((buttons & 12u) == 12u) expected &= (joystick_state_t)~12u;
            assert(md_pad_to_msx(state) == expected);
        }
    }

    expect_invalid((md_pad_samples_t){0x3f, 0x3f, 0x3f, 0x3f, 0x3f});
    md_pad_samples_t samples = make_samples(MD_B, true);
    samples.first_low |= 0x04u;
    expect_invalid(samples);
    samples = make_samples(MD_C, true);
    samples.third_low |= 0x04u;
    expect_invalid(samples);
    samples = make_samples(MD_B | MD_C, true);
    samples.fourth_low = 0x35u;
    expect_invalid(samples);
    samples = make_samples(MD_UP, false);
    samples.fourth_low = 0x3fu;
    expect_invalid(samples);

    md_pad_state_t disconnected = {MD_PAD_NONE, 0xffffu};
    assert(md_pad_to_msx(disconnected) == 0);
    puts("MD decoder tests passed: all 256 3-button and 4096 6-button states, invalid frames, B/C mapping");
    return 0;
}
