/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>
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

#define PROFILE_NAME_LENGTH 24U
#define PROFILE_MAX_STEPS 4U
#define PRESENTER_PROFILE_COUNT 6U

#define PRESENTER_PROFILE_POWERPOINT 1U
#define PRESENTER_PROFILE_GOOGLE_SLIDES 2U
#define PRESENTER_PROFILE_LIBREOFFICE 3U
#define PRESENTER_PROFILE_PDF 4U
#define PRESENTER_PROFILE_CUSTOM_1 5U
#define PRESENTER_PROFILE_CUSTOM_2 6U

typedef struct {
    uint8_t modifier;
    uint8_t keycode;
    uint16_t delay_after_ms;
} presenter_key_step_t;

typedef struct {
    bool enabled;
    uint8_t step_count;
    presenter_key_step_t steps[PROFILE_MAX_STEPS];
} presenter_binding_t;

typedef struct {
    uint32_t id;
    uint32_t revision;
    char name[PROFILE_NAME_LENGTH];
    presenter_binding_t next;
    presenter_binding_t previous;
    presenter_binding_t start;
    presenter_binding_t start_current;
    presenter_binding_t stop;
    presenter_binding_t black;
    presenter_binding_t white;
    presenter_binding_t first;
    presenter_binding_t last;
    bool goto_enabled;
    uint8_t goto_submit_key;
    uint16_t digit_delay_ms;
} presenter_profile_t;

const char *presenter_command_type_name(presenter_command_type_t type);
esp_err_t presenter_command_key(presenter_command_type_t type, presenter_key_t *key);
esp_err_t presenter_init(const presenter_profile_t *profile);
esp_err_t presenter_set_profile(const presenter_profile_t *profile);
void presenter_get_profile(presenter_profile_t *profile);
void presenter_profile_factory(uint32_t id, presenter_profile_t *profile);
esp_err_t presenter_profile_validate(const presenter_profile_t *profile);
esp_err_t presenter_binding_validate(const presenter_binding_t *binding);
presenter_binding_t *presenter_profile_binding(presenter_profile_t *profile,
                                               presenter_command_type_t type);
const presenter_binding_t *presenter_profile_binding_const(
    const presenter_profile_t *profile, presenter_command_type_t type);
bool presenter_keycode_allowed(uint8_t keycode);
esp_err_t presenter_keycode_from_name(const char *name, uint8_t *keycode);
const char *presenter_keycode_name(uint8_t keycode);
esp_err_t presenter_execute_binding(const presenter_binding_t *binding);
esp_err_t presenter_execute(const presenter_command_t *command);

#ifdef __cplusplus
}
#endif
