#include "retrolink/hid_mouse.h"

#include <string.h>

#define MAX_USAGES 32u
#define MAX_COLLECTIONS 8u
#define MAX_GLOBAL_STACK 4u
#define DESKTOP_USAGE(value) (0x00010000u | (value))
#define BUTTON_USAGE(value) (0x00090000u | (value))
#define INPUT_CONSTANT 0x01u
#define INPUT_VARIABLE 0x02u
#define INPUT_RELATIVE 0x04u

typedef struct {
    uint32_t page;
    int64_t minimum;
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

static void map_field(hid_mouse_field_t *field, uint16_t offset, uint32_t size, bool is_signed)
{
    if (field->size == 0) {
        *field = (hid_mouse_field_t){.offset = offset, .size = (uint8_t)size, .is_signed = is_signed};
    }
}

static bool add_input(hid_mouse_t *mouse, const global_items_t *global, const local_items_t *local,
                      uint32_t flags, bool in_mouse)
{
    uint8_t report_index = 0;
    while (report_index < mouse->report_count && mouse->reports[report_index].id != global->id) {
        ++report_index;
    }
    if (report_index == mouse->report_count) {
        if (mouse->report_count == HID_MOUSE_MAX_REPORTS) {
            return false;
        }
        mouse->reports[mouse->report_count++].id = global->id;
    }
    hid_mouse_layout_t *report = &mouse->reports[report_index];
    uint64_t added_bits = (uint64_t)global->size * global->count;
    if (global->size == 0 || global->count == 0 || added_bits > HID_MOUSE_MAX_REPORT_BITS - report->bits) {
        return false;
    }
    if (in_mouse && (flags & (INPUT_CONSTANT | INPUT_VARIABLE)) == INPUT_VARIABLE && global->size <= 32) {
        bool is_signed = global->minimum < 0;
        for (uint32_t index = 0; index < global->count; ++index) {
            uint32_t usage = usage_at(local, index);
            uint16_t offset = (uint16_t)(report->bits + index * global->size);
            if ((flags & INPUT_RELATIVE) != 0 && usage == DESKTOP_USAGE(0x30)) {
                map_field(&report->x, offset, global->size, is_signed);
            } else if ((flags & INPUT_RELATIVE) != 0 && usage == DESKTOP_USAGE(0x31)) {
                map_field(&report->y, offset, global->size, is_signed);
            } else if (usage >= BUTTON_USAGE(1) && usage <= BUTTON_USAGE(HID_MOUSE_BUTTON_COUNT)) {
                map_field(&report->buttons[usage - BUTTON_USAGE(1)], offset, global->size, false);
            }
        }
    }
    report->bits += (uint16_t)added_bits;
    return true;
}

static bool parse_descriptor(hid_mouse_t *mouse, const uint8_t *descriptor, size_t length)
{
    global_items_t global = {0};
    global_items_t stack[MAX_GLOBAL_STACK];
    local_items_t local = {0};
    bool collections[MAX_COLLECTIONS];
    size_t depth = 0;
    size_t stack_depth = 0;
    bool in_mouse = false;

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
            case 7: global.size = value; break;
            case 8:
                if (value == 0 || value > 255) return false;
                global.id = (uint8_t)value;
                mouse->has_report_ids = true;
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
                if (depth == 0 || !add_input(mouse, &global, &local, value, in_mouse)) return false;
                break;
            case 10:
                if (depth == MAX_COLLECTIONS) return false;
                collections[depth++] = in_mouse;
                if (value == 1) {
                    in_mouse = usage_at(&local, 0) == DESKTOP_USAGE(0x02);
                }
                break;
            case 12:
                if (depth == 0) return false;
                in_mouse = collections[--depth];
                break;
            case 9:
            case 11: break;
            default: return false;
            }
            memset(&local, 0, sizeof(local));
        }
    }
    if (depth != 0 || stack_depth != 0) return false;
    bool has_motion = false;
    for (uint8_t index = 0; index < mouse->report_count; ++index) {
        if (mouse->has_report_ids && mouse->reports[index].id == 0) return false;
        has_motion |= mouse->reports[index].x.size != 0 || mouse->reports[index].y.size != 0;
    }
    return has_motion;
}

bool hid_mouse_parse(hid_mouse_t *mouse, const uint8_t *descriptor, size_t length)
{
    memset(mouse, 0, sizeof(*mouse));
    if (descriptor == NULL || !parse_descriptor(mouse, descriptor, length)) {
        memset(mouse, 0, sizeof(*mouse));
        return false;
    }
    return true;
}

void hid_mouse_init_boot(hid_mouse_t *mouse)
{
    memset(mouse, 0, sizeof(*mouse));
    mouse->boot_protocol = true;
    mouse->report_count = 1;
    hid_mouse_layout_t *report = &mouse->reports[0];
    for (uint16_t index = 0; index < HID_MOUSE_BUTTON_COUNT; ++index) {
        report->buttons[index] = (hid_mouse_field_t){.offset = index, .size = 1};
    }
    report->x = (hid_mouse_field_t){.offset = 8, .size = 8, .is_signed = true};
    report->y = (hid_mouse_field_t){.offset = 16, .size = 8, .is_signed = true};
    report->bits = 24;
}

static int32_t read_field(const hid_mouse_field_t *field, const uint8_t *data)
{
    uint32_t raw = 0;
    for (unsigned bit = 0; bit < field->size; ++bit) {
        unsigned offset = field->offset + bit;
        raw |= (uint32_t)((data[offset / 8u] >> (offset % 8u)) & 1u) << bit;
    }
    int64_t value = field->is_signed ? signed_value(raw, field->size) : raw;
    if (value > INT32_MAX) return INT32_MAX;
    return (int32_t)value;
}

hid_mouse_decode_result_t hid_mouse_decode(const hid_mouse_t *mouse, const uint8_t *data, size_t length,
                                           hid_mouse_input_t *report)
{
    memset(report, 0, sizeof(*report));
    if (data == NULL || length == 0) return HID_MOUSE_DECODE_INVALID;
    uint8_t id = 0;
    if (mouse->has_report_ids) {
        id = *data++;
        --length;
    }
    for (uint8_t index = 0; index < mouse->report_count; ++index) {
        const hid_mouse_layout_t *layout = &mouse->reports[index];
        if (layout->id != id) continue;
        if (length < (layout->bits + 7u) / 8u) return HID_MOUSE_DECODE_INVALID;
        if (layout->x.size != 0) {
            report->dx = read_field(&layout->x, data);
            report->has_motion = true;
        }
        if (layout->y.size != 0) {
            report->dy = read_field(&layout->y, data);
            report->has_motion = true;
        }
        for (unsigned button = 0; button < HID_MOUSE_BUTTON_COUNT; ++button) {
            if (layout->buttons[button].size == 0) continue;
            report->has_buttons = true;
            if (read_field(&layout->buttons[button], data) != 0) report->buttons |= (uint8_t)(1u << button);
        }
        return report->has_motion || report->has_buttons ? HID_MOUSE_DECODE_OK : HID_MOUSE_DECODE_IGNORED;
    }
    return HID_MOUSE_DECODE_IGNORED;
}
