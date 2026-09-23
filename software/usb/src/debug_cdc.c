#include "retrolink/debug_cdc.h"

#include <stdarg.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "tusb.h"

#include "retrolink/board_config.h"

static bool was_connected = false;
static absolute_time_t next_heartbeat;

void debug_cdc_init(void)
{
    tud_init(0);
    next_heartbeat = make_timeout_time_ms(RETROLINK_DEBUG_CDC_HEARTBEAT_MS);
}

void debug_cdc_task(void)
{
    bool connected = tud_cdc_connected();

    if (connected && !was_connected) {
        debug_cdc_log("RetroLink CDC connected\r\n");
        debug_cdc_log("Waiting for USB HID joystick/gamepad reports on USB-A host port\r\n");
    }

    was_connected = connected;

    if (connected && absolute_time_diff_us(get_absolute_time(), next_heartbeat) <= 0) {
        debug_cdc_log("RetroLink alive: LED GPIO%u, USB host D+ GPIO%u, D- GPIO%u\r\n",
                      RETROLINK_STATUS_LED_GPIO,
                      RETROLINK_USB_HOST_DP_GPIO,
                      RETROLINK_USB_HOST_DM_GPIO);
        next_heartbeat = make_timeout_time_ms(RETROLINK_DEBUG_CDC_HEARTBEAT_MS);
    }

    tud_cdc_write_flush();
}

void debug_cdc_log(char const *format, ...)
{
    if (!tud_cdc_connected()) {
        return;
    }

    char buffer[192];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (count <= 0) {
        return;
    }

    if ((size_t)count > sizeof(buffer)) {
        count = (int)sizeof(buffer);
    }

    tud_cdc_write(buffer, (uint32_t)count);
    tud_cdc_write_flush();
}

bool debug_cdc_is_connected(void)
{
    return tud_cdc_connected();
}