#ifndef RETROLINK_BOARD_CONFIG_H
#define RETROLINK_BOARD_CONFIG_H

#include <stdint.h>

#define RETROLINK_SPEC_VERSION "1.10"

#ifndef RETROLINK_FW_VERSION
#define RETROLINK_FW_VERSION "1.00"
#endif

#ifndef RETROLINK_HW_REVISION
#define RETROLINK_HW_REVISION "1.10"
#endif

#define RETROLINK_USB_HOST_DP_GPIO 27u
#define RETROLINK_USB_HOST_DM_GPIO 28u

#define RETROLINK_MSX_UP_GPIO 0u
#define RETROLINK_MSX_DOWN_GPIO 2u
#define RETROLINK_MSX_LEFT_GPIO 4u
#define RETROLINK_MSX_RIGHT_GPIO 6u
#define RETROLINK_MSX_TRIGGER_A_GPIO 1u
#define RETROLINK_MSX_TRIGGER_B_GPIO 3u
#define RETROLINK_MSX_OUT_STROBE_GPIO 5u

#define RETROLINK_STATUS_LED_GPIO 16u
#define RETROLINK_STATUS_LED_PULSE_MS 80u

#define RETROLINK_SYS_CLOCK_KHZ 120000u

#define RETROLINK_DEBUG_CDC_HEARTBEAT_MS 2000u
#define RETROLINK_DEBUG_CDC_INPUT_NAME "USB HID mouse"

/* A strobe edge arriving later than this after the previous edge restarts the X/Y nibble sequence. */
#ifndef RETROLINK_MOUSE_STROBE_TIMEOUT_US
#define RETROLINK_MOUSE_STROBE_TIMEOUT_US 1500u
#endif

/* USB mouse counts per MSX mouse count; raise it to slow the pointer down. */
#ifndef RETROLINK_MOUSE_DIVISOR
#define RETROLINK_MOUSE_DIVISOR 2
#endif

/* Joystick emulation: holding the left button in the first report after mounting selects it. */
#define RETROLINK_MOUSE_JOYSTICK_SELECT_MS 1000u
#define RETROLINK_MOUSE_JOYSTICK_SAMPLE_MS 20u
#define RETROLINK_MOUSE_JOYSTICK_THRESHOLD 2

#endif
