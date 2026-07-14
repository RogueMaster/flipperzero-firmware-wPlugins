#include "mtp_ops.h"
#include "utils.h"

void GetStorageIDs(AppMTP* mtp, uint32_t* storage_ids, uint32_t* count) {
    SDInfo sd_info;
    FS_Error err = storage_sd_info(mtp->storage, &sd_info);

    // Check if SD card is present
    if(err != FSE_OK) {
        FURI_LOG_E("MTP", "SD Card not found");
        *count = 0; // No storage.
        return;
    }

    storage_ids[0] = EXTERNAL_STORAGE_ID;
    *count = 1;
}

int GetStorageInfo(AppMTP* mtp, uint32_t storage_id, uint8_t* buf) {
    MTPStorageInfoHeader* info = (MTPStorageInfoHeader*)buf;
    uint8_t* ptr = buf + sizeof(MTPStorageInfoHeader);
    uint16_t length;

    info->free_space_in_objects = 20ul;
    info->filesystem_type = 0x0002; // Generic hierarchical
    info->access_capability = 0x0000; // Read-write
    FURI_LOG_I("MTP", "Getting storage info for storage ID %04lx", storage_id);

    if(storage_id == INTERNAL_STORAGE_ID) {
        // Fill in details for internal storage
        info->storage_type = 0x0003; // Fixed RAM

        // Fill in details for internal storage
        uint64_t total_space;
        uint64_t free_space;
        FS_Error err = storage_common_fs_info(
            mtp->storage, STORAGE_INT_PATH_PREFIX, &total_space, &free_space);
        if(err != FSE_OK) {
            info->max_capacity = 0;
            info->free_space_in_bytes = 0;
        } else {
            info->max_capacity = total_space / BLOCK_SIZE;
            info->free_space_in_bytes = free_space / BLOCK_SIZE;
        }

        WriteMTPString(ptr, "Internal Storage", &length);
        ptr += length;

        WriteMTPString(ptr, "INT_STORAGE", &length);
        ptr += length;
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        SDInfo sd_info;
        FS_Error err = storage_sd_info(mtp->storage, &sd_info);

        // Fill in details for internal storage
        info->storage_type = 0x0004; // Removable RAM

        if(err != FSE_OK) {
            info->max_capacity = 0;
            info->free_space_in_bytes = 0;

            FURI_LOG_E("MTP", "SD Card not found");
        } else {
            // Fill in details for external storage
            info->max_capacity = (uint64_t)sd_info.kb_total * 1024 / BLOCK_SIZE;
            info->free_space_in_bytes = (uint64_t)sd_info.kb_free * 1024 / BLOCK_SIZE;
        }

        WriteMTPString(ptr, "SD Card", &length);
        ptr += length;

        WriteMTPString(ptr, "SD_CARD", &length);
        ptr += length;
    }

    return ptr - buf;
}

