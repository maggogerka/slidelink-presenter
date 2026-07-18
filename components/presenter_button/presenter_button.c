/*
 * SPDX-License-Identifier: MIT
 */

#include "presenter_button.h"

#include <inttypes.h>
#include <stdbool.h>

#include "command_router.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_BUTTON_GPIO       GPIO_NUM_0
#define BUTTON_SAMPLE_MS       10U
#define BUTTON_DEBOUNCE_MS     40U
#define BUTTON_LONG_PRESS_MS   1000U

static const char *TAG = "BUTTON";

static void button_task(void *argument)
{
    (void)argument;
    bool sampled_pressed = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
    bool armed = !sampled_pressed;
    bool stable_pressed = sampled_pressed;
    uint32_t sampled_since_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t pressed_since_ms = 0;

    while (true) {
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const bool pressed = gpio_get_level(BOOT_BUTTON_GPIO) == 0;
        if (pressed != sampled_pressed) {
            sampled_pressed = pressed;
            sampled_since_ms = now_ms;
        }

        if (sampled_pressed != stable_pressed &&
            (uint32_t)(now_ms - sampled_since_ms) >= BUTTON_DEBOUNCE_MS) {
            stable_pressed = sampled_pressed;
            if (!stable_pressed) {
                if (!armed) {
                    armed = true;
                } else if (pressed_since_ms != 0) {
                    const uint32_t held_ms = now_ms - pressed_since_ms;
                    const presenter_command_type_t type =
                        held_ms >= BUTTON_LONG_PRESS_MS ?
                        PRESENTER_COMMAND_PREVIOUS : PRESENTER_COMMAND_NEXT;
                    uint32_t id = 0;
                    const esp_err_t err = command_router_submit(type, 0, &id);
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "press_ms=%" PRIu32 " command=%s id=%" PRIu32,
                                 held_ms, presenter_command_type_name(type), id);
                    } else {
                        ESP_LOGW(TAG, "press rejected: %s", esp_err_to_name(err));
                    }
                    pressed_since_ms = 0;
                }
            } else if (armed) {
                pressed_since_ms = now_ms;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BUTTON_SAMPLE_MS));
    }
}

esp_err_t presenter_button_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    if (xTaskCreate(button_task, "presenter_button", 3072, NULL, 3, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "BOOT button initialized short=next long=previous");
    return ESP_OK;
}
