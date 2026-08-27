/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t firmware_update_init(void);
esp_err_t firmware_update_begin(size_t image_size);
esp_err_t firmware_update_write(const uint8_t *data, size_t size);
esp_err_t firmware_update_finish(char *version, size_t version_capacity);
void firmware_update_abort(void);
esp_err_t firmware_update_confirm_running(void);
bool firmware_update_in_progress(void);

#ifdef __cplusplus
}
#endif
