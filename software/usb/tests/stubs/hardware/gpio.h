#ifndef TEST_HARDWARE_GPIO_H
#define TEST_HARDWARE_GPIO_H

#include <stdbool.h>

typedef unsigned int uint;
#define GPIO_IN false
#define GPIO_OUT true

void gpio_init(uint gpio);
void gpio_set_dir(uint gpio, bool output);
void gpio_disable_pulls(uint gpio);
void gpio_put(uint gpio, bool value);

#endif