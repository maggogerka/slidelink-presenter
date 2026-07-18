/* SPDX-License-Identifier: MIT */

#include "web_server.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "command_router.h"
#include "device_state.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "presenter.h"
#include "profile_store.h"
#include "session_manager.h"

#define JSON_BODY_MAX 4096U
#define WS_BODY_MAX 512U

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t styles_css_start[] asm("_binary_styles_css_start");
extern const uint8_t styles_css_end[] asm("_binary_styles_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t manifest_json_start[] asm("_binary_manifest_json_start");
extern const uint8_t manifest_json_end[] asm("_binary_manifest_json_end");

typedef struct {
    const uint8_t *start;
    const uint8_t *end;
    const char *content_type;
} static_asset_t;

static const char *TAG = "WEB";
static httpd_handle_t s_server;

static const presenter_command_type_t s_binding_types[] = {
    PRESENTER_COMMAND_NEXT, PRESENTER_COMMAND_PREVIOUS, PRESENTER_COMMAND_START,
    PRESENTER_COMMAND_START_CURRENT, PRESENTER_COMMAND_STOP, PRESENTER_COMMAND_BLACK,
    PRESENTER_COMMAND_WHITE, PRESENTER_COMMAND_FIRST, PRESENTER_COMMAND_LAST,
};

static esp_err_t send_json(httpd_req_t *request, const char *status, cJSON *root)
{
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == NULL) return ESP_ERR_NO_MEM;
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(request, text, HTTPD_RESP_USE_STRLEN);
    cJSON_free(text);
    return err;
}

static esp_err_t send_error(httpd_req_t *request, const char *status,
                            const char *code, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *error = cJSON_AddObjectToObject(root, "error");
    cJSON_AddStringToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    return send_json(request, status, root);
}

static bool object_has_only(const cJSON *object, const char *const *allowed,
                            size_t allowed_count)
{
    const cJSON *child;
    cJSON_ArrayForEach(child, object) {
        bool known = false;
        for (size_t i = 0; i < allowed_count; ++i) {
            if (strcmp(child->string, allowed[i]) == 0) { known = true; break; }
        }
        if (!known) return false;
    }
    return true;
}

static cJSON *receive_json(httpd_req_t *request, size_t maximum)
{
    if (request->content_len <= 0 || (size_t)request->content_len > maximum) return NULL;
    char content_type[48] = {0};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", content_type,
                                    sizeof(content_type)) != ESP_OK ||
        strncmp(content_type, "application/json", 16) != 0) return NULL;
    char *buffer = calloc((size_t)request->content_len + 1U, 1);
    if (buffer == NULL) return NULL;
    size_t received = 0;
    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(request, buffer + received,
                                    request->content_len - received);
        if (result <= 0) { free(buffer); return NULL; }
        received += (size_t)result;
    }
    cJSON *json = cJSON_ParseWithLength(buffer, received);
    free(buffer);
    if (!cJSON_IsObject(json)) { cJSON_Delete(json); return NULL; }
    return json;
}

static bool request_authorized(httpd_req_t *request)
{
    char authorization[80];
    if (httpd_req_get_hdr_value_str(request, "Authorization", authorization,
                                    sizeof(authorization)) != ESP_OK ||
        strncmp(authorization, "Bearer ", 7) != 0) return false;
    return session_manager_validate(authorization + 7);
}

static bool require_authorized(httpd_req_t *request)
{
    if (request_authorized(request)) return true;
    (void)send_error(request, "401 Unauthorized", "unauthorized",
                     "A valid session token is required");
    return false;
}

static esp_err_t static_handler(httpd_req_t *request)
{
    const static_asset_t *asset = request->user_ctx;
    size_t length = (size_t)(asset->end - asset->start);
    if (length > 0U && asset->start[length - 1U] == '\0') {
        --length;
    }
    httpd_resp_set_type(request, asset->content_type);
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, (const char *)asset->start,
                           (ssize_t)length);
}

