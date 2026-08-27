/* SPDX-License-Identifier: MIT */

#include "device_state.h"

#include <stdio.h>
#include <string.h>

#include "command_router.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "presenter.h"
#include "usb_hid.h"
#include "usb_network.h"
#include "wifi_manager.h"

void device_state_get(device_state_snapshot_t *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    usb_hid_state_t usb;
    usb_hid_get_state(&usb);
    usb_network_state_t usb_network;
    usb_network_get_state(&usb_network);
    wifi_manager_state_t wifi;
    wifi_manager_get_state(&wifi);
    presenter_profile_t profile;
    presenter_get_profile(&profile);
    state->uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
    state->free_heap = esp_get_free_heap_size();
    state->minimum_free_heap = esp_get_minimum_free_heap_size();
    state->usb_mounted = usb.mounted;
    state->usb_ready = usb_hid_is_ready();
    state->usb_suspended = usb.suspended;
    state->usb_session = usb.session;
    state->usb_network_initialized = usb_network.initialized;
    state->usb_network_link_up = usb_network.link_up;
    (void)snprintf(state->usb_network_ip, sizeof(state->usb_network_ip), "%s",
                   USB_NETWORK_IPV4_ADDRESS);
    state->usb_network_received_packets = usb_network.received_packets;
    state->usb_network_transmitted_packets = usb_network.transmitted_packets;
    state->usb_network_dropped_packets = usb_network.dropped_packets;
    (void)memcpy(state->device_name, wifi.device_name, sizeof(state->device_name));
    (void)memcpy(state->wifi_ip, wifi.ip, sizeof(state->wifi_ip));
    (void)memcpy(state->wifi_ssid, wifi.ssid, sizeof(state->wifi_ssid));
    state->wifi_clients = wifi.clients;
    state->setup_mode = wifi.setup_mode;
    state->active_profile_id = profile.id;
    state->active_profile_revision = profile.revision;
    (void)memcpy(state->active_profile_name, profile.name,
                 sizeof(state->active_profile_name));
    state->queue_depth = command_router_queue_depth();
}
