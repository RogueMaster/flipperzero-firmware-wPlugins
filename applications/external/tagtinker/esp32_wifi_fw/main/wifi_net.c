/*
 * WiFi station - implementation.
 */
#include "wifi_net.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <string.h>

static const char* TAG = "net";
static const char* NVS_NS = "tt_wifi";

static EventGroupHandle_t s_eg;
#define EV_CONNECTED (1U << 0)
#define EV_FAIL      (1U << 1)

static char s_ssid[33];
static char s_pwd[65];
static char s_ip[16];
static int8_t s_rssi = 0;
static uint8_t s_state = TT_WIFI_DISCONNECTED;

uint8_t wifi_net_state(void) {
    return s_state;
}
int8_t wifi_net_rssi(void) {
    return s_rssi;
}
const char* wifi_net_ssid(void) {
    return s_ssid;
}
const char* wifi_net_ip(void) {
    return s_ip;
}

static void load_creds_from_nvs(void) {
    nvs_handle_t h;
    if(nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t l = sizeof(s_ssid);
    if(nvs_get_str(h, "ssid", s_ssid, &l) != ESP_OK) s_ssid[0] = 0;
    l = sizeof(s_pwd);
    if(nvs_get_str(h, "pwd", s_pwd, &l) != ESP_OK) s_pwd[0] = 0;
    nvs_close(h);
}

static void save_creds_to_nvs(void) {
    nvs_handle_t h;
    if(nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid", s_ssid);
    nvs_set_str(h, "pwd", s_pwd);
    nvs_commit(h);
    nvs_close(h);
}

static void wifi_event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    (void)arg;
    if(base == WIFI_EVENT) {
        switch(id) {
        case WIFI_EVENT_STA_START:
            s_state = TT_WIFI_CONNECTING;
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t* d = data;
            ESP_LOGW(TAG, "disconnected reason=%d", d->reason);
            if(d->reason == WIFI_REASON_NO_AP_FOUND)
                s_state = TT_WIFI_NO_AP;
            else if(d->reason == WIFI_REASON_AUTH_FAIL || d->reason == WIFI_REASON_HANDSHAKE_TIMEOUT)
                s_state = TT_WIFI_AUTH_FAILED;
            else
                s_state = TT_WIFI_DISCONNECTED;
            xEventGroupSetBits(s_eg, EV_FAIL);
            esp_wifi_connect();
            break;
        }
        }
    } else if(base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* g = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&g->ip_info.ip));
        s_state = TT_WIFI_CONNECTED;
        xEventGroupSetBits(s_eg, EV_CONNECTED);
    }
}

static void try_connect(void) {
    if(!s_ssid[0]) return;
    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, s_ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char*)cfg.sta.password, s_pwd, sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable = true;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_disconnect();
    esp_wifi_connect();
}

void wifi_net_init(void) {
    s_eg = xEventGroupCreate();

    esp_err_t r = nvs_flash_init();
    if(r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    load_creds_from_nvs();
    esp_wifi_start();
    if(s_ssid[0]) try_connect();
}

void wifi_net_set_creds(const char* ssid, const char* pwd) {
    strncpy(s_ssid, ssid ? ssid : "", sizeof(s_ssid) - 1);
    strncpy(s_pwd, pwd ? pwd : "", sizeof(s_pwd) - 1);
    save_creds_to_nvs();
    try_connect();
}

void wifi_net_forget(void) {
    s_ssid[0] = 0;
    s_pwd[0] = 0;
    save_creds_to_nvs();
    esp_wifi_disconnect();
    s_state = TT_WIFI_DISCONNECTED;
}

bool wifi_net_wait_connected(uint32_t timeout_ms) {
    if(s_state == TT_WIFI_CONNECTED) return true;
    EventBits_t bits =
        xEventGroupWaitBits(s_eg, EV_CONNECTED, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & EV_CONNECTED) != 0;
}
