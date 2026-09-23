#include <assert.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "retrolink/board_config.h"
#include "retrolink/md_port.h"
#include "retrolink/msx_output.h"

static const uint md_inputs[] = {28, 26, 14, 12, 27, 13};
static const uint msx_outputs[] = {0, 2, 4, 6, 1, 3};
static bool initialized[30];
static bool output[30];
static bool latch[30];
static bool pull_up[30];
static bool pulls_disabled[30];
static bool common_high = true;
static bool interrupts_enabled = true;
static gpio_irq_callback_t gpio_callback;
static uint32_t now;
static uint32_t injected_delay;
static bool raise_common_during_poll;
static unsigned phase;
static unsigned reads;
static uint8_t samples[5];
static bool polling;
static uint32_t frame_start;

static void set_common(bool high)
{
    common_high = high;
    assert(interrupts_enabled);
    assert(gpio_callback != NULL);
    gpio_callback(5, high ? GPIO_IRQ_EDGE_RISE : GPIO_IRQ_EDGE_FALL);
}

void gpio_init(uint gpio)
{
    assert(gpio < 30);
    assert(!initialized[gpio]);
    initialized[gpio] = true;
}

void gpio_set_dir(uint gpio, bool enabled)
{
    assert(gpio < 30 && initialized[gpio]);
    if (gpio == 15) {
        assert(latch[gpio]);
    } else {
        assert(!enabled);
    }
    output[gpio] = enabled;
}

void gpio_set_dir_masked(uint32_t mask, uint32_t values)
{
    assert(mask == 0x5fu);
    assert((values & ~mask) == 0);
    for (uint gpio = 0; gpio < 30; ++gpio) {
        if (mask & (1u << gpio)) {
            assert(initialized[gpio]);
            assert(!latch[gpio]);
            output[gpio] = (values & (1u << gpio)) != 0;
        }
    }
}

void gpio_put(uint gpio, bool value)
{
    assert(gpio < 30 && initialized[gpio]);
    if (gpio != 15) {
        assert(gpio <= 6 && gpio != 5);
        assert(!value);
    } else if (polling) {
        ++phase;
        assert(phase <= 8);
        assert(value == (phase % 2 == 0));
    }
    latch[gpio] = value;
}

void gpio_pull_up(uint gpio)
{
    assert(gpio < 30 && initialized[gpio] && !output[gpio]);
    pull_up[gpio] = true;
}

void gpio_disable_pulls(uint gpio)
{
    assert(gpio < 30 && initialized[gpio]);
    pulls_disabled[gpio] = true;
}

bool gpio_get(uint gpio)
{
    assert(gpio == 5);
    return common_high;
}

uint32_t gpio_get_all(void)
{
    const unsigned expected_phases[] = {0, 1, 5, 6, 7};
    assert(polling && reads < 5);
    assert(phase == expected_phases[reads]);
    assert((uint32_t)(now - frame_start) >= phase * 10u);
    uint32_t pins = 0;
    for (uint index = 0; index < 6; ++index) {
        assert(!output[md_inputs[index]]);
        if (samples[reads] & (1u << index)) pins |= 1u << md_inputs[index];
    }
    ++reads;
    return pins;
}

void gpio_set_irq_enabled_with_callback(uint gpio, uint32_t events, bool enabled,
                                        gpio_irq_callback_t callback)
{
    assert(gpio == 5 && enabled);
    assert(events == (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL));
    gpio_callback = callback;
}

uint32_t save_and_disable_interrupts(void)
{
    uint32_t previous = interrupts_enabled;
    interrupts_enabled = false;
    return previous;
}

void restore_interrupts(uint32_t state)
{
    interrupts_enabled = state != 0;
}

void busy_wait_us_32(uint32_t delay)
{
    assert(interrupts_enabled);
    assert(delay == (polling ? 10u : 3000u));
    now += delay + injected_delay;
    injected_delay = 0;
    if (raise_common_during_poll) {
        raise_common_during_poll = false;
        set_common(true);
        for (uint index = 0; index < 6; ++index) assert(!output[msx_outputs[index]]);
    }
}

uint32_t time_us_32(void)
{
    return now;
}

static md_poll_result_t poll(md_pad_state_t *state, md_pad_samples_t data)
{
    samples[0] = data.idle_high;
    samples[1] = data.first_low;
    samples[2] = data.third_low;
    samples[3] = data.third_high;
    samples[4] = data.fourth_low;
    reads = 0;
    phase = 0;
    frame_start = now;
    polling = true;
    md_poll_result_t result = md_port_poll(state);
    polling = false;
    assert(reads == 5 && phase == 8);
    assert(latch[15]);
    now += 5000u;
    return result;
}

