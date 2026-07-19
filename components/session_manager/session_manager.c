/* SPDX-License-Identifier: MIT */

#include "session_manager.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "psa/crypto.h"

#define SECURITY_MAGIC 0x534C5343U
#define SECURITY_SCHEMA 1U
#define SESSION_COUNT 4U
#define SESSION_TTL_US (30LL * 60LL * 1000000LL)
#define LOGIN_FAILURE_LIMIT 5U
#define LOGIN_BLOCK_US (30LL * 1000000LL)

typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint16_t configured;
    char wifi_password[WIFI_PASSWORD_MAX_LENGTH];
    uint8_t pin_salt[16];
    uint8_t pin_hash[32];
} security_blob_t;

typedef struct {
    bool active;
    char token[SESSION_TOKEN_LENGTH];
    int64_t expires_at_us;
} session_slot_t;

static const char *TAG = "SESSION";
static SemaphoreHandle_t s_mutex;
static security_blob_t s_security;
static session_slot_t s_sessions[SESSION_COUNT];
static uint32_t s_failures;
static int64_t s_blocked_until_us;

static bool hash_pin(const char *pin, const uint8_t salt[16], uint8_t hash[32])
{
    uint8_t input[24];
    const size_t pin_length = strlen(pin);
    memcpy(input, salt, 16);
    memcpy(input + 16, pin, pin_length);
    size_t hash_length = 0;
    return psa_hash_compute(PSA_ALG_SHA_256, input, 16 + pin_length, hash, 32,
                            &hash_length) == PSA_SUCCESS && hash_length == 32;
}

static bool constant_time_equal(const uint8_t *left, const uint8_t *right, size_t size)
{
    uint8_t difference = 0;
    for (size_t i = 0; i < size; ++i) difference |= left[i] ^ right[i];
    return difference == 0;
}

static bool pin_format_valid(const char *pin)
{
    if (pin == NULL) return false;
    const size_t length = strlen(pin);
    if (length < 4 || length > 8) return false;
    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)pin[i])) return false;
    }
    return true;
}

static esp_err_t save_security(void)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("security", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_blob(handle, "config", &s_security,
                                           sizeof(s_security));
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err;
}

esp_err_t session_manager_init(void)
{
    if (s_mutex != NULL) return ESP_ERR_INVALID_STATE;
    if (psa_crypto_init() != PSA_SUCCESS) return ESP_FAIL;
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) return ESP_ERR_NO_MEM;
    nvs_handle_t handle = 0;
    size_t size = sizeof(s_security);
    esp_err_t err = nvs_open("security", NVS_READONLY, &handle);
    if (err == ESP_OK) err = nvs_get_blob(handle, "config", &s_security, &size);
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK || size != sizeof(s_security) ||
        s_security.magic != SECURITY_MAGIC ||
        s_security.schema != SECURITY_SCHEMA || s_security.configured != 1) {
        memset(&s_security, 0, sizeof(s_security));
        s_security.magic = SECURITY_MAGIC;
        s_security.schema = SECURITY_SCHEMA;
        ESP_LOGW(TAG, "first-run provisioning required");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "security configuration loaded");
    return ESP_OK;
}

bool session_manager_is_configured(void)
{
    bool configured = false;
    if (s_mutex != NULL && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        configured = s_security.configured == 1;
        xSemaphoreGive(s_mutex);
    }
    return configured;
}

