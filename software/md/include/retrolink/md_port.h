#ifndef RETROLINK_MD_PORT_H
#define RETROLINK_MD_PORT_H

#include "retrolink/md_pad.h"

typedef enum {
    MD_POLL_OK,
    MD_POLL_NO_PAD,
    MD_POLL_TIMING_ERROR,
} md_poll_result_t;

void md_port_init(void);
md_poll_result_t md_port_poll(md_pad_state_t *state);

#endif
