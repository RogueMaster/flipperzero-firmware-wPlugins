#include "gpx.h"

#define TAG "gpx"

bool gpx_open_file(Storage* storage, GPXFile* gpx, const char* path) {
    gpx->file = storage_file_alloc(storage);
    if(!storage_file_open(gpx->file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // must call close() even if the operation fails
        FURI_LOG_E(TAG, "failed to open GPX file!");
        storage_file_close(gpx->file);
        storage_file_free(gpx->file);
        return false;
    }

    // with the file opened, we need to write the intro GPX tags
    const char* gpx_intro =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<gpx version=\"1.0\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"

        "xmlns=\"http://www.topografix.com/GPX/1/0\"\n"
        "xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd\">\n"
        "    <name>Paths</name>\n"
        "    <trk><name>Paths</name><number>1</number><trkseg>\n";

    if(!storage_file_write(gpx->file, gpx_intro, strlen(gpx_intro))) {
        FURI_LOG_E(TAG, "failed to write GPX starting header!");
        storage_file_close(gpx->file);
        storage_file_free(gpx->file);
        return false;
    }

    // keeps track of writes for periodic flushes
    gpx->write_counter = 0;
    FURI_LOG_I(TAG, "file opened successfully");
    return true;
}

bool gpx_add_path_point(GPXFile* gpx, double lat, double lon, uint32_t alt) {
    // TODO: it would be nice to include time
    FuriString* point = furi_string_alloc_printf(
        "        <trkpt lat=\"%f\" lon=\"%f\"><ele>%lu</ele></trkpt>\n", lat, lon, alt);
    if(!storage_file_write(gpx->file, furi_string_get_cstr(point), furi_string_size(point))) {
        FURI_LOG_E(TAG, "failed to write line to GPX file!");
        return false;
    }

    furi_string_free(point);

    gpx->write_counter += 1;
    if(gpx->write_counter == 16) {
        if(!storage_file_sync(gpx->file)) {
            FURI_LOG_E(TAG, "failed to periodic flush file!");
        }
        // reset
        gpx->write_counter = 0;
    }

    return true;
}

bool gpx_close_file(GPXFile* gpx) {
    const char* gpx_outro = "    </trkseg></trk>\n"
                            "</gpx>";

    if(!storage_file_write(gpx->file, gpx_outro, strlen(gpx_outro))) {
        FURI_LOG_E(TAG, "failed to close GPX file!");
        storage_file_close(gpx->file);
        storage_file_free(gpx->file);
        return false;
    }

    storage_file_close(gpx->file);
    storage_file_free(gpx->file);

    return true;
}
