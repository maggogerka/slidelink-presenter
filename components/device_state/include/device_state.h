/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t uptime_ms;
    uint32_t free_heap;
    uint32_t minimum_free_heap;
    bool usb_mounted;
    bool usb_ready;
    bool usb_suspended;
    uint32_t usb_session;
    bool usb_network_initialized;
    bool usb_network_link_up;
    char usb_network_ip[16];
    uint32_t usb_network_received_packets;
    uint32_t usb_network_transmitted_packets;
    uint32_t usb_network_dropped_packets;
    char device_name[24];
    char wifi_ip[16];
    char wifi_ssid[33];
    uint8_t wifi_clients;
    bool setup_mode;
    uint32_t active_profile_id;
    uint32_t active_profile_revision;
    char active_profile_name[24];
    uint32_t queue_depth;
} device_state_snapshot_t;

void device_state_get(device_state_snapshot_t *state);
