/*
 * SPDX-License-Identifier: MIT
 */

#include "presenter.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "class/hid/hid.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb_hid.h"

#define ALLOWED_MODIFIERS (KEYBOARD_MODIFIER_LEFTSHIFT | \
                           KEYBOARD_MODIFIER_LEFTCTRL | \
                           KEYBOARD_MODIFIER_LEFTALT)

typedef struct {
    const char *name;
    uint8_t keycode;
} allowed_key_t;

static const char *s_command_names[PRESENTER_COMMAND_TYPE_COUNT] = {
    [PRESENTER_COMMAND_NEXT] = "next",
    [PRESENTER_COMMAND_PREVIOUS] = "previous",
    [PRESENTER_COMMAND_START] = "start",
    [PRESENTER_COMMAND_START_CURRENT] = "start-current",
    [PRESENTER_COMMAND_STOP] = "stop",
    [PRESENTER_COMMAND_BLACK] = "black",
    [PRESENTER_COMMAND_WHITE] = "white",
    [PRESENTER_COMMAND_FIRST] = "first",
    [PRESENTER_COMMAND_LAST] = "last",
    [PRESENTER_COMMAND_GOTO_SLIDE] = "goto",
};

static const allowed_key_t s_allowed_keys[] = {
    {"Arrow Left", HID_KEY_ARROW_LEFT}, {"Arrow Right", HID_KEY_ARROW_RIGHT},
    {"Page Up", HID_KEY_PAGE_UP}, {"Page Down", HID_KEY_PAGE_DOWN},
    {"Space", HID_KEY_SPACE}, {"Enter", HID_KEY_ENTER},
    {"Escape", HID_KEY_ESCAPE}, {"Home", HID_KEY_HOME}, {"End", HID_KEY_END},
    {"F1", HID_KEY_F1}, {"F2", HID_KEY_F2}, {"F3", HID_KEY_F3},
    {"F4", HID_KEY_F4}, {"F5", HID_KEY_F5}, {"F6", HID_KEY_F6},
    {"F7", HID_KEY_F7}, {"F8", HID_KEY_F8}, {"F9", HID_KEY_F9},
    {"F10", HID_KEY_F10}, {"F11", HID_KEY_F11}, {"F12", HID_KEY_F12},
    {"B", HID_KEY_B}, {"W", HID_KEY_W},
    {"0", HID_KEY_0}, {"1", HID_KEY_1}, {"2", HID_KEY_2},
    {"3", HID_KEY_3}, {"4", HID_KEY_4}, {"5", HID_KEY_5},
    {"6", HID_KEY_6}, {"7", HID_KEY_7}, {"8", HID_KEY_8}, {"9", HID_KEY_9},
};

static SemaphoreHandle_t s_profile_mutex;
static presenter_profile_t s_active_profile;

static void set_binding(presenter_binding_t *binding, uint8_t modifier,
                        uint8_t keycode)
{
    *binding = (presenter_binding_t) {
        .enabled = true,
        .step_count = 1,
        .steps = {{modifier, keycode, 0}},
    };
}

static bool names_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left++) != tolower((unsigned char)*right++)) {
            return false;
        }
    }
    return *left == *right;
}

const char *presenter_command_type_name(presenter_command_type_t type)
{
    return (type >= 0 && type < PRESENTER_COMMAND_TYPE_COUNT) ?
           s_command_names[type] : "unknown";
}

presenter_binding_t *presenter_profile_binding(presenter_profile_t *profile,
                                               presenter_command_type_t type)
{
    if (profile == NULL) {
        return NULL;
    }
    switch (type) {
    case PRESENTER_COMMAND_NEXT: return &profile->next;
    case PRESENTER_COMMAND_PREVIOUS: return &profile->previous;
    case PRESENTER_COMMAND_START: return &profile->start;
    case PRESENTER_COMMAND_START_CURRENT: return &profile->start_current;
    case PRESENTER_COMMAND_STOP: return &profile->stop;
    case PRESENTER_COMMAND_BLACK: return &profile->black;
    case PRESENTER_COMMAND_WHITE: return &profile->white;
    case PRESENTER_COMMAND_FIRST: return &profile->first;
    case PRESENTER_COMMAND_LAST: return &profile->last;
    default: return NULL;
    }
}