int GetObjectInfo(AppMTP* mtp, uint32_t handle, uint8_t* buffer) {
    ObjectInfoHeader* header = (ObjectInfoHeader*)buffer;
    uint8_t* ptr = buffer + sizeof(ObjectInfoHeader);

    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        return -1;
    }

    FURI_LOG_I("MTP", "Getting object info for handle %ld", handle);
    FURI_LOG_I("MTP", "Path: %s", path);

    header->protection_status = 0;

    Storage* storage = mtp->storage;
    File* file = storage_file_alloc(storage);
    uint16_t length;

    FileInfo fileinfo;
    FS_Error err = storage_common_stat(storage, path, &fileinfo);
    if(err != FSE_OK) {
        FURI_LOG_E("MTP", "Failed to get file info: %s", filesystem_api_error_get_desc(err));
        return -1;
    }

    if(memcmp(path, STORAGE_INT_PATH_PREFIX, strlen(STORAGE_INT_PATH_PREFIX)) == 0) {
        FURI_LOG_I("MTP", "Object in Internal storage");
        header->storage_id = INTERNAL_STORAGE_ID;
    } else if(memcmp(path, STORAGE_EXT_PATH_PREFIX, strlen(STORAGE_EXT_PATH_PREFIX)) == 0) {
        FURI_LOG_I("MTP", "Object in External storage");
        header->storage_id = EXTERNAL_STORAGE_ID;
    } else {
        return -1;
    }

    if(file_info_is_dir(&fileinfo)) {
        FURI_LOG_I("MTP", "Directory");

        header->format = MTP_FORMAT_ASSOCIATION;
        header->association_type = 0x0001; // Generic folder
    } else {
        FURI_LOG_I("MTP", "Undefined type");

        header->format = MTP_FORMAT_UNDEFINED;
    }

    header->compressed_size = fileinfo.size;

    header->thumb_format = 0;
    header->thumb_compressed_size = 0;
    header->thumb_pix_width = 0;
    header->thumb_pix_height = 0;
    header->image_pix_width = 0;
    header->image_pix_height = 0;
    header->image_bit_depth = 0;

    char* file_name = strrchr(path, '/');
    FURI_LOG_I("MTP", "File name: %s", file_name + 1);

    WriteMTPString(ptr, file_name + 1, &length);
    ptr += length;

    // get created
    WriteMTPString(ptr, "20240608T010702", &length);
    ptr += length;

    // get last modified
    WriteMTPString(ptr, "20240608T010702", &length);
    ptr += length;

    // get keywords
    WriteMTPString(ptr, "", &length);
    ptr += length;

    storage_file_free(file);

    return ptr - buffer;
}

int GetObject_callback(void* ctx, uint8_t* buffer, int length) {
    struct GetObjectContext* obj_ctx = (struct GetObjectContext*)ctx;
    return storage_file_read(obj_ctx->file, buffer, length);
}

void GetObject(AppMTP* mtp, uint32_t transaction_id, uint32_t handle) {
    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
        return;
    }

    FURI_LOG_I("MTP", "Getting object: %s", path);

    Storage* storage = mtp->storage;
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("MTP", "Failed to open file: %s", path);
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
        return;
    }

    uint32_t size = storage_file_size(file);

    struct GetObjectContext ctx = {
        .file = file,
    };

    send_mtp_response_stream(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_OBJECT, transaction_id, &ctx, GetObject_callback, size);

    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);

    storage_file_close(file);
    storage_file_free(file);
}

int GetObjectHandles(AppMTP* mtp, uint32_t storage_id, uint32_t association, uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    UNUSED(length);

    // For now, just return a single object handle
    int count = list_and_issue_handles(mtp, storage_id, association, NULL);

    *(uint32_t*)ptr = count;
    ptr += sizeof(uint32_t);

    uint32_t* handles = (uint32_t*)ptr;
    length = list_and_issue_handles(mtp, storage_id, association, handles);
    ptr += length * sizeof(uint32_t);

    return ptr - buffer;
}

bool DeleteObject(AppMTP* mtp, uint32_t handle) {
    UNUSED(mtp);
    FURI_LOG_I("MTP", "Deleting object %ld", handle);

    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        return false;
    }

    FileInfo fileinfo;
    FS_Error err = storage_common_stat(mtp->storage, path, &fileinfo);

    if(err == FSE_OK) {
        if(file_info_is_dir(&fileinfo)) {
            if(!storage_simply_remove_recursive(mtp->storage, path)) {
                return false;
            }
        } else {
            if(storage_common_remove(mtp->storage, path) != FSE_OK) {
                return false;
            }
        }

        return true;
    }

    return false;
}

