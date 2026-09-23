#include "retrolink/usb_host.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#include "retrolink/debug_cdc.h"
#include "retrolink/hid_joystick.h"
#include "retrolink/msx_port.h"
#include "retrolink/status_led.h"

#define RETROLINK_MAX_HID_SLOTS 8u

_Static_assert(RETROLINK_USB_HOST_DM_GPIO == RETROLINK_USB_HOST_DP_GPIO + 1u,
               "PIO USB DPDM pinout requires D- on D+ GPIO + 1");

typedef struct {
    bool in_use;
    uint8_t dev_addr;
    uint8_t instance;
    bool supported;
    joystick_state_t state;
    hid_joystick_t joystick;
} hid_slot_t;

static hid_slot_t hid_slots[RETROLINK_MAX_HID_SLOTS];

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
    if (slot != NULL) {
        return slot;
    }

    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        if (!hid_slots[index].in_use) {
            hid_slots[index].in_use = true;
            hid_slots[index].dev_addr = dev_addr;
            hid_slots[index].instance = instance;
            return &hid_slots[index];
        }
    }

    return NULL;
}

static void release_slot(uint8_t dev_addr, uint8_t instance)
{
    hid_slot_t *slot = find_slot(dev_addr, instance);
    if (slot != NULL) {
        memset(slot, 0, sizeof(*slot));
    }
}

static void update_msx_state(void)
{
    joystick_state_t state = 0;
    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        if (hid_slots[index].in_use) state |= hid_slots[index].state;
    }
    msx_port_set_state(state);
}

static void request_report(hid_slot_t *slot)
{
    if (!tuh_hid_receive_report(slot->dev_addr, slot->instance)) {
        slot->state = 0;
        for (uint8_t index = 0; index < slot->joystick.report_count; ++index) {
            slot->joystick.reports[index].state = 0;
        }
        update_msx_state();
        debug_cdc_log("HID receive request failed; outputs released: addr=%u instance=%u\r\n",
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
                  BOARD_TUH_RHPORT,
                  RETROLINK_USB_HOST_DP_GPIO,
                  RETROLINK_USB_HOST_DM_GPIO);
}

void usb_host_task(void)
{
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    debug_cdc_log("HID mounted: addr=%u instance=%u vid=%04x pid=%04x protocol=%u\r\n",
                  dev_addr,
                  instance,
                  vid,
                  pid,
                  tuh_hid_interface_protocol(dev_addr, instance));

    hid_slot_t *slot = claim_slot(dev_addr, instance);
    if (slot == NULL) {
        debug_cdc_log("HID slot capacity exceeded: addr=%u instance=%u\r\n", dev_addr, instance);
        return;
    }
    slot->state = 0;
    slot->supported = hid_joystick_parse(&slot->joystick, desc_report, desc_len);
    update_msx_state();
    debug_cdc_log("HID joystick mapping: addr=%u instance=%u supported=%u fields=%u reports=%u descriptor_len=%u\r\n",
                  dev_addr, instance, slot->supported, slot->joystick.field_count,
                  slot->joystick.report_count, desc_len);
    request_report(slot);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    debug_cdc_log("HID unmounted: addr=%u instance=%u\r\n", dev_addr, instance);
    release_slot(dev_addr, instance);
    update_msx_state();
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t report_len)
{
    hid_slot_t *slot = find_slot(dev_addr, instance);
    if (slot == NULL) return;
    if (slot->supported) {
        joystick_state_t previous = slot->state;
        if (!hid_joystick_decode(&slot->joystick, report, report_len, &slot->state)) {
            debug_cdc_log("HID invalid report; outputs released: addr=%u instance=%u len=%u\r\n",
                          dev_addr, instance, report_len);
        }
        if ((slot->state & (joystick_state_t)~previous) != 0) {
            status_led_pulse();
        }
        if (previous != slot->state) {
            debug_cdc_log("HID joystick: addr=%u instance=%u state=%02x\r\n", dev_addr, instance, slot->state);
        }
        update_msx_state();
    }
    request_report(slot);
}