static void expect_outputs(joystick_state_t state)
{
    for (uint index = 0; index < 6; ++index) {
        uint gpio = msx_outputs[index];
        assert(output[gpio] == (!common_high && ((state & (1u << index)) != 0)));
        assert(!latch[gpio]);
        assert(pulls_disabled[gpio]);
    }
    assert(!output[5]);
}

static void test_pin_map(void)
{
    const uint pins[] = {
        RETROLINK_MSX_UP_GPIO, RETROLINK_MSX_TRIGGER_A_GPIO,
        RETROLINK_MSX_DOWN_GPIO, RETROLINK_MSX_TRIGGER_B_GPIO,
        RETROLINK_MSX_LEFT_GPIO, RETROLINK_MSX_OUT_STROBE_GPIO, RETROLINK_MSX_RIGHT_GPIO,
        RETROLINK_MD_UP_GPIO, RETROLINK_MD_BA_GPIO, RETROLINK_MD_DOWN_GPIO,
        RETROLINK_MD_THSEL_GPIO, RETROLINK_MD_LEFT_GPIO,
        RETROLINK_MD_CSTART_GPIO, RETROLINK_MD_RIGHT_GPIO, RETROLINK_STATUS_LED_GPIO,
    };
    const uint expected[] = {0, 1, 2, 3, 4, 5, 6, 28, 27, 26, 15, 14, 13, 12, 16};
    uint32_t used = 0;
    for (uint index = 0; index < sizeof(pins) / sizeof(pins[0]); ++index) {
        assert(pins[index] == expected[index]);
        assert((used & (1u << pins[index])) == 0);
        used |= 1u << pins[index];
    }
    assert(RETROLINK_MD_POLL_US >= RETROLINK_MD_RESET_US);
}

int main(void)
{
    test_pin_map();
    msx_output_init();
    expect_outputs(0);
    assert(pull_up[5]);
    md_port_init();
    assert(now == 3000u && output[15] && latch[15]);
    for (uint index = 0; index < 6; ++index) assert(pull_up[md_inputs[index]]);
    for (uint gpio = 7; gpio < 30; ++gpio) {
        if ((gpio < 12 || gpio > 15) && gpio < 26) assert(!initialized[gpio]);
    }

    for (joystick_state_t state = 0; state < 64; ++state) {
        set_common(false);
        msx_output_set_state(state);
        expect_outputs(state);
        set_common(true);
        expect_outputs(0);
        set_common(false);
        expect_outputs(state);
    }

    md_pad_samples_t three = {0x0e, 0x32, 0x32, 0x0e, 0x32};
    md_pad_state_t pad;
    assert(poll(&pad, three) == MD_POLL_OK);
    assert(pad.type == MD_PAD_3_BUTTON && pad.buttons == (MD_UP | MD_B | MD_C));
    msx_output_set_state(md_pad_to_msx(pad));
    expect_outputs(JOYSTICK_UP | JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B);
    md_pad_samples_t six = {0x3f, 0x33, 0x30, 0x30, 0x3f};
    assert(poll(&pad, six) == MD_POLL_OK);
    assert(pad.type == MD_PAD_6_BUTTON && pad.buttons == (MD_X | MD_Y | MD_Z | MD_MODE));
    msx_output_set_state(md_pad_to_msx(pad));
    expect_outputs(0);

    msx_output_set_state(JOYSTICK_BUTTON_A);
    raise_common_during_poll = true;
    assert(poll(&pad, three) == MD_POLL_OK);
    expect_outputs(0);
    set_common(false);
    expect_outputs(JOYSTICK_BUTTON_A);

    assert(poll(&pad, (md_pad_samples_t){0x3f, 0x3f, 0x3f, 0x3f, 0x3f}) == MD_POLL_NO_PAD);
    msx_output_set_state(md_pad_to_msx(pad));
    expect_outputs(0);
    assert(poll(&pad, three) == MD_POLL_OK);
    msx_output_set_state(md_pad_to_msx(pad));
    expect_outputs(JOYSTICK_UP | JOYSTICK_BUTTON_A | JOYSTICK_BUTTON_B);

    injected_delay = 420u;
    assert(poll(&pad, six) == MD_POLL_OK);
    injected_delay = 421u;
    assert(poll(&pad, six) == MD_POLL_TIMING_ERROR);
    assert(pad.type == MD_PAD_NONE && pad.buttons == 0);
    msx_output_set_state(md_pad_to_msx(pad));
    expect_outputs(0);
    now = UINT32_MAX - 40u;
    assert(poll(&pad, six) == MD_POLL_OK);
    assert(interrupts_enabled);
    puts("MD GPIO tests passed: exact pins, TH sequence/timing, timeout/wrap, reconnect, MSX common and release");
    return 0;
}
