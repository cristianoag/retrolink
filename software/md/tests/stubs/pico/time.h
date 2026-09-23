#ifndef TEST_PICO_TIME_H
#define TEST_PICO_TIME_H

#include <stdint.h>

void busy_wait_us_32(uint32_t delay);
uint32_t time_us_32(void);

#endif