static esp_err_t system_handler(httpd_req_t *request)
{
    device_state_snapshot_t state;
    device_state_get(&state);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmware", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "device", state.device_name);
    cJSON_AddBoolToObject(root, "setup_required", state.setup_mode);
    cJSON_AddNumberToObject(root, "uptime_ms", state.uptime_ms);
    cJSON_AddNumberToObject(root, "free_heap", state.free_heap);
    cJSON_AddNumberToObject(root, "minimum_free_heap", state.minimum_free_heap);
    cJSON *usb = cJSON_AddObjectToObject(root, "usb");
    cJSON_AddBoolToObject(usb, "mounted", state.usb_mounted);
    cJSON_AddBoolToObject(usb, "ready", state.usb_ready);
    cJSON_AddBoolToObject(usb, "suspended", state.usb_suspended);
    cJSON_AddNumberToObject(usb, "session", state.usb_session);
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddNumberToObject(wifi, "clients", state.wifi_clients);
    cJSON_AddStringToObject(wifi, "ip", state.wifi_ip);
    cJSON *presenter = cJSON_AddObjectToObject(root, "presenter");
    cJSON_AddNumberToObject(presenter, "active_profile_id", state.active_profile_id);
    cJSON_AddNumberToObject(presenter, "active_profile_revision",
                            state.active_profile_revision);
    cJSON_AddStringToObject(presenter, "active_profile_name",
                            state.active_profile_name);
    cJSON_AddNumberToObject(presenter, "queue_depth", state.queue_depth);
    return send_json(request, "200 OK", root);
}

static void delayed_restart(void *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(750));
    esp_restart();
}

static esp_err_t setup_handler(httpd_req_t *request)
{
    if (session_manager_is_configured()) {
        return send_error(request, "409 Conflict", "already_configured",
                          "First-run setup is already complete");
    }
    cJSON *json = receive_json(request, 256);
    static const char *const allowed[] = {"wifi_password", "pin"};
    if (json == NULL || !object_has_only(json, allowed, 2)) {
        cJSON_Delete(json);
        return send_error(request, "400 Bad Request", "invalid_json",
                          "Expected only wifi_password and pin");
    }
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "wifi_password");
    const cJSON *pin = cJSON_GetObjectItemCaseSensitive(json, "pin");
    esp_err_t err = (cJSON_IsString(password) && cJSON_IsString(pin)) ?
                    session_manager_configure(password->valuestring, pin->valuestring) :
                    ESP_ERR_INVALID_ARG;
    cJSON_Delete(json);
    if (err != ESP_OK) {
        return send_error(request, "422 Unprocessable Entity", "invalid_setup",
                          "Password must be 8-63 characters and PIN 4-8 digits");
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "restart_required", true);
    esp_err_t response = send_json(request, "200 OK", root);
    (void)xTaskCreate(delayed_restart, "setup_restart", 2048, NULL, 3, NULL);
    return response;
}

static esp_err_t session_handler(httpd_req_t *request)
{
    cJSON *json = receive_json(request, 128);
    static const char *const allowed[] = {"pin"};
    if (json == NULL || !object_has_only(json, allowed, 1)) {
        cJSON_Delete(json);
        return send_error(request, "400 Bad Request", "invalid_json",
                          "Expected a PIN");
    }
    const cJSON *pin = cJSON_GetObjectItemCaseSensitive(json, "pin");
    char token[SESSION_TOKEN_LENGTH] = {0};
    esp_err_t err = cJSON_IsString(pin) ?
                    session_manager_login(pin->valuestring, token) :
                    ESP_ERR_INVALID_ARG;
    cJSON_Delete(json);
    if (err == ESP_ERR_TIMEOUT) {
        return send_error(request, "429 Too Many Requests", "login_throttled",
                          "Too many failed attempts; wait 30 seconds");
    }
    if (err != ESP_OK) {
        return send_error(request, "401 Unauthorized", "invalid_pin",
                          "The PIN is incorrect");
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "token", token);
    cJSON_AddNumberToObject(root, "expires_in_seconds", 1800);
    return send_json(request, "200 OK", root);
}

