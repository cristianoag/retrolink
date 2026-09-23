#include "retrolink/md_port.h"

#include "hardware/gpio.h"
#include "pico/time.h"

#include "retrolink/board_config.h"

static const uint data_gpios[] = {
    RETROLINK_MD_UP_GPIO, RETROLINK_MD_DOWN_GPIO,
    RETROLINK_MD_LEFT_GPIO, RETROLINK_MD_RIGHT_GPIO,
    RETROLINK_MD_BA_GPIO, RETROLINK_MD_CSTART_GPIO,
};

static uint8_t read_data(void)
{
    uint32_t pins = gpio_get_all();
    uint8_t data = 0;
    for (uint index = 0; index < 6; ++index) {
        if (pins & (1u << data_gpios[index])) {
            data |= (uint8_t)(1u << index);
        }
    }
    return data;
}

static void select_level(bool high)
{
    gpio_put(RETROLINK_MD_THSEL_GPIO, high);
    busy_wait_us_32(RETROLINK_MD_SETTLE_US);
}

void md_port_init(void)
{
    for (uint index = 0; index < 6; ++index) {
        gpio_init(data_gpios[index]);
        gpio_set_dir(data_gpios[index], GPIO_IN);
        gpio_pull_up(data_gpios[index]);
    }
    gpio_init(RETROLINK_MD_THSEL_GPIO);
    gpio_put(RETROLINK_MD_THSEL_GPIO, true);
    gpio_set_dir(RETROLINK_MD_THSEL_GPIO, GPIO_OUT);
    busy_wait_us_32(RETROLINK_MD_RESET_US);
}

md_poll_result_t md_port_poll(md_pad_state_t *state)
{
    uint32_t start = time_us_32();
    md_pad_samples_t samples;
    samples.idle_high = read_data();
    select_level(false);
    samples.first_low = read_data();
    select_level(true);
    select_level(false);
    select_level(true);
    select_level(false);
    samples.third_low = read_data();
    select_level(true);
    samples.third_high = read_data();
    select_level(false);
    samples.fourth_low = read_data();
    select_level(true);

    /* Keep MSX IRQs enabled; reject a frame interrupted beyond the timing budget. */
    if ((uint32_t)(time_us_32() - start) > RETROLINK_MD_MAX_FRAME_US) {
        *state = (md_pad_state_t){MD_PAD_NONE, 0};
        return MD_POLL_TIMING_ERROR;
    }
    *state = md_pad_decode(samples);
    return state->type == MD_PAD_NONE ? MD_POLL_NO_PAD : MD_POLL_OK;
}
