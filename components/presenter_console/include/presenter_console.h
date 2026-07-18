/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "presenter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRESENTER_CONSOLE_MAX_LINE_LENGTH 64U

typedef enum {
    CONSOLE_PARSE_EMPTY,
    CONSOLE_PARSE_COMMAND,
    CONSOLE_PARSE_STATUS,
    CONSOLE_PARSE_HELP,
    CONSOLE_PARSE_ERROR_UNSUPPORTED,
    CONSOLE_PARSE_ERROR_SLIDE_NUMBER,
    CONSOLE_PARSE_ERROR_TOO_LONG,
} presenter_console_parse_result_t;

typedef struct {
    presenter_console_parse_result_t result;
    presenter_command_type_t command_type;
    uint16_t slide_number;
} presenter_console_parsed_t;

presenter_console_parsed_t presenter_console_parse(const char *line);
esp_err_t presenter_console_init(void);

#ifdef __cplusplus
}
#endif