static bool parse_action(const char *action, presenter_command_type_t *type)
{
    if (action == NULL || type == NULL) return false;
    for (int value = 0; value < PRESENTER_COMMAND_TYPE_COUNT; ++value) {
        if (strcmp(action, presenter_command_type_name((presenter_command_type_t)value)) == 0) {
            *type = (presenter_command_type_t)value;
            return true;
        }
    }
    return false;
}

static const char *router_error_code(command_router_error_t error)
{
    switch (error) {
    case COMMAND_ERROR_USB_NOT_MOUNTED:
    case COMMAND_ERROR_USB_DISCONNECTED: return "usb_not_mounted";
    case COMMAND_ERROR_QUEUE_FULL: return "queue_full";
    case COMMAND_ERROR_ROUTER_BUSY: return "router_busy";
    case COMMAND_ERROR_ACTION_DISABLED: return "action_disabled";
    case COMMAND_ERROR_PROFILE_CHANGED: return "profile_changed";
    case COMMAND_ERROR_HID_TIMEOUT: return "hid_timeout";
    default: return "execution_failed";
    }
}

static esp_err_t command_response(httpd_req_t *request, presenter_command_type_t type,
                                  uint16_t slide, uint32_t request_id)
{
    uint32_t command_id = 0;
    esp_err_t err = command_router_submit_with_request(type, slide, request_id,
                                                        &command_id);
    if (err != ESP_OK) {
        command_router_stats_t stats;
        command_router_get_stats(&stats);
        return send_error(request, err == ESP_ERR_NO_MEM ? "429 Too Many Requests" :
                          "409 Conflict", router_error_code(stats.last_error),
                          command_router_error_name(stats.last_error));
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "command_accepted");
    cJSON_AddNumberToObject(root, "request_id", request_id);
    cJSON_AddNumberToObject(root, "command_id", command_id);
    cJSON_AddNumberToObject(root, "queue_depth", command_router_queue_depth());
    return send_json(request, "202 Accepted", root);
}

static esp_err_t commands_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    cJSON *json = receive_json(request, 256);
    static const char *const allowed[] = {"action", "slide", "request_id"};
    if (json == NULL || !object_has_only(json, allowed, 3)) {
        cJSON_Delete(json);
        return send_error(request, "400 Bad Request", "invalid_json",
                          "Unknown or malformed command fields");
    }
    const cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
    const cJSON *slide_json = cJSON_GetObjectItemCaseSensitive(json, "slide");
    const cJSON *request_json = cJSON_GetObjectItemCaseSensitive(json, "request_id");
    presenter_command_type_t type;
    uint16_t slide = 0;
    uint32_t request_id = cJSON_IsNumber(request_json) && request_json->valuedouble >= 0 ?
                          (uint32_t)request_json->valuedouble : 0;
    bool valid = cJSON_IsString(action) && parse_action(action->valuestring, &type);
    if (valid && type == PRESENTER_COMMAND_GOTO_SLIDE) {
        valid = cJSON_IsNumber(slide_json) && slide_json->valuedouble >= 1 &&
                slide_json->valuedouble <= 9999;
        if (valid) slide = (uint16_t)slide_json->valuedouble;
    } else if (slide_json != NULL) {
        valid = false;
    }
    cJSON_Delete(json);
    if (!valid) return send_error(request, "422 Unprocessable Entity",
                                  "invalid_command", "Invalid action or slide number");
    return command_response(request, type, slide, request_id);
}

