/*
 * SPDX-License-Identifier: MIT
 */

#include "presenter.h"

#include "class/hid/hid.h"
#include "usb_hid.h"

typedef struct {
    const char *name;
    presenter_key_t key;
} command_mapping_t;

static const command_mapping_t s_mappings[PRESENTER_COMMAND_TYPE_COUNT] = {
    [PRESENTER_COMMAND_NEXT] = {"next", {0, HID_KEY_ARROW_RIGHT}},
    [PRESENTER_COMMAND_PREVIOUS] = {"previous", {0, HID_KEY_ARROW_LEFT}},
    [PRESENTER_COMMAND_START] = {"start", {0, HID_KEY_F5}},
    [PRESENTER_COMMAND_START_CURRENT] = {"start-current", {KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_F5}},
    [PRESENTER_COMMAND_STOP] = {"stop", {0, HID_KEY_ESCAPE}},
    [PRESENTER_COMMAND_BLACK] = {"black", {0, HID_KEY_B}},
    [PRESENTER_COMMAND_WHITE] = {"white", {0, HID_KEY_W}},
    [PRESENTER_COMMAND_FIRST] = {"first", {0, HID_KEY_HOME}},
    [PRESENTER_COMMAND_LAST] = {"last", {0, HID_KEY_END}},
    [PRESENTER_COMMAND_GOTO_SLIDE] = {"goto_slide", {0, 0}},
};

const char *presenter_command_type_name(presenter_command_type_t type)
{
    return (type >= 0 && type < PRESENTER_COMMAND_TYPE_COUNT) ?
           s_mappings[type].name : "unknown";
}

esp_err_t presenter_command_key(presenter_command_type_t type, presenter_key_t *key)
{
    if (key == NULL || type < 0 || type >= PRESENTER_COMMAND_TYPE_COUNT ||
        type == PRESENTER_COMMAND_GOTO_SLIDE) {
        return ESP_ERR_INVALID_ARG;
    }
    *key = s_mappings[type].key;
    return ESP_OK;
}

esp_err_t presenter_execute(const presenter_command_t *command)
{
    if (command == NULL || command->type < 0 ||
        command->type >= PRESENTER_COMMAND_TYPE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (command->type == PRESENTER_COMMAND_GOTO_SLIDE) {
        if (command->slide_number < 1 || command->slide_number > 9999) {
            return ESP_ERR_INVALID_ARG;
        }
        esp_err_t err = usb_hid_type_digits(command->slide_number);
        if (err != ESP_OK) {
            return err;
        }
        return usb_hid_tap(0, HID_KEY_ENTER);
    }

    presenter_key_t key;
    esp_err_t err = presenter_command_key(command->type, &key);
    return (err == ESP_OK) ? usb_hid_tap(key.modifier, key.keycode) : err;
}
