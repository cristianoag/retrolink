#include "retrolink/usb_host.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"
#include "host/hcd.h"

#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"
#include "retrolink/hid_joystick.h"
#include "retrolink/msx_port.h"
#include "retrolink/status_led.h"

#define RETROLINK_MAX_HID_SLOTS 8u
#define RETROLINK_HID_RETRY_MS 100u
#define RETROLINK_DISCOVERY_RETRY_MS 5000u
#define RETROLINK_USB_RESET_MS 50u

_Static_assert(RETROLINK_USB_HOST_DM_GPIO == RETROLINK_USB_HOST_DP_GPIO + 1u,
               "PIO USB DPDM pinout requires D- on D+ GPIO + 1");

typedef struct {
    bool in_use;
    uint8_t dev_addr;
    uint8_t instance;
    bool supported;
    bool receive_failed;
    absolute_time_t next_retry;
    joystick_state_t state;
    hid_joystick_t joystick;
} hid_slot_t;

static hid_slot_t hid_slots[RETROLINK_MAX_HID_SLOTS];
static absolute_time_t discovery_deadline;
static bool discovery_reset_active;

static bool has_supported_joystick(void)
{
    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        if (hid_slots[index].in_use && hid_slots[index].supported) return true;
    }
    return false;
}

static void restart_discovery(void)
{
    if (!tuh_rhport_reset_bus(BOARD_TUH_RHPORT, true)) {
        debug_cdc_log("USB discovery: could not start USB-A bus reset\r\n");
        return;
    }

    debug_cdc_log("USB discovery: no supported joystick; restarting USB-A enumeration\r\n");

    // Cancel downstream enumeration before removing the whole tree.
    hcd_devtree_info_t tree;
    hcd_devtree_get_info(0, &tree);
    if (tree.hub_addr != 0) {
        hcd_event_t event = {
            .rhport = BOARD_TUH_RHPORT,
            .event_id = HCD_EVENT_DEVICE_REMOVE,
            .connection = {.hub_addr = tree.hub_addr, .hub_port = tree.hub_port},
        };
        hcd_event_handler(&event, false);
    }
    hcd_event_device_remove(BOARD_TUH_RHPORT, false);

    // TinyUSB removal does not clear a pending control transfer at address zero.
    if (hcd_edpt_abort_xfer(BOARD_TUH_RHPORT, 0, 0)) {
        hcd_event_xfer_complete(0, 0, 0, XFER_RESULT_FAILED, false);
    }
    hcd_device_close(BOARD_TUH_RHPORT, 0);

    discovery_reset_active = true;
    discovery_deadline = make_timeout_time_ms(RETROLINK_USB_RESET_MS);
}

static void discovery_task(void)
{
    if (!time_reached(discovery_deadline)) return;

    if (discovery_reset_active) {
        if (!tuh_rhport_reset_bus(BOARD_TUH_RHPORT, false)) {
            debug_cdc_log("USB discovery: could not finish USB-A bus reset; retrying\r\n");
            discovery_deadline = make_timeout_time_ms(RETROLINK_DISCOVERY_RETRY_MS);
            return;
        }
        discovery_reset_active = false;
        if (hcd_port_connect_status(BOARD_TUH_RHPORT)) {
            hcd_event_device_attach(BOARD_TUH_RHPORT, false);
            debug_cdc_log("USB discovery: receiver present; enumeration queued\r\n");
        } else {
            debug_cdc_log("USB discovery: receiver disconnected during recovery\r\n");
        }
    } else if (!has_supported_joystick() && !tuh_task_event_ready()) {
        if (hcd_port_connect_status(BOARD_TUH_RHPORT)) {
            restart_discovery();
            if (discovery_reset_active) return;
        } else {
            debug_cdc_log("USB discovery: waiting for receiver (no USB connection)\r\n");
        }
    }
    discovery_deadline = make_timeout_time_ms(RETROLINK_DISCOVERY_RETRY_MS);
}

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
    slot->next_retry = make_timeout_time_ms(RETROLINK_HID_RETRY_MS);
    if (!tuh_hid_receive_ready(slot->dev_addr, slot->instance)) {
        return;
    }

    if (!tuh_hid_receive_report(slot->dev_addr, slot->instance)) {
        slot->state = 0;
        for (uint8_t index = 0; index < slot->joystick.report_count; ++index) {
            slot->joystick.reports[index].state = 0;
        }
        update_msx_state();
        if (!slot->receive_failed) {
            debug_cdc_log("HID receive request failed; outputs released; retrying every %u ms: addr=%u instance=%u\r\n",
                          RETROLINK_HID_RETRY_MS, slot->dev_addr, slot->instance);
        }
        slot->receive_failed = true;
    } else {
        if (slot->receive_failed) {
            debug_cdc_log("HID reception resumed: addr=%u instance=%u\r\n", slot->dev_addr, slot->instance);
        }
        slot->receive_failed = false;
    }
}

void usb_host_init(void)
{
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = RETROLINK_USB_HOST_DP_GPIO;
    pio_cfg.pinout = PIO_USB_PINOUT_DPDM;

    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(BOARD_TUH_RHPORT);
    discovery_deadline = make_timeout_time_ms(RETROLINK_DISCOVERY_RETRY_MS);

    debug_cdc_log("USB host initialized on root port %u, D+ GPIO%u, D- GPIO%u\r\n",
                  BOARD_TUH_RHPORT,
                  RETROLINK_USB_HOST_DP_GPIO,
                  RETROLINK_USB_HOST_DM_GPIO);
}

void usb_host_task(void)
{
    discovery_task();
    if (discovery_reset_active) return;

    for (size_t index = 0; index < RETROLINK_MAX_HID_SLOTS; ++index) {
        hid_slot_t *slot = &hid_slots[index];
        if (slot->in_use && time_reached(slot->next_retry)) {
            request_report(slot);
        }
    }
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
    slot->receive_failed = false;
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