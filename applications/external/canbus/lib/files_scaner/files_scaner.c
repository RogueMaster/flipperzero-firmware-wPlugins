#include "files_scaner.h"

FileActive* file_active_alloc(void) {
    FileActive* file_active = malloc(sizeof(FileActive));

    file_active->path = furi_string_alloc();

    return file_active;
}

void file_active_free(FileActive* file_active) {
    furi_string_free(file_active->path);

    free(file_active);
}
