#include <assert.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "retrolink/board_config.h"
#include "retrolink/debug_cdc.h"
#include "retrolink/msx_port.h"

static bool initialized[30];
static bool outputs[30];
static bool pulls_disabled[30];
static unsigned direction_changes[30];
static const uint expected_gpios[] = {0, 2, 4, 6, 1, 3};

void gpio_init(uint gpio)
{
    assert(gpio <= 6 && gpio != 5);
    initialized[gpio] = true;
}

void gpio_set_dir(uint gpio, bool output)
{
    assert(initialized[gpio]);
    outputs[gpio] = output;
    ++direction_changes[gpio];
}

void gpio_disable_pulls(uint gpio)
{
    pulls_disabled[gpio] = true;
}

void gpio_put(uint gpio, bool value)
{
    assert(initialized[gpio]);
    assert(!value);
}

void debug_cdc_log(const char *format, ...)
{
    (void)format;
}

static void expect_outputs(joystick_state_t state)
{
    for (unsigned index = 0; index < 6; ++index) {
        uint gpio = expected_gpios[index];
        assert(initialized[gpio]);
        assert(outputs[gpio] == ((state & (1u << index)) != 0));
        assert(pulls_disabled[gpio]);
    }
    for (unsigned gpio = 0; gpio < 30; ++gpio) {
        if (gpio == 5 || gpio > 6) {
            assert(!initialized[gpio]);
            assert(!outputs[gpio]);
            assert(!pulls_disabled[gpio]);
            assert(direction_changes[gpio] == 0);
        }
    }
}

int main(void)
{
    assert(RETROLINK_MSX_OUT_STROBE_GPIO == 5u);
    msx_port_init();
    expect_outputs(0);
    const joystick_state_t controls[] = {
        JOYSTICK_UP, JOYSTICK_DOWN, JOYSTICK_LEFT, JOYSTICK_RIGHT, JOYSTICK_BUTTON_A, JOYSTICK_BUTTON_B,
    };
    for (unsigned index = 0; index < 6; ++index) {
        msx_port_set_state(controls[index]);
        expect_outputs(controls[index]);
        msx_port_set_state(0);
        expect_outputs(0);
    }
    msx_port_set_state(JOYSTICK_BUTTON_A);
    unsigned previous_changes = direction_changes[1];
    msx_port_set_state(JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B | JOYSTICK_UP | JOYSTICK_LEFT);
    expect_outputs(JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B | JOYSTICK_UP | JOYSTICK_LEFT);
    assert(direction_changes[1] == previous_changes);
    msx_port_set_state(JOYSTICK_UP | JOYSTICK_DOWN | JOYSTICK_LEFT | JOYSTICK_RIGHT | JOYSTICK_BUTTON_B);
    expect_outputs(JOYSTICK_BUTTON_B);
    msx_port_set_trigger_a(true);
    assert(msx_port_trigger_a_is_pressed());
    expect_outputs(JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B);
    msx_port_set_trigger_a(false);
    expect_outputs(JOYSTICK_BUTTON_B);
    msx_port_set_state(0);
    expect_outputs(0);
    puts("MSX GPIO tests passed");
    return 0;
}