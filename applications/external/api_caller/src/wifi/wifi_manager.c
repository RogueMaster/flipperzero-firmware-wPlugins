#include "wifi_manager.h"

#include "../api/flipper_http.h"

#define WIFI_MANAGER_POLL_MS            50
#define WIFI_MANAGER_SHORT_TIMEOUT_MS   3000
#define WIFI_MANAGER_SCAN_TIMEOUT_MS    15000
#define WIFI_MANAGER_CONNECT_TIMEOUT_MS 20000

/** Wait until last_response contains `needle` or timeout expires. */
static bool wifi_manager_wait_for(AppContext* app, const char* needle, uint32_t timeout_ms) {
    furi_assert(app);

    uint32_t elapsed = 0;
    while(elapsed < timeout_ms) {
        if(strstr(app->fhttp->last_response, needle) != NULL) {
            return true;
        }
        furi_delay_ms(WIFI_MANAGER_POLL_MS);
        elapsed += WIFI_MANAGER_POLL_MS;
    }
    return false;
}

/**
 * Clear the previous reply, send a command and wait for the first new line.
 * Stores the raw reply into `out` (may be empty on timeout).
 */
static bool
    wifi_manager_query(AppContext* app, HTTPCommand command, FuriString* out, uint32_t timeout_ms) {
    furi_assert(app);
    furi_assert(out);

    if(app->fhttp == NULL) {
        return false;
    }

    // Clear the previous reply so the wait below detects the new one
    memset(app->fhttp->last_response, 0, RX_BUF_SIZE);

    if(!flipper_http_send_command(app->fhttp, command)) {
        return false;
    }

    uint32_t elapsed = 0;
    while(elapsed < timeout_ms) {
        if(app->fhttp->last_response[0] != '\0') {
            furi_string_set_str(out, app->fhttp->last_response);
            return true;
        }
        furi_delay_ms(WIFI_MANAGER_POLL_MS);
        elapsed += WIFI_MANAGER_POLL_MS;
    }
    return false;
}

bool wifi_manager_init(AppContext* app) {
    furi_assert(app);

    app->fhttp = flipper_http_alloc();
    if(app->fhttp == NULL) {
        FURI_LOG_E("ApiCaller", "FlipperHTTP allocation failed (UART busy?)");
        return false;
    }
    return true;
}

void wifi_manager_deinit(AppContext* app) {
    furi_assert(app);

    if(app->fhttp != NULL) {
        flipper_http_free(app->fhttp);
        app->fhttp = NULL;
    }
}

bool wifi_manager_ping(AppContext* app) {
    furi_assert(app);

    if(app->fhttp == NULL) {
        return false;
    }
    if(!flipper_http_send_command(app->fhttp, HTTP_CMD_PING)) {
        return false;
    }
    return wifi_manager_wait_for(app, "[PONG]", WIFI_MANAGER_SHORT_TIMEOUT_MS);
}

bool wifi_manager_get_version(AppContext* app, FuriString* version) {
    FuriString* reply = furi_string_alloc();
    bool ok = wifi_manager_query(app, HTTP_CMD_VERSION, reply, WIFI_MANAGER_SHORT_TIMEOUT_MS);

    if(ok) {
        furi_string_set(version, reply);
    }
    furi_string_free(reply);
    return ok;
}

/** Parse the {"networks":[...]} JSON reply into a newline-separated list. */
static bool wifi_manager_parse_scan_reply(const char* json, FuriString* ssid_list) {
    FuriString* reply = furi_string_alloc();
    furi_string_set_str(reply, json);

    bool ok = false;

    size_t start = furi_string_search_str(reply, "\"networks\"", 0);
    if(start == FURI_STRING_FAILURE) {
        furi_string_free(reply);
        return false;
    }
    // Move after the opening '[' of the array
    const char* cursor = json + start + strlen("\"networks\"");
    while(*cursor != '\0' && *cursor != '[') {
        cursor++;
    }
    if(*cursor == '[') {
        cursor++;

        while(*cursor != '\0' && *cursor != ']') {
            // Each entry is a quoted string
            if(*cursor == '"') {
                cursor++;
                const char* end = cursor;
                while(*end != '\0' && *end != '"') {
                    end++;
                }
                if(end > cursor) {
                    // Append the entry (no cat_strn in this SDK)
                    const char* p = cursor;
                    while(p < end) {
                        furi_string_push_back(ssid_list, *p);
                        p++;
                    }
                    furi_string_push_back(ssid_list, '\n');
                    cursor = end;
                }
            }
            cursor++;
        }
        ok = true;
    }

    furi_string_free(reply);
    return ok;
}

void wifi_manager_scan_start(AppContext* app) {
    furi_assert(app);

    furi_string_reset(app->wifi_ssid_list);

    if(app->fhttp == NULL) {
        return;
    }

    // Clear the previous reply so the poll below detects the new one
    memset(app->fhttp->last_response, 0, RX_BUF_SIZE);

    flipper_http_send_command(app->fhttp, HTTP_CMD_SCAN);
    app->wifi_scan_start_tick = furi_get_tick();
}

bool wifi_manager_scan_poll(AppContext* app, FuriString* ssid_list) {
    furi_assert(app);
    furi_assert(ssid_list);

    if(app->fhttp == NULL) {
        furi_string_reset(ssid_list);
        return true;
    }

    // The board replies with several lines: [GET/SUCCESS], the JSON with the
    // network list, then [GET/END]. last_response keeps the last meaningful
    // line, so wait until the JSON line shows up.
    if(strstr(app->fhttp->last_response, "\"networks\"") != NULL) {
        return wifi_manager_parse_scan_reply(app->fhttp->last_response, ssid_list);
    }
    if(strstr(app->fhttp->last_response, "[ERROR]") != NULL) {
        furi_string_reset(ssid_list);
        return true;
    }
    if(furi_get_tick() - app->wifi_scan_start_tick >= WIFI_MANAGER_SCAN_TIMEOUT_MS) {
        FURI_LOG_W("ApiCaller", "WiFi scan timed out");
        furi_string_reset(ssid_list);
        return true;
    }
    return false;
}

bool wifi_manager_get_ssid(AppContext* app, char* ssid, size_t ssid_size) {
    FuriString* reply = furi_string_alloc();
    bool ok = false;

    ssid[0] = '\0';

    if(wifi_manager_query(app, HTTP_CMD_SSID, reply, WIFI_MANAGER_SHORT_TIMEOUT_MS)) {
        if(strstr(furi_string_get_cstr(reply), "[ERROR]") == NULL) {
            snprintf(ssid, ssid_size, "%s", furi_string_get_cstr(reply));
            ok = true;
        }
    }

    furi_string_free(reply);
    return ok;
}

bool wifi_manager_get_ip(AppContext* app, char* ip, size_t ip_size) {
    FuriString* reply = furi_string_alloc();
    bool ok = false;

    ip[0] = '\0';

    if(wifi_manager_query(app, HTTP_CMD_IP_ADDRESS, reply, WIFI_MANAGER_SHORT_TIMEOUT_MS)) {
        snprintf(ip, ip_size, "%s", furi_string_get_cstr(reply));
        ok = true;
    }

    furi_string_free(reply);
    return ok;
}

bool wifi_manager_is_connected(AppContext* app) {
    FuriString* reply = furi_string_alloc();
    bool connected = false;

    if(wifi_manager_query(app, HTTP_CMD_STATUS, reply, WIFI_MANAGER_SHORT_TIMEOUT_MS)) {
        connected = strstr(furi_string_get_cstr(reply), "true") != NULL;
    }

    furi_string_free(reply);
    return connected;
}

bool wifi_manager_save_and_connect(
    AppContext* app,
    const char* ssid,
    const char* password,
    FuriString* reply) {
    furi_assert(app);

    if(app->fhttp == NULL || ssid == NULL || password == NULL || ssid[0] == '\0') {
        return false;
    }

    memset(app->fhttp->last_response, 0, RX_BUF_SIZE);

    if(!flipper_http_save_wifi(app->fhttp, ssid, password)) {
        return false;
    }

    // The board replies with [SUCCESS] or [ERROR] after trying to connect
    bool success = wifi_manager_wait_for(app, "[SUCCESS]", WIFI_MANAGER_CONNECT_TIMEOUT_MS);
    bool error = strstr(app->fhttp->last_response, "[ERROR]") != NULL;

    if(reply != NULL) {
        furi_string_set_str(reply, app->fhttp->last_response);
    }
    return success && !error;
}

bool wifi_manager_disconnect(AppContext* app) {
    furi_assert(app);

    if(app->fhttp == NULL) {
        return false;
    }
    if(!flipper_http_send_command(app->fhttp, HTTP_CMD_WIFI_DISCONNECT)) {
        return false;
    }
    return wifi_manager_wait_for(app, "[DISCONNECTED]", WIFI_MANAGER_SHORT_TIMEOUT_MS);
}
