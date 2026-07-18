/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "presenter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COMMAND_ROUTER_QUEUE_LENGTH 8U

typedef enum {
    COMMAND_ERROR_NONE,
    COMMAND_ERROR_INVALID_COMMAND,
    COMMAND_ERROR_USB_NOT_MOUNTED,
    COMMAND_ERROR_USB_DISCONNECTED,
    COMMAND_ERROR_QUEUE_FULL,
    COMMAND_ERROR_HID_TIMEOUT,
    COMMAND_ERROR_ROUTER_BUSY,
    COMMAND_ERROR_COMMAND_TOO_LONG,
    COMMAND_ERROR_INVALID_SLIDE_NUMBER,
    COMMAND_ERROR_UNSUPPORTED_COMMAND,
    COMMAND_ERROR_ACTION_DISABLED,
    COMMAND_ERROR_PROFILE_CHANGED,
    COMMAND_ERROR_EXECUTION_FAILED,
} command_router_error_t;

typedef enum {
    COMMAND_RESULT_EXECUTED,
    COMMAND_RESULT_FAILED,
} command_router_result_status_t;

typedef struct {
    uint32_t request_id;
    uint32_t command_id;
    command_router_result_status_t status;
    command_router_error_t error;
    uint32_t duration_ms;
} command_router_result_t;

typedef void (*command_router_result_callback_t)(
    const command_router_result_t *result, void *context);

typedef struct {
    uint32_t commands_received;
    uint32_t commands_accepted;
    uint32_t commands_executed;
    uint32_t commands_rejected;
    uint32_t commands_failed;
    uint32_t queue_overflows;
    uint32_t usb_disconnects;
    uint32_t last_command_id;
    command_router_error_t last_error;
} command_router_stats_t;

esp_err_t command_router_init(void);
esp_err_t command_router_submit(presenter_command_type_t type,
                                uint16_t slide_number,
                                uint32_t *command_id);
esp_err_t command_router_submit_with_request(presenter_command_type_t type,
                                             uint16_t slide_number,
                                             uint32_t request_id,
                                             uint32_t *command_id);
void command_router_set_result_callback(command_router_result_callback_t callback,
                                        void *context);
esp_err_t command_router_begin_profile_update(void);
void command_router_end_profile_update(void);
void command_router_record_rejected(command_router_error_t error);
const char *command_router_error_name(command_router_error_t error);
void command_router_get_stats(command_router_stats_t *stats);
uint32_t command_router_queue_depth(void);
bool command_router_is_accepting(void);

#ifdef __cplusplus
}
#endif
