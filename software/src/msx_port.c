#include "retrolink/msx_port.h"

#include "hardware/gpio.h"

#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"

static joystick_state_t current_state;
static const uint output_gpios[] = {
    RETROLINK_MSX_UP_GPIO,
    RETROLINK_MSX_DOWN_GPIO,
    RETROLINK_MSX_LEFT_GPIO,
    RETROLINK_MSX_RIGHT_GPIO,
    RETROLINK_MSX_TRIGGER_A_GPIO,
    RETROLINK_MSX_TRIGGER_B_GPIO,
};

static void release_open_drain_gpio(uint gpio)
{
    gpio_set_dir(gpio, GPIO_IN);
    gpio_disable_pulls(gpio);
}

static void assert_open_drain_gpio(uint gpio)
{
    gpio_put(gpio, 0);
    gpio_set_dir(gpio, GPIO_OUT);
}

void msx_port_init(void)
{
    for (uint index = 0; index < sizeof(output_gpios) / sizeof(output_gpios[0]); ++index) {
        gpio_init(output_gpios[index]);
        release_open_drain_gpio(output_gpios[index]);
    }
    current_state = 0;
    debug_cdc_log("MSX joystick ready: GPIO6-11 / DB9 pins 1,2,3,4,6,7\r\n");
}

void msx_port_set_state(joystick_state_t state)
{
    if ((state & (JOYSTICK_UP | JOYSTICK_DOWN)) == (JOYSTICK_UP | JOYSTICK_DOWN)) {
        state &= (joystick_state_t)~(JOYSTICK_UP | JOYSTICK_DOWN);
    }
    if ((state & (JOYSTICK_LEFT | JOYSTICK_RIGHT)) == (JOYSTICK_LEFT | JOYSTICK_RIGHT)) {
        state &= (joystick_state_t)~(JOYSTICK_LEFT | JOYSTICK_RIGHT);
    }
    if (state == current_state) {
        return;
    }

    joystick_state_t changed = state ^ current_state;
    for (uint index = 0; index < sizeof(output_gpios) / sizeof(output_gpios[0]); ++index) {
        if ((changed & (1u << index)) != 0 && (state & (1u << index)) == 0) {
            release_open_drain_gpio(output_gpios[index]);
        }
    }
    for (uint index = 0; index < sizeof(output_gpios) / sizeof(output_gpios[0]); ++index) {
        if ((changed & state & (1u << index)) != 0) {
            assert_open_drain_gpio(output_gpios[index]);
        }
    }
    current_state = state;
    debug_cdc_log("MSX joystick state=%02x\r\n", state);
}

void msx_port_set_trigger_a(bool pressed)
{
    msx_port_set_state(pressed ? current_state | JOYSTICK_BUTTON_A : current_state & (joystick_state_t)~JOYSTICK_BUTTON_A);
}

bool msx_port_trigger_a_is_pressed(void)
{
    return (current_state & JOYSTICK_BUTTON_A) != 0;
}