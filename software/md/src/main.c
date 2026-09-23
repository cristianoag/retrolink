#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"

#include "retrolink/board_config.h"
#include "retrolink/md_port.h"
#include "retrolink/msx_output.h"
#include "retrolink/status_led.h"

int main(void)
{
    msx_output_init();
    md_port_init();
    status_led_init();
    stdio_init_all();

    md_pad_state_t previous = {MD_PAD_NONE, 0};
    md_poll_result_t previous_result = MD_POLL_NO_PAD;
    joystick_state_t previous_controls = 0;
    bool was_connected = false;
    uint32_t next_poll = time_us_32();
    uint32_t next_heartbeat = next_poll;

    while (true) {
        if ((int32_t)(time_us_32() - next_poll) >= 0) {
            md_pad_state_t state;
            md_poll_result_t result = md_port_poll(&state);
            joystick_state_t controls = md_pad_to_msx(state);
            msx_output_set_state(controls);
            if ((controls & (joystick_state_t)~previous_controls) != 0) {
                status_led_pulse();
            }
            bool connected = stdio_usb_connected();
            if (connected && !was_connected) {
                printf("RetroLink MD firmware %s, hardware %s\r\n",
                       RETROLINK_FW_VERSION, RETROLINK_HW_REVISION);
                printf("MD D0..D5 GPIO28,26,14,12,27,13; TH GPIO15; B->MSX A, C->MSX B\r\n");
            }
            if (connected && (!was_connected || result != previous_result ||
                              state.type != previous.type || state.buttons != previous.buttons ||
                              (int32_t)(time_us_32() - next_heartbeat) >= 0)) {
                printf("MD result=%u (0=ok,1=absent/invalid,2=timing) pad=%u (0=none,1=3btn,2=6btn) buttons=%03x MSX=%02x\r\n",
                       (unsigned)result, (unsigned)state.type, (unsigned)state.buttons, (unsigned)controls);
                next_heartbeat = time_us_32() + 2000000u;
            }
            was_connected = connected;
            previous = state;
            previous_result = result;
            previous_controls = controls;
            /* Schedule from completion so delayed diagnostics cannot shorten TH reset time. */
            next_poll = time_us_32() + RETROLINK_MD_POLL_US;
        }
        status_led_task();
        tight_loop_contents();
    }
}
