/* SPDX-License-Identifier: MIT */

#include "firmware_update.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "UPDATE";
static SemaphoreHandle_t s_mutex;
static bool s_active;
static esp_ota_handle_t s_handle;
static const esp_partition_t *s_partition;
static size_t s_expected;
static size_t s_written;

esp_err_t firmware_update_init(void)
{
    if (s_mutex != NULL) return ESP_ERR_INVALID_STATE;
    s_mutex = xSemaphoreCreateMutex();
    return s_mutex == NULL ? ESP_ERR_NO_MEM : ESP_OK;
}

bool firmware_update_in_progress(void)
{
    return s_active;
}

esp_err_t firmware_update_begin(size_t image_size)
{
    if (s_mutex == NULL || image_size == 0U) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    s_partition = esp_ota_get_next_update_partition(NULL);
    if (s_partition == NULL || image_size > s_partition->size) {
        s_partition = NULL;
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_SIZE;
    }
    const esp_err_t err = esp_ota_begin(s_partition, image_size, &s_handle);
    if (err != ESP_OK) {
        s_partition = NULL;
        xSemaphoreGive(s_mutex);
        return err;
    }
    s_expected = image_size;
    s_written = 0U;
    s_active = true;
    ESP_LOGI(TAG, "receiving %u bytes into %s", (unsigned)image_size,
             s_partition->label);
    return ESP_OK;
}

esp_err_t firmware_update_write(const uint8_t *data, size_t size)
{
    if (!s_active || data == NULL || size == 0U || s_written > s_expected ||
        size > s_expected - s_written) return ESP_ERR_INVALID_ARG;
    const esp_err_t err = esp_ota_write(s_handle, data, size);
    if (err == ESP_OK) s_written += size;
    return err;
}

static void finish_unlock(void)
{
    s_active = false;
    s_handle = 0;
    s_partition = NULL;
    s_expected = 0U;
    s_written = 0U;
    xSemaphoreGive(s_mutex);
}

void firmware_update_abort(void)
{
    if (!s_active) return;
    (void)esp_ota_abort(s_handle);
    finish_unlock();
    ESP_LOGW(TAG, "firmware update aborted");
}

esp_err_t firmware_update_finish(char *version, size_t version_capacity)
{
    if (!s_active || version == NULL || version_capacity == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_written != s_expected) {
        firmware_update_abort();
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = esp_ota_end(s_handle);
    s_handle = 0;
    if (err == ESP_OK) {
        esp_app_desc_t description = {0};
        err = esp_ota_get_partition_description(s_partition, &description);
        if (err == ESP_OK && strcmp(description.project_name, "slidelink") != 0) {
            ESP_LOGE(TAG, "rejected project '%s'", description.project_name);
            err = ESP_ERR_INVALID_RESPONSE;
        }
        if (err == ESP_OK) {
            err = esp_ota_set_boot_partition(s_partition);
            if (err == ESP_OK) {
                (void)snprintf(version, version_capacity, "%s", description.version);
                ESP_LOGI(TAG, "validated version %s; reboot required", version);
            }
        }
    }
    finish_unlock();
    return err;
}

esp_err_t firmware_update_confirm_running(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    const esp_err_t err = esp_ota_get_state_partition(running, &state);
    if (err == ESP_ERR_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "startup checks passed; confirming OTA image");
        return esp_ota_mark_app_valid_cancel_rollback();
    }
    return ESP_OK;
}
