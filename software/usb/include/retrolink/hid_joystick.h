#ifndef RETROLINK_HID_JOYSTICK_H
#define RETROLINK_HID_JOYSTICK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "retrolink/joystick.h"

#define HID_JOYSTICK_MAX_FIELDS 32u
#define HID_JOYSTICK_MAX_REPORTS 8u
#define HID_JOYSTICK_MAX_REPORT_BITS 512u

typedef struct {
    uint32_t usage;
    int64_t minimum;
    int64_t maximum;
    uint16_t offset;
    uint8_t size;
    uint8_t report_index;
} hid_joystick_field_t;

typedef struct {
    uint16_t bits;
    uint8_t id;
    joystick_state_t state;
} hid_joystick_report_t;

typedef struct {
    hid_joystick_field_t fields[HID_JOYSTICK_MAX_FIELDS];
    hid_joystick_report_t reports[HID_JOYSTICK_MAX_REPORTS];
    uint8_t field_count;
    uint8_t report_count;
    bool has_report_ids;
} hid_joystick_t;

bool hid_joystick_parse(hid_joystick_t *joystick, const uint8_t *descriptor, size_t length);
bool hid_joystick_decode(hid_joystick_t *joystick, const uint8_t *data, size_t length, joystick_state_t *state);

#endif