/* Test double for the production usb_hid component. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define USB_HID_READY_TIMEOUT_MS 250U

esp_err_t usb_hid_init(void);
bool usb_hid_is_mounted(void);
bool usb_hid_is_ready(void);
bool usb_hid_is_suspended(void);
uint32_t usb_hid_session(void);
uint32_t usb_hid_disconnect_count(void);
esp_err_t usb_hid_tap(uint8_t modifier, uint8_t keycode);
esp_err_t usb_hid_type_digits(uint16_t number);
void usb_hid_release_all(void);
