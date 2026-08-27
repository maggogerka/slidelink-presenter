/* SPDX-License-Identifier: MIT */

#include "usb_network.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "freertos/FreeRTOS.h"
#include "tinyusb_net.h"
#include "tusb.h"

#define USB_NETWORK_TX_TIMEOUT_MS 150U

typedef struct {
    esp_netif_driver_base_t base;
} usb_network_driver_t;

static const char *TAG = "USB_NET";
static usb_network_driver_t s_driver;
static esp_netif_t *s_netif;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static usb_network_state_t s_state;

static void count_received(void)
{
    portENTER_CRITICAL(&s_state_lock);
    ++s_state.received_packets;
    portEXIT_CRITICAL(&s_state_lock);
}

static void count_transmitted(void)
{
    portENTER_CRITICAL(&s_state_lock);
    ++s_state.transmitted_packets;
    portEXIT_CRITICAL(&s_state_lock);
}

static void count_dropped(void)
{
    portENTER_CRITICAL(&s_state_lock);
    ++s_state.dropped_packets;
    portEXIT_CRITICAL(&s_state_lock);
}

static esp_err_t usb_network_transmit(void *handle, void *buffer, size_t length)
{
    (void)handle;
    if (buffer == NULL || length == 0 || length > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = tinyusb_net_send_sync(buffer, (uint16_t)length, NULL,
                                          pdMS_TO_TICKS(USB_NETWORK_TX_TIMEOUT_MS));
    if (err == ESP_OK) {
        count_transmitted();
    } else {
        count_dropped();
    }
    return err;
}

static void usb_network_free_rx(void *handle, void *buffer)
{
    (void)handle;
    free(buffer);
}

static esp_err_t usb_network_post_attach(esp_netif_t *netif,
                                         esp_netif_iodriver_handle handle)
{
    usb_network_driver_t *driver = handle;
    driver->base.netif = netif;
    esp_netif_driver_ifconfig_t driver_config = {
        .handle = driver,
        .transmit = usb_network_transmit,
        .driver_free_rx_buffer = usb_network_free_rx,
    };
    return esp_netif_set_driver_config(netif, &driver_config);
}

static esp_err_t usb_network_receive(void *buffer, uint16_t length, void *context)
{
    (void)context;
    if (buffer == NULL || length == 0 || s_netif == NULL) {
        count_dropped();
        return ESP_ERR_INVALID_STATE;
    }

    /* TinyUSB owns its receive buffer and renews it immediately after this
     * callback, whereas esp-netif may retain an L2 buffer asynchronously. */
    void *copy = malloc(length);
    if (copy == NULL) {
        count_dropped();
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, buffer, length);
    esp_err_t err = esp_netif_receive(s_netif, copy, length, NULL);
    if (err == ESP_OK) {
        count_received();
    } else {
        /* ethernetif_input owns and releases the copied L2 buffer on every
         * path once esp_netif_receive() has been called. */
        count_dropped();
    }
    return err;
}

static void usb_network_class_ready(void *context)
{
    (void)context;
    tud_network_link_state(0, true);
    portENTER_CRITICAL(&s_state_lock);
    s_state.link_up = true;
    portEXIT_CRITICAL(&s_state_lock);
    ESP_LOGI(TAG, "NCM link ready at http://%s", USB_NETWORK_IPV4_ADDRESS);
}

esp_err_t usb_network_init(void)
{
    if (s_netif != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info = {0};
    ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(USB_NETWORK_IPV4_ADDRESS,
                                             &ip_info.ip),
                        TAG, "invalid USB IP");
    ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4("255.255.255.0",
                                             &ip_info.netmask),
                        TAG, "invalid USB netmask");
    ip_info.gw.addr = 0;

    uint8_t mac[6] = {0};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG,
                        "read base MAC");
    mac[0] |= 0x02U;
    mac[0] &= 0xFEU;
    mac[5] ^= 0x55U;

    esp_netif_inherent_config_t base_config = {
        .flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER |
                                     ESP_NETIF_FLAG_AUTOUP |
                                     ESP_NETIF_FLAG_GARP),
        .ip_info = &ip_info,
        .if_key = "SL_USB",
        .if_desc = "SlideLink USB NCM",
        /* Keep this isolated interface from becoming the ESP default route. */
        .route_prio = -10,
        .mtu = 1500,
    };
    memcpy(base_config.mac, mac, sizeof(mac));
    esp_netif_config_t netif_config = {
        .base = &base_config,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    s_driver.base.post_attach = usb_network_post_attach;
    s_netif = esp_netif_new(&netif_config);
    ESP_RETURN_ON_FALSE(s_netif != NULL, ESP_ERR_NO_MEM, TAG,
                        "create USB netif");
    ESP_RETURN_ON_ERROR(esp_netif_attach(s_netif, &s_driver), TAG,
                        "attach USB netif");
    ESP_RETURN_ON_ERROR(esp_netif_set_hostname(s_netif, "slidelink"), TAG,
                        "set USB hostname");

    /* Windows receives only an address and subnet. Deliberately omit router
     * and DNS options so SlideLink never captures the host's default route or
     * ordinary Internet DNS traffic. */
    uint8_t disabled = 0;
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_option(
                            s_netif, ESP_NETIF_OP_SET,
                            ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
                            &disabled, sizeof(disabled)),
                        TAG, "disable DHCP router offer");
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_option(
                            s_netif, ESP_NETIF_OP_SET,
                            ESP_NETIF_DOMAIN_NAME_SERVER,
                            &disabled, sizeof(disabled)),
                        TAG, "disable DHCP DNS offer");
    esp_netif_action_start(s_netif, NULL, 0, NULL);

    tinyusb_net_config_t tinyusb_config = {
        .on_recv_callback = usb_network_receive,
        .on_init_callback = usb_network_class_ready,
        .user_context = NULL,
    };
    memcpy(tinyusb_config.mac_addr, mac, sizeof(mac));
    ESP_RETURN_ON_ERROR(tinyusb_net_init(&tinyusb_config), TAG,
                        "initialize NCM class");

    portENTER_CRITICAL(&s_state_lock);
    s_state.initialized = true;
    portEXIT_CRITICAL(&s_state_lock);
    ESP_LOGI(TAG, "isolated subnet %s/24 (no gateway, no DNS)",
             USB_NETWORK_IPV4_ADDRESS);
    return ESP_OK;
}

void usb_network_get_state(usb_network_state_t *state)
{
    if (state == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_state_lock);
    *state = s_state;
    portEXIT_CRITICAL(&s_state_lock);
    state->link_up = state->initialized && tud_mounted();
}
