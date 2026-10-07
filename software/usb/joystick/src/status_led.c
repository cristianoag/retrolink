#include "retrolink/status_led.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "retrolink/board_config.h"
#include "ws2812.pio.h"

static PIO led_pio = pio1;
static uint led_sm = 0;
static bool led_ready = false;
static bool pulse_active = false;
static absolute_time_t pulse_until;

static uint32_t rgb_to_grb(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((uint32_t)green << 16) | ((uint32_t)red << 8) | blue;
}

static void status_led_write(uint32_t grb)
{
    if (!led_ready) {
        return;
    }

    pio_sm_put_blocking(led_pio, led_sm, grb << 8u);
}

void status_led_init(void)
{
    uint offset = pio_add_program(led_pio, &ws2812_program);
    pio_sm_config config = ws2812_program_get_default_config(offset);

    sm_config_set_sideset_pins(&config, RETROLINK_STATUS_LED_GPIO);
    sm_config_set_out_shift(&config, false, true, 24);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

    float divider = (float)clock_get_hz(clk_sys) / (800000.0f * (float)(ws2812_T1 + ws2812_T2 + ws2812_T3));
    sm_config_set_clkdiv(&config, divider);

    pio_gpio_init(led_pio, RETROLINK_STATUS_LED_GPIO);
    pio_sm_set_consecutive_pindirs(led_pio, led_sm, RETROLINK_STATUS_LED_GPIO, 1, true);
    pio_sm_init(led_pio, led_sm, offset, &config);
    pio_sm_set_enabled(led_pio, led_sm, true);

    led_ready = true;
    status_led_write(0);
}

void status_led_task(void)
{
    if (pulse_active && absolute_time_diff_us(get_absolute_time(), pulse_until) <= 0) {
        pulse_active = false;
        status_led_write(0);
    }
}

void status_led_pulse(void)
{
    if (!led_ready) {
        return;
    }

    status_led_write(rgb_to_grb(0, 32, 0));
    pulse_until = make_timeout_time_ms(RETROLINK_STATUS_LED_PULSE_MS);
    pulse_active = true;
}

bool status_led_is_ready(void)
{
    return led_ready;
}