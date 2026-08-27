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

#define BUTTON_SAMPLE_MS       10U
#define BUTTON_DEBOUNCE_MS     40U
#define BUTTON_LONG_PRESS_MS   1000U
#define BUTTON_FACTORY_RESET_MS 8000U

static const char *TAG = "BUTTON";
static presenter_button_factory_reset_callback_t s_factory_reset_callback;

typedef struct {
    gpio_num_t gpio;
    presenter_command_type_t command;
    bool single_button_mode;
    bool sampled_pressed;
    bool stable_pressed;
    bool armed;
    uint32_t sampled_since_ms;
    uint32_t pressed_since_ms;
} button_state_t;

void presenter_button_set_factory_reset_callback(
    presenter_button_factory_reset_callback_t callback)
{
    s_factory_reset_callback = callback;
}

static void dispatch_release(button_state_t *button, uint32_t held_ms)
{
    if (button->command == PRESENTER_COMMAND_NEXT &&
        held_ms >= BUTTON_FACTORY_RESET_MS && s_factory_reset_callback != NULL) {
        ESP_LOGW(TAG, "factory reset requested gpio=%d press_ms=%" PRIu32,
                 button->gpio, held_ms);
        s_factory_reset_callback();
        return;
    }
    presenter_command_type_t type = button->command;
    if (button->single_button_mode && held_ms >= BUTTON_LONG_PRESS_MS) {
        type = PRESENTER_COMMAND_PREVIOUS;
    }
    uint32_t id = 0;
    const esp_err_t err = command_router_submit(type, 0, &id);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "gpio=%d press_ms=%" PRIu32 " command=%s id=%" PRIu32,
                 button->gpio, held_ms, presenter_command_type_name(type), id);
    } else {
        ESP_LOGW(TAG, "press rejected: %s", esp_err_to_name(err));
    }
}

static void sample_button(button_state_t *button, uint32_t now_ms)
{
    const bool pressed = gpio_get_level(button->gpio) == 0;
    if (pressed != button->sampled_pressed) {
        button->sampled_pressed = pressed;
        button->sampled_since_ms = now_ms;
    }
    if (button->sampled_pressed == button->stable_pressed ||
        (uint32_t)(now_ms - button->sampled_since_ms) < BUTTON_DEBOUNCE_MS) return;

    button->stable_pressed = button->sampled_pressed;
    if (button->stable_pressed) {
        if (button->armed) button->pressed_since_ms = now_ms;
        return;
    }
    if (!button->armed) {
        button->armed = true;
    } else if (button->pressed_since_ms != 0U) {
        dispatch_release(button, now_ms - button->pressed_since_ms);
        button->pressed_since_ms = 0U;
    }
}

static void button_task(void *argument)
{
    (void)argument;
    const bool dedicated_previous = CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO >= 0 &&
        CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO != CONFIG_SLIDELINK_BUTTON_NEXT_GPIO;
    button_state_t buttons[2] = {
        {
            .gpio = (gpio_num_t)CONFIG_SLIDELINK_BUTTON_NEXT_GPIO,
            .command = PRESENTER_COMMAND_NEXT,
            .single_button_mode = !dedicated_previous,
        },
        {
            .gpio = (gpio_num_t)CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO,
            .command = PRESENTER_COMMAND_PREVIOUS,
        },
    };
    const size_t count = dedicated_previous ? 2U : 1U;
    const uint32_t started_ms = (uint32_t)(esp_timer_get_time() / 1000);
    for (size_t i = 0; i < count; ++i) {
        buttons[i].sampled_pressed = gpio_get_level(buttons[i].gpio) == 0;
        buttons[i].stable_pressed = buttons[i].sampled_pressed;
        buttons[i].armed = !buttons[i].sampled_pressed;
        buttons[i].sampled_since_ms = started_ms;
    }

    while (true) {
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        for (size_t i = 0; i < count; ++i) sample_button(&buttons[i], now_ms);
        vTaskDelay(pdMS_TO_TICKS(BUTTON_SAMPLE_MS));
    }
}

esp_err_t presenter_button_init(void)
{
    uint64_t mask = BIT64(CONFIG_SLIDELINK_BUTTON_NEXT_GPIO);
#if CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO >= 0
    if (CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO != CONFIG_SLIDELINK_BUTTON_NEXT_GPIO) {
        mask |= BIT64(CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO);
    }
#endif
    const gpio_config_t config = {
        .pin_bit_mask = mask,
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
    ESP_LOGI(TAG, "input initialized next_gpio=%d previous_gpio=%d",
             CONFIG_SLIDELINK_BUTTON_NEXT_GPIO,
             CONFIG_SLIDELINK_BUTTON_PREVIOUS_GPIO);
    return ESP_OK;
}
