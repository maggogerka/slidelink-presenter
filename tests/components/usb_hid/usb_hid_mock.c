/*
 * SPDX-License-Identifier: MIT
 */

#include "usb_hid.h"
#include "usb_hid_test.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool s_mounted;
static uint32_t s_session = 1;
static uint32_t s_tap_delay_ms;

void usb_hid_test_set_mounted(bool mounted)
{
    if (s_mounted != mounted) {
        ++s_session;
    }
    s_mounted = mounted;
}

void usb_hid_test_set_tap_delay_ms(uint32_t delay_ms)
{
    s_tap_delay_ms = delay_ms;
}

esp_err_t usb_hid_init(void) { return ESP_OK; }
void usb_hid_get_state(usb_hid_state_t *state)
{
    if (state != NULL) {
        *state = (usb_hid_state_t) {
            .mounted = s_mounted,
            .suspended = false,
            .session = s_session,
            .disconnects = 0,
        };
    }
}
bool usb_hid_is_mounted(void) { return s_mounted; }
bool usb_hid_is_ready(void) { return s_mounted; }
bool usb_hid_is_suspended(void) { return false; }
uint32_t usb_hid_session(void) { return s_session; }
uint32_t usb_hid_disconnect_count(void) { return 0; }

esp_err_t usb_hid_tap(uint8_t modifier, uint8_t keycode)
{
    (void)modifier;
    (void)keycode;
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_tap_delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(s_tap_delay_ms));
    }
    return ESP_OK;
}

esp_err_t usb_hid_type_digits(uint16_t number)
{
    return (s_mounted && number >= 1 && number <= 9999) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

void usb_hid_release_all(void) {}
