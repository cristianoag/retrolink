#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "host/hcd.h"
#include "pico/stdlib.h"
#include "pio_usb.h"

#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"
#include "retrolink/msx_port.h"
#include "retrolink/status_led.h"
#include "retrolink/usb_host.h"

typedef enum {
    RECEIVER_FAILS,
    RECEIVER_HANGS_AT_ZERO,
    RECEIVER_HANGS_AT_REPORT_DESCRIPTOR,
    RECEIVER_READY,
} receiver_mode_t;

static receiver_mode_t mode;
static uint64_t now_ms;
static bool reset_active;
static bool control_pending;
static uint8_t control_address;
static tusb_control_request_t control_request;
static unsigned host_initializations;
static unsigned recovery_resets;
static unsigned interrupt_requests;
static unsigned aborted_transfers;
static joystick_state_t msx_state;
static uint8_t *input_report;

static const uint8_t report_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x02,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02,
    0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0xc0,
};

static const uint8_t device_descriptor[] = {
    18, TUSB_DESC_DEVICE, 0x00, 0x02, 0, 0, 0, 8,
    0x34, 0x12, 0x78, 0x56, 0, 1, 0, 0, 0, 1,
};

static const uint8_t configuration_descriptor[] = {
    9, TUSB_DESC_CONFIGURATION, 34, 0, 1, 1, 0, 0x80, 50,
    9, TUSB_DESC_INTERFACE, 0, 0, 1, TUSB_CLASS_HID, 0, 0, 0,
    9, HID_DESC_TYPE_HID, 0x11, 0x01, 0, 1, HID_DESC_TYPE_REPORT, sizeof(report_descriptor), 0,
    7, TUSB_DESC_ENDPOINT, 0x81, TUSB_XFER_INTERRUPT, 8, 0, 10,
};

absolute_time_t make_timeout_time_ms(uint32_t ms)
{
    return now_ms + ms;
}

bool time_reached(absolute_time_t deadline)
{
    return now_ms >= deadline;
}

void osal_task_delay(uint32_t ms)
{
    now_ms += ms;
}

bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param)
{
    assert(rhport == BOARD_TUH_RHPORT);
    assert(cfg_id == TUH_CFGID_RPI_PIO_USB_CONFIGURATION);
    const pio_usb_configuration_t *cfg = cfg_param;
    assert(cfg->pin_dp == RETROLINK_USB_HOST_DP_GPIO);
    return true;
}

bool hcd_init(uint8_t rhport)
{
    assert(rhport == BOARD_TUH_RHPORT);
    ++host_initializations;
    return true;
}

void hcd_int_enable(uint8_t rhport) { (void)rhport; }
void hcd_int_disable(uint8_t rhport) { (void)rhport; }
void hcd_int_handler(uint8_t rhport, bool in_isr) { (void)rhport; (void)in_isr; }

uint32_t hcd_frame_number(uint8_t rhport)
{
    (void)rhport;
    return (uint32_t)now_ms;
}

bool hcd_port_connect_status(uint8_t rhport)
{
    (void)rhport;
    return !reset_active;
}

void hcd_port_reset(uint8_t rhport)
{
    (void)rhport;
    assert(!reset_active);
    reset_active = true;
}

void hcd_port_reset_end(uint8_t rhport)
{
    (void)rhport;
    assert(reset_active);
    reset_active = false;
}

tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
    (void)rhport;
    return TUSB_SPEED_FULL;
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
    (void)rhport;
    if (control_address == dev_addr) control_pending = false;
}

bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, const tusb_desc_endpoint_t *ep)
{
    (void)rhport;
    (void)dev_addr;
    (void)ep;
    return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
    (void)rhport;
    if (ep_addr != 0 || !control_pending || control_address != dev_addr) return false;
    control_pending = false;
    ++aborted_transfers;
    return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
    (void)rhport;
    (void)dev_addr;
    (void)ep_addr;
    return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, const uint8_t setup[8])
{
    (void)rhport;
    assert(!reset_active);
    memcpy(&control_request, setup, sizeof(control_request));
    control_address = dev_addr;
    control_pending = true;
    if ((mode == RECEIVER_HANGS_AT_ZERO && dev_addr == 0) ||
        (mode == RECEIVER_HANGS_AT_REPORT_DESCRIPTOR &&
         control_request.bRequest == TUSB_REQ_GET_DESCRIPTOR &&
         (control_request.wValue >> 8) == HID_DESC_TYPE_REPORT)) {
        return true;
    }
    control_pending = false;
    hcd_event_xfer_complete(dev_addr, 0, 8,
                            mode == RECEIVER_FAILS ? XFER_RESULT_FAILED : XFER_RESULT_SUCCESS, false);
    return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t *buffer, uint16_t length)
{
    (void)rhport;
    assert(!reset_active);
    if (ep_addr == 0x81) {
        input_report = buffer;
        ++interrupt_requests;
        return true;
    }
    if (length != 0) {
        const uint8_t *source;
        size_t source_length;
        switch (control_request.wValue >> 8) {
        case TUSB_DESC_DEVICE:
            source = device_descriptor;
            source_length = sizeof(device_descriptor);
            break;
        case TUSB_DESC_CONFIGURATION:
            source = configuration_descriptor;
            source_length = sizeof(configuration_descriptor);
            break;
        case HID_DESC_TYPE_REPORT:
            source = report_descriptor;
            source_length = sizeof(report_descriptor);
            break;
        default:
            assert(false);
            return false;
        }
        if (length > source_length) length = (uint16_t)source_length;
        memcpy(buffer, source, length);
    }
    hcd_event_xfer_complete(dev_addr, ep_addr, length, XFER_RESULT_SUCCESS, false);
    return true;
}

void debug_cdc_log(const char *format, ...)
{
    if (strstr(format, "restarting USB-A enumeration") != NULL) ++recovery_resets;
}

void msx_port_set_state(joystick_state_t state) { msx_state = state; }
void status_led_pulse(void) {}

static void step(uint32_t ms)
{
    now_ms += ms;
    tuh_task();
    usb_host_task();
}

static void test_enumeration_recovery(receiver_mode_t failure_mode)
{
    hcd_event_device_remove(BOARD_TUH_RHPORT, false);
    tuh_task();
    assert(msx_state == 0);
    mode = failure_mode;
    unsigned previous_requests = interrupt_requests;
    unsigned previous_resets = recovery_resets;
    hcd_event_device_attach(BOARD_TUH_RHPORT, false);
    tuh_task();
    assert(interrupt_requests == previous_requests);

    step(5000);
    assert(reset_active);
    assert(recovery_resets == previous_resets + 1);
    step(49);
    assert(reset_active);
    step(1);
    assert(!reset_active);
    step(0);
    assert(interrupt_requests == previous_requests);

    mode = RECEIVER_READY;
    step(5000);
    assert(reset_active);
    step(50);
    step(0);
    assert(tuh_mounted(1));
    assert(interrupt_requests == previous_requests + 1);
    assert(!control_pending);

    unsigned stable_resets = recovery_resets;
    step(30000);
    assert(recovery_resets == stable_resets);
    assert(interrupt_requests == previous_requests + 1);
    assert(msx_state == 0);

    input_report[0] = 1;
    hcd_event_xfer_complete(1, 0x81, 1, XFER_RESULT_SUCCESS, false);
    tuh_task();
    assert(msx_state == JOYSTICK_BUTTON_A);
    assert(interrupt_requests == previous_requests + 2);
    input_report[0] = 0;
    hcd_event_xfer_complete(1, 0x81, 1, XFER_RESULT_SUCCESS, false);
    tuh_task();
    assert(msx_state == 0);
    assert(interrupt_requests == previous_requests + 3);
}

int main(void)
{
    usb_host_init();
    test_enumeration_recovery(RECEIVER_FAILS);
    test_enumeration_recovery(RECEIVER_HANGS_AT_ZERO);
    assert(aborted_transfers >= 2);
    test_enumeration_recovery(RECEIVER_HANGS_AT_REPORT_DESCRIPTOR);
    assert(host_initializations == 1);
    puts("USB discovery tests passed against real TinyUSB host/HID drivers");
    return 0;
}
