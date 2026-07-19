/* SPDX-License-Identifier: MIT */

#include "profile_store.h"

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "esp_crc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define PROFILE_STORE_MAGIC 0x534C5052U
#define PROFILE_STORE_SCHEMA 1U
#define PROFILE_NAMESPACE "profiles"
#define PROFILE_KEY "store"

typedef struct {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t profile_count;
    uint32_t active_profile_id;
    presenter_profile_t profiles[PRESENTER_PROFILE_COUNT];
    uint32_t crc32;
} profile_store_blob_t;

static const char *TAG = "PROFILE_STORE";
static SemaphoreHandle_t s_mutex;
static profile_store_blob_t s_blob;

static uint32_t blob_crc(const profile_store_blob_t *blob)
{
    return esp_crc32_le(0, (const uint8_t *)blob,
                        (uint32_t)offsetof(profile_store_blob_t, crc32));
}

static presenter_profile_t *find_profile(uint32_t id)
{
    for (size_t i = 0; i < s_blob.profile_count; ++i) {
        if (s_blob.profiles[i].id == id) return &s_blob.profiles[i];
    }
    return NULL;
}

static void load_defaults(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_blob.magic = PROFILE_STORE_MAGIC;
    s_blob.schema_version = PROFILE_STORE_SCHEMA;
    s_blob.profile_count = PRESENTER_PROFILE_COUNT;
    s_blob.active_profile_id = PRESENTER_PROFILE_POWERPOINT;
    for (uint32_t id = 1; id <= PRESENTER_PROFILE_COUNT; ++id) {
        presenter_profile_factory(id, &s_blob.profiles[id - 1U]);
    }
    s_blob.crc32 = blob_crc(&s_blob);
}

static bool blob_valid(const profile_store_blob_t *blob, size_t size)
{
    if (size != sizeof(*blob) || blob->magic != PROFILE_STORE_MAGIC ||
        blob->schema_version != PROFILE_STORE_SCHEMA ||
        blob->profile_count != PRESENTER_PROFILE_COUNT ||
        blob->active_profile_id < 1 ||
        blob->active_profile_id > PRESENTER_PROFILE_COUNT ||
        blob->crc32 != blob_crc(blob)) {
        return false;
    }
    for (size_t i = 0; i < blob->profile_count; ++i) {
        if (blob->profiles[i].id != i + 1U ||
            presenter_profile_validate(&blob->profiles[i]) != ESP_OK) {
            return false;
        }
    }
    return true;
}

static esp_err_t persist_locked(void)
{
    s_blob.crc32 = blob_crc(&s_blob);
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(PROFILE_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_blob(handle, PROFILE_KEY, &s_blob, sizeof(s_blob));
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err;
}

esp_err_t profile_store_init(void)
{
    if (s_mutex != NULL) return ESP_ERR_INVALID_STATE;
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) return ESP_ERR_NO_MEM;

    nvs_handle_t handle = 0;
    size_t size = sizeof(s_blob);
    esp_err_t err = nvs_open(PROFILE_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) err = nvs_get_blob(handle, PROFILE_KEY, &s_blob, &size);
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK || !blob_valid(&s_blob, size)) {
        ESP_LOGW(TAG, "profile_storage_invalid error=%s; loading factory defaults",
                 esp_err_to_name(err));
        load_defaults();
        err = persist_locked();
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "loaded %u profiles active=%" PRIu32,
                 s_blob.profile_count, s_blob.active_profile_id);
    }
    return err;
}

esp_err_t profile_store_get_all(presenter_profile_t *profiles, size_t capacity,
                                size_t *count, uint32_t *active_profile_id)
{
    if (profiles == NULL || count == NULL || capacity < PRESENTER_PROFILE_COUNT ||
        s_mutex == NULL) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    memcpy(profiles, s_blob.profiles, sizeof(s_blob.profiles));
    *count = s_blob.profile_count;
    if (active_profile_id != NULL) *active_profile_id = s_blob.active_profile_id;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t profile_store_get(uint32_t id, presenter_profile_t *profile)
{
    if (profile == NULL || s_mutex == NULL) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    presenter_profile_t *found = find_profile(id);
    if (found != NULL) *profile = *found;
    xSemaphoreGive(s_mutex);
    return found != NULL ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t profile_store_get_active(presenter_profile_t *profile)
{
    if (profile == NULL || s_mutex == NULL) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    presenter_profile_t *found = find_profile(s_blob.active_profile_id);
    if (found != NULL) *profile = *found;
    xSemaphoreGive(s_mutex);
    return found != NULL ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t profile_store_update(uint32_t id, const presenter_profile_t *profile,
                               presenter_profile_t *saved_profile)
{
    if (profile == NULL || id != profile->id || s_mutex == NULL ||
        presenter_profile_validate(profile) != ESP_OK) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    presenter_profile_t *target = find_profile(id);
    if (target == NULL) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    presenter_profile_t updated = *profile;
    updated.revision = target->revision + 1U;
    if (updated.revision == 0) updated.revision = 1;
    *target = updated;
    esp_err_t err = persist_locked();
    if (err == ESP_OK && saved_profile != NULL) *saved_profile = updated;
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t profile_store_activate(uint32_t id, presenter_profile_t *profile)
{
    if (s_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    presenter_profile_t *target = find_profile(id);
    if (target == NULL) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    s_blob.active_profile_id = id;
    esp_err_t err = persist_locked();
    if (err == ESP_OK && profile != NULL) *profile = *target;
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t profile_store_reset(uint32_t id, presenter_profile_t *profile)
{
    if (id < 1 || id > PRESENTER_PROFILE_COUNT || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    presenter_profile_t factory;
    presenter_profile_factory(id, &factory);
    presenter_profile_t *target = find_profile(id);
    factory.revision = target->revision + 1U;
    *target = factory;
    esp_err_t err = persist_locked();
    if (err == ESP_OK && profile != NULL) *profile = factory;
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t profile_store_factory_reset(void)
{
    if (s_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    load_defaults();
    esp_err_t err = persist_locked();
    xSemaphoreGive(s_mutex);
    return err;
}
