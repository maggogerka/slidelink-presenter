/*
 * SPDX-License-Identifier: MIT
 */

#include "command_router.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "usb_hid.h"

typedef struct {
    presenter_command_t command;
    uint32_t usb_session;
} queued_command_t;

static const char *TAG = "COMMAND";
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_submit_mutex;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;
static command_router_stats_t s_stats;
static uint32_t s_next_id = 1;

static void set_last_error_locked(command_router_error_t error)
{
    s_stats.last_error = error;
}

static void record_rejected_internal(command_router_error_t error)
{
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.commands_rejected;
    set_last_error_locked(error);
    portEXIT_CRITICAL(&s_stats_lock);
}

const char *command_router_error_name(command_router_error_t error)
{
    switch (error) {
    case COMMAND_ERROR_NONE: return "none";
    case COMMAND_ERROR_INVALID_COMMAND: return "invalid command";
    case COMMAND_ERROR_USB_NOT_MOUNTED: return "usb not mounted";
    case COMMAND_ERROR_USB_DISCONNECTED: return "usb disconnected";
    case COMMAND_ERROR_QUEUE_FULL: return "command queue full";
    case COMMAND_ERROR_HID_TIMEOUT: return "hid endpoint timeout";
    case COMMAND_ERROR_ROUTER_BUSY: return "router busy";
    case COMMAND_ERROR_COMMAND_TOO_LONG: return "command too long";
    case COMMAND_ERROR_INVALID_SLIDE_NUMBER: return "invalid slide number";
    case COMMAND_ERROR_UNSUPPORTED_COMMAND: return "unsupported command";
    case COMMAND_ERROR_EXECUTION_FAILED: return "execution failed";
    default: return "unknown";
    }
}

