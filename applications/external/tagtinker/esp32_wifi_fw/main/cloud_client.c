/*
 * Cloud client - implementation.
 *
 * Uses esp_http_client. URL escapes parameters before they're appended to
 * the query string. The render path streams the response directly via
 * the HTTP_EVENT_ON_DATA callback to avoid buffering the (potentially
 * 50+ KB) framebuffer in memory.
 */
#include "cloud_client.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "cloud";
static const char* NVS_NS = "tt_cloud";

static char s_base_url[128] = TT_CLOUD_DEFAULT_URL;

const char* cloud_client_url(void) {
    return s_base_url;
}

void cloud_client_set_url(const char* url) {
    if(!url || !*url) return;
    strncpy(s_base_url, url, sizeof(s_base_url) - 1);
    s_base_url[sizeof(s_base_url) - 1] = 0;
    nvs_handle_t h;
    if(nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "url", s_base_url);
        nvs_commit(h);
        nvs_close(h);
    }
}

void cloud_client_load(void) {
    nvs_handle_t h;
    if(nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t l = sizeof(s_base_url);
    if(nvs_get_str(h, "url", s_base_url, &l) != ESP_OK) {
        strncpy(s_base_url, TT_CLOUD_DEFAULT_URL, sizeof(s_base_url) - 1);
    }
    nvs_close(h);
}

/* ---- Tiny URL builder --------------------------------------------------- */

static int hexv(uint8_t b) {
    return b < 10 ? '0' + b : 'a' + (b - 10);
}

static void url_append_escaped(char* dst, size_t cap, size_t* pos, const char* s) {
    while(*s && *pos + 4 < cap) {
        unsigned char c = (unsigned char)*s++;
        bool safe = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    c == '-' || c == '_' || c == '.';
        if(safe) {
            dst[(*pos)++] = (char)c;
        } else {
            dst[(*pos)++] = '%';
            dst[(*pos)++] = (char)hexv(c >> 4);
            dst[(*pos)++] = (char)hexv(c & 0xF);
        }
    }
    dst[*pos] = 0;
}

/* ---- /plugins ----------------------------------------------------------- */

typedef struct {
    char* buf;
    size_t cap;
    size_t len;
} BodyBuf;

static esp_err_t body_evt(esp_http_client_event_t* e) {
    BodyBuf* b = e->user_data;
    if(e->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    size_t take = e->data_len;
    if(b->len + take + 1 > b->cap) take = (b->cap > b->len + 1) ? (b->cap - b->len - 1) : 0;
    if(take == 0) return ESP_OK;
    memcpy(b->buf + b->len, e->data, take);
    b->len += take;
    b->buf[b->len] = 0;
    return ESP_OK;
}

char* cloud_client_fetch_plugins_json(size_t* out_len) {
    char url[256];
    snprintf(url, sizeof(url), "%s/plugins", s_base_url);

    BodyBuf b = {.buf = malloc(8192), .cap = 8192, .len = 0};
    if(!b.buf) return NULL;
    b.buf[0] = 0;

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = body_evt,
        .user_data = &b,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if(!c) {
        free(b.buf);
        return NULL;
    }
    esp_http_client_set_header(c, "User-Agent", "TagTinker-WiFi/2.0");
    esp_err_t r = esp_http_client_perform(c);
    int code = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);
    if(r != ESP_OK || code != 200) {
        ESP_LOGW(TAG, "plugins: err=%d code=%d", r, code);
        free(b.buf);
        return NULL;
    }
    if(out_len) *out_len = b.len;
    return b.buf;
}

/* ---- /render -- streaming -------------------------------------------- */

bool cloud_client_render(
    const char* plugin_id,
    uint16_t target_w,
    uint16_t target_h,
    uint8_t accent,
    const char* const* keys,
    const char* const* values,
    uint8_t n_params,
    uint16_t* out_w,
    uint16_t* out_h,
    uint8_t* out_planes,
    uint16_t* out_row_stride,
    tt_cloud_chunk_cb chunk_cb,
    void* user,
    char err_msg[64]) {
    /* Build URL: <base>/render/<id>?w=<>&h=<>&accent=<>&<k>=<v>... */
    char url[768];
    size_t pos = 0;
    int wrote = snprintf(url, sizeof(url), "%s/render/", s_base_url);
    if(wrote < 0 || (size_t)wrote >= sizeof(url)) {
        if(err_msg) snprintf(err_msg, 64, "url too long");
        return false;
    }
    pos = (size_t)wrote;
    url_append_escaped(url, sizeof(url), &pos, plugin_id);

    int wrote2 = snprintf(
        url + pos,
        sizeof(url) - pos,
        "?w=%u&h=%u&accent=%s",
        (unsigned)target_w,
        (unsigned)target_h,
        accent == 1 ? "red" :
        accent == 2 ? "yellow" :
                      "none");
    if(wrote2 < 0) {
        if(err_msg) snprintf(err_msg, 64, "url build failed");
        return false;
    }
    pos += (size_t)wrote2;

    for(uint8_t i = 0; i < n_params; i++) {
        if(pos + 4 >= sizeof(url)) break;
        url[pos++] = '&';
        url_append_escaped(url, sizeof(url), &pos, keys[i]);
        url[pos++] = '=';
        url_append_escaped(url, sizeof(url), &pos, values[i]);
    }
    url[pos] = 0;

    /* We use the streaming HTTP API so we can read the 8-byte header
     * first (and surface w/h/planes to the caller before forwarding any
     * payload), then loop reading body bytes as they arrive. This lets
     * the caller emit RESULT_BEGIN at exactly the right moment. */
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 25000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if(!c) {
        if(err_msg) snprintf(err_msg, 64, "client init");
        return false;
    }
    esp_http_client_set_header(c, "User-Agent", "TagTinker-WiFi/2.0");

    esp_err_t r = esp_http_client_open(c, 0);
    if(r != ESP_OK) {
        esp_http_client_cleanup(c);
        if(err_msg) snprintf(err_msg, 64, "open %d", r);
        return false;
    }
    int64_t total = esp_http_client_fetch_headers(c);
    int code = esp_http_client_get_status_code(c);
    if(code != 200) {
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        if(err_msg) snprintf(err_msg, 64, "http %d", code);
        return false;
    }
    (void)total;

    /* Read the 8-byte header. */
    uint8_t hdr[8];
    size_t got = 0;
    while(got < 8) {
        int n = esp_http_client_read(c, (char*)hdr + got, 8 - got);
        if(n <= 0) break;
        got += (size_t)n;
    }
    if(got < 8) {
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        if(err_msg) snprintf(err_msg, 64, "short header");
        return false;
    }
    uint16_t W = (uint16_t)hdr[0] | ((uint16_t)hdr[1] << 8);
    uint16_t H = (uint16_t)hdr[2] | ((uint16_t)hdr[3] << 8);
    uint8_t P = hdr[4];
    uint16_t RS = (uint16_t)hdr[6] | ((uint16_t)hdr[7] << 8);
    if(out_w) *out_w = W;
    if(out_h) *out_h = H;
    if(out_planes) *out_planes = P;
    if(out_row_stride) *out_row_stride = RS;

    /* Stream the rest in <= 512-byte chunks. */
    uint8_t chunk[512];
    while(true) {
        int n = esp_http_client_read(c, (char*)chunk, sizeof(chunk));
        if(n < 0) break;
        if(n == 0) {
            if(esp_http_client_is_complete_data_received(c)) break;
            continue;
        }
        if(chunk_cb) chunk_cb(chunk, (uint16_t)n, user);
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return true;
}
