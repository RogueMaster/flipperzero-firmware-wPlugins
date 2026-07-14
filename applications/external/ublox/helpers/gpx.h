#pragma once

#include <furi.h>
#include <storage/storage.h>

#include "../helpers/ublox_types.h"

typedef struct GPXFile {
    File* file;
    int write_counter;
} GPXFile;

/**
 * Open a GPX file and write out initial XML tags for list of points in path.
 * This assumes that `path` is a valid and complete Flipper filesystem path.
 */
bool gpx_open_file(Storage* storage, GPXFile* gpx, const char* path);

bool gpx_add_path_point(GPXFile* gpx, double lat, double lon, uint32_t alt);

bool gpx_close_file(GPXFile* gpx);
