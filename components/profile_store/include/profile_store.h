/* SPDX-License-Identifier: MIT */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "presenter.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t profile_store_init(void);
esp_err_t profile_store_get_all(presenter_profile_t *profiles, size_t capacity,
                                size_t *count, uint32_t *active_profile_id);
esp_err_t profile_store_get(uint32_t id, presenter_profile_t *profile);
esp_err_t profile_store_get_active(presenter_profile_t *profile);
esp_err_t profile_store_update(uint32_t id, const presenter_profile_t *profile,
                               presenter_profile_t *saved_profile);
esp_err_t profile_store_activate(uint32_t id, presenter_profile_t *profile);
esp_err_t profile_store_reset(uint32_t id, presenter_profile_t *profile);
esp_err_t profile_store_factory_reset(void);

#ifdef __cplusplus
}
#endif
