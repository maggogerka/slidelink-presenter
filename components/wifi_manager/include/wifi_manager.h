/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char device_name[24];
    char ssid[33];
    char ip[16];
    uint8_t clients;
    bool setup_mode;
} wifi_manager_state_t;

esp_err_t wifi_manager_init(void);
void wifi_manager_get_state(wifi_manager_state_t *state);

#ifdef __cplusplus
}
#endif
