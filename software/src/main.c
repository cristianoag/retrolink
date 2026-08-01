#include "pico/stdlib.h"
#include "tusb.h"

#include "retrolink/status_led.h"
#include "retrolink/usb_host.h"

int main(void)
{
    stdio_init_all();

    status_led_init();
    usb_host_init();

    while (true) {
        tuh_task();
        usb_host_task();
        status_led_task();
        tight_loop_contents();
    }
}