const presenter_binding_t *presenter_profile_binding_const(
    const presenter_profile_t *profile, presenter_command_type_t type)
{
    return presenter_profile_binding((presenter_profile_t *)profile, type);
}

bool presenter_keycode_allowed(uint8_t keycode)
{
    for (size_t i = 0; i < sizeof(s_allowed_keys) / sizeof(s_allowed_keys[0]); ++i) {
        if (s_allowed_keys[i].keycode == keycode) {
            return true;
        }
    }
    return false;
}

esp_err_t presenter_keycode_from_name(const char *name, uint8_t *keycode)
{
    if (name == NULL || keycode == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < sizeof(s_allowed_keys) / sizeof(s_allowed_keys[0]); ++i) {
        if (names_equal(name, s_allowed_keys[i].name)) {
            *keycode = s_allowed_keys[i].keycode;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

const char *presenter_keycode_name(uint8_t keycode)
{
    for (size_t i = 0; i < sizeof(s_allowed_keys) / sizeof(s_allowed_keys[0]); ++i) {
        if (s_allowed_keys[i].keycode == keycode) {
            return s_allowed_keys[i].name;
        }
    }
    return "Unknown";
}

void presenter_profile_factory(uint32_t id, presenter_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }
    memset(profile, 0, sizeof(*profile));
    profile->id = (id >= 1 && id <= PRESENTER_PROFILE_COUNT) ? id :
                  PRESENTER_PROFILE_POWERPOINT;
    profile->revision = 1;
    const char *name = "PowerPoint";
    if (profile->id == PRESENTER_PROFILE_GOOGLE_SLIDES) name = "Google Slides";
    else if (profile->id == PRESENTER_PROFILE_LIBREOFFICE) name = "LibreOffice Impress";
    else if (profile->id == PRESENTER_PROFILE_PDF) name = "Generic PDF";
    else if (profile->id == PRESENTER_PROFILE_CUSTOM_1) name = "Custom 1";
    else if (profile->id == PRESENTER_PROFILE_CUSTOM_2) name = "Custom 2";
    (void)strncpy(profile->name, name, sizeof(profile->name) - 1U);

    const uint8_t next_key = (profile->id == PRESENTER_PROFILE_PDF) ?
                             HID_KEY_PAGE_DOWN : HID_KEY_ARROW_RIGHT;
    const uint8_t previous_key = (profile->id == PRESENTER_PROFILE_PDF) ?
                                 HID_KEY_PAGE_UP : HID_KEY_ARROW_LEFT;
    set_binding(&profile->next, 0, next_key);
    set_binding(&profile->previous, 0, previous_key);
    set_binding(&profile->start, profile->id == PRESENTER_PROFILE_GOOGLE_SLIDES ?
                KEYBOARD_MODIFIER_LEFTCTRL : 0, HID_KEY_F5);
    set_binding(&profile->start_current, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_F5);
    set_binding(&profile->stop, 0, HID_KEY_ESCAPE);
    set_binding(&profile->black, 0, HID_KEY_B);
    set_binding(&profile->white, 0, HID_KEY_W);
    set_binding(&profile->first, 0, HID_KEY_HOME);
    set_binding(&profile->last, 0, HID_KEY_END);
    profile->goto_enabled = true;
    profile->goto_submit_key = HID_KEY_ENTER;
    profile->digit_delay_ms = 20;
}

esp_err_t presenter_profile_validate(const presenter_profile_t *profile)
{
    if (profile == NULL || profile->id < 1 || profile->id > PRESENTER_PROFILE_COUNT ||
        profile->name[0] == '\0' ||
        memchr(profile->name, '\0', sizeof(profile->name)) == NULL ||
        profile->digit_delay_ms > 1000 ||
        (profile->goto_enabled && !presenter_keycode_allowed(profile->goto_submit_key))) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t command_total;
    for (int type = PRESENTER_COMMAND_NEXT; type < PRESENTER_COMMAND_GOTO_SLIDE; ++type) {
        const presenter_binding_t *binding = presenter_profile_binding_const(
            profile, (presenter_command_type_t)type);
        if (binding == NULL || binding->step_count > PROFILE_MAX_STEPS ||
            (binding->enabled && binding->step_count == 0)) {
            return ESP_ERR_INVALID_ARG;
        }
        command_total = 0;
        for (uint8_t step = 0; step < binding->step_count; ++step) {
            if (!presenter_keycode_allowed(binding->steps[step].keycode) ||
                (binding->steps[step].modifier & ~ALLOWED_MODIFIERS) != 0 ||
                binding->steps[step].delay_after_ms > 1000) {
                return ESP_ERR_INVALID_ARG;
            }
            command_total += binding->steps[step].delay_after_ms;
        }
        if (command_total > 2000) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}

esp_err_t presenter_init(const presenter_profile_t *profile)
{
    if (s_profile_mutex != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_profile_mutex = xSemaphoreCreateMutex();
    if (s_profile_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    presenter_profile_t initial;
    if (profile == NULL) {
        presenter_profile_factory(PRESENTER_PROFILE_POWERPOINT, &initial);
        profile = &initial;
    }
    return presenter_set_profile(profile);
}

esp_err_t presenter_set_profile(const presenter_profile_t *profile)
{
    if (s_profile_mutex == NULL || presenter_profile_validate(profile) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_profile_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_active_profile = *profile;
    xSemaphoreGive(s_profile_mutex);
    return ESP_OK;
}

void presenter_get_profile(presenter_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }
    if (s_profile_mutex == NULL ||
        xSemaphoreTake(s_profile_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        presenter_profile_factory(PRESENTER_PROFILE_POWERPOINT, profile);
        return;
    }
    *profile = s_active_profile;
    xSemaphoreGive(s_profile_mutex);
}

esp_err_t presenter_command_key(presenter_command_type_t type, presenter_key_t *key)
{
    if (key == NULL || type < 0 || type >= PRESENTER_COMMAND_GOTO_SLIDE) {
        return ESP_ERR_INVALID_ARG;
    }
    presenter_profile_t profile;
    presenter_get_profile(&profile);
    const presenter_binding_t *binding = presenter_profile_binding_const(&profile, type);
    if (binding == NULL || !binding->enabled || binding->step_count != 1) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    key->modifier = binding->steps[0].modifier;
    key->keycode = binding->steps[0].keycode;
    return ESP_OK;
}

static esp_err_t execute_goto(const presenter_profile_t *profile, uint16_t number)
{
    if (!profile->goto_enabled || number < 1 || number > 9999) {
        return ESP_ERR_INVALID_ARG;
    }
    char digits[5];
    (void)snprintf(digits, sizeof(digits), "%u", (unsigned)number);
    for (size_t i = 0; digits[i] != '\0'; ++i) {
        const uint8_t keycode = digits[i] == '0' ? HID_KEY_0 :
                                (uint8_t)(HID_KEY_1 + digits[i] - '1');
        esp_err_t err = usb_hid_tap(0, keycode);
        if (err != ESP_OK) return err;
        if (profile->digit_delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(profile->digit_delay_ms));
        }
    }
    return usb_hid_tap(0, profile->goto_submit_key);
}

esp_err_t presenter_execute(const presenter_command_t *command)
{
    if (command == NULL || command->type < 0 ||
        command->type >= PRESENTER_COMMAND_TYPE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    presenter_profile_t profile;
    presenter_get_profile(&profile);
    if (command->type == PRESENTER_COMMAND_GOTO_SLIDE) {
        return execute_goto(&profile, command->slide_number);
    }
    const presenter_binding_t *binding = presenter_profile_binding_const(
        &profile, command->type);
    if (binding == NULL || !binding->enabled || binding->step_count == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    for (uint8_t i = 0; i < binding->step_count; ++i) {
        esp_err_t err = usb_hid_tap(binding->steps[i].modifier,
                                    binding->steps[i].keycode);
        if (err != ESP_OK) return err;
        if (binding->steps[i].delay_after_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(binding->steps[i].delay_after_ms));
        }
    }
    return ESP_OK;
}
