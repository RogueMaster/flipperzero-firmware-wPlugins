#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Keep these values stable: the embedded NFC plugin uses them. */
typedef enum {
    GtaUrlSource = 0,
    GtaUrlMapImage = 1,
    GtaUrlSource2 = 2,
} GtaUrlKind;

typedef struct {
    const char* url;
    const char* label; // Optional location-specific descriptor.
    GtaUrlKind kind;
    bool map_index;
} GtaUrlChoice;

#define GTA_URL_CHOICES_MAX 16u
#define GTA_URL_LABEL_MAX   80u

typedef struct {
    size_t count;
    GtaUrlChoice choices[GTA_URL_CHOICES_MAX];
} GtaUrlLinks;

/* The active result owns at most four TSV rows, released before NFC loads. */
GtaUrlLinks* gta_url_links_resolve(uint16_t id, const char* asset, const char* title);
void gta_url_links_add(GtaUrlLinks* links, GtaUrlChoice choice);
void gta_url_choice_label(const GtaUrlLinks* links, size_t index, char* out, size_t size);
