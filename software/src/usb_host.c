#include "retrolink/usb_host.h"

#include <string.h>

#include "pico/stdlib.h"
#include "tusb.h"

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
    tusb_init();
}

void usb_host_task(void)
{
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
    (void)desc_report;
    (void)desc_len;

    claim_slot(dev_addr, instance);
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    release_slot(dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t report_len)
{
    hid_slot_t *slot = claim_slot(dev_addr, instance);
    if (slot != NULL) {
        if (report_has_new_pressed_bit(slot, report, report_len)) {
            status_led_pulse();
        }

        store_report(slot, report, report_len);
    }

    tuh_hid_receive_report(dev_addr, instance);
}