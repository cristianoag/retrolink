#ifndef RETROLINK_STATUS_LED_H
#define RETROLINK_STATUS_LED_H

#include <stdbool.h>

void status_led_init(void);
void status_led_task(void);
void status_led_pulse(void);
bool status_led_is_ready(void);

#endif