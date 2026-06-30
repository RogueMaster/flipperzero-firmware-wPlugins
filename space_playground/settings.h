#ifndef SETTINGS_H
#define SETTINGS_H

#include "storage/storage.h"
#include <stddef.h>

typedef struct {
    double gravity_force;
    size_t num_planets;
    double launch_interval;
    double grid_speed;
    size_t grid_spacing;
    size_t trail_duration;
    size_t asteroid_speed;
    size_t debri_count;
} Settings;

static Settings DEFAULT_SETTINGS = {
    .gravity_force = 50.0,
    .num_planets = 3,
    .launch_interval = 1.0,
    .grid_speed = 16,
    .grid_spacing = 16,
    .trail_duration = 1,
    .asteroid_speed = 2,
    .debri_count = 10,
};

Settings settings;

static void settings_load() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, "/ext/spground.cfg", FSAM_READ, FSOM_OPEN_EXISTING)) {
        int gravity_force = 0;
        int launch_interval = 0;
        int grid_speed = 0;

        char buf[256] = {0};
        storage_file_read(file, buf, sizeof(buf) - 1);
        sscanf(
            buf,
            "%d %d %d %d %d %d %d %d",
            &gravity_force,
            &settings.num_planets,
            &launch_interval,
            &grid_speed,
            &settings.grid_spacing,
            &settings.trail_duration,
            &settings.asteroid_speed,
            &settings.debri_count);

        settings.gravity_force = (double)gravity_force / (double)1000.0;
        settings.launch_interval = (double)launch_interval / (double)1000.0;
        settings.grid_speed = (double)grid_speed / (double)1000.0;

        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_clear() {
    settings.gravity_force = DEFAULT_SETTINGS.gravity_force;
    settings.num_planets = DEFAULT_SETTINGS.num_planets;
    settings.launch_interval = DEFAULT_SETTINGS.launch_interval;
    settings.grid_speed = DEFAULT_SETTINGS.grid_speed;
    settings.grid_spacing = DEFAULT_SETTINGS.grid_spacing;
    settings.trail_duration = DEFAULT_SETTINGS.trail_duration;
    settings.asteroid_speed = DEFAULT_SETTINGS.asteroid_speed;
    settings.debri_count = DEFAULT_SETTINGS.debri_count;
}

void settings_init() {
    settings_clear();
    settings_load();
}

void settings_save() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, "/ext/spground.cfg", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        int n = snprintf(
            buf,
            sizeof(buf),
            "%d %d %d %d %d %d %d %d",
            (int)(settings.gravity_force * (int)1000),
            settings.num_planets,
            (int)(settings.launch_interval * (int)1000),
            (int)(settings.grid_speed * (int)1000),
            settings.grid_spacing,
            settings.trail_duration,
            settings.asteroid_speed,
            settings.debri_count);
        storage_file_write(file, buf, n);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

#endif
