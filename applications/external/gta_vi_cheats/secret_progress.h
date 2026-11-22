#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

/* Progress IDs are stable and independent of the chronological display order. */
enum {
    GtaSecretProgressSlots = 2,
    GtaSecretProgressBytes = 8
};
typedef struct {
    uint32_t magic;
    uint32_t generation;
    uint8_t checked[GtaSecretProgressBytes];
    uint32_t checksum;
} GtaSecretSave;
typedef struct {
    GtaSecretSave save;
    size_t slot;
    bool save_error;
} GtaSecretProgress;

void gta_secret_progress_load(GtaSecretProgress* progress, Storage* storage);
bool gta_secret_progress_is_checked(const GtaSecretProgress* progress, uint8_t id);
bool gta_secret_progress_toggle(GtaSecretProgress* progress, Storage* storage, uint8_t id);
