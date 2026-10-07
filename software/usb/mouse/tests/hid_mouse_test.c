#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "retrolink/hid_mouse.h"

/* Typical three-button wheel mouse without report IDs. */
static const uint8_t basic_mouse_descriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01,
    0x09, 0x01, 0xa1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f,
    0x75, 0x08, 0x95, 0x03, 0x81, 0x06,
    0xc0, 0xc0,
};

/* Wireless-receiver style: report ID 2, 16 buttons, 12-bit X/Y, plus a consumer report ID 3. */
static const uint8_t report_id_mouse_descriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x02,
    0x09, 0x01, 0xa1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x10, 0x75, 0x01, 0x81, 0x02,
    0x05, 0x01, 0x16, 0x01, 0xf8, 0x26, 0xff, 0x07, 0x75, 0x0c, 0x95, 0x02,
    0x09, 0x30, 0x09, 0x31, 0x81, 0x06,
    0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x01, 0x09, 0x38, 0x81, 0x06,
    0xc0, 0xc0,
    0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x03,
    0x15, 0x00, 0x26, 0xff, 0x03, 0x19, 0x00, 0x2a, 0xff, 0x03,
    0x75, 0x10, 0x95, 0x01, 0x81, 0x00, 0xc0,
};

/* Absolute X/Y (tablet-style) must not be treated as mouse movement. */
static const uint8_t absolute_descriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x02, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x02, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x06, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02,
    0xc0,
};

static const uint8_t gamepad_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7f,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x08, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0xc0,
};

static hid_mouse_input_t decode_ok(const hid_mouse_t *mouse, const uint8_t *data, size_t length)
{
    hid_mouse_input_t input;
    assert(hid_mouse_decode(mouse, data, length, &input) == HID_MOUSE_DECODE_OK);
    return input;
}

static void test_boot_protocol(void)
{
    hid_mouse_t mouse;
    hid_mouse_init_boot(&mouse);
    assert(mouse.boot_protocol && !mouse.has_report_ids);
    const uint8_t report[] = {0x05, 0xfb, 0x07, 0x01};
    hid_mouse_input_t input = decode_ok(&mouse, report, sizeof(report));
    assert(input.dx == -5 && input.dy == 7);
    assert(input.buttons == (HID_MOUSE_BUTTON_LEFT | HID_MOUSE_BUTTON_MIDDLE));
    assert(input.has_motion && input.has_buttons);
    const uint8_t extremes[] = {0x02, 0x80, 0x7f};
    input = decode_ok(&mouse, extremes, sizeof(extremes));
    assert(input.dx == -128 && input.dy == 127 && input.buttons == HID_MOUSE_BUTTON_RIGHT);
    hid_mouse_input_t invalid;
    assert(hid_mouse_decode(&mouse, report, 2, &invalid) == HID_MOUSE_DECODE_INVALID);
    assert(hid_mouse_decode(&mouse, NULL, 0, &invalid) == HID_MOUSE_DECODE_INVALID);
}

static void test_basic_descriptor(void)
{
    hid_mouse_t mouse;
    assert(hid_mouse_parse(&mouse, basic_mouse_descriptor, sizeof(basic_mouse_descriptor)));
    assert(!mouse.has_report_ids && mouse.report_count == 1 && mouse.reports[0].bits == 32);
    const uint8_t report[] = {0x02, 0x0a, 0xf6, 0x01};
    hid_mouse_input_t input = decode_ok(&mouse, report, sizeof(report));
    assert(input.dx == 10 && input.dy == -10 && input.buttons == HID_MOUSE_BUTTON_RIGHT);
    hid_mouse_input_t invalid;
    assert(hid_mouse_decode(&mouse, report, 3, &invalid) == HID_MOUSE_DECODE_INVALID);
    for (size_t length = 0; length < sizeof(basic_mouse_descriptor); ++length) {
        assert(!hid_mouse_parse(&mouse, basic_mouse_descriptor, length));
        assert(mouse.report_count == 0);
    }
}

static void test_report_ids_and_12_bit_axes(void)
{
    hid_mouse_t mouse;
    assert(hid_mouse_parse(&mouse, report_id_mouse_descriptor, sizeof(report_id_mouse_descriptor)));
    assert(mouse.has_report_ids && mouse.report_count == 2);
    /* Buttons 1 and 3, X = -300 (0xed4), Y = +200 (0x0c8), wheel 0. */
    const uint8_t report[] = {0x02, 0x05, 0x00, 0xd4, 0x8e, 0x0c, 0x00};
    hid_mouse_input_t input = decode_ok(&mouse, report, sizeof(report));
    assert(input.dx == -300 && input.dy == 200);
    assert(input.buttons == (HID_MOUSE_BUTTON_LEFT | HID_MOUSE_BUTTON_MIDDLE));
    const uint8_t consumer[] = {0x03, 0xe9, 0x00};
    hid_mouse_input_t ignored;
    assert(hid_mouse_decode(&mouse, consumer, sizeof(consumer), &ignored) == HID_MOUSE_DECODE_IGNORED);
    const uint8_t unknown[] = {0x09, 0x00, 0x00};
    assert(hid_mouse_decode(&mouse, unknown, sizeof(unknown), &ignored) == HID_MOUSE_DECODE_IGNORED);
    assert(hid_mouse_decode(&mouse, report, 6, &ignored) == HID_MOUSE_DECODE_INVALID);
}

static void test_unsupported_descriptors(void)
{
    hid_mouse_t mouse;
    assert(!hid_mouse_parse(&mouse, absolute_descriptor, sizeof(absolute_descriptor)));
    assert(!hid_mouse_parse(&mouse, gamepad_descriptor, sizeof(gamepad_descriptor)));
    assert(!hid_mouse_parse(&mouse, NULL, 0));
    const uint8_t long_item[] = {0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0xfe, 0x00, 0x00, 0xc0};
    assert(!hid_mouse_parse(&mouse, long_item, sizeof(long_item)));
    const uint8_t unbalanced[] = {0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0xc0, 0xc0};
    assert(!hid_mouse_parse(&mouse, unbalanced, sizeof(unbalanced)));
}

int main(void)
{
    test_boot_protocol();
    test_basic_descriptor();
    test_report_ids_and_12_bit_axes();
    test_unsupported_descriptors();
    puts("HID mouse tests passed");
    return 0;
}
