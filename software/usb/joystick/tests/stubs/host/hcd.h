#ifndef TEST_HOST_HCD_H
#define TEST_HOST_HCD_H

#include "tusb.h"

typedef enum {
    HCD_EVENT_DEVICE_ATTACH,
    HCD_EVENT_DEVICE_REMOVE,
    HCD_EVENT_XFER_COMPLETE,
} hcd_eventid_t;

typedef struct {
    uint8_t rhport;
    uint8_t event_id;
    struct {
        uint8_t hub_addr;
        uint8_t hub_port;
    } connection;
} hcd_event_t;

typedef struct {
    uint8_t rhport;
    uint8_t hub_addr;
    uint8_t hub_port;
    uint8_t speed;
} hcd_devtree_info_t;

bool hcd_port_connect_status(uint8_t rhport);
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr);
void hcd_device_close(uint8_t rhport, uint8_t dev_addr);
void hcd_devtree_get_info(uint8_t dev_addr, hcd_devtree_info_t *info);
void hcd_event_handler(const hcd_event_t *event, bool in_isr);
void hcd_event_device_remove(uint8_t rhport, bool in_isr);
void hcd_event_device_attach(uint8_t rhport, bool in_isr);
void hcd_event_xfer_complete(uint8_t dev_addr, uint8_t ep_addr, uint32_t length, xfer_result_t result, bool in_isr);

#endif
