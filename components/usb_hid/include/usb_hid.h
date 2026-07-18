/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_HID_READY_TIMEOUT_MS 250U

typedef struct {
    bool mounted;
    bool suspended;
    uint32_t session;
    uint32_t disconnects;
} usb_hid_state_t;

esp_err_t usb_hid_init(void);
void usb_hid_get_state(usb_hid_state_t *state);
bool usb_hid_is_mounted(void);
bool usb_hid_is_ready(void);
bool usb_hid_is_suspended(void);
uint32_t usb_hid_session(void);
uint32_t usb_hid_disconnect_count(void);
esp_err_t usb_hid_tap(uint8_t modifier, uint8_t keycode);
esp_err_t usb_hid_type_digits(uint16_t number);
void usb_hid_release_all(void);

#ifdef __cplusplus
}
#endif
