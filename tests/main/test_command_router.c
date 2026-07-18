/*
 * SPDX-License-Identifier: MIT
 */

#include "command_router.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "usb_hid_test.h"

TEST_CASE("router generates IDs and rejects queue overflow", "[router]")
{
    usb_hid_test_set_mounted(true);
    usb_hid_test_set_tap_delay_ms(200);
    TEST_ASSERT_EQUAL(ESP_OK, command_router_init());

    uint32_t previous_id = 0;
    uint32_t accepted = 0;
    bool overflow_seen = false;
    for (uint32_t i = 0; i < COMMAND_ROUTER_QUEUE_LENGTH + 4U; ++i) {
        uint32_t id = 0;
        const esp_err_t err = command_router_submit(PRESENTER_COMMAND_NEXT, 0, &id);
        if (err == ESP_OK) {
            TEST_ASSERT_EQUAL_UINT32(previous_id + 1U, id);
            previous_id = id;
            ++accepted;
        } else {
            TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, err);
            overflow_seen = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(overflow_seen);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(COMMAND_ROUTER_QUEUE_LENGTH, accepted);
    command_router_stats_t stats;
    command_router_get_stats(&stats);
    TEST_ASSERT_EQUAL_UINT32(accepted, stats.commands_accepted);
    TEST_ASSERT_EQUAL_UINT32(1, stats.queue_overflows);
    TEST_ASSERT_EQUAL_UINT32(previous_id, stats.last_command_id);
    TEST_ASSERT_EQUAL(COMMAND_ERROR_QUEUE_FULL, stats.last_error);
    TEST_ASSERT_EQUAL_STRING("command queue full",
                             command_router_error_name(stats.last_error));
}
