/*
 * SPDX-License-Identifier: MIT
 */

#include "command_router.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "presenter.h"
#include "presenter_button.h"
#include "presenter_console.h"
#include "profile_store.h"
#include "usb_hid.h"

static const char *TAG = "SLIDELINK";

void app_main(void)
{
    ESP_LOGI(TAG, "SlideLink v%s starting", esp_app_get_description()->version);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(profile_store_init());
    presenter_profile_t active_profile;
    ESP_ERROR_CHECK(profile_store_get_active(&active_profile));
    ESP_ERROR_CHECK(presenter_init(&active_profile));
    ESP_ERROR_CHECK(usb_hid_init());
    ESP_ERROR_CHECK(command_router_init());
    ESP_ERROR_CHECK(presenter_button_init());
    ESP_ERROR_CHECK(presenter_console_init());

    ESP_LOGI(TAG, "ready; connect the native USB port and type 'help' on UART");
}
