#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pio_usb.h"
#include "tusb.h"
#include "host/hcd.h"

#include "retrolink/debug_cdc.h"
#include "retrolink/msx_port.h"
#include "retrolink/status_led.h"
#include "retrolink/usb_host.h"

typedef struct {
    bool mounted;
    bool busy;
    bool fail_receive;
    unsigned requests;
} test_interface_t;

static test_interface_t interfaces[5][4];
static uint64_t now_ms;
static joystick_state_t msx_state;
static unsigned failure_logs;
static unsigned recovery_logs;
static unsigned host_initializations;
static unsigned led_pulses;
static bool receiver_connected;
static bool bus_reset_active;
static bool address_zero_pending;
static bool fail_reset_start;
static bool fail_reset_end;
static uint64_t reset_started_ms;
static unsigned reset_starts;
static unsigned reset_ends;
static unsigned attach_events;
static unsigned removal_events;
static unsigned aborted_completions;
static unsigned closed_address_zero;
static uint8_t enumerating_hub;
static hcd_event_t events[16];
static size_t event_count;

static const uint8_t descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
    0x05, 0x09, 0x15, 0x00, 0x25, 0x01,
    0x85, 0x01, 0x09, 0x01, 0x75, 0x01, 0x95, 0x01, 0x81, 0x02,
    0x75, 0x07, 0x81, 0x03,
    0x85, 0x02, 0x09, 0x02, 0x75, 0x01, 0x81, 0x02,
    0x75, 0x07, 0x81, 0x03, 0xc0,
};

absolute_time_t make_timeout_time_ms(uint32_t ms)
{
    return now_ms + ms;
}

bool time_reached(absolute_time_t deadline)
{
    return now_ms >= deadline;
}

bool tuh_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param)
{
    assert(rhport == BOARD_TUH_RHPORT);
    assert(cfg_id == TUH_CFGID_RPI_PIO_USB_CONFIGURATION);
    const pio_usb_configuration_t *cfg = cfg_param;
    assert(cfg->pin_dp == RETROLINK_USB_HOST_DP_GPIO);
    assert(cfg->pinout == PIO_USB_PINOUT_DPDM);
    return true;
}

bool tuh_init(uint8_t rhport)
{
    assert(rhport == BOARD_TUH_RHPORT);
    ++host_initializations;
    return true;
}

bool tuh_task_event_ready(void)
{
    return event_count != 0;
}

bool tuh_rhport_reset_bus(uint8_t rhport, bool active)
{
    assert(rhport == BOARD_TUH_RHPORT);
    if (active) {
        if (fail_reset_start) return false;
        assert(!bus_reset_active);
        ++reset_starts;
        reset_started_ms = now_ms;
    } else {
        assert(bus_reset_active);
        assert(now_ms - reset_started_ms >= 50);
        if (fail_reset_end) return false;
        ++reset_ends;
    }
    bus_reset_active = active;
    return true;
}

bool hcd_port_connect_status(uint8_t rhport)
{
    assert(rhport == BOARD_TUH_RHPORT);
    assert(!bus_reset_active);
    return receiver_connected;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
    assert(rhport == BOARD_TUH_RHPORT && dev_addr == 0 && ep_addr == 0);
    assert(bus_reset_active);
    bool pending = address_zero_pending;
    address_zero_pending = false;
    return pending;
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
    assert(rhport == BOARD_TUH_RHPORT && dev_addr == 0);
    ++closed_address_zero;
}

void hcd_devtree_get_info(uint8_t dev_addr, hcd_devtree_info_t *info)
{
    assert(dev_addr == 0);
    *info = (hcd_devtree_info_t){.rhport = BOARD_TUH_RHPORT, .hub_addr = enumerating_hub, .hub_port = 1};
}

void hcd_event_handler(const hcd_event_t *event, bool in_isr)
{
    assert(!in_isr);
    assert(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count++] = *event;
    if (event->event_id == HCD_EVENT_DEVICE_REMOVE) ++removal_events;
}

