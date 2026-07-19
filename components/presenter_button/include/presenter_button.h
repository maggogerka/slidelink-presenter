/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t presenter_button_init(void);
typedef void (*presenter_button_factory_reset_callback_t)(void);
void presenter_button_set_factory_reset_callback(
    presenter_button_factory_reset_callback_t callback);

#ifdef __cplusplus
}
#endif
