#include "storage_ops.h"
#include "utils.h"

char* get_base_path_from_storage_id(uint32_t storage_id) {
    if(storage_id == INTERNAL_STORAGE_ID) {
        return STORAGE_INT_PATH_PREFIX;
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        return STORAGE_EXT_PATH_PREFIX;
    }
    return NULL;
}

char* get_path_from_handle(AppMTP* mtp, uint32_t handle) {
    FileHandle* current = mtp->handles;
    while(current != NULL) {
        if(current->handle == handle) {
            return current->path;
        }
        current = current->next;
    }
    return NULL;
}

uint32_t issue_object_handle(AppMTP* mtp, char* path) {
    int handle = 1;
    int length = strlen(path);
    char* path_store = malloc(sizeof(char) * (length + 1));
    strcpy(path_store, path);

    if(mtp->handles == NULL) {
        mtp->handles = malloc(sizeof(FileHandle));
        mtp->handles->handle = handle;
        mtp->handles->path = path_store;
        mtp->handles->next = NULL;
        return handle;
    }

    FileHandle* current = mtp->handles;
    if(strcmp(current->path, path) == 0) {
        return current->handle;
    }

    while(current->next != NULL) {
        if(strcmp(current->path, path) == 0) {
            return current->handle;
        }
        current = current->next;
        handle++;
    }

    current->next = malloc(sizeof(FileHandle));
    current = current->next;
    handle++;

    current->handle = handle;
    current->path = path_store;
    current->next = NULL;
    return handle;
}

uint32_t update_object_handle_path(AppMTP* mtp, uint32_t handle, char* path) {
    FileHandle* current = mtp->handles;
    while(current != NULL) {
        if(current->handle == handle) {
            free(current->path);
            current->path = path;
            return handle;
        }
        current = current->next;
    }
    return 0;
}

int list_and_issue_handles(
    AppMTP* mtp,
    uint32_t storage_id,
    uint32_t association,
    uint32_t* handles) {
    Storage* storage = mtp->storage;
    char* base_path = get_base_path_from_storage_id(storage_id);
    if(base_path == NULL) {
        base_path = "";
    }

    File* dir = storage_file_alloc(storage);
    if(association == 0xffffffff) {
        // count the objects in the root directory
        storage_dir_open(dir, base_path);
    } else {
        char* path = get_path_from_handle(mtp, association);
        FURI_LOG_I("MTP", "Association path: %s", path);
        if(path == NULL) {
            return 0;
        }
        storage_dir_open(dir, path);
        base_path = path;
    }

    int count = 0;
    FileInfo fileinfo;
    char* file_name = malloc(sizeof(char) * 256);
    char* full_path = malloc(sizeof(char) * 256);
    while(storage_dir_read(dir, &fileinfo, file_name, 256)) {
        if(file_info_is_dir(&fileinfo)) {
            FURI_LOG_I("MTP", "Found directory: %s", file_name);
        } else {
            FURI_LOG_I("MTP", "Found file: %s", file_name);
        }

        merge_path(full_path, base_path, file_name);
        FURI_LOG_I("MTP", "Full path: %s", full_path);

        uint32_t handle = issue_object_handle(mtp, full_path);
        if(handles != NULL) {
            handles[count] = handle;
        }
        count++;
    }

    FURI_LOG_I("MTP", "Getting number of objects in storage %ld", storage_id);
    FURI_LOG_I("MTP", "Base path: %s", base_path);
    FURI_LOG_I("MTP", "Association: %ld", association);
    FURI_LOG_I("MTP", "Number of objects: %d", count);

    storage_dir_close(dir);
    storage_file_free(dir);
    free(file_name);
    free(full_path);
    return count;
}
