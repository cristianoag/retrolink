#ifndef RETROLINK_HID_MOUSE_H
#define RETROLINK_HID_MOUSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HID_MOUSE_MAX_REPORTS 8u
#define HID_MOUSE_MAX_REPORT_BITS 512u
#define HID_MOUSE_BUTTON_COUNT 3u

enum {
    HID_MOUSE_BUTTON_LEFT = 1u << 0,
    HID_MOUSE_BUTTON_RIGHT = 1u << 1,
    HID_MOUSE_BUTTON_MIDDLE = 1u << 2,
};

typedef struct {
    uint16_t offset;
    uint8_t size;
    bool is_signed;
} hid_mouse_field_t;

typedef struct {
    hid_mouse_field_t x;
    hid_mouse_field_t y;
    hid_mouse_field_t buttons[HID_MOUSE_BUTTON_COUNT];
    uint16_t bits;
    uint8_t id;
} hid_mouse_layout_t;

typedef struct {
    hid_mouse_layout_t reports[HID_MOUSE_MAX_REPORTS];
    uint8_t report_count;
    bool has_report_ids;
    bool boot_protocol;
} hid_mouse_t;

typedef struct {
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    bool has_motion;
    bool has_buttons;
} hid_mouse_input_t;

typedef enum {
    HID_MOUSE_DECODE_INVALID = 0,
    HID_MOUSE_DECODE_IGNORED,
    HID_MOUSE_DECODE_OK,
} hid_mouse_decode_result_t;

/* Maps relative X/Y and buttons 1-3 inside Generic Desktop Mouse application collections. */
bool hid_mouse_parse(hid_mouse_t *mouse, const uint8_t *descriptor, size_t length);

/* Uses the fixed HID boot mouse report: buttons, signed 8-bit X, signed 8-bit Y. */
void hid_mouse_init_boot(hid_mouse_t *mouse);

/* Unknown report IDs and reports without mapped controls return HID_MOUSE_DECODE_IGNORED. */
hid_mouse_decode_result_t hid_mouse_decode(const hid_mouse_t *mouse, const uint8_t *data, size_t length,
                                           hid_mouse_input_t *report);

#endif