esp_err_t session_manager_get_wifi_password(char *password, size_t capacity)
{
    if (password == NULL || capacity < WIFI_PASSWORD_MAX_LENGTH || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return ESP_ERR_TIMEOUT;
    const char *source = s_security.configured == 1 ? s_security.wifi_password :
                         "slidelink-setup";
    (void)snprintf(password, capacity, "%s", source);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t session_manager_configure(const char *wifi_password, const char *pin)
{
    if (wifi_password == NULL || !pin_format_valid(pin) || s_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t password_length = strlen(wifi_password);
    if (password_length < 8 || password_length > 63) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    memset(&s_security, 0, sizeof(s_security));
    s_security.magic = SECURITY_MAGIC;
    s_security.schema = SECURITY_SCHEMA;
    s_security.configured = 1;
    (void)snprintf(s_security.wifi_password, sizeof(s_security.wifi_password),
                   "%s", wifi_password);
    esp_fill_random(s_security.pin_salt, sizeof(s_security.pin_salt));
    esp_err_t err = hash_pin(pin, s_security.pin_salt, s_security.pin_hash) ?
                    save_security() : ESP_FAIL;
    xSemaphoreGive(s_mutex);
    return err;
}

static esp_err_t verify_pin_locked(const char *pin)
{
    const int64_t now = esp_timer_get_time();
    if (now < s_blocked_until_us) return ESP_ERR_TIMEOUT;
    if (s_security.configured != 1 || !pin_format_valid(pin)) return ESP_ERR_INVALID_ARG;
    uint8_t candidate[32];
    if (!hash_pin(pin, s_security.pin_salt, candidate)) return ESP_FAIL;
    if (!constant_time_equal(candidate, s_security.pin_hash, sizeof(candidate))) {
        ++s_failures;
        if (s_failures >= LOGIN_FAILURE_LIMIT) {
            s_blocked_until_us = now + LOGIN_BLOCK_US;
            s_failures = 0;
            ESP_LOGW(TAG, "login throttled for 30 seconds");
        }
        return ESP_ERR_INVALID_CRC;
    }
    s_failures = 0;
    s_blocked_until_us = 0;
    return ESP_OK;
}

esp_err_t session_manager_login(const char *pin,
                                char token[SESSION_TOKEN_LENGTH])
{
    if (token == NULL || s_mutex == NULL) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = verify_pin_locked(pin);
    if (err == ESP_OK) {
        const int64_t now = esp_timer_get_time();
        size_t slot = 0;
        for (size_t i = 0; i < SESSION_COUNT; ++i) {
            if (!s_sessions[i].active || s_sessions[i].expires_at_us <= now) {
                slot = i;
                break;
            }
            if (s_sessions[i].expires_at_us < s_sessions[slot].expires_at_us) slot = i;
        }
        uint8_t random[32];
        esp_fill_random(random, sizeof(random));
        for (size_t i = 0; i < sizeof(random); ++i) {
            (void)snprintf(&s_sessions[slot].token[i * 2U], 3, "%02x", random[i]);
        }
        s_sessions[slot].active = true;
        s_sessions[slot].expires_at_us = now + SESSION_TTL_US;
        (void)memcpy(token, s_sessions[slot].token, SESSION_TOKEN_LENGTH);
    }
    xSemaphoreGive(s_mutex);
    return err;
}

bool session_manager_validate(const char *token)
{
    if (token == NULL || strlen(token) != SESSION_TOKEN_LENGTH - 1U ||
        s_mutex == NULL) return false;
    bool valid = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        const int64_t now = esp_timer_get_time();
        for (size_t i = 0; i < SESSION_COUNT; ++i) {
            if (s_sessions[i].active && s_sessions[i].expires_at_us > now &&
                strcmp(token, s_sessions[i].token) == 0) {
                valid = true;
                s_sessions[i].expires_at_us = now + SESSION_TTL_US;
                break;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    return valid;
}

esp_err_t session_manager_reauthorize_pin(const char *pin)
{
    if (s_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = verify_pin_locked(pin);
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t session_manager_factory_reset(void)
{
    if (s_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("security", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_erase_key(handle, "config");
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    memset(&s_security, 0, sizeof(s_security));
    memset(s_sessions, 0, sizeof(s_sessions));
    s_security.magic = SECURITY_MAGIC;
    s_security.schema = SECURITY_SCHEMA;
    xSemaphoreGive(s_mutex);
    return err;
}