void MoveObject(
    AppMTP* mtp,
    uint32_t transaction_id,
    uint32_t handle,
    uint32_t storage_id,
    uint32_t parent) {
    UNUSED(storage_id);

    FURI_LOG_I("MTP", "Moving object %ld to storage %ld, parent %ld", handle, storage_id, parent);
    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
        return;
    }

    char* parentPath;

    if(parent != 0) {
        parentPath = get_path_from_handle(mtp, parent);
        if(parentPath == NULL) {
            send_mtp_response(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
            return;
        }
    } else {
        parentPath = get_base_path_from_storage_id(storage_id);
    }

    Storage* storage = mtp->storage;

    char* filename = strrchr(path, '/');
    // remove the beginning slash
    char* realFilename = filename + 1;

    char* newPath = malloc(sizeof(char) * 256);
    merge_path(newPath, parentPath, realFilename);

    FURI_LOG_I("MTP", "Moving object: %s to %s", path, newPath);

    if(storage_common_rename(storage, path, newPath) != FSE_OK) {
        FURI_LOG_E("MTP", "Failed to move object: %s", path);
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
        return;
    }

    FURI_LOG_I("MTP", "Object moved successfully");
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);

    update_object_handle_path(mtp, handle, newPath);
    free(path);

    return;
}

int GetObjectPropValueInternal(AppMTP* mtp, const char* path, uint32_t prop_code, uint8_t* buffer) {
    UNUSED(mtp);
    uint8_t* ptr = buffer;
    uint16_t length;

    switch(prop_code) {
    case MTP_PROP_STORAGE_ID: {
        FURI_LOG_I("MTP", "Getting storage ID for object: %s", path);

        *(uint32_t*)ptr = prop_code;
        ptr += sizeof(uint32_t);

        *(uint32_t*)ptr = 0x0006;
        ptr += sizeof(uint32_t);

        *(uint8_t*)ptr = 0x00;
        ptr += sizeof(uint8_t);

        if(memcmp(path, STORAGE_INT_PATH_PREFIX, strlen(STORAGE_INT_PATH_PREFIX)) == 0) {
            *(uint32_t*)ptr = INTERNAL_STORAGE_ID;
        } else if(memcmp(path, STORAGE_EXT_PATH_PREFIX, strlen(STORAGE_EXT_PATH_PREFIX)) == 0) {
            *(uint32_t*)ptr = EXTERNAL_STORAGE_ID;
        } else {
            *(uint32_t*)ptr = 0;
        }
        ptr += sizeof(uint32_t);

        *(uint8_t*)ptr = 0x00;
        break;
    }
    case MTP_PROP_OBJECT_FORMAT: {
        FURI_LOG_I("MTP", "Getting object format for object: %s", path);

        *(uint32_t*)ptr = prop_code;
        ptr += sizeof(uint32_t);

        *(uint32_t*)ptr = 0x0006;
        ptr += sizeof(uint32_t);

        *(uint8_t*)ptr = 0x00;
        ptr += sizeof(uint8_t);

        *(uint16_t*)ptr = MTP_FORMAT_UNDEFINED;
        ptr += sizeof(uint16_t);

        *(uint8_t*)ptr = 0x00;
        break;
    }
    case MTP_PROP_OBJECT_FILE_NAME: {
        FURI_LOG_I("MTP", "Getting object file name for object: %s", path);

        *(uint32_t*)ptr = prop_code;
        ptr += sizeof(uint32_t);

        *(uint32_t*)ptr = 0xffff;
        ptr += sizeof(uint32_t);

        *(uint8_t*)ptr = 0x01;
        ptr += sizeof(uint8_t);

        char* file_name = strrchr(path, '/');
        WriteMTPString(ptr, file_name + 1, &length);
        ptr += length;

        *(uint8_t*)ptr = 0x00;
        break;
    }
    }

    return ptr - buffer;
}

void GetObjectPropValue(AppMTP* mtp, uint32_t transaction_id, uint32_t handle, uint32_t prop_code) {
    FURI_LOG_I(
        "MTP", "Getting object property value for handle %ld, prop code %04lx", handle, prop_code);

    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
        return;
    }

    uint8_t* buffer = malloc(sizeof(uint8_t) * 256);
    int length = GetObjectPropValueInternal(mtp, path, prop_code, buffer);
    if(length < 0) {
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id, NULL);
        free(buffer);
        return;
    }

    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_OBJECT_PROP_VALUE, transaction_id, buffer, length);
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);
    free(buffer);
}