void hcd_event_device_remove(uint8_t rhport, bool in_isr)
{
    hcd_event_t event = {.rhport = rhport, .event_id = HCD_EVENT_DEVICE_REMOVE};
    hcd_event_handler(&event, in_isr);
}

void hcd_event_device_attach(uint8_t rhport, bool in_isr)
{
    assert(!bus_reset_active);
    ++attach_events;
    hcd_event_t event = {.rhport = rhport, .event_id = HCD_EVENT_DEVICE_ATTACH};
    hcd_event_handler(&event, in_isr);
}

void hcd_event_xfer_complete(uint8_t dev_addr, uint8_t ep_addr, uint32_t length, xfer_result_t result, bool in_isr)
{
    assert(dev_addr == 0 && ep_addr == 0 && length == 0 && result == XFER_RESULT_FAILED);
    assert(!address_zero_pending);
    ++aborted_completions;
    hcd_event_t event = {.event_id = HCD_EVENT_XFER_COMPLETE};
    hcd_event_handler(&event, in_isr);
}

bool tuh_vid_pid_get(uint8_t dev_addr, uint16_t *vid, uint16_t *pid)
{
    (void)dev_addr;
    *vid = 0x1234;
    *pid = 0x5678;
    return true;
}

uint8_t tuh_hid_interface_protocol(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr;
    (void)instance;
    return 0;
}

bool tuh_hid_receive_ready(uint8_t dev_addr, uint8_t instance)
{
    test_interface_t *itf = &interfaces[dev_addr][instance];
    assert(itf->mounted);
    return !itf->busy;
}

bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t instance)
{
    test_interface_t *itf = &interfaces[dev_addr][instance];
    assert(itf->mounted);
    assert(!itf->busy);
    ++itf->requests;
    if (itf->fail_receive) return false;
    itf->busy = true;
    return true;
}

void debug_cdc_log(const char *format, ...)
{
    if (strstr(format, "receive request failed") != NULL) ++failure_logs;
    if (strstr(format, "reception resumed") != NULL) ++recovery_logs;
}

void msx_port_set_state(joystick_state_t state)
{
    msx_state = state;
}

void status_led_pulse(void)
{
    ++led_pulses;
}

static void mount_interface(uint8_t dev_addr, uint8_t instance)
{
    interfaces[dev_addr][instance].mounted = true;
    tuh_hid_mount_cb(dev_addr, instance, descriptor, sizeof(descriptor));
}

static void unmount_interface(uint8_t dev_addr, uint8_t instance)
{
    interfaces[dev_addr][instance].mounted = false;
    interfaces[dev_addr][instance].busy = false;
    tuh_hid_umount_cb(dev_addr, instance);
}

static void receive_report(uint8_t dev_addr, uint8_t instance, uint8_t report_id, uint8_t buttons)
{
    test_interface_t *itf = &interfaces[dev_addr][instance];
    assert(itf->busy);
    itf->busy = false;
    const uint8_t report[] = {report_id, buttons};
    tuh_hid_report_received_cb(dev_addr, instance, report, sizeof(report));
}

static void process_host_events(void)
{
    for (size_t index = 0; index < event_count; ++index) {
        if (events[index].event_id != HCD_EVENT_DEVICE_REMOVE) continue;
        if (events[index].connection.hub_addr != 0) continue;
        for (uint8_t dev_addr = 1; dev_addr < 5; ++dev_addr) {
            for (uint8_t instance = 0; instance < 4; ++instance) {
                if (interfaces[dev_addr][instance].mounted) unmount_interface(dev_addr, instance);
            }
        }
    }
    event_count = 0;
}

static void advance_time(uint32_t ms)
{
    now_ms += ms;
    process_host_events();
    usb_host_task();
}

