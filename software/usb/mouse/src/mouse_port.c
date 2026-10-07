#include "retrolink/mouse_port.h"

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/time.h"

#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"
#include "retrolink/msx_mouse.h"

#define OUTPUT_COUNT 6u

/* Ordered by joystick_state_t bit: up, down, left, right, trigger A, trigger B. */
static const uint output_gpios[OUTPUT_COUNT] = {
    RETROLINK_MSX_UP_GPIO,
    RETROLINK_MSX_DOWN_GPIO,
    RETROLINK_MSX_LEFT_GPIO,
    RETROLINK_MSX_RIGHT_GPIO,
    RETROLINK_MSX_TRIGGER_A_GPIO,
    RETROLINK_MSX_TRIGGER_B_GPIO,
};

static uint32_t output_masks[1u << OUTPUT_COUNT];

/* Written only by core 0 and read by core 1; movement is a wrapping running total. */
static volatile uint32_t shared_motion_x;
static volatile uint32_t shared_motion_y;
/* Motion totals captured by core 0 when it publishes a new generation. */
static volatile uint32_t shared_baseline_x;
static volatile uint32_t shared_baseline_y;
static volatile uint32_t shared_generation;
static volatile uint8_t shared_mode;
static volatile uint8_t shared_buttons;
static volatile uint8_t shared_directions;

static void __not_in_flash_func(core1_main)(void)
{
    msx_mouse_t mouse;
    uint32_t generation = shared_generation - 1u;
    uint32_t consumed_x = 0;
    uint32_t consumed_y = 0;
    uint32_t applied = 0;

    while (true) {
        uint32_t now = time_us_32();
        bool strobe = (sio_hw->gpio_in & (1u << RETROLINK_MSX_OUT_STROBE_GPIO)) != 0;
        if (shared_generation != generation) {
            generation = shared_generation;
            __dmb();
            msx_mouse_reset(&mouse, strobe, now, RETROLINK_MOUSE_STROBE_TIMEOUT_US);
            consumed_x = shared_baseline_x;
            consumed_y = shared_baseline_y;
        }

        joystick_state_t lines = 0;
        uint8_t mode = shared_mode;
        if (mode == MOUSE_PORT_MODE_MOUSE) {
            uint32_t total_x = shared_motion_x;
            uint32_t total_y = shared_motion_y;
            if (total_x != consumed_x || total_y != consumed_y) {
                msx_mouse_add_motion(&mouse, (int32_t)(total_x - consumed_x), (int32_t)(total_y - consumed_y));
                consumed_x = total_x;
                consumed_y = total_y;
            }
            msx_mouse_strobe(&mouse, strobe, now);
            lines = (joystick_state_t)(msx_mouse_lines(&mouse) | shared_buttons);
        } else if (mode == MOUSE_PORT_MODE_JOYSTICK) {
            lines = (joystick_state_t)(shared_directions | shared_buttons);
        }

        uint32_t mask = output_masks[lines & ((1u << OUTPUT_COUNT) - 1u)];
        if (mask != applied) {
            sio_hw->gpio_oe_set = mask & ~applied;
            sio_hw->gpio_oe_clr = applied & ~mask;
            applied = mask;
        }
    }
}

void mouse_port_init(void)
{
    for (uint index = 0; index < OUTPUT_COUNT; ++index) {
        gpio_init(output_gpios[index]);
        gpio_disable_pulls(output_gpios[index]);
        gpio_put(output_gpios[index], 0);
        gpio_set_dir(output_gpios[index], GPIO_IN);
    }
    gpio_init(RETROLINK_MSX_OUT_STROBE_GPIO);
    gpio_disable_pulls(RETROLINK_MSX_OUT_STROBE_GPIO);
    gpio_set_dir(RETROLINK_MSX_OUT_STROBE_GPIO, GPIO_IN);

    for (uint state = 0; state < (1u << OUTPUT_COUNT); ++state) {
        uint32_t mask = 0;
        for (uint index = 0; index < OUTPUT_COUNT; ++index) {
            if ((state & (1u << index)) != 0) mask |= 1u << output_gpios[index];
        }
        output_masks[state] = mask;
    }

    shared_mode = MOUSE_PORT_MODE_RELEASED;
    multicore_launch_core1(core1_main);
    debug_cdc_log("MSX mouse port ready: data GPIO%u,%u,%u,%u buttons GPIO%u,%u strobe GPIO%u\r\n",
                  RETROLINK_MSX_UP_GPIO, RETROLINK_MSX_DOWN_GPIO, RETROLINK_MSX_LEFT_GPIO,
                  RETROLINK_MSX_RIGHT_GPIO, RETROLINK_MSX_TRIGGER_A_GPIO, RETROLINK_MSX_TRIGGER_B_GPIO,
                  RETROLINK_MSX_OUT_STROBE_GPIO);
}

void mouse_port_set_mode(mouse_port_mode_t mode)
{
    if (shared_mode == mode) return;
    shared_mode = (uint8_t)mode;
    shared_directions = 0;
    shared_baseline_x = shared_motion_x;
    shared_baseline_y = shared_motion_y;
    __dmb();
    shared_generation = shared_generation + 1u;
}

mouse_port_mode_t mouse_port_mode(void)
{
    return (mouse_port_mode_t)shared_mode;
}

void mouse_port_set_buttons(joystick_state_t buttons)
{
    shared_buttons = (uint8_t)(buttons & (JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B));
}

void mouse_port_add_motion(int32_t msx_dx, int32_t msx_dy)
{
    shared_motion_x = shared_motion_x + (uint32_t)msx_dx;
    shared_motion_y = shared_motion_y + (uint32_t)msx_dy;
}

void mouse_port_set_directions(joystick_state_t directions)
{
    shared_directions = (uint8_t)(directions & (JOYSTICK_UP | JOYSTICK_DOWN | JOYSTICK_LEFT | JOYSTICK_RIGHT));
}
