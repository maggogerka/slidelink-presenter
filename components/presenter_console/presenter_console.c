/*
 * SPDX-License-Identifier: MIT
 */

#include "presenter_console.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "command_router.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "usb_hid.h"

#if CONFIG_ESP_CONSOLE_NONE

esp_err_t presenter_console_init(void)
{
    return ESP_OK;
}

#else

static const char *TAG = "CONSOLE";

static const char *firmware_version(void)
{
    return esp_app_get_description()->version;
}

static void print_help(void)
{
    printf("Commands:\n"
           "  next | previous | start | start-current | stop\n"
           "  black | white | first | last | goto <1..9999>\n"
           "  status | help\n");
}

static void print_status(void)
{
    command_router_stats_t stats;
    command_router_get_stats(&stats);
    printf("firmware: %s\n"
           "usb: %s\n"
           "hid: %s\n"
           "queue_depth: %" PRIu32 "\n"
           "commands_received: %" PRIu32 "\n"
           "commands_accepted: %" PRIu32 "\n"
           "commands_executed: %" PRIu32 "\n"
           "commands_rejected: %" PRIu32 "\n"
           "commands_failed: %" PRIu32 "\n"
           "queue_overflows: %" PRIu32 "\n"
           "usb_disconnects: %" PRIu32 "\n"
           "last_command_id: %" PRIu32 "\n"
           "last_error: %s\n"
           "uptime_ms: %" PRIu32 "\n",
           firmware_version(),
           usb_hid_is_mounted() ? "mounted" : "not-mounted",
           usb_hid_is_ready() ? "ready" : "not-ready",
           command_router_queue_depth(), stats.commands_received,
           stats.commands_accepted, stats.commands_executed,
           stats.commands_rejected, stats.commands_failed,
           stats.queue_overflows, stats.usb_disconnects,
           stats.last_command_id, command_router_error_name(stats.last_error),
           (uint32_t)(esp_timer_get_time() / 1000));
}

static void process_line(const char *line)
{
    const presenter_console_parsed_t parsed = presenter_console_parse(line);
    if (parsed.result == CONSOLE_PARSE_EMPTY) {
        return;
    }
    if (parsed.result == CONSOLE_PARSE_STATUS) {
        print_status();
        return;
    }
    if (parsed.result == CONSOLE_PARSE_HELP) {
        print_help();
        return;
    }
    if (parsed.result == CONSOLE_PARSE_ERROR_TOO_LONG) {
        command_router_record_rejected(COMMAND_ERROR_COMMAND_TOO_LONG);
        printf("ERR command too long; maximum 64 characters\n");
        return;
    }
    if (parsed.result == CONSOLE_PARSE_ERROR_SLIDE_NUMBER) {
        command_router_record_rejected(COMMAND_ERROR_INVALID_SLIDE_NUMBER);
        printf("ERR invalid slide number; expected 1..9999\n");
        return;
    }
    if (parsed.result == CONSOLE_PARSE_ERROR_UNSUPPORTED) {
        command_router_record_rejected(COMMAND_ERROR_UNSUPPORTED_COMMAND);
        printf("ERR unsupported command\n");
        return;
    }

    uint32_t id = 0;
    const esp_err_t err = command_router_submit(parsed.command_type,
                                                 parsed.slide_number, &id);
    if (err == ESP_OK) {
        if (parsed.command_type == PRESENTER_COMMAND_GOTO_SLIDE) {
            printf("OK id=%" PRIu32 " queued slide=%u\n", id,
                   (unsigned)parsed.slide_number);
        } else {
            printf("OK id=%" PRIu32 " queued\n", id);
        }
    } else if (err == ESP_ERR_INVALID_STATE) {
        printf("ERR usb not mounted\n");
    } else if (err == ESP_ERR_NO_MEM) {
        printf("ERR command queue full\n");
    } else {
        printf("ERR command rejected: %s\n", esp_err_to_name(err));
    }
}

static void console_task(void *argument)
{
    (void)argument;
    char line[PRESENTER_CONSOLE_MAX_LINE_LENGTH + 1U];
    size_t length = 0;
    bool overflow = false;

    printf("\nSlideLink v%s\nUSB state: %s\nProfile: PowerPoint\n\n> ",
           firmware_version(), usb_hid_is_mounted() ? "mounted" : "not mounted");
    fflush(stdout);

    while (true) {
        const int input = fgetc(stdin);
        if (input < 0) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        const char ch = (char)input;
        if (ch == '\r' || ch == '\n') {
            putchar('\n');
            line[length] = '\0';
            if (overflow) {
                process_line("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
            } else {
                process_line(line);
            }
            length = 0;
            overflow = false;
            printf("\n> ");
            fflush(stdout);
        } else if (ch == '\b' || ch == 0x7f) {
            if (length > 0 && !overflow) {
                --length;
                printf("\b \b");
                fflush(stdout);
            }
        } else if ((unsigned char)ch >= 0x20 && (unsigned char)ch <= 0x7e) {
            putchar(ch);
            fflush(stdout);
            if (length < PRESENTER_CONSOLE_MAX_LINE_LENGTH) {
                line[length++] = ch;
            } else {
                overflow = true;
            }
        }
    }
}

esp_err_t presenter_console_init(void)
{
    const uart_port_t port = (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM;
    setvbuf(stdin, NULL, _IONBF, 0);
    if (!uart_is_driver_installed(port)) {
        esp_err_t err = uart_driver_install(port, 256, 0, 0, NULL, 0);
        if (err != ESP_OK) {
            return err;
        }
    }
    uart_vfs_dev_use_driver(port);
    uart_vfs_dev_port_set_rx_line_endings(port, ESP_LINE_ENDINGS_CRLF);
    uart_vfs_dev_port_set_tx_line_endings(port, ESP_LINE_ENDINGS_CRLF);

    if (xTaskCreate(console_task, "presenter_console", 4096, NULL, 3, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "UART console initialized port=%d baud=%d", (int)port,
             CONFIG_ESP_CONSOLE_UART_BAUDRATE);
    return ESP_OK;
}

#endif