static void reset_test(void)
{
    for (uint8_t dev_addr = 1; dev_addr < 5; ++dev_addr) {
        for (uint8_t instance = 0; instance < 4; ++instance) {
            if (interfaces[dev_addr][instance].mounted) unmount_interface(dev_addr, instance);
        }
    }
    memset(interfaces, 0, sizeof(interfaces));
    receiver_connected = false;
    fail_reset_start = false;
    fail_reset_end = false;
    advance_time(5000);
    process_host_events();
    assert(msx_state == 0);
    failure_logs = 0;
    recovery_logs = 0;
    led_pulses = 0;
    reset_starts = 0;
    reset_ends = 0;
    attach_events = 0;
    removal_events = 0;
    aborted_completions = 0;
    closed_address_zero = 0;
    enumerating_hub = 0;
    address_zero_pending = false;
}

static void test_late_mount_retry(void)
{
    reset_test();
    advance_time(5000);
    test_interface_t *itf = &interfaces[1][0];
    itf->fail_receive = true;
    mount_interface(1, 0);
    assert(itf->requests == 1);
    assert(failure_logs == 1);
    advance_time(99);
    assert(itf->requests == 1);
    advance_time(1);
    assert(itf->requests == 2);
    assert(failure_logs == 1);
    advance_time(100);
    assert(itf->requests == 3);
    assert(failure_logs == 1);
    itf->fail_receive = false;
    advance_time(99);
    assert(itf->requests == 3);
    advance_time(1);
    assert(itf->requests == 4 && itf->busy);
    assert(recovery_logs == 1);
    receive_report(1, 0, 1, 1);
    assert(msx_state == JOYSTICK_BUTTON_A);
    assert(led_pulses == 1);
    assert(itf->requests == 5 && itf->busy);
    advance_time(10000);
    assert(itf->requests == 5);
    assert(msx_state == JOYSTICK_BUTTON_A);
    assert(failure_logs == 1);
    unmount_interface(1, 0);
    assert(msx_state == 0);
    advance_time(1000);
    assert(itf->requests == 5);
}

static void test_rearm_failure_isolation(void)
{
    reset_test();
    mount_interface(1, 0);
    mount_interface(1, 1);
    mount_interface(2, 0);
    receive_report(1, 0, 1, 1);
    receive_report(1, 0, 2, 1);
    receive_report(1, 1, 2, 1);
    receive_report(2, 0, 2, 1);
    assert(msx_state == (JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B));

    test_interface_t *itf = &interfaces[1][0];
    itf->fail_receive = true;
    receive_report(1, 0, 2, 1);
    assert(msx_state == JOYSTICK_BUTTON_B);
    assert(failure_logs == 1);
    advance_time(100);
    assert(msx_state == JOYSTICK_BUTTON_B);
    assert(failure_logs == 1);
    itf->fail_receive = false;
    advance_time(100);
    assert(itf->busy);
    receive_report(1, 0, 2, 0);
    assert(msx_state == JOYSTICK_BUTTON_B);
    unmount_interface(1, 1);
    assert(msx_state == JOYSTICK_BUTTON_B);
    unmount_interface(2, 0);
    assert(msx_state == 0);
}

static void test_busy_and_idle_endpoints(void)
{
    reset_test();
    test_interface_t *itf = &interfaces[1][0];
    itf->busy = true;
    mount_interface(1, 0);
    assert(itf->requests == 0);
    assert(failure_logs == 0);
    advance_time(100);
    assert(itf->requests == 0);
    itf->busy = false;
    advance_time(99);
    assert(itf->requests == 0);
    advance_time(1);
    assert(itf->requests == 1 && itf->busy);

    // An idle endpoint is recovered even if no report callback was delivered.
    itf->busy = false;
    advance_time(100);
    assert(itf->requests == 2 && itf->busy);
    usb_host_task();
    assert(itf->requests == 2);
}

static void test_unmount_cancels_retry_and_address_reuse(void)
{
    reset_test();
    test_interface_t *itf = &interfaces[1][0];
    itf->fail_receive = true;
    mount_interface(1, 0);
    unmount_interface(1, 0);
    advance_time(1000);
    assert(itf->requests == 1);
    itf->fail_receive = false;
    mount_interface(1, 0);
    assert(itf->requests == 2 && itf->busy);
    receive_report(1, 0, 2, 1);
    assert(msx_state == JOYSTICK_BUTTON_B);
    unmount_interface(1, 0);
    assert(msx_state == 0);
    const uint8_t stale_report[] = {1, 1};
    tuh_hid_report_received_cb(1, 0, stale_report, sizeof(stale_report));
    advance_time(1000);
    assert(itf->requests == 3);
    assert(msx_state == 0);
}

