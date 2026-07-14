#pragma once

#include <gui/view.h>

typedef struct ClippyMassStorage ClippyMassStorage;

ClippyMassStorage* clippy_mass_storage_alloc();

void clippy_mass_storage_free(ClippyMassStorage* mass_storage);

View* clippy_mass_storage_get_view(ClippyMassStorage* mass_storage);

void clippy_mass_storage_set_file_name(ClippyMassStorage* mass_storage, FuriString* name);

void clippy_mass_storage_set_stats(
    ClippyMassStorage* mass_storage,
    uint32_t read,
    uint32_t written);
