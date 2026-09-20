#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"
#include "retrolink/msx_port.h"
#include "retrolink/status_led.h"
#include "retrolink/usb_host.h"

int main(void)
{
    set_sys_clock_khz(RETROLINK_SYS_CLOCK_KHZ, true);

    status_led_init();
    debug_cdc_init();
    msx_port_init();
    usb_host_init();

    debug_cdc_log("RetroLink firmware %s, hardware %s\r\n", RETROLINK_FW_VERSION, RETROLINK_HW_REVISION);
    debug_cdc_log("USB-C CDC debug active; USB-A host on GPIO%u/GPIO%u\r\n", RETROLINK_USB_HOST_DP_GPIO, RETROLINK_USB_HOST_DM_GPIO);

    while (true) {
        tud_task();
        tuh_task();
        debug_cdc_task();
        usb_host_task();
        status_led_task();
        tight_loop_contents();
    }
}