static cJSON *binding_to_json(const presenter_binding_t *binding)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "enabled", binding->enabled);
    cJSON *steps = cJSON_AddArrayToObject(root, "steps");
    for (uint8_t i = 0; i < binding->step_count; ++i) {
        cJSON *step = cJSON_CreateObject();
        cJSON_AddStringToObject(step, "key", presenter_keycode_name(binding->steps[i].keycode));
        cJSON *modifiers = cJSON_AddArrayToObject(step, "modifiers");
        if ((binding->steps[i].modifier & 0x02U) != 0) cJSON_AddItemToArray(modifiers, cJSON_CreateString("Shift"));
        if ((binding->steps[i].modifier & 0x01U) != 0) cJSON_AddItemToArray(modifiers, cJSON_CreateString("Ctrl"));
        if ((binding->steps[i].modifier & 0x04U) != 0) cJSON_AddItemToArray(modifiers, cJSON_CreateString("Alt"));
        cJSON_AddNumberToObject(step, "delay_after_ms", binding->steps[i].delay_after_ms);
        cJSON_AddItemToArray(steps, step);
    }
    return root;
}

static cJSON *profile_to_json(const presenter_profile_t *profile, bool active)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", profile->id);
    cJSON_AddNumberToObject(root, "revision", profile->revision);
    cJSON_AddStringToObject(root, "name", profile->name);
    cJSON_AddBoolToObject(root, "active", active);
    cJSON *bindings = cJSON_AddObjectToObject(root, "bindings");
    for (size_t i = 0; i < sizeof(s_binding_types) / sizeof(s_binding_types[0]); ++i) {
        const presenter_command_type_t type = s_binding_types[i];
        cJSON_AddItemToObject(bindings, presenter_command_type_name(type),
                              binding_to_json(presenter_profile_binding_const(profile, type)));
    }
    cJSON *go = cJSON_AddObjectToObject(root, "goto");
    cJSON_AddBoolToObject(go, "enabled", profile->goto_enabled);
    cJSON_AddStringToObject(go, "submit_key",
                            presenter_keycode_name(profile->goto_submit_key));
    cJSON_AddNumberToObject(go, "digit_delay_ms", profile->digit_delay_ms);
    return root;
}

static esp_err_t profiles_get_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    presenter_profile_t profiles[PRESENTER_PROFILE_COUNT];
    size_t count = 0;
    uint32_t active = 0;
    if (profile_store_get_all(profiles, PRESENTER_PROFILE_COUNT, &count, &active) != ESP_OK) {
        return send_error(request, "500 Internal Server Error", "storage_error",
                          "Profiles could not be loaded");
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "active_profile_id", active);
    cJSON *array = cJSON_AddArrayToObject(root, "profiles");
    for (size_t i = 0; i < count; ++i) {
        cJSON_AddItemToArray(array, profile_to_json(&profiles[i], profiles[i].id == active));
    }
    return send_json(request, "200 OK", root);
}

static bool parse_modifiers(const cJSON *array, uint8_t *modifier)
{
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) > 3) return false;
    *modifier = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item)) return false;
        if (strcmp(item->valuestring, "Shift") == 0) *modifier |= 0x02U;
        else if (strcmp(item->valuestring, "Ctrl") == 0) *modifier |= 0x01U;
        else if (strcmp(item->valuestring, "Alt") == 0) *modifier |= 0x04U;
        else return false;
    }
    return true;
}

static bool parse_binding(const cJSON *json, presenter_binding_t *binding)
{
    static const char *const allowed[] = {"enabled", "steps"};
    if (!cJSON_IsObject(json) || !object_has_only(json, allowed, 2)) return false;
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    const cJSON *steps = cJSON_GetObjectItemCaseSensitive(json, "steps");
    if (!cJSON_IsBool(enabled) || !cJSON_IsArray(steps) ||
        cJSON_GetArraySize(steps) > PROFILE_MAX_STEPS) return false;
    memset(binding, 0, sizeof(*binding));
    binding->enabled = cJSON_IsTrue(enabled);
    binding->step_count = (uint8_t)cJSON_GetArraySize(steps);
    for (uint8_t index = 0; index < binding->step_count; ++index) {
        const cJSON *step = cJSON_GetArrayItem(steps, index);
        static const char *const step_allowed[] = {"key", "modifiers", "delay_after_ms"};
        if (!cJSON_IsObject(step) || !object_has_only(step, step_allowed, 3)) return false;
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(step, "key");
        const cJSON *modifiers = cJSON_GetObjectItemCaseSensitive(step, "modifiers");
        const cJSON *delay = cJSON_GetObjectItemCaseSensitive(step, "delay_after_ms");
        if (!cJSON_IsString(key) || !cJSON_IsNumber(delay) || delay->valuedouble < 0 ||
            delay->valuedouble > 1000 ||
            presenter_keycode_from_name(key->valuestring,
                                        &binding->steps[index].keycode) != ESP_OK ||
            !parse_modifiers(modifiers, &binding->steps[index].modifier)) return false;
        binding->steps[index].delay_after_ms = (uint16_t)delay->valuedouble;
    }
    return !binding->enabled || binding->step_count > 0;
}

static bool parse_profile_json(const cJSON *json, presenter_profile_t *profile)
{
    static const char *const allowed[] = {"name", "bindings", "goto"};
    if (!object_has_only(json, allowed, 3)) return false;
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON *bindings = cJSON_GetObjectItemCaseSensitive(json, "bindings");
    const cJSON *go = cJSON_GetObjectItemCaseSensitive(json, "goto");
    if (!cJSON_IsString(name) || strlen(name->valuestring) >= PROFILE_NAME_LENGTH ||
        !cJSON_IsObject(bindings) || !cJSON_IsObject(go)) return false;
    (void)snprintf(profile->name, sizeof(profile->name), "%s", name->valuestring);
    for (size_t i = 0; i < sizeof(s_binding_types) / sizeof(s_binding_types[0]); ++i) {
        const presenter_command_type_t type = s_binding_types[i];
        const cJSON *binding = cJSON_GetObjectItemCaseSensitive(
            bindings, presenter_command_type_name(type));
        if (!parse_binding(binding, presenter_profile_binding(profile, type))) return false;
    }
    static const char *const goto_allowed[] = {"enabled", "submit_key", "digit_delay_ms"};
    if (!object_has_only(go, goto_allowed, 3)) return false;
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(go, "enabled");
    const cJSON *submit = cJSON_GetObjectItemCaseSensitive(go, "submit_key");
    const cJSON *delay = cJSON_GetObjectItemCaseSensitive(go, "digit_delay_ms");
    if (!cJSON_IsBool(enabled) || !cJSON_IsString(submit) || !cJSON_IsNumber(delay) ||
        delay->valuedouble < 0 || delay->valuedouble > 1000 ||
        presenter_keycode_from_name(submit->valuestring,
                                    &profile->goto_submit_key) != ESP_OK) return false;
    profile->goto_enabled = cJSON_IsTrue(enabled);
    profile->digit_delay_ms = (uint16_t)delay->valuedouble;
    return presenter_profile_validate(profile) == ESP_OK;
}

static bool profile_path(const char *uri, uint32_t *id, const char **operation)
{
    const char *prefix = "/api/v1/profiles/";
    if (strncmp(uri, prefix, strlen(prefix)) != 0) return false;
    char *end = NULL;
    unsigned long value = strtoul(uri + strlen(prefix), &end, 10);
    if (value < 1 || value > PRESENTER_PROFILE_COUNT) return false;
    *id = (uint32_t)value;
    *operation = end;
    return true;
}

static bool pin_reauthorized(httpd_req_t *request)
{
    char pin[12];
    return httpd_req_get_hdr_value_str(request, "X-SlideLink-PIN", pin,
                                        sizeof(pin)) == ESP_OK &&
           session_manager_reauthorize_pin(pin) == ESP_OK;
}

static esp_err_t profile_put_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    if (!pin_reauthorized(request)) return send_error(request, "403 Forbidden",
        "pin_required", "Profile changes require the current PIN");
    uint32_t id;
    const char *operation;
    if (!profile_path(request->uri, &id, &operation) || *operation != '\0') {
        return send_error(request, "404 Not Found", "not_found", "Unknown profile");
    }
    presenter_profile_t profile;
    if (profile_store_get(id, &profile) != ESP_OK) return send_error(request,
        "404 Not Found", "not_found", "Unknown profile");
    cJSON *json = receive_json(request, JSON_BODY_MAX);
    bool valid = json != NULL && parse_profile_json(json, &profile);
    cJSON_Delete(json);
    if (!valid) return send_error(request, "422 Unprocessable Entity",
        "invalid_profile", "The profile violates the key or timing allowlist");
    esp_err_t err = command_router_begin_profile_update();
    const bool update_started = err == ESP_OK;
    if (update_started) err = profile_store_update(id, &profile, &profile);
    presenter_profile_t active = {0};
    if (err == ESP_OK && profile_store_get_active(&active) == ESP_OK && active.id == id) {
        err = presenter_set_profile(&profile);
    }
    if (update_started) command_router_end_profile_update();
    if (err != ESP_OK) return send_error(request, "500 Internal Server Error",
        "storage_error", "The profile could not be saved");
    return send_json(request, "200 OK", profile_to_json(&profile, active.id == id));
}

static esp_err_t profile_post_handler(httpd_req_t *request)
{
    if (!require_authorized(request)) return ESP_OK;
    uint32_t id;
    const char *operation;
    if (!profile_path(request->uri, &id, &operation)) return send_error(request,
        "404 Not Found", "not_found", "Unknown profile operation");
    const bool test = strcmp(operation, "/test") == 0;
    const bool reset = strcmp(operation, "/reset") == 0;
    const bool activate = strcmp(operation, "/activate") == 0;
    if (!reset && !activate && !test) return send_error(request, "404 Not Found",
        "not_found", "Unknown profile operation");
    if ((reset || test) && !pin_reauthorized(request)) return send_error(request,
        "403 Forbidden", "pin_required", "This operation requires the current PIN");
    cJSON *body = receive_json(request, test ? 1024 : 32);
    if (body == NULL) return send_error(request, "400 Bad Request", "invalid_json",
        "A JSON object is required");
    if (test) {
        static const char *const allowed[] = {"action", "binding"};
        const cJSON *action = cJSON_GetObjectItemCaseSensitive(body, "action");
        const cJSON *binding_json = cJSON_GetObjectItemCaseSensitive(body, "binding");
        presenter_command_type_t command_type;
        presenter_binding_t binding;
        const bool valid = object_has_only(body, allowed, 2) && cJSON_IsString(action) &&
            parse_action(action->valuestring, &command_type) &&
            command_type != PRESENTER_COMMAND_GOTO_SLIDE &&
            parse_binding(binding_json, &binding);
        cJSON_Delete(body);
        if (!valid) return send_error(request, "422 Unprocessable Entity",
            "invalid_binding", "The temporary binding is invalid");
        esp_err_t test_err = command_router_begin_profile_update();
        const bool update_started = test_err == ESP_OK;
        if (update_started) test_err = presenter_execute_binding(&binding);
        if (update_started) command_router_end_profile_update();
        if (test_err != ESP_OK) return send_error(request, "409 Conflict",
            test_err == ESP_ERR_INVALID_STATE ? "usb_not_mounted" : "test_failed",
            "The temporary binding could not be executed");
        cJSON *result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "status", "executed");
        cJSON_AddStringToObject(result, "action",
                                presenter_command_type_name(command_type));
        return send_json(request, "200 OK", result);
    }
    const bool empty_body = body->child == NULL;
    cJSON_Delete(body);
    if (!empty_body) return send_error(request, "400 Bad Request", "unknown_field",
        "This operation accepts an empty JSON object");
    presenter_profile_t profile;
    esp_err_t err = command_router_begin_profile_update();
    const bool update_started = err == ESP_OK;
    if (update_started) err = reset ? profile_store_reset(id, &profile) :
                                    profile_store_activate(id, &profile);
    presenter_profile_t current = {0};
    const bool is_active = activate ||
        (profile_store_get_active(&current) == ESP_OK && current.id == id);
    if (err == ESP_OK && is_active) {
        err = presenter_set_profile(&profile);
    }
    if (update_started) command_router_end_profile_update();
    if (err != ESP_OK) return send_error(request, "500 Internal Server Error",
        "profile_error", "The profile operation failed");
    return send_json(request, "200 OK", profile_to_json(&profile, is_active));
}

static void broadcast_text(const char *text)
{
    if (s_server == NULL || text == NULL) return;
    int clients[8];
    size_t count = sizeof(clients) / sizeof(clients[0]);
    if (httpd_get_client_list(s_server, &count, clients) != ESP_OK) return;
    httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT,
                              .payload = (uint8_t *)text,
                              .len = strlen(text)};
    for (size_t i = 0; i < count; ++i) {
        if (httpd_ws_get_fd_info(s_server, clients[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            (void)httpd_ws_send_frame_async(s_server, clients[i], &frame);
        }
    }
}

static void router_result_callback(const command_router_result_t *result, void *context)
{
    (void)context;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "command_result");
    cJSON_AddNumberToObject(root, "request_id", result->request_id);
    cJSON_AddNumberToObject(root, "command_id", result->command_id);
    cJSON_AddStringToObject(root, "status", result->status == COMMAND_RESULT_EXECUTED ?
                            "executed" : "failed");
    cJSON_AddNumberToObject(root, "duration_ms", result->duration_ms);
    if (result->error != COMMAND_ERROR_NONE) {
        cJSON *error = cJSON_AddObjectToObject(root, "error");
        cJSON_AddStringToObject(error, "code", router_error_code(result->error));
        cJSON_AddStringToObject(error, "message",
                                command_router_error_name(result->error));
    }
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text != NULL) { broadcast_text(text); cJSON_free(text); }
}

static esp_err_t ws_handler(httpd_req_t *request)
{
    if (request->method == HTTP_GET) return ESP_OK;
    httpd_ws_frame_t frame = {0};
    frame.type = HTTPD_WS_TYPE_TEXT;
    if (httpd_ws_recv_frame(request, &frame, 0) != ESP_OK || frame.len > WS_BODY_MAX) {
        return ESP_FAIL;
    }
    uint8_t *payload = calloc(frame.len + 1U, 1);
    if (payload == NULL) return ESP_ERR_NO_MEM;
    frame.payload = payload;
    esp_err_t err = httpd_ws_recv_frame(request, &frame, frame.len);
    cJSON *json = err == ESP_OK ? cJSON_ParseWithLength((char *)payload, frame.len) : NULL;
    free(payload);
    if (!cJSON_IsObject(json)) { cJSON_Delete(json); return ESP_FAIL; }
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(json, "token");
    const cJSON *type_json = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsString(token) || !session_manager_validate(token->valuestring) ||
        !cJSON_IsString(type_json)) { cJSON_Delete(json); return ESP_FAIL; }
    if (strcmp(type_json->valuestring, "execute_command") == 0) {
        const cJSON *request_json = cJSON_GetObjectItemCaseSensitive(json, "request_id");
        const cJSON *action = cJSON_GetObjectItemCaseSensitive(json, "action");
        const cJSON *slide_json = cJSON_GetObjectItemCaseSensitive(json, "slide");
        presenter_command_type_t command_type;
        uint16_t slide = 0;
        uint32_t request_id = cJSON_IsNumber(request_json) ?
                              (uint32_t)request_json->valuedouble : 0;
        bool valid = cJSON_IsString(action) &&
                     parse_action(action->valuestring, &command_type);
        if (valid && command_type == PRESENTER_COMMAND_GOTO_SLIDE) {
            valid = cJSON_IsNumber(slide_json) && slide_json->valuedouble >= 1 &&
                    slide_json->valuedouble <= 9999;
            if (valid) slide = (uint16_t)slide_json->valuedouble;
        }
        cJSON *response = cJSON_CreateObject();
        uint32_t command_id = 0;
        esp_err_t submit = valid ? command_router_submit_with_request(
            command_type, slide, request_id, &command_id) : ESP_ERR_INVALID_ARG;
        if (submit == ESP_OK) {
            cJSON_AddStringToObject(response, "type", "command_accepted");
            cJSON_AddNumberToObject(response, "request_id", request_id);
            cJSON_AddNumberToObject(response, "command_id", command_id);
            cJSON_AddNumberToObject(response, "queue_depth", command_router_queue_depth());
        } else {
            command_router_stats_t stats;
            command_router_get_stats(&stats);
            cJSON_AddStringToObject(response, "type", "command_result");
            cJSON_AddNumberToObject(response, "request_id", request_id);
            cJSON_AddStringToObject(response, "status", "rejected");
            cJSON *error = cJSON_AddObjectToObject(response, "error");
            cJSON_AddStringToObject(error, "code", valid ? router_error_code(stats.last_error) :
                                    "invalid_command");
            cJSON_AddStringToObject(error, "message", valid ?
                                    command_router_error_name(stats.last_error) :
                                    "Invalid action or slide number");
        }
        char *text = cJSON_PrintUnformatted(response);
        cJSON_Delete(response);
        if (text != NULL) {
            httpd_ws_frame_t outgoing = {.type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)text, .len = strlen(text)};
            err = httpd_ws_send_frame(request, &outgoing);
            cJSON_free(text);
        }
    }
    cJSON_Delete(json);
    return err;
}

esp_err_t web_server_init(void)
{
    if (s_server != NULL) return ESP_ERR_INVALID_STATE;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) return err;
    static const static_asset_t index_asset = {index_html_start, index_html_end, "text/html"};
    static const static_asset_t css_asset = {styles_css_start, styles_css_end, "text/css"};
    static const static_asset_t js_asset = {app_js_start, app_js_end, "application/javascript"};
    static const static_asset_t manifest_asset = {manifest_json_start, manifest_json_end,
                                                   "application/manifest+json"};
    const httpd_uri_t handlers[] = {
        {.uri="/", .method=HTTP_GET, .handler=static_handler, .user_ctx=(void *)&index_asset},
        {.uri="/styles.css", .method=HTTP_GET, .handler=static_handler, .user_ctx=(void *)&css_asset},
        {.uri="/app.js", .method=HTTP_GET, .handler=static_handler, .user_ctx=(void *)&js_asset},
        {.uri="/manifest.json", .method=HTTP_GET, .handler=static_handler, .user_ctx=(void *)&manifest_asset},
        {.uri="/api/v1/system", .method=HTTP_GET, .handler=system_handler},
        {.uri="/api/v1/setup", .method=HTTP_POST, .handler=setup_handler},
        {.uri="/api/v1/session", .method=HTTP_POST, .handler=session_handler},
        {.uri="/api/v1/commands", .method=HTTP_POST, .handler=commands_handler},
        {.uri="/api/v1/profiles", .method=HTTP_GET, .handler=profiles_get_handler},
        {.uri="/api/v1/profiles/*", .method=HTTP_PUT, .handler=profile_put_handler},
        {.uri="/api/v1/profiles/*", .method=HTTP_POST, .handler=profile_post_handler},
        {.uri="/ws", .method=HTTP_GET, .handler=ws_handler, .is_websocket=true},
    };
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); ++i) {
        err = httpd_register_uri_handler(s_server, &handlers[i]);
        if (err != ESP_OK) { httpd_stop(s_server); s_server = NULL; return err; }
    }
    command_router_set_result_callback(router_result_callback, NULL);
    ESP_LOGI(TAG, "HTTP/WebSocket server ready on port 80");
    return ESP_OK;
}