static void discard_stale_commands(uint32_t stale_session,
                                   command_router_error_t error)
{
    if (xSemaphoreTake(s_submit_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    queued_command_t retained[COMMAND_ROUTER_QUEUE_LENGTH];
    size_t retained_count = 0;
    uint32_t discarded = 0;
    queued_command_t queued;
    usb_hid_state_t usb_state;
    usb_hid_get_state(&usb_state);
    while (xQueueReceive(s_queue, &queued, 0) == pdTRUE) {
        if (queued.usb_session == stale_session || !usb_state.mounted) {
            ++discarded;
        } else {
            retained[retained_count++] = queued;
        }
    }
    for (size_t i = 0; i < retained_count; ++i) {
        (void)xQueueSend(s_queue, &retained[i], 0);
    }
    xSemaphoreGive(s_submit_mutex);

    if (discarded > 0) {
        portENTER_CRITICAL(&s_stats_lock);
        s_stats.commands_failed += discarded;
        set_last_error_locked(error);
        portEXIT_CRITICAL(&s_stats_lock);
        ESP_LOGW(TAG, "discarded %u queued command(s): %s",
                 (unsigned)discarded, command_router_error_name(error));
    }
}

static void command_worker(void *argument)
{
    (void)argument;
    queued_command_t item;

    while (true) {
        if (xQueueReceive(s_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const int64_t started_us = esp_timer_get_time();
        esp_err_t err;
        usb_hid_state_t usb_state;
        usb_hid_get_state(&usb_state);
        if (!usb_state.mounted || item.usb_session != usb_state.session) {
            err = ESP_ERR_INVALID_STATE;
        } else {
            err = presenter_execute(&item.command);
        }

        const uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - started_us) / 1000);
        if (err == ESP_OK) {
            portENTER_CRITICAL(&s_stats_lock);
            ++s_stats.commands_executed;
            set_last_error_locked(COMMAND_ERROR_NONE);
            portEXIT_CRITICAL(&s_stats_lock);
            ESP_LOGI("USB_HID", "executed id=%" PRIu32 " duration_ms=%" PRIu32,
                     item.command.id, duration_ms);
        } else {
            const command_router_error_t error =
                (err == ESP_ERR_TIMEOUT) ? COMMAND_ERROR_HID_TIMEOUT :
                (err == ESP_ERR_INVALID_STATE) ? COMMAND_ERROR_USB_DISCONNECTED :
                COMMAND_ERROR_EXECUTION_FAILED;
            usb_hid_release_all();
            portENTER_CRITICAL(&s_stats_lock);
            ++s_stats.commands_failed;
            set_last_error_locked(error);
            portEXIT_CRITICAL(&s_stats_lock);
            ESP_LOGE("USB_HID", "failed id=%" PRIu32 " error=%s duration_ms=%" PRIu32,
                     item.command.id, command_router_error_name(error), duration_ms);
            usb_hid_get_state(&usb_state);
            if (!usb_state.mounted || item.usb_session != usb_state.session) {
                discard_stale_commands(item.usb_session,
                                       COMMAND_ERROR_USB_DISCONNECTED);
            }
        }
    }
}

esp_err_t command_router_init(void)
{
    if (s_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_queue = xQueueCreate(COMMAND_ROUTER_QUEUE_LENGTH, sizeof(queued_command_t));
    s_submit_mutex = xSemaphoreCreateMutex();
    if (s_queue == NULL || s_submit_mutex == NULL) {
        if (s_queue != NULL) {
            vQueueDelete(s_queue);
            s_queue = NULL;
        }
        if (s_submit_mutex != NULL) {
            vSemaphoreDelete(s_submit_mutex);
            s_submit_mutex = NULL;
        }
        return ESP_ERR_NO_MEM;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    set_last_error_locked(COMMAND_ERROR_NONE);
    if (xTaskCreate(command_worker, "presenter_cmd", 4096, NULL, 5, NULL) != pdPASS) {
        vQueueDelete(s_queue);
        vSemaphoreDelete(s_submit_mutex);
        s_queue = NULL;
        s_submit_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "router initialized queue_length=%u", COMMAND_ROUTER_QUEUE_LENGTH);
    return ESP_OK;
}

esp_err_t command_router_submit(presenter_command_type_t type,
                                uint16_t slide_number,
                                uint32_t *command_id)
{
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.commands_received;
    portEXIT_CRITICAL(&s_stats_lock);

    if (s_queue == NULL || type < 0 || type >= PRESENTER_COMMAND_TYPE_COUNT ||
        (type == PRESENTER_COMMAND_GOTO_SLIDE &&
         (slide_number < 1 || slide_number > 9999))) {
        record_rejected_internal(COMMAND_ERROR_INVALID_COMMAND);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_submit_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        record_rejected_internal(COMMAND_ERROR_ROUTER_BUSY);
        return ESP_ERR_TIMEOUT;
    }
    usb_hid_state_t usb_state;
    usb_hid_get_state(&usb_state);
    if (!usb_state.mounted) {
        xSemaphoreGive(s_submit_mutex);
        record_rejected_internal(COMMAND_ERROR_USB_NOT_MOUNTED);
        return ESP_ERR_INVALID_STATE;
    }

    if (uxQueueSpacesAvailable(s_queue) == 0) {
        xSemaphoreGive(s_submit_mutex);
        portENTER_CRITICAL(&s_stats_lock);
        ++s_stats.commands_rejected;
        ++s_stats.queue_overflows;
        set_last_error_locked(COMMAND_ERROR_QUEUE_FULL);
        portEXIT_CRITICAL(&s_stats_lock);
        return ESP_ERR_NO_MEM;
    }

    queued_command_t item = {
        .command = {
            .type = type,
            .slide_number = slide_number,
            .created_at_ms = (uint32_t)(esp_timer_get_time() / 1000),
        },
        .usb_session = usb_state.session,
    };

    portENTER_CRITICAL(&s_stats_lock);
    item.command.id = s_next_id++;
    if (s_next_id == 0) {
        s_next_id = 1;
    }
    portEXIT_CRITICAL(&s_stats_lock);

    if (xQueueSend(s_queue, &item, 0) != pdTRUE) {
        xSemaphoreGive(s_submit_mutex);
        portENTER_CRITICAL(&s_stats_lock);
        ++s_stats.commands_rejected;
        ++s_stats.queue_overflows;
        set_last_error_locked(COMMAND_ERROR_QUEUE_FULL);
        portEXIT_CRITICAL(&s_stats_lock);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_submit_mutex);

    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.commands_accepted;
    s_stats.last_command_id = item.command.id;
    set_last_error_locked(COMMAND_ERROR_NONE);
    portEXIT_CRITICAL(&s_stats_lock);
    if (command_id != NULL) {
        *command_id = item.command.id;
    }
    ESP_LOGI(TAG, "accepted id=%" PRIu32 " type=%s slide=%u",
             item.command.id, presenter_command_type_name(type),
             (unsigned)slide_number);
    return ESP_OK;
}

void command_router_record_rejected(command_router_error_t error)
{
    portENTER_CRITICAL(&s_stats_lock);
    ++s_stats.commands_received;
    ++s_stats.commands_rejected;
    set_last_error_locked(error);
    portEXIT_CRITICAL(&s_stats_lock);
}

void command_router_get_stats(command_router_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_stats_lock);
    *stats = s_stats;
    portEXIT_CRITICAL(&s_stats_lock);
    stats->usb_disconnects = usb_hid_disconnect_count();
}

uint32_t command_router_queue_depth(void)
{
    return s_queue != NULL ? (uint32_t)uxQueueMessagesWaiting(s_queue) : 0;
}
