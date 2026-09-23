#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "retrolink/hid_joystick.h"

static const uint8_t gamepad_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
    0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x02, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x02, 0x81, 0x02,
    0x75, 0x02, 0x95, 0x01, 0x81, 0x03, 0xc0,
};

static joystick_state_t decode(hid_joystick_t *joystick, const uint8_t *data, size_t length)
{
    joystick_state_t state = 0;
    assert(hid_joystick_decode(joystick, data, length, &state));
    return state;
}

static void test_axes_buttons_hat(void)
{
    hid_joystick_t joystick;
    assert(hid_joystick_parse(&joystick, gamepad_descriptor, sizeof(gamepad_descriptor)));
    uint8_t data[] = {128, 128, 0x08};
    assert(decode(&joystick, data, sizeof(data)) == 0);
    data[2] = 0x18;
    assert(decode(&joystick, data, sizeof(data)) == JOYSTICK_BUTTON_A);
    data[2] = 0x28;
    assert(decode(&joystick, data, sizeof(data)) == JOYSTICK_BUTTON_B);
    data[0] = 0;
    data[1] = 255;
    data[2] = 0x38;
    assert(decode(&joystick, data, sizeof(data)) == (JOYSTICK_LEFT | JOYSTICK_DOWN | JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B));
    data[0] = 255;
    data[1] = 0;
    data[2] = 0x08;
    assert(decode(&joystick, data, sizeof(data)) == (JOYSTICK_RIGHT | JOYSTICK_UP));
    data[0] = 128;
    data[1] = 128;
    static const joystick_state_t expected[] = {
        JOYSTICK_UP, JOYSTICK_UP | JOYSTICK_RIGHT, JOYSTICK_RIGHT, JOYSTICK_RIGHT | JOYSTICK_DOWN,
        JOYSTICK_DOWN, JOYSTICK_DOWN | JOYSTICK_LEFT, JOYSTICK_LEFT, JOYSTICK_LEFT | JOYSTICK_UP, 0,
    };
    for (uint8_t direction = 0; direction < sizeof(expected); ++direction) {
        data[2] = direction;
        assert(decode(&joystick, data, sizeof(data)) == expected[direction]);
    }
    joystick_state_t state;
    assert(!hid_joystick_decode(&joystick, data, 1, &state));
    assert(state == 0);
    for (size_t length = 0; length < sizeof(gamepad_descriptor); ++length) {
        assert(!hid_joystick_parse(&joystick, gamepad_descriptor, length));
        assert(joystick.field_count == 0);
    }
}

static void test_report_ids_signed_axes(void)
{
    const uint8_t descriptor[] = {
        0x05, 0x01, 0x09, 0x04, 0xa1, 0x01,
        0x85, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7f,
        0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
        0xa4, 0x85, 0x02, 0x05, 0x09, 0x19, 0x01, 0x29, 0x02,
        0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02,
        0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0xb4, 0xc0,
    };
    hid_joystick_t joystick;
    assert(hid_joystick_parse(&joystick, descriptor, sizeof(descriptor)));
    const uint8_t axes[] = {1, 0x81, 0x7f};
    const uint8_t buttons[] = {2, 2};
    const uint8_t unknown[] = {3, 0};
    const uint8_t centered[] = {1, 0, 0};
    const uint8_t released[] = {2, 0};
    assert(decode(&joystick, axes, sizeof(axes)) == (JOYSTICK_LEFT | JOYSTICK_DOWN));
    assert(decode(&joystick, buttons, sizeof(buttons)) == (JOYSTICK_LEFT | JOYSTICK_DOWN | JOYSTICK_BUTTON_B));
    assert(decode(&joystick, unknown, sizeof(unknown)) == (JOYSTICK_LEFT | JOYSTICK_DOWN | JOYSTICK_BUTTON_B));
    assert(decode(&joystick, centered, sizeof(centered)) == JOYSTICK_BUTTON_B);
    assert(decode(&joystick, released, sizeof(released)) == 0);
}

static void test_non_gamepad_and_bounds(void)
{
    hid_joystick_t joystick;
    uint8_t descriptor[sizeof(gamepad_descriptor)];
    memcpy(descriptor, gamepad_descriptor, sizeof(descriptor));
    descriptor[3] = 2;
    assert(!hid_joystick_parse(&joystick, descriptor, sizeof(descriptor)));
    memcpy(descriptor, gamepad_descriptor, sizeof(descriptor));
    descriptor[18] = 255;
    assert(!hid_joystick_parse(&joystick, descriptor, sizeof(descriptor)));
    assert(!hid_joystick_parse(&joystick, NULL, 0));
}

int main(void)
{
    test_axes_buttons_hat();
    test_report_ids_signed_axes();
    test_non_gamepad_and_bounds();
    puts("HID joystick tests passed");
    return 0;
}