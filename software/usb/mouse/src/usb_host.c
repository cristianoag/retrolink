#include "retrolink/usb_host.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"
#include "retrolink/hid_mouse.h"
#include "retrolink/mouse_port.h"
#include "retrolink/msx_mouse.h"
#include "retrolink/status_led.h"

#define RETROLINK_MAX_HID_SLOTS 8u

_Static_assert(RETROLINK_USB_HOST_DM_GPIO == RETROLINK_USB_HOST_DP_GPIO + 1u,
               "PIO USB DPDM pinout requires D- on D+ GPIO + 1");

typedef struct {
    bool in_use;
    uint8_t dev_addr;
    uint8_t instance;
    uint8_t buttons;
    hid_mouse_t mouse;
} hid_slot_t;

static hid_slot_t hid_slots[RETROLINK_MAX_HID_SLOTS];
static msx_mouse_scaler_t scaler;
static joystick_state_t msx_buttons;
static bool mode_selection_pending;
static absolute_time_t mode_selection_deadline;
static int32_t joystick_x;
static int32_t joystick_y;
static absolute_time_t next_joystick_sample;

static hid_slot_t *find_slot(uint8_t dev_addr, uint8_t instance)
{
    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        if (hid_slots[index].in_use && hid_slots[index].dev_addr == dev_addr && hid_slots[index].instance == instance) {
            return &hid_slots[index];
        }
    }
    return NULL;
}

static hid_slot_t *claim_slot(uint8_t dev_addr, uint8_t instance)
{
    hid_slot_t *slot = find_slot(dev_addr, instance);
    if (slot != NULL) return slot;
    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        if (!hid_slots[index].in_use) {
            memset(&hid_slots[index], 0, sizeof(hid_slots[index]));
            hid_slots[index].in_use = true;
            hid_slots[index].dev_addr = dev_addr;
            hid_slots[index].instance = instance;
            return &hid_slots[index];
        }
    }
    return NULL;
}

static void update_buttons(void)
{
    uint8_t buttons = 0;
    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        if (hid_slots[index].in_use) buttons |= hid_slots[index].buttons;
    }
    joystick_state_t state = msx_mouse_buttons(buttons);
    if (state == msx_buttons) return;
    if ((state & (joystick_state_t)~msx_buttons) != 0) status_led_pulse();
    msx_buttons = state;
    mouse_port_set_buttons(state);
    debug_cdc_log("MSX mouse buttons: A=%u B=%u\r\n",
                  (state & JOYSTICK_BUTTON_A) != 0, (state & JOYSTICK_BUTTON_B) != 0);
}

static void set_mode(mouse_port_mode_t mode)
{
    if (mouse_port_mode() == mode) return;
    memset(&scaler, 0, sizeof(scaler));
    joystick_x = 0;
    joystick_y = 0;
    next_joystick_sample = make_timeout_time_ms(RETROLINK_MOUSE_JOYSTICK_SAMPLE_MS);
    mouse_port_set_mode(mode);
    static const char *const names[] = {"released", "MSX mouse", "joystick emulation"};
    debug_cdc_log("MSX port mode: %s\r\n", names[mode]);
}

static void update_mode(void)
{
    bool mounted = false;
    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        mounted |= hid_slots[index].in_use;
    }
    if (!mounted) {
        mode_selection_pending = false;
        set_mode(MOUSE_PORT_MODE_RELEASED);
    } else if (mouse_port_mode() == MOUSE_PORT_MODE_RELEASED) {
        mode_selection_pending = true;
        mode_selection_deadline = make_timeout_time_ms(RETROLINK_MOUSE_JOYSTICK_SELECT_MS);
        set_mode(MOUSE_PORT_MODE_MOUSE);
    }
}

static void release_slot(hid_slot_t *slot)
{
    memset(slot, 0, sizeof(*slot));
    update_buttons();
    update_mode();
}

static void request_report(hid_slot_t *slot)
{
    if (!tuh_hid_receive_report(slot->dev_addr, slot->instance)) {
        slot->buttons = 0;
        update_buttons();
        debug_cdc_log("HID receive request failed; buttons released: addr=%u instance=%u\r\n",
                      slot->dev_addr, slot->instance);
    }
}

