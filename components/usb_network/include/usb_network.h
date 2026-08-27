/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_NETWORK_IPV4_ADDRESS "192.168.55.1"

typedef struct {
    bool initialized;
    bool link_up;
    uint32_t received_packets;
    uint32_t transmitted_packets;
    uint32_t dropped_packets;
} usb_network_state_t;

esp_err_t usb_network_init(void);
void usb_network_get_state(usb_network_state_t *state);

#ifdef __cplusplus
}
#endif
