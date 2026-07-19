/* SPDX-License-Identifier: MIT */

#include "device_state.h"

#include <string.h>

#include "command_router.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "presenter.h"
#include "usb_hid.h"
#include "wifi_manager.h"

void device_state_get(device_state_snapshot_t *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    usb_hid_state_t usb;
    usb_hid_get_state(&usb);
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
    (void)memcpy(state->device_name, wifi.device_name, sizeof(state->device_name));
    (void)memcpy(state->wifi_ip, wifi.ip, sizeof(state->wifi_ip));
    state->wifi_clients = wifi.clients;
    state->setup_mode = wifi.setup_mode;
    state->active_profile_id = profile.id;
    state->active_profile_revision = profile.revision;
    (void)memcpy(state->active_profile_name, profile.name,
                 sizeof(state->active_profile_name));
    state->queue_depth = command_router_queue_depth();
}
