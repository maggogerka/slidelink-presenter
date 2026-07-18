/*
 * SPDX-License-Identifier: MIT
 */

#include "usb_hid.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "class/hid/hid_device.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"

extern const uint8_t slidelink_hid_report_descriptor[];
extern const uint8_t slidelink_configuration_descriptor[];

static const char *TAG = "USB_HID";
static SemaphoreHandle_t s_report_complete;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_mounted;
static volatile bool s_suspended;
static volatile uint32_t s_session;
static volatile uint32_t s_disconnects;
static char s_serial[12];
static const char *s_string_descriptor[] = {
    (const char[]){0x09, 0x04},
    "Maggogerka",
    "SlideLink USB Presenter",
    s_serial,
    "SlideLink HID Keyboard",
};

static void tinyusb_event_handler(tinyusb_event_t *event, void *argument)
{
    (void)argument;
    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:
        portENTER_CRITICAL(&s_state_lock);
        s_mounted = true;
        s_suspended = false;
        ++s_session;
        const uint32_t mounted_session = s_session;
        portEXIT_CRITICAL(&s_state_lock);
        ESP_LOGI(TAG, "mounted session=%" PRIu32, mounted_session);
        break;
    case TINYUSB_EVENT_DETACHED:
        portENTER_CRITICAL(&s_state_lock);
        s_mounted = false;
        s_suspended = false;
        ++s_session;
        ++s_disconnects;
        const uint32_t detached_session = s_session;
        portEXIT_CRITICAL(&s_state_lock);
        ESP_LOGW(TAG, "disconnected session=%" PRIu32, detached_session);
        break;
#ifdef CONFIG_TINYUSB_SUSPEND_CALLBACK
    case TINYUSB_EVENT_SUSPENDED:
        s_suspended = true;
        ESP_LOGI(TAG, "suspended");
        break;
#endif
#ifdef CONFIG_TINYUSB_RESUME_CALLBACK
    case TINYUSB_EVENT_RESUMED:
        s_suspended = false;
        ESP_LOGI(TAG, "resumed");
        break;
#endif
    default:
        break;
    }
}

static bool wait_until_ready(uint32_t timeout_ms)
{
    const int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000);
    do {
        if (!usb_hid_is_mounted() || usb_hid_is_suspended()) {
            return false;
        }
        if (tud_hid_ready()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (esp_timer_get_time() < deadline_us);
    return false;
}

static bool send_report_and_wait(uint8_t modifier, uint8_t keycode)
{
    uint8_t keys[6] = {0};
    if (keycode != 0) {
        keys[0] = keycode;
    }

    (void)xSemaphoreTake(s_report_complete, 0);
    if (!tud_hid_keyboard_report(0, modifier, keys)) {
        return false;
    }
    return xSemaphoreTake(s_report_complete,
                          pdMS_TO_TICKS(USB_HID_READY_TIMEOUT_MS)) == pdTRUE;
}

esp_err_t usb_hid_init(void)
{
    if (s_report_complete != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_report_complete = xSemaphoreCreateBinary();
    if (s_report_complete == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_report_complete);
        s_report_complete = NULL;
        return err;
    }
    (void)snprintf(s_serial, sizeof(s_serial), "SL-%02X%02X%02X%02X",
                   mac[2], mac[3], mac[4], mac[5]);

    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG(tinyusb_event_handler);
    config.descriptor.device = NULL;
    config.descriptor.full_speed_config = slidelink_configuration_descriptor;
    config.descriptor.string = s_string_descriptor;
    config.descriptor.string_count = sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]);
#if TUD_OPT_HIGH_SPEED
    config.descriptor.high_speed_config = slidelink_configuration_descriptor;
#endif

    err = tinyusb_driver_install(&config);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_report_complete);
        s_report_complete = NULL;
        return err;
    }

    ESP_LOGI(TAG, "initialized product='SlideLink USB Presenter' serial='%s'", s_serial);
    return ESP_OK;
}

bool usb_hid_is_mounted(void)
{
    return s_mounted && tud_mounted();
}

bool usb_hid_is_ready(void)
{
    return usb_hid_is_mounted() && !s_suspended && tud_hid_ready();
}

bool usb_hid_is_suspended(void)
{
    return s_suspended;
}

uint32_t usb_hid_session(void)
{
    uint32_t value;
    portENTER_CRITICAL(&s_state_lock);
    value = s_session;
    portEXIT_CRITICAL(&s_state_lock);
    return value;
}

uint32_t usb_hid_disconnect_count(void)
{
    uint32_t value;
    portENTER_CRITICAL(&s_state_lock);
    value = s_disconnects;
    portEXIT_CRITICAL(&s_state_lock);
    return value;
}

esp_err_t usb_hid_tap(uint8_t modifier, uint8_t keycode)
{
    if (!usb_hid_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!wait_until_ready(USB_HID_READY_TIMEOUT_MS)) {
        return ESP_ERR_TIMEOUT;
    }

    const uint32_t session = usb_hid_session();
    bool pressed = send_report_and_wait(modifier, keycode);
    if (!pressed) {
        usb_hid_release_all();
        return usb_hid_is_mounted() ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_STATE;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    if (session != usb_hid_session() || !wait_until_ready(USB_HID_READY_TIMEOUT_MS) ||
        !send_report_and_wait(0, 0)) {
        usb_hid_release_all();
        return usb_hid_is_mounted() ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t usb_hid_type_digits(uint16_t number)
{
    if (number < 1 || number > 9999) {
        return ESP_ERR_INVALID_ARG;
    }

    char digits[5];
    (void)snprintf(digits, sizeof(digits), "%u", (unsigned)number);
    for (size_t i = 0; digits[i] != '\0'; ++i) {
        const uint8_t keycode = (digits[i] == '0') ? HID_KEY_0 :
                                (uint8_t)(HID_KEY_1 + (digits[i] - '1'));
        esp_err_t err = usb_hid_tap(0, keycode);
        if (err != ESP_OK) {
            usb_hid_release_all();
            return err;
        }
    }
    return ESP_OK;
}

void usb_hid_release_all(void)
{
    if (s_report_complete == NULL || !usb_hid_is_mounted()) {
        return;
    }
    if (wait_until_ready(20)) {
        (void)send_report_and_wait(0, 0);
    }
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return slidelink_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t requested_length)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)requested_length;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t buffer_size)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)buffer_size;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report,
                                uint16_t length)
{
    (void)instance;
    (void)report;
    (void)length;
    if (s_report_complete != NULL) {
        (void)xSemaphoreGive(s_report_complete);
    }
}
