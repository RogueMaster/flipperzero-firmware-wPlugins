/*
 * Cloud plugin client.
 *
 * Talks to the TagTinker Cloudflare Worker that hosts plugin manifests
 * and renders. Default URL is hard-coded below; override at runtime by
 * calling cloud_client_set_url() (the value is persisted in NVS).
 *
 *   GET <base>/plugins        -> JSON manifest list
 *   GET <base>/render/<id>?...-> binary framebuffer (see worker docs)
 *
 * The framebuffer header (8 bytes, little-endian) is:
 *   uint16 width, uint16 height, uint8 planes, uint8 reserved,
 *   uint16 row_stride
 * followed by `row_stride * height * planes` bytes of pixel data.
 */
#ifndef TT_CLOUD_CLIENT_H
#define TT_CLOUD_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define TT_CLOUD_DEFAULT_URL "https://tagtinker.jhackerr.workers.dev"

/* Persisted base URL (no trailing slash). */
const char* cloud_client_url(void);
void cloud_client_set_url(const char* url);
void cloud_client_load(void); /* call once after nvs_flash_init */

/* Forwarded JSON for /plugins. Caller frees with free(). NULL on failure. */
char* cloud_client_fetch_plugins_json(size_t* out_len);

/* Streaming /render call.
 *
 * Returns true on 200 OK. The header (width, height, planes, row_stride)
 * is filled in before any chunk_cb is invoked. chunk_cb is then called
 * one or more times with consecutive plane bytes (chunk_len <= 1024).
 * Plane 0 is delivered first, then plane 1 (if planes==2). */
typedef void (*tt_cloud_chunk_cb)(const uint8_t* data, uint16_t len, void* user);

bool cloud_client_render(
    const char* plugin_id,
    uint16_t target_w,
    uint16_t target_h,
    uint8_t accent, /* 0=none, 1=red, 2=yellow */
    const char* const* keys,
    const char* const* values,
    uint8_t n_params,
    /* Output header: */
    uint16_t* out_w,
    uint16_t* out_h,
    uint8_t* out_planes,
    uint16_t* out_row_stride,
    tt_cloud_chunk_cb chunk_cb,
    void* user,
    char err_msg[64]);

#endif /* TT_CLOUD_CLIENT_H */
