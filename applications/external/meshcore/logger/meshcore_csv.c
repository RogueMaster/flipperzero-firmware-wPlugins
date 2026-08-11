#include "meshcore_csv.h"

struct MeshCoreCsv {
    File* file;
    uint32_t lines;
};

MeshCoreCsv* meshcore_csv_open(Storage* storage, const char* path, const char* header) {
    MeshCoreCsv* csv = malloc(sizeof(MeshCoreCsv));
    csv->file = storage_file_alloc(storage);
    csv->lines = 0;

    /* FSOM_OPEN_APPEND creates the file if it is not there and seeks to the
     * end if it is. */
    if(!storage_file_open(csv->file, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        /* The API requires a close even after a failed open. */
        storage_file_close(csv->file);
        storage_file_free(csv->file);
        free(csv);
        return NULL;
    }

    if(header && storage_file_tell(csv->file) == 0) {
        size_t len = strlen(header);
        storage_file_write(csv->file, header, len);
        storage_file_write(csv->file, "\n", 1);
        storage_file_sync(csv->file);
    }

    return csv;
}

void meshcore_csv_close(MeshCoreCsv* csv) {
    if(!csv) return;
    storage_file_close(csv->file);
    storage_file_free(csv->file);
    free(csv);
}

bool meshcore_csv_write(MeshCoreCsv* csv, const char* line) {
    furi_assert(csv);

    size_t len = strlen(line);
    if(storage_file_write(csv->file, line, len) != len) return false;
    if(storage_file_write(csv->file, "\n", 1) != 1) return false;

    /* The whole point: on disk before we return. */
    storage_file_sync(csv->file);
    csv->lines++;
    return true;
}

uint32_t meshcore_csv_lines(const MeshCoreCsv* csv) {
    furi_assert(csv);
    return csv->lines;
}
