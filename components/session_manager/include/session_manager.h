/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SESSION_TOKEN_LENGTH 65U
#define WIFI_PASSWORD_MAX_LENGTH 64U

esp_err_t session_manager_init(void);
bool session_manager_is_configured(void);
esp_err_t session_manager_get_wifi_password(char *password, size_t capacity);
esp_err_t session_manager_configure(const char *wifi_password, const char *pin);
esp_err_t session_manager_login(const char *pin,
                                char token[SESSION_TOKEN_LENGTH]);
bool session_manager_validate(const char *token);
esp_err_t session_manager_reauthorize_pin(const char *pin);
esp_err_t session_manager_factory_reset(void);

#ifdef __cplusplus
}
#endif
