#include "retrolink/usb_host.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"

#include "retrolink/debug_cdc.h"
#include "retrolink/status_led.h"

#define RETROLINK_MAX_HID_SLOTS 8u
#define RETROLINK_MAX_REPORT_BYTES 64u
#define RETROLINK_BUTTON_SCAN_BYTES 8u

typedef struct {
    bool in_use;
    uint8_t dev_addr;
    uint8_t instance;
    uint8_t last_report[RETROLINK_MAX_REPORT_BYTES];
    uint16_t last_report_len;
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
            hid_slots[index].last_report_len = 0;
            memset(hid_slots[index].last_report, 0, sizeof(hid_slots[index].last_report));
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

static bool report_has_new_pressed_bit(hid_slot_t const *slot, uint8_t const *report, uint16_t report_len)
{
    uint16_t scan_len = report_len < RETROLINK_BUTTON_SCAN_BYTES ? report_len : RETROLINK_BUTTON_SCAN_BYTES;

    for (uint16_t index = 0; index < scan_len; ++index) {
        uint8_t previous = index < slot->last_report_len ? slot->last_report[index] : 0u;
        if ((uint8_t)(report[index] & (uint8_t)~previous) != 0u) {
            return true;
        }
    }

    return false;
}

static void store_report(hid_slot_t *slot, uint8_t const *report, uint16_t report_len)
{
    uint16_t copy_len = report_len < RETROLINK_MAX_REPORT_BYTES ? report_len : RETROLINK_MAX_REPORT_BYTES;

    memset(slot->last_report, 0, sizeof(slot->last_report));
    memcpy(slot->last_report, report, copy_len);
    slot->last_report_len = copy_len;
}

void usb_host_init(void)
{
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = RETROLINK_USB_HOST_DP_GPIO;

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
    (void)desc_report;
    (void)desc_len;

    uint16_t vid = 0;
    uint16_t pid = 0;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    debug_cdc_log("HID mounted: addr=%u instance=%u vid=%04x pid=%04x protocol=%u\r\n",
                  dev_addr,
                  instance,
                  vid,
                  pid,
                  tuh_hid_interface_protocol(dev_addr, instance));

    claim_slot(dev_addr, instance);
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        debug_cdc_log("HID receive request failed: addr=%u instance=%u\r\n", dev_addr, instance);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    debug_cdc_log("HID unmounted: addr=%u instance=%u\r\n", dev_addr, instance);
    release_slot(dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t report_len)
{
    hid_slot_t *slot = claim_slot(dev_addr, instance);
    if (slot != NULL) {
        if (report_has_new_pressed_bit(slot, report, report_len)) {
            debug_cdc_log("HID press detected: addr=%u instance=%u len=%u first=%02x %02x %02x %02x\r\n",
                          dev_addr,
                          instance,
                          report_len,
                          report_len > 0 ? report[0] : 0,
                          report_len > 1 ? report[1] : 0,
                          report_len > 2 ? report[2] : 0,
                          report_len > 3 ? report[3] : 0);
            status_led_pulse();
        }

        store_report(slot, report, report_len);
    }

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        debug_cdc_log("HID receive re-request failed: addr=%u instance=%u\r\n", dev_addr, instance);
    }
}