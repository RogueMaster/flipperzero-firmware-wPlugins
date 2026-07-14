// Pure helpers for Pack Track live tracking: config parsing, URL templating,
// JSON field extraction, and status keyword mapping. No Flipper/firmware deps,
// so this file is host-testable.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define TU_SSID_MAX 48
#define TU_PASS_MAX 48
#define TU_URL_MAX  256
#define TU_PATH_MAX 48
#define TU_HDR_MAX  4
#define TU_HDR_LEN  96

typedef enum {
    StatusPending,
    StatusInTransit,
    StatusOutForDelivery,
    StatusDelivered,
    StatusException,
} PackageStatus;

typedef struct {
    char wifi_ssid[TU_SSID_MAX];
    char wifi_pass[TU_PASS_MAX];
    char url[TU_URL_MAX]; // template with {tracking} / {carrier}
    char headers[TU_HDR_MAX][TU_HDR_LEN];
    int header_count;
    char field_status[TU_PATH_MAX];
    char field_location[TU_PATH_MAX];
    char field_updated[TU_PATH_MAX];
    bool has_url;
    bool has_wifi;
} TrackerConfig;

// Parse a config.txt buffer (KEY = value lines) into cfg. Returns true if a URL
// template was provided (the minimum for live tracking).
bool config_parse(const char* buf, TrackerConfig* cfg);

// Substitute {tracking} and {carrier} in tmpl into out (bounded by cap).
// Returns the written length.
size_t url_build(const char* tmpl, const char* tracking, const char* carrier, char* out, size_t cap);

// Extract a dot/index path (e.g. "data.0.status") from a JSON string into out.
// Returns true if the path resolved to a string/number leaf.
bool json_extract(const char* json, const char* path, char* out, size_t cap);

// Best-effort map of a fetched status string to a PackageStatus (for the glyph).
PackageStatus status_from_text(const char* s);
