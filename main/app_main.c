/*
 * SPDX-License-Identifier: MIT
 */

#include "command_router.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "presenter_button.h"
#include "presenter_console.h"
#include "usb_hid.h"

static const char *TAG = "SLIDELINK";

void app_main(void)
{
    ESP_LOGI(TAG, "SlideLink v%s starting", esp_app_get_description()->version);

    ESP_ERROR_CHECK(usb_hid_init());
    ESP_ERROR_CHECK(command_router_init());
    ESP_ERROR_CHECK(presenter_button_init());
    ESP_ERROR_CHECK(presenter_console_init());

    ESP_LOGI(TAG, "ready; connect the native USB port and type 'help' on UART");
}