static void test_delayed_wireless_enumeration(void)
{
    reset_test();
    receiver_connected = true;
    address_zero_pending = true;
    advance_time(4999);
    assert(reset_starts == 0);
    advance_time(1);
    assert(reset_starts == 1 && bus_reset_active);
    assert(removal_events == 1 && closed_address_zero == 1);
    assert(aborted_completions == 1 && !address_zero_pending);
    assert(events[0].event_id == HCD_EVENT_DEVICE_REMOVE);
    assert(events[1].event_id == HCD_EVENT_XFER_COMPLETE);
    advance_time(49);
    assert(reset_ends == 0 && attach_events == 0);
    advance_time(1);
    assert(reset_ends == 1 && attach_events == 1);

    for (unsigned attempt = 2; attempt <= 4; ++attempt) {
        advance_time(5000);
        assert(reset_starts == attempt);
        advance_time(50);
        assert(attach_events == attempt);
    }
    advance_time(0);
    mount_interface(1, 0);
    advance_time(30000);
    assert(reset_starts == 4);
    assert(interfaces[1][0].requests == 1);
    receive_report(1, 0, 1, 1);
    assert(msx_state == JOYSTICK_BUTTON_A);
    advance_time(30000);
    assert(reset_starts == 4 && msx_state == JOYSTICK_BUTTON_A);
}

static void test_unsupported_receiver_then_gamepad(void)
{
    reset_test();
    receiver_connected = true;
    interfaces[1][0].mounted = true;
    tuh_hid_mount_cb(1, 0, NULL, 0);
    advance_time(5000);
    assert(reset_starts == 1 && aborted_completions == 0);
    advance_time(50);
    assert(!interfaces[1][0].mounted);
    advance_time(0);
    mount_interface(1, 0);
    receive_report(1, 0, 2, 1);
    assert(msx_state == JOYSTICK_BUTTON_B);
}

static void test_no_receiver_and_unplug_during_recovery(void)
{
    reset_test();
    advance_time(30000);
    assert(reset_starts == 0);
    receiver_connected = true;
    advance_time(5000);
    assert(reset_starts == 1);
    receiver_connected = false;
    advance_time(50);
    assert(reset_ends == 1 && attach_events == 0);
    advance_time(10000);
    assert(reset_starts == 1);
}

static void test_discovery_errors_and_pending_events(void)
{
    reset_test();
    receiver_connected = true;
    event_count = 1;
    events[0] = (hcd_event_t){.event_id = HCD_EVENT_DEVICE_ATTACH};
    now_ms += 5000;
    usb_host_task();
    assert(reset_starts == 0);
    process_host_events();
    fail_reset_start = true;
    advance_time(5000);
    assert(reset_starts == 0 && removal_events == 0);
    fail_reset_start = false;
    enumerating_hub = 5;
    advance_time(5000);
    assert(reset_starts == 1 && removal_events == 2);
    assert(events[0].connection.hub_addr == 5);
    assert(events[1].connection.hub_addr == 0);
    fail_reset_end = true;
    advance_time(50);
    assert(bus_reset_active && attach_events == 0);
    fail_reset_end = false;
    advance_time(5000);
    assert(!bus_reset_active && attach_events == 1);
}

int main(void)
{
    usb_host_init();
    test_late_mount_retry();
    test_rearm_failure_isolation();
    test_busy_and_idle_endpoints();
    test_unmount_cancels_retry_and_address_reuse();
    test_delayed_wireless_enumeration();
    test_unsupported_receiver_then_gamepad();
    test_no_receiver_and_unplug_during_recovery();
    test_discovery_errors_and_pending_events();
    reset_test();
    assert(host_initializations == 1);
    puts("USB host tests passed");
    return 0;
}