void usb_host_init(void)
{
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = RETROLINK_USB_HOST_DP_GPIO;
    pio_cfg.pinout = PIO_USB_PINOUT_DPDM;

    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);

    debug_cdc_log("USB host initialized on root port %u, D+ GPIO%u, D- GPIO%u\r\n",
                  BOARD_TUH_RHPORT, RETROLINK_USB_HOST_DP_GPIO, RETROLINK_USB_HOST_DM_GPIO);
}

void usb_host_task(void)
{
    if (mode_selection_pending && time_reached(mode_selection_deadline)) {
        mode_selection_pending = false;
    }
    if (mouse_port_mode() == MOUSE_PORT_MODE_JOYSTICK && time_reached(next_joystick_sample)) {
        mouse_port_set_directions(msx_mouse_joystick_directions(joystick_x, joystick_y,
                                                                RETROLINK_MOUSE_JOYSTICK_THRESHOLD));
        joystick_x = 0;
        joystick_y = 0;
        next_joystick_sample = make_timeout_time_ms(RETROLINK_MOUSE_JOYSTICK_SAMPLE_MS);
    }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    bool boot = itf_protocol == HID_ITF_PROTOCOL_MOUSE && tuh_hid_get_protocol(dev_addr, instance) == HID_PROTOCOL_BOOT;

    debug_cdc_log("HID mounted: addr=%u instance=%u vid=%04x pid=%04x protocol=%u boot=%u\r\n",
                  dev_addr, instance, vid, pid, itf_protocol, boot);

    hid_mouse_t mouse = {0};
    bool supported;
    if (boot) {
        hid_mouse_init_boot(&mouse);
        supported = true;
    } else if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        supported = false;
    } else {
        supported = hid_mouse_parse(&mouse, desc_report, desc_len);
    }
    debug_cdc_log("HID mouse mapping: addr=%u instance=%u supported=%u reports=%u descriptor_len=%u\r\n",
                  dev_addr, instance, supported, supported ? mouse.report_count : 0u, desc_len);
    if (!supported) return;

    hid_slot_t *slot = claim_slot(dev_addr, instance);
    if (slot == NULL) {
        debug_cdc_log("HID slot capacity exceeded: addr=%u instance=%u\r\n", dev_addr, instance);
        return;
    }
    slot->mouse = mouse;
    slot->buttons = 0;
    update_mode();
    request_report(slot);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    debug_cdc_log("HID unmounted: addr=%u instance=%u\r\n", dev_addr, instance);
    hid_slot_t *slot = find_slot(dev_addr, instance);
    if (slot != NULL) release_slot(slot);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t report_len)
{
    hid_slot_t *slot = find_slot(dev_addr, instance);
    if (slot == NULL) return;

    hid_mouse_input_t decoded;
    hid_mouse_decode_result_t result = hid_mouse_decode(&slot->mouse, report, report_len, &decoded);
    if (result == HID_MOUSE_DECODE_INVALID) {
        slot->buttons = 0;
        update_buttons();
        debug_cdc_log("HID invalid mouse report; buttons released: addr=%u instance=%u len=%u\r\n",
                      dev_addr, instance, report_len);
    } else if (result == HID_MOUSE_DECODE_OK) {
        if (decoded.has_buttons) slot->buttons = decoded.buttons;
        if (mode_selection_pending) {
            mode_selection_pending = false;
            if ((slot->buttons & HID_MOUSE_BUTTON_LEFT) != 0) set_mode(MOUSE_PORT_MODE_JOYSTICK);
        }
        if (decoded.dx != 0 || decoded.dy != 0) {
            int32_t msx_dx;
            int32_t msx_dy;
            msx_mouse_scale(&scaler, decoded.dx, decoded.dy, RETROLINK_MOUSE_DIVISOR, &msx_dx, &msx_dy);
            if (mouse_port_mode() == MOUSE_PORT_MODE_MOUSE) {
                mouse_port_add_motion(msx_dx, msx_dy);
            } else if (mouse_port_mode() == MOUSE_PORT_MODE_JOYSTICK) {
                joystick_x += msx_dx;
                joystick_y += msx_dy;
            }
            status_led_pulse();
        }
        update_buttons();
    }
    request_report(slot);
}
