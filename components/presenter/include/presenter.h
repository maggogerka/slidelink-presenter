/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PRESENTER_COMMAND_NEXT,
    PRESENTER_COMMAND_PREVIOUS,
    PRESENTER_COMMAND_START,
    PRESENTER_COMMAND_START_CURRENT,
    PRESENTER_COMMAND_STOP,
    PRESENTER_COMMAND_BLACK,
    PRESENTER_COMMAND_WHITE,
    PRESENTER_COMMAND_FIRST,
    PRESENTER_COMMAND_LAST,
    PRESENTER_COMMAND_GOTO_SLIDE,
    PRESENTER_COMMAND_TYPE_COUNT
} presenter_command_type_t;

typedef struct {
    uint32_t id;
    presenter_command_type_t type;
    uint16_t slide_number;
    uint32_t created_at_ms;
} presenter_command_t;

typedef struct {
    uint8_t modifier;
    uint8_t keycode;
} presenter_key_t;

const char *presenter_command_type_name(presenter_command_type_t type);
esp_err_t presenter_command_key(presenter_command_type_t type, presenter_key_t *key);
esp_err_t presenter_execute(const presenter_command_t *command);

#ifdef __cplusplus
}
#endif
