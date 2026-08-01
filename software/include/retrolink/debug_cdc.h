#ifndef RETROLINK_DEBUG_CDC_H
#define RETROLINK_DEBUG_CDC_H

#include <stdbool.h>

void debug_cdc_init(void);
void debug_cdc_task(void);
void debug_cdc_log(char const *format, ...);
bool debug_cdc_is_connected(void);

#endif