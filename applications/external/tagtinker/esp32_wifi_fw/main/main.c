/*
 * TagTinker WiFi firmware - thin cloud-renderer.
 *
 * The ESP no longer carries plugins, fonts, drawing primitives or HTTP
 * APIs for individual data sources. Instead it acts as a small bridge:
 *
 *   Flipper LIST_PLUGINS  -> GET <cloud>/plugins        -> forward N x PLUGIN
 *   Flipper RUN_PLUGIN    -> GET <cloud>/render/<id>?...-> forward as
 *                            RESULT_BEGIN + N x CHUNK + RESULT_END.
 *
 * Plugins live in the Cloudflare Worker at the URL printed during
 * `wrangler deploy`; updating plugins doesn't require re-flashing.
 *
 * A minimal cJSON-based parser converts the worker's /plugins JSON into
 * the framed-protocol PLUGIN frames the FAP already understands, so the
 * Flipper-side code is unchanged.
 */
#include "tt_wifi_proto.h"
#include "wifi_link.h"
#include "wifi_net.h"
#include "cloud_client.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cJSON.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "main";

/* ---- Encoding helpers --------------------------------------------------- */
typedef struct {
    uint8_t* p;
    uint16_t cap;
    uint16_t len;
} Wb;
static void wb_init(Wb* b, uint8_t* buf, uint16_t cap) {
    b->p = buf;
    b->cap = cap;
    b->len = 0;
}
static void wb_u8(Wb* b, uint8_t v) {
    if(b->len < b->cap) b->p[b->len++] = v;
}
static void wb_u16(Wb* b, uint16_t v) {
    wb_u8(b, v & 0xFF);
    wb_u8(b, v >> 8);
}
static void wb_u32(Wb* b, uint32_t v) {
    wb_u16(b, v);
    wb_u16(b, v >> 16);
}
static void wb_i32(Wb* b, int32_t v) {
    wb_u32(b, (uint32_t)v);
}
static void wb_zstr(Wb* b, const char* s) {
    if(!s) s = "";
    size_t l = strlen(s);
    if(l > 255) l = 255;
    wb_u8(b, (uint8_t)l);
    for(size_t i = 0; i < l && b->len < b->cap; i++)
        b->p[b->len++] = (uint8_t)s[i];
}

/* ---- Decoding helpers --------------------------------------------------- */
typedef struct {
    const uint8_t* p;
    uint16_t len;
    uint16_t pos;
} Rb;
static void rb_init(Rb* r, const uint8_t* p, uint16_t len) {
    r->p = p;
    r->len = len;
    r->pos = 0;
}
static bool rb_u8(Rb* r, uint8_t* v) {
    if(r->pos + 1 > r->len) return false;
    *v = r->p[r->pos++];
    return true;
}
static bool rb_u16(Rb* r, uint16_t* v) {
    uint8_t a, b;
    if(!rb_u8(r, &a) || !rb_u8(r, &b)) return false;
    *v = (uint16_t)a | ((uint16_t)b << 8);
    return true;
}
static bool rb_zstr(Rb* r, char* out, size_t cap) {
    uint8_t l;
    if(!rb_u8(r, &l)) return false;
    if(r->pos + l > r->len) return false;
    size_t n = (l < cap - 1) ? l : cap - 1;
    memcpy(out, r->p + r->pos, n);
    out[n] = 0;
    r->pos += l;
    return true;
}

/* ---- HELLO / STATUS ---------------------------------------------------- */
static void send_hello(void) {
    uint8_t buf[80];
    Wb w;
    wb_init(&w, buf, sizeof(buf));
    wb_u16(&w, 0x0200); /* fw version 2.0 (cloud) */
    wb_u32(&w, esp_get_free_heap_size());
    wb_zstr(&w, "TagTinker WiFi");
    wifi_link_send(TT_FRAME_HELLO, buf, w.len);
}

static void send_wifi_status(void) {
    uint8_t buf[80];
    Wb w;
    wb_init(&w, buf, sizeof(buf));
    wb_u8(&w, wifi_net_state());
    wb_u8(&w, (uint8_t)(int8_t)wifi_net_rssi());
    wb_zstr(&w, wifi_net_ssid());
    wb_zstr(&w, wifi_net_ip());
    wifi_link_send(TT_FRAME_WIFI_STATUS, buf, w.len);
}

/* ---- Cached plugins JSON --------------------------------------------- */
/* Filled by handle_list_plugins() and reused by handle_run_plugin() so we
 * don't have to do a fresh TLS handshake just to translate plugin index
 * back into id - critical on the S2 where we can barely fit one TLS
 * session at a time. */
static cJSON* s_cached_root = NULL;
static const cJSON* s_cached_arr = NULL;

static void cached_set(cJSON* root) {
    if(s_cached_root) {
        cJSON_Delete(s_cached_root);
    }
    s_cached_root = root;
    s_cached_arr = root ? cJSON_GetObjectItemCaseSensitive(root, "plugins") : NULL;
}

/* ---- /plugins JSON -> PLUGIN frames ----------------------------------- */

/* type tags from the worker -> wire type IDs the FAP expects. */
static uint8_t param_type_from(const char* t) {
    if(!t) return 0;
    if(strcmp(t, "string") == 0) return 0;
    if(strcmp(t, "int") == 0) return 1;
    if(strcmp(t, "enum") == 0) return 2;
    if(strcmp(t, "bool") == 0) return 3;
    return 0;
}

static void emit_plugin_from_json(int idx, const cJSON* p) {
    uint8_t buf[TT_FRAME_MAX_PAYLOAD];
    Wb w;
    wb_init(&w, buf, sizeof(buf));
    wb_u8(&w, (uint8_t)idx);

    const cJSON* id = cJSON_GetObjectItemCaseSensitive(p, "id");
    const cJSON* name = cJSON_GetObjectItemCaseSensitive(p, "name");
    const cJSON* desc = cJSON_GetObjectItemCaseSensitive(p, "description");
    const cJSON* acc = cJSON_GetObjectItemCaseSensitive(p, "accent_modes");
    const cJSON* params = cJSON_GetObjectItemCaseSensitive(p, "params");

    wb_zstr(&w, cJSON_IsString(id) ? id->valuestring : "");
    wb_zstr(&w, cJSON_IsString(name) ? name->valuestring : "");
    wb_zstr(&w, cJSON_IsString(desc) ? desc->valuestring : "");
    wb_u8(&w, (uint8_t)(cJSON_IsNumber(acc) ? acc->valueint : 1));

    int pc = cJSON_IsArray(params) ? cJSON_GetArraySize(params) : 0;
    if(pc > 6) pc = 6;
    wb_u8(&w, (uint8_t)pc);

    for(int i = 0; i < pc; i++) {
        const cJSON* sp = cJSON_GetArrayItem(params, i);
        const cJSON* k = cJSON_GetObjectItemCaseSensitive(sp, "key");
        const cJSON* l = cJSON_GetObjectItemCaseSensitive(sp, "label");
        const cJSON* tt = cJSON_GetObjectItemCaseSensitive(sp, "type");
        const cJSON* dv = cJSON_GetObjectItemCaseSensitive(sp, "default");
        const cJSON* opts = cJSON_GetObjectItemCaseSensitive(sp, "options");
        const cJSON* mn = cJSON_GetObjectItemCaseSensitive(sp, "min");
        const cJSON* mx = cJSON_GetObjectItemCaseSensitive(sp, "max");

        wb_zstr(&w, cJSON_IsString(k) ? k->valuestring : "");
        wb_zstr(&w, cJSON_IsString(l) ? l->valuestring : "");
        uint8_t pt = param_type_from(cJSON_IsString(tt) ? tt->valuestring : "string");
        wb_u8(&w, pt);
        wb_zstr(&w, cJSON_IsString(dv) ? dv->valuestring : "");
        if(pt == 2) {
            int oc = cJSON_IsArray(opts) ? cJSON_GetArraySize(opts) : 0;
            if(oc > 8) oc = 8;
            wb_u8(&w, (uint8_t)oc);
            for(int j = 0; j < oc; j++) {
                const cJSON* o = cJSON_GetArrayItem(opts, j);
                wb_zstr(&w, cJSON_IsString(o) ? o->valuestring : "");
            }
        } else if(pt == 1) {
            wb_i32(&w, cJSON_IsNumber(mn) ? mn->valueint : 0);
            wb_i32(&w, cJSON_IsNumber(mx) ? mx->valueint : 100);
        }
    }
    wifi_link_send(TT_FRAME_PLUGIN, buf, w.len);
}

static void handle_list_plugins(void) {
    if(!wifi_net_wait_connected(8000)) {
        wifi_link_send_error("WiFi not connected");
        wifi_link_send(TT_FRAME_PLUGINS_END, NULL, 0);
        return;
    }
    size_t n = 0;
    char* body = cloud_client_fetch_plugins_json(&n);
    if(!body) {
        wifi_link_send_error("plugin fetch failed");
        wifi_link_send(TT_FRAME_PLUGINS_END, NULL, 0);
        return;
    }
    cJSON* root = cJSON_Parse(body);
    free(body);
    if(!root) {
        wifi_link_send_error("plugin JSON parse failed");
        wifi_link_send(TT_FRAME_PLUGINS_END, NULL, 0);
        return;
    }
    cached_set(root);
    int total = cJSON_IsArray(s_cached_arr) ? cJSON_GetArraySize(s_cached_arr) : 0;
    for(int i = 0; i < total && i < 16; i++) {
        emit_plugin_from_json(i, cJSON_GetArrayItem(s_cached_arr, i));
    }
    wifi_link_send(TT_FRAME_PLUGINS_END, NULL, 0);
}

/* ---- /render -- plugin run -------------------------------------------- */

/* Shared state for the render flow: cloud_client fills the dims first,
 * then drives our chunk_cb which emits RESULT_BEGIN on its first call,
 * then RESULT_CHUNKs, then RESULT_END after the call returns. */
typedef struct {
    bool begin_sent;
    uint16_t* out_w;
    uint16_t* out_h;
    uint8_t* out_planes;
    uint16_t* out_rs;
    uint32_t total_bytes;
    uint32_t recv_bytes;
    uint8_t last_pct_emitted;
} RenderFwd;

static void render_chunk_cb(const uint8_t* data, uint16_t len, void* user) {
    RenderFwd* f = user;
    if(!f->begin_sent) {
        uint8_t hdr[16];
        uint16_t off = 0;
        uint16_t w = *f->out_w, h = *f->out_h;
        uint8_t p = *f->out_planes;
        uint16_t rs = *f->out_rs;
        hdr[off++] = (uint8_t)(w & 0xFF);
        hdr[off++] = (uint8_t)(w >> 8);
        hdr[off++] = (uint8_t)(h & 0xFF);
        hdr[off++] = (uint8_t)(h >> 8);
        hdr[off++] = p;
        uint32_t total = (uint32_t)rs * h * p;
        hdr[off++] = (uint8_t)(total & 0xFF);
        hdr[off++] = (uint8_t)((total >> 8) & 0xFF);
        hdr[off++] = (uint8_t)((total >> 16) & 0xFF);
        hdr[off++] = (uint8_t)((total >> 24) & 0xFF);
        wifi_link_send(TT_FRAME_RESULT_BEGIN, hdr, off);
        f->begin_sent = true;
        f->total_bytes = total;
        f->recv_bytes = 0;
        f->last_pct_emitted = 50;
        wifi_link_send_progress(50, "Receiving image");
    }
    while(len > 0) {
        uint16_t take = len > 512 ? 512 : len;
        wifi_link_send(TT_FRAME_RESULT_CHUNK, data, take);
        data += take;
        len -= take;
        f->recv_bytes += take;
    }
    /* 50..95% during streaming. Emit at most one progress frame per 10%
     * boundary - sending one after every chunk doubled the frame count
     * and starved the Flipper's UART RX during the burst, which was
     * dropping tail bytes (see tagtinker_wifi.c history). */
    if(f->total_bytes) {
        uint32_t pct = 50 + (f->recv_bytes * 45) / f->total_bytes;
        if(pct > 95) pct = 95;
        if(pct >= f->last_pct_emitted + 10 || (pct >= 95 && f->last_pct_emitted < 95)) {
            wifi_link_send_progress((uint8_t)pct, "Receiving image");
            f->last_pct_emitted = (uint8_t)pct;
        }
    }
}

static void handle_run_plugin(const uint8_t* payload, uint16_t len) {
    Rb r;
    rb_init(&r, payload, len);
    char id[32];
    uint8_t accent = 0, n_params = 0;
    uint16_t target_w = 0, target_h = 0;
    uint8_t plugin_idx = 0;

    if(!rb_u8(&r, &plugin_idx)) {
        wifi_link_send_error("bad RUN frame");
        return;
    }
    if(!rb_u16(&r, &target_w)) {
        wifi_link_send_error("bad RUN frame");
        return;
    }
    if(!rb_u16(&r, &target_h)) {
        wifi_link_send_error("bad RUN frame");
        return;
    }
    if(!rb_u8(&r, &accent)) {
        wifi_link_send_error("bad RUN frame");
        return;
    }
    if(!rb_u8(&r, &n_params)) {
        wifi_link_send_error("bad RUN frame");
        return;
    }

    /* Use the manifest cached during the most recent /plugins call - this
     * avoids a second TLS handshake and the associated memory pressure. */
    if(!s_cached_arr) {
        wifi_link_send_error("no plugin cache (refresh first)");
        return;
    }
    const cJSON* p = cJSON_GetArrayItem(s_cached_arr, plugin_idx);
    const cJSON* idj = p ? cJSON_GetObjectItemCaseSensitive(p, "id") : NULL;
    if(!cJSON_IsString(idj)) {
        wifi_link_send_error("bad plugin index");
        return;
    }
    strncpy(id, idj->valuestring, sizeof(id) - 1);
    id[sizeof(id) - 1] = 0;

    /* Read params into parallel arrays for cloud_client_render. */
    static char keys[6][32];
    static char vals[6][96];
    const char* k_ptrs[6];
    const char* v_ptrs[6];
    if(n_params > 6) n_params = 6;
    for(uint8_t i = 0; i < n_params; i++) {
        if(!rb_zstr(&r, keys[i], sizeof(keys[i])) || !rb_zstr(&r, vals[i], sizeof(vals[i]))) {
            wifi_link_send_error("bad param");
            return;
        }
        k_ptrs[i] = keys[i];
        v_ptrs[i] = vals[i];
    }

    if(!wifi_net_wait_connected(8000)) {
        wifi_link_send_error("WiFi not connected");
        return;
    }

    wifi_link_send_progress(15, "Connecting to cloud");

    uint16_t rw = 0, rh = 0, rs = 0;
    uint8_t rp = 0;
    RenderFwd fwd = {
        .begin_sent = false,
        .out_w = &rw,
        .out_h = &rh,
        .out_planes = &rp,
        .out_rs = &rs,
    };
    char err[64] = {0};
    bool ok = cloud_client_render(
        id,
        target_w,
        target_h,
        accent,
        k_ptrs,
        v_ptrs,
        n_params,
        &rw,
        &rh,
        &rp,
        &rs,
        render_chunk_cb,
        &fwd,
        err);

    if(!ok) {
        wifi_link_send_error(err[0] ? err : "render failed");
        return;
    }
    if(!fwd.begin_sent) {
        wifi_link_send_error("empty body");
        return;
    }
    wifi_link_send(TT_FRAME_RESULT_END, NULL, 0);
    wifi_link_send_progress(100, "Done");
}

/* ---- WIFI_SET / WIFI_FORGET ------------------------------------------- */
static void handle_wifi_set(const uint8_t* payload, uint16_t len) {
    Rb r;
    rb_init(&r, payload, len);
    char ssid[33], pwd[65];
    if(!rb_zstr(&r, ssid, sizeof(ssid)) || !rb_zstr(&r, pwd, sizeof(pwd))) {
        wifi_link_send_error("bad WIFI_SET");
        return;
    }
    wifi_net_set_creds(ssid, pwd);
    send_wifi_status();
}

/* ---- Main RX dispatch --------------------------------------------------- */
static void on_frame(uint8_t type, const uint8_t* payload, uint16_t len, void* user) {
    (void)user;
    switch(type) {
    case TT_FRAME_PING:
        wifi_link_send(TT_FRAME_PING, NULL, 0);
        break;
    case TT_FRAME_WIFI_SET:
        handle_wifi_set(payload, len);
        break;
    case TT_FRAME_WIFI_FORGET:
        wifi_net_forget();
        send_wifi_status();
        break;
    case TT_FRAME_WIFI_STATUS:
        send_wifi_status();
        break;
    case TT_FRAME_LIST_PLUGINS:
        handle_list_plugins();
        break;
    case TT_FRAME_RUN_PLUGIN:
        handle_run_plugin(payload, len);
        break;
    default:
        ESP_LOGW(TAG, "unhandled type 0x%02X", type);
        break;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "TagTinker cloud-renderer booting");
    wifi_net_init();
    cloud_client_load();
    wifi_link_init(on_frame, NULL);

    vTaskDelay(pdMS_TO_TICKS(200));
    send_hello();

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        send_wifi_status();
    }
}
