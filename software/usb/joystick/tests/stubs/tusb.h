#ifndef TEST_TUSB_H
#define TEST_TUSB_H

#include <stdbool.h>
#include <stdint.h>

#include "retrolink/board_config.h"

#define BOARD_TUH_RHPORT 1
#define TUH_CFGID_RPI_PIO_USB_CONFIGURATION 100

typedef enum {
    XFER_RESULT_SUCCESS,
    XFER_RESULT_FAILED,
} xfer_result_t;

bool tuh_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param);
bool tuh_init(uint8_t rhport);
bool tuh_task_event_ready(void);
bool tuh_rhport_reset_bus(uint8_t rhport, bool active);
bool tuh_vid_pid_get(uint8_t dev_addr, uint16_t *vid, uint16_t *pid);
uint8_t tuh_hid_interface_protocol(uint8_t dev_addr, uint8_t instance);
bool tuh_hid_receive_ready(uint8_t dev_addr, uint8_t instance);
bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t instance);

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *descriptor, uint16_t length);
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, const uint8_t *report, uint16_t length);

#endif
