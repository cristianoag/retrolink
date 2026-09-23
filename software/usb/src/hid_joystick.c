#include "retrolink/hid_joystick.h"

#include <string.h>

#define MAX_USAGES 32u
#define MAX_COLLECTIONS 8u
#define MAX_GLOBAL_STACK 4u
#define DESKTOP_USAGE(value) (0x00010000u | (value))
#define BUTTON_USAGE(value) (0x00090000u | (value))

typedef struct {
    uint32_t page;
    int64_t minimum;
    int64_t maximum;
    uint32_t size;
    uint32_t count;
    uint8_t id;
} global_items_t;

typedef struct {
    uint32_t usages[MAX_USAGES];
    uint32_t minimum;
    uint32_t maximum;
    size_t count;
    bool has_minimum;
    bool has_maximum;
} local_items_t;

static int64_t signed_value(uint32_t value, unsigned bits)
{
    return (value & (1u << (bits - 1u))) != 0 ? (int64_t)value - (INT64_C(1) << bits) : value;
}

static uint32_t usage_at(const local_items_t *local, uint32_t index)
{
    if (local->count != 0) {
        return local->usages[index < local->count ? index : local->count - 1u];
    }
    if (local->has_minimum && local->has_maximum && local->maximum >= local->minimum) {
        uint32_t span = local->maximum - local->minimum;
        return local->minimum + (index < span ? index : span);
    }
    return 0;
}

static bool supported_usage(uint32_t usage)
{
    return usage == DESKTOP_USAGE(0x30) || usage == DESKTOP_USAGE(0x31) ||
           usage == DESKTOP_USAGE(0x39) ||
           (usage >= DESKTOP_USAGE(0x90) && usage <= DESKTOP_USAGE(0x93)) ||
           usage == BUTTON_USAGE(1) || usage == BUTTON_USAGE(2);
}

static bool add_input(hid_joystick_t *joystick, const global_items_t *global,
                      const local_items_t *local, uint32_t flags, bool gamepad)
{
    uint8_t report_index = 0;
    while (report_index < joystick->report_count && joystick->reports[report_index].id != global->id) {
        ++report_index;
    }
    if (report_index == joystick->report_count) {
        if (joystick->report_count == HID_JOYSTICK_MAX_REPORTS) {
            return false;
        }
        joystick->reports[joystick->report_count++].id = global->id;
    }
    hid_joystick_report_t *report = &joystick->reports[report_index];
    uint64_t added_bits = (uint64_t)global->size * global->count;
    if (global->size == 0 || global->count == 0 || added_bits > HID_JOYSTICK_MAX_REPORT_BITS - report->bits) {
        return false;
    }
    if (gamepad && (flags & 7u) == 2u) {
        for (uint32_t index = 0; index < global->count; ++index) {
            uint32_t usage = usage_at(local, index);
            if (!supported_usage(usage)) {
                continue;
            }
            if (global->size > 32 || global->maximum <= global->minimum ||
                joystick->field_count == HID_JOYSTICK_MAX_FIELDS) {
                return false;
            }
            if (usage == DESKTOP_USAGE(0x39) && global->maximum - global->minimum != 7 &&
                global->maximum - global->minimum != 3) {
                return false;
            }
            joystick->fields[joystick->field_count++] = (hid_joystick_field_t){
                .usage = usage, .minimum = global->minimum, .maximum = global->maximum,
                .offset = (uint16_t)(report->bits + index * global->size),
                .size = (uint8_t)global->size, .report_index = report_index,
            };
        }
    }
    report->bits += (uint16_t)added_bits;
    return true;
}

static bool parse_descriptor(hid_joystick_t *joystick, const uint8_t *descriptor, size_t length)
{
    global_items_t global = {0};
    global_items_t stack[MAX_GLOBAL_STACK];
    local_items_t local = {0};
    bool collections[MAX_COLLECTIONS];
    size_t depth = 0;
    size_t stack_depth = 0;
    bool gamepad = false;

    for (size_t position = 0; position < length;) {
        uint8_t prefix = descriptor[position++];
        unsigned size = prefix & 3u;
        size = size == 3 ? 4 : size;
        unsigned type = (prefix >> 2) & 3u;
        unsigned tag = prefix >> 4;
        if (prefix == 0xfe || length - position < size || type == 3) {
            return false;
        }
        uint32_t value = 0;
        for (unsigned byte_index = 0; byte_index < size; ++byte_index) {
            value |= (uint32_t)descriptor[position++] << (8u * byte_index);
        }
        if (type == 1) {
            switch (tag) {
            case 0: global.page = value; break;
            case 1:
                if (size == 0) return false;
                global.minimum = signed_value(value, size * 8u);
                break;
            case 2:
                if (size == 0) return false;
                global.maximum = global.minimum < 0 ? signed_value(value, size * 8u) : value;
                break;
            case 7: global.size = value; break;
            case 8:
                if (value == 0 || value > 255) return false;
                global.id = (uint8_t)value;
                joystick->has_report_ids = true;
                break;
            case 9: global.count = value; break;
            case 10:
                if (stack_depth == MAX_GLOBAL_STACK) return false;
                stack[stack_depth++] = global;
                break;
            case 11:
                if (stack_depth == 0) return false;
                global = stack[--stack_depth];
                break;
            default: break;
            }
        } else if (type == 2) {
            uint32_t usage = size == 4 ? value : (global.page << 16) | value;
            switch (tag) {
            case 0:
                if (local.count == MAX_USAGES || local.has_minimum) return false;
                local.usages[local.count++] = usage;
                break;
            case 1:
                if (local.count != 0 || local.has_minimum) return false;
                local.minimum = usage;
                local.has_minimum = true;
                break;
            case 2:
                local.maximum = usage;
                local.has_maximum = true;
                break;
            case 10: return false;
            default: break;
            }
        } else if (type == 0) {
            if (local.has_minimum != local.has_maximum ||
                (local.has_minimum && local.maximum < local.minimum)) return false;
            switch (tag) {
            case 8:
                if (depth == 0 || !add_input(joystick, &global, &local, value, gamepad)) return false;
                break;
            case 10:
                if (depth == MAX_COLLECTIONS) return false;
                collections[depth++] = gamepad;
                if (value == 1) {
                    uint32_t usage = usage_at(&local, 0);
                    gamepad = usage == DESKTOP_USAGE(4) || usage == DESKTOP_USAGE(5);
                }
                break;
            case 12:
                if (depth == 0) return false;
                gamepad = collections[--depth];
                break;
            case 9:
            case 11: break;
            default: return false;
            }
            memset(&local, 0, sizeof(local));
        }
    }
    if (depth != 0 || stack_depth != 0 || joystick->field_count == 0) return false;
    for (uint8_t index = 0; index < joystick->report_count; ++index) {
        if (joystick->has_report_ids && joystick->reports[index].id == 0) return false;
    }
    return true;
}

