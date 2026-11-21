#ifndef TEST_PIO_USB_H
#define TEST_PIO_USB_H

#include <stdint.h>

typedef struct {
    uint8_t pin_dp;
    uint8_t pinout;
} pio_usb_configuration_t;

#define PIO_USB_DEFAULT_CONFIG {0}
#define PIO_USB_PINOUT_DPDM 0

#endif
