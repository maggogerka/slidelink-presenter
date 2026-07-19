/* SPDX-License-Identifier: MIT */

#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "session_manager.h"

static const char *TAG = "WIFI";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static wifi_manager_state_t s_state;

static void wifi_event_handler(void *argument, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)argument;
    (void)base;
    (void)event_data;
    portENTER_CRITICAL(&s_lock);
    if (event_id == WIFI_EVENT_AP_STACONNECTED && s_state.clients < 2) {
        ++s_state.clients;
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED && s_state.clients > 0) {
        --s_state.clients;
    }
    const uint8_t clients = s_state.clients;
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "clients=%u", clients);
}

esp_err_t wifi_manager_init(void)
{
    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG,
                        "read Wi-Fi MAC");
    const bool setup = !session_manager_is_configured();
    (void)snprintf(s_state.device_name, sizeof(s_state.device_name),
                   "SlideLink-%02X%02X", mac[4], mac[5]);
    (void)snprintf(s_state.ssid, sizeof(s_state.ssid), "%s%s",
                   s_state.device_name, setup ? "-Setup" : "");
    (void)snprintf(s_state.ip, sizeof(s_state.ip), "192.168.4.1");
    s_state.setup_mode = setup;

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (esp_netif_create_default_wifi_ap() == NULL) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                    wifi_event_handler, NULL),
                        TAG, "event handler");
    char password[WIFI_PASSWORD_MAX_LENGTH];
    ESP_RETURN_ON_ERROR(session_manager_get_wifi_password(password, sizeof(password)),
                        TAG, "read password");
    wifi_config_t config = {0};
    const size_t ssid_length = strlen(s_state.ssid);
    const size_t password_length = strlen(password);
    memcpy(config.ap.ssid, s_state.ssid, ssid_length);
    memcpy(config.ap.password, password, password_length);
    config.ap.ssid_len = (uint8_t)ssid_length;
    config.ap.channel = 6;
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.pmf_cfg.required = true;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "set AP mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "set AP config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start AP");

    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mDNS init");
    ESP_RETURN_ON_ERROR(mdns_hostname_set("slidelink"), TAG, "mDNS hostname");
    ESP_RETURN_ON_ERROR(mdns_instance_name_set("SlideLink Presenter"), TAG,
                        "mDNS instance");
    ESP_RETURN_ON_ERROR(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0),
                        TAG, "mDNS HTTP service");
    ESP_LOGI(TAG, "SoftAP ssid='%s' ip=%s max_clients=2 setup=%s", s_state.ssid,
             s_state.ip, setup ? "yes" : "no");
    return ESP_OK;
}

void wifi_manager_get_state(wifi_manager_state_t *state)
{
    if (state == NULL) return;
    portENTER_CRITICAL(&s_lock);
    *state = s_state;
    portEXIT_CRITICAL(&s_lock);
}
