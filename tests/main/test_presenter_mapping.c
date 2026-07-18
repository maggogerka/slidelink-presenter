/*
 * SPDX-License-Identifier: MIT
 */

#include "class/hid/hid.h"
#include "presenter.h"
#include "unity.h"

typedef struct {
    presenter_command_type_t type;
    uint8_t modifier;
    uint8_t keycode;
} expected_mapping_t;

TEST_CASE("every named presenter command maps to the expected HID key", "[mapping]")
{
    static const expected_mapping_t expected[] = {
        {PRESENTER_COMMAND_NEXT, 0, HID_KEY_ARROW_RIGHT},
        {PRESENTER_COMMAND_PREVIOUS, 0, HID_KEY_ARROW_LEFT},
        {PRESENTER_COMMAND_START, 0, HID_KEY_F5},
        {PRESENTER_COMMAND_START_CURRENT, KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_F5},
        {PRESENTER_COMMAND_STOP, 0, HID_KEY_ESCAPE},
        {PRESENTER_COMMAND_BLACK, 0, HID_KEY_B},
        {PRESENTER_COMMAND_WHITE, 0, HID_KEY_W},
        {PRESENTER_COMMAND_FIRST, 0, HID_KEY_HOME},
        {PRESENTER_COMMAND_LAST, 0, HID_KEY_END},
    };

    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        presenter_key_t actual = {0};
        TEST_ASSERT_EQUAL(ESP_OK, presenter_command_key(expected[i].type, &actual));
        TEST_ASSERT_EQUAL_HEX8(expected[i].modifier, actual.modifier);
        TEST_ASSERT_EQUAL_HEX8(expected[i].keycode, actual.keycode);
    }
}

TEST_CASE("goto is a sequence and has no single-key mapping", "[mapping]")
{
    presenter_key_t key = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      presenter_command_key(PRESENTER_COMMAND_GOTO_SLIDE, &key));
}
