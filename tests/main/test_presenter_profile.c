/* SPDX-License-Identifier: MIT */

#include "class/hid/hid.h"
#include "presenter.h"
#include "unity.h"

TEST_CASE("factory profiles pass strict validation", "[profile]")
{
    for (uint32_t id = 1; id <= PRESENTER_PROFILE_COUNT; ++id) {
        presenter_profile_t profile;
        presenter_profile_factory(id, &profile);
        TEST_ASSERT_EQUAL_UINT32(id, profile.id);
        TEST_ASSERT_EQUAL(ESP_OK, presenter_profile_validate(&profile));
    }
}

TEST_CASE("profile rejects unsafe keys modifiers and excessive delays", "[profile]")
{
    presenter_profile_t profile;
    presenter_profile_factory(PRESENTER_PROFILE_CUSTOM_1, &profile);
    profile.next.steps[0].keycode = 0xFF;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, presenter_profile_validate(&profile));

    presenter_profile_factory(PRESENTER_PROFILE_CUSTOM_1, &profile);
    profile.next.steps[0].modifier = 0x08;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, presenter_profile_validate(&profile));

    presenter_profile_factory(PRESENTER_PROFILE_CUSTOM_1, &profile);
    profile.next.steps[0].delay_after_ms = 1001;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, presenter_profile_validate(&profile));
}

TEST_CASE("profile key names expose only the allowlist", "[profile]")
{
    uint8_t keycode = 0;
    TEST_ASSERT_EQUAL(ESP_OK, presenter_keycode_from_name("Page Down", &keycode));
    TEST_ASSERT_EQUAL_HEX8(HID_KEY_PAGE_DOWN, keycode);
    TEST_ASSERT_EQUAL_STRING("Page Down", presenter_keycode_name(keycode));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      presenter_keycode_from_name("Windows", &keycode));
}
