#include "retrolink/msx_output.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"

#include "retrolink/board_config.h"

#define MSX_OUTPUT_MASK ((1u << RETROLINK_MSX_UP_GPIO) | \
                        (1u << RETROLINK_MSX_DOWN_GPIO) | \
                        (1u << RETROLINK_MSX_LEFT_GPIO) | \
                        (1u << RETROLINK_MSX_RIGHT_GPIO) | \
                        (1u << RETROLINK_MSX_TRIGGER_A_GPIO) | \
                        (1u << RETROLINK_MSX_TRIGGER_B_GPIO))

static const uint output_gpios[] = {
    RETROLINK_MSX_UP_GPIO, RETROLINK_MSX_DOWN_GPIO,
    RETROLINK_MSX_LEFT_GPIO, RETROLINK_MSX_RIGHT_GPIO,
    RETROLINK_MSX_TRIGGER_A_GPIO, RETROLINK_MSX_TRIGGER_B_GPIO,
};
static volatile uint32_t desired_outputs;

static void apply_outputs(void)
{
    uint32_t active = gpio_get(RETROLINK_MSX_OUT_STROBE_GPIO) ? 0 : desired_outputs;
    /* The output latches stay low; only output-enable changes, never drive high. */
    gpio_set_dir_masked(MSX_OUTPUT_MASK, active);
}

static void strobe_changed(uint gpio, uint32_t events)
{
    (void)events;
    if (gpio == RETROLINK_MSX_OUT_STROBE_GPIO) {
        apply_outputs();
    }
}

void msx_output_init(void)
{
    desired_outputs = 0;
    for (uint index = 0; index < 6; ++index) {
        gpio_init(output_gpios[index]);
        gpio_set_dir(output_gpios[index], GPIO_IN);
        gpio_disable_pulls(output_gpios[index]);
        gpio_put(output_gpios[index], false);
    }
    gpio_init(RETROLINK_MSX_OUT_STROBE_GPIO);
    gpio_set_dir(RETROLINK_MSX_OUT_STROBE_GPIO, GPIO_IN);
    gpio_pull_up(RETROLINK_MSX_OUT_STROBE_GPIO);
    gpio_set_irq_enabled_with_callback(RETROLINK_MSX_OUT_STROBE_GPIO,
                                      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                      true, strobe_changed);
}

void msx_output_set_state(joystick_state_t state)
{
    uint32_t pins = 0;
    for (uint index = 0; index < 6; ++index) {
        if (state & (1u << index)) {
            pins |= 1u << output_gpios[index];
        }
    }
    uint32_t irq_state = save_and_disable_interrupts();
    desired_outputs = pins;
    apply_outputs();
    restore_interrupts(irq_state);
}
