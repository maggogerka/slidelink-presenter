/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "presenter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COMMAND_ROUTER_QUEUE_LENGTH 8U
#define COMMAND_ROUTER_ERROR_LENGTH 48U

typedef struct {
    uint32_t commands_received;
    uint32_t commands_accepted;
    uint32_t commands_executed;
    uint32_t commands_rejected;
    uint32_t commands_failed;
    uint32_t queue_overflows;
    uint32_t usb_disconnects;
    uint32_t last_command_id;
    char last_error[COMMAND_ROUTER_ERROR_LENGTH];
} command_router_stats_t;

esp_err_t command_router_init(void);
esp_err_t command_router_submit(presenter_command_type_t type,
                                uint16_t slide_number,
                                uint32_t *command_id);
void command_router_record_rejected(const char *reason);
void command_router_get_stats(command_router_stats_t *stats);
uint32_t command_router_queue_depth(void);

#ifdef __cplusplus
}
#endif