bool hid_joystick_parse(hid_joystick_t *joystick, const uint8_t *descriptor, size_t length)
{
    memset(joystick, 0, sizeof(*joystick));
    if (descriptor == NULL || !parse_descriptor(joystick, descriptor, length)) {
        memset(joystick, 0, sizeof(*joystick));
        return false;
    }
    return true;
}

static joystick_state_t decode_field(const hid_joystick_field_t *field, const uint8_t *data)
{
    uint32_t raw = 0;
    for (unsigned bit = 0; bit < field->size; ++bit) {
        unsigned offset = field->offset + bit;
        raw |= (uint32_t)((data[offset / 8u] >> (offset % 8u)) & 1u) << bit;
    }
    int64_t value = field->minimum < 0 ? signed_value(raw, field->size) : raw;
    if (value < field->minimum || value > field->maximum) return 0;
    if (field->usage == BUTTON_USAGE(1)) return value != 0 ? JOYSTICK_BUTTON_A : 0;
    if (field->usage == BUTTON_USAGE(2)) return value != 0 ? JOYSTICK_BUTTON_B : 0;
    if (field->usage >= DESKTOP_USAGE(0x90) && field->usage <= DESKTOP_USAGE(0x93)) {
        static const joystick_state_t directions[] = {JOYSTICK_UP, JOYSTICK_DOWN, JOYSTICK_RIGHT, JOYSTICK_LEFT};
        return value != 0 ? directions[field->usage - DESKTOP_USAGE(0x90)] : 0;
    }
    if (field->usage == DESKTOP_USAGE(0x39)) {
        static const joystick_state_t hats[] = {
            JOYSTICK_UP, JOYSTICK_UP | JOYSTICK_RIGHT, JOYSTICK_RIGHT, JOYSTICK_DOWN | JOYSTICK_RIGHT,
            JOYSTICK_DOWN, JOYSTICK_DOWN | JOYSTICK_LEFT, JOYSTICK_LEFT, JOYSTICK_UP | JOYSTICK_LEFT,
        };
        unsigned direction = (unsigned)(value - field->minimum);
        if (field->maximum - field->minimum == 3) direction *= 2;
        return hats[direction];
    }
    int64_t span = field->maximum - field->minimum;
    int64_t position = value - field->minimum;
    bool horizontal = field->usage == DESKTOP_USAGE(0x30);
    if (position * 4 < span) return horizontal ? JOYSTICK_LEFT : JOYSTICK_UP;
    if (position * 4 > span * 3) return horizontal ? JOYSTICK_RIGHT : JOYSTICK_DOWN;
    return 0;
}

bool hid_joystick_decode(hid_joystick_t *joystick, const uint8_t *data, size_t length, joystick_state_t *state)
{
    *state = 0;
    if (data == NULL || length == 0) goto invalid;
    uint8_t id = joystick->has_report_ids ? *data++ : 0;
    if (joystick->has_report_ids) --length;
    for (uint8_t report_index = 0; report_index < joystick->report_count; ++report_index) {
        hid_joystick_report_t *report = &joystick->reports[report_index];
        if (report->id == id) {
            if (length < (report->bits + 7u) / 8u) goto invalid;
            report->state = 0;
            for (uint8_t field_index = 0; field_index < joystick->field_count; ++field_index) {
                const hid_joystick_field_t *field = &joystick->fields[field_index];
                if (field->report_index == report_index) report->state |= decode_field(field, data);
            }
        }
        *state |= report->state;
    }
    return true;

invalid:
    for (uint8_t index = 0; index < joystick->report_count; ++index) joystick->reports[index].state = 0;
    *state = 0;
    return false;
}