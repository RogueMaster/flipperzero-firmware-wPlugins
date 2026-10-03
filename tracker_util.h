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
#define TU_BODY_MAX 192

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
    char body[TU_BODY_MAX]; // POST payload template, same substitutions
    bool is_post;
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

#define TU_SSID_LEN 33

// Pull SSIDs out of the board's scan reply, e.g. {"networks":["Home","Cafe"]}.
// Returns how many were written into out.
int ssid_list_parse(const char* json, char out[][TU_SSID_LEN], int max);

// Substitute {tracking} and {carrier} in tmpl into out (bounded by cap).
// Returns the written length.
size_t
    url_build(const char* tmpl, const char* tracking, const char* carrier, char* out, size_t cap);

// Extract a dot/index path (e.g. "data.0.status") from a JSON string into out.
// A segment of "last" selects the final element of an array, which is how APIs
// that return an event timeline expose the most recent one.
// Returns true if the path resolved to a string/number leaf.
bool json_extract(const char* json, const char* path, char* out, size_t cap);

// Shorten an ISO 8601 timestamp (2026-07-27T17:53:00.000Z) to something that
// fits the detail screen (Jul 27 17:53). Anything that is not an ISO timestamp
// is copied through unchanged, since other services format dates their own way.
void iso_to_short(const char* in, char* out, size_t cap);

// Best-effort map of a fetched status string to a PackageStatus (for the glyph).
PackageStatus status_from_text(const char* s);
