#ifndef RETROLINK_MSX_OUTPUT_H
#define RETROLINK_MSX_OUTPUT_H

#include <stdbool.h>

#include "retrolink/joystick.h"

void msx_output_init(void);
void msx_output_set_state(joystick_state_t state);
/* MSX pin 8 (OUT/common) level; high releases all outputs. */
bool msx_output_common_high(void);

#endif
