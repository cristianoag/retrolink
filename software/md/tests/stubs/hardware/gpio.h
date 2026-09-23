#ifndef TEST_HARDWARE_GPIO_H
#define TEST_HARDWARE_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef unsigned int uint;
typedef void (*gpio_irq_callback_t)(uint gpio, uint32_t events);
#define GPIO_IN false
#define GPIO_OUT true
#define GPIO_IRQ_EDGE_RISE 8u
#define GPIO_IRQ_EDGE_FALL 4u

void gpio_init(uint gpio);
void gpio_set_dir(uint gpio, bool output);
void gpio_set_dir_masked(uint32_t mask, uint32_t values);
void gpio_put(uint gpio, bool value);
void gpio_pull_up(uint gpio);
void gpio_disable_pulls(uint gpio);
bool gpio_get(uint gpio);
uint32_t gpio_get_all(void);
void gpio_set_irq_enabled_with_callback(uint gpio, uint32_t events, bool enabled,
                                        gpio_irq_callback_t callback);

#endif
