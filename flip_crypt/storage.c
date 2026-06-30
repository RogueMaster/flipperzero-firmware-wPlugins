#include <storage/storage.h>

// callback for saving from output screen
void save_result(const char* text, char* file_name) {
    File* file = storage_file_alloc(furi_record_open(RECORD_STORAGE));

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "/ext/flip_crypt_saved/%s.txt", file_name);
    
    if(storage_simply_mkdir(furi_record_open(RECORD_STORAGE), "/ext/flip_crypt_saved")) {
        if (storage_file_open(file, buffer, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, text, strlen(text));
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// back end saving and loading for QR and NFC functionality

void save_result_generic(const char* filename, const char* text) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, text, strlen(text));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

char* load_result_generic(const char* filename) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    char* result = NULL;

    if(storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t size = storage_file_size(file);
        if(size > 0) {
            result = malloc(size + 1);
            storage_file_read(file, result, size);
            result[size] = '\0';
        } else {
            result = strdup("FAILURE");
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}