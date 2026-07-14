#include <furi.h>
#include <power/power_service/power.h>
#include <storage/storage.h>
#include "main.h"
#include "mtp.h"
#include "mtp_ops.h"
#include "mtp_support.h"
#include "device_props.h"
#include "storage_ops.h"
#include "usb_desc.h"
#include "utils.h"

// temporary storage for buffer
MTPDataPersistence persistence;

void mtp_handle_bulk(AppMTP* mtp, uint8_t* buffer, uint32_t length) {
    UNUSED(mtp);
    if(persistence.left_bytes > 0) {
        FURI_LOG_I("MTP", "Left bytes: %ld", persistence.left_bytes);
        handle_mtp_data_packet(mtp, buffer, length, 1);
        return;
    }

    if(length < 12) {
        FURI_LOG_E("MTP", "Invalid MTP packet");
        return;
    }

    struct MTPHeader* header = (struct MTPHeader*)buffer;
    uint16_t type = header->type;

    if(type == MTP_TYPE_COMMAND) {
        struct MTPContainer* container = (struct MTPContainer*)buffer;
        handle_mtp_command(mtp, container);
    } else if(type == MTP_TYPE_DATA) {
        if(header->len > length) {
            persistence.left_bytes = header->len;
        }
        handle_mtp_data_packet(mtp, buffer, length, 0);
    } else if(type == MTP_TYPE_RESPONSE) {
        handle_mtp_response(mtp, header);
    } else {
        FURI_LOG_W("MTP", "Unsupported MTP packet type: %d", type);
    }
}

void setup_persistence(struct MTPContainer* container) {
    persistence.transaction_id = container->header.transaction_id;
    memcpy(persistence.params, container->params, sizeof(uint32_t) * 5);
}

void handle_mtp_data_packet(AppMTP* mtp, uint8_t* buffer, int32_t length, bool cont) {
    UNUSED(mtp);
    uint8_t* ptr = buffer;
    if(!cont) {
        struct MTPHeader* header = (struct MTPHeader*)buffer;

        if(header->transaction_id != persistence.transaction_id) {
            // the params value can not be trusted.
            // reset the params
            for(int i = 0; i < 5; i++) {
                persistence.params[i] = 0;
            }
        }

        persistence.global_buffer = NULL;
        uint32_t transaction_id = header->transaction_id;
        persistence.op = header->op;

        FURI_LOG_I("MTP", "Handling MTP data: 0x%04x", header->op);
        FURI_LOG_I("MTP", "Transaction ID: %ld", transaction_id);
        FURI_LOG_I("MTP", "Length: %ld", length);

        ptr = (uint8_t*)buffer + sizeof(struct MTPHeader);

        switch(header->op) {
        case MTP_OP_SEND_OBJECT_INFO:
        case MTP_OP_SET_OBJECT_PROP_VALUE: {
            persistence.global_buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
            persistence.buffer_offset = 0;
            break;
        }
        case MTP_OP_SEND_OBJECT: {
            persistence.buffer_offset = 0;
            persistence.left_bytes = header->len - sizeof(struct MTPHeader);
        }
        }
    }

    // for speicific operations.
    if(persistence.op == MTP_OP_SEND_OBJECT) {
        uint32_t handle = persistence.prev_handle;
        if(handle == 0) {
            FURI_LOG_E("MTP", "Invalid handle for MTP_OP_SEND_OBJECT");
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, persistence.transaction_id, NULL);
            return;
        }

        char* path = get_path_from_handle(mtp, handle);
        int bytes_length = length - (ptr - buffer);
        int offset = persistence.buffer_offset;

        FURI_LOG_I("MTP", "Writing to file: %s with offset %02x", path, offset);
        if(persistence.current_file == NULL) {
            persistence.current_file = storage_file_alloc(mtp->storage);
            if(!storage_file_open(persistence.current_file, path, FSAM_WRITE, FSOM_OPEN_EXISTING)) {
                FURI_LOG_E("MTP", "Failed to open file: %s", path);
                send_mtp_response(
                    mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, persistence.transaction_id, NULL);
                storage_file_free(persistence.current_file);
                persistence.current_file = NULL;
                return;
            }
        }

        File* file = persistence.current_file;

        // Write with error handling
        uint16_t bytes_written = storage_file_write(file, ptr, bytes_length);
        if(bytes_written != bytes_length) {
            FURI_LOG_E("MTP", "Write failed: %d/%d bytes written", bytes_written, bytes_length);
            send_mtp_response(mtp, 3, MTP_RESP_STORE_FULL, persistence.transaction_id, NULL);
            storage_file_close(file);
            storage_file_free(file);
            persistence.current_file = NULL;
            return;
        }

        persistence.buffer_offset += bytes_written;
        persistence.left_bytes -= bytes_written;

        // Sync periodically based on configured interval
        if(persistence.buffer_offset % MTP_FILE_SYNC_INTERVAL == 0) {
            storage_file_sync(file);
            FURI_LOG_I("MTP", "Progress: %ld bytes remaining", persistence.left_bytes);
        }

        if(persistence.left_bytes <= 0) {
            storage_file_sync(file);
            send_mtp_response(mtp, 3, MTP_RESP_OK, persistence.transaction_id, NULL);
            storage_file_close(file);
            storage_file_free(file);
            persistence.current_file = NULL;
            FURI_LOG_I("MTP", "File transfer completed successfully");
        }
    } else if(persistence.op == MTP_OP_SET_OBJECT_PROP_VALUE) {
        uint32_t handle = persistence.params[0];
        uint32_t prop_code = persistence.params[1];

        char* path = get_path_from_handle(mtp, handle);
        if(path == NULL) {
            FURI_LOG_E("MTP", "Invalid handle for MTP_OP_SET_OBJECT_PROP_VALUE");
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, persistence.transaction_id, NULL);
            return;
        }

        if(prop_code == MTP_PROP_OBJECT_FILE_NAME) {
            // dump the ptr
            print_bytes("MTP FileName", ptr, length - sizeof(struct MTPHeader));

            char* name = ReadMTPString(ptr);
            char* full_path = malloc(sizeof(char) * MTP_PATH_SIZE);

            merge_path(full_path, get_base_path_from_storage_id(persistence.params[2]), name);

            // if(!storage_file_exists(mtp->storage, full_path)) {
            //     if(!storage_file_rename(mtp->storage, path, full_path)) {
            //         FURI_LOG_E("MTP", "Failed to rename file: %s to %s", path, full_path);
            //         send_mtp_response(
            //             mtp, 3, MTP_RESP_GENERAL_ERROR, persistence.transaction_id, NULL);
            //         free(full_path);
            //         free(name);
            //         return;
            //     }
            // }

            update_object_handle_path(mtp, handle, full_path);
            send_mtp_response(mtp, 3, MTP_RESP_OK, persistence.transaction_id, NULL);
            free(full_path);
            free(name);
        } else {
            FURI_LOG_W("MTP", "Unsupported property code: 0x%04lx", prop_code);
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_OBJECT_PROP_CODE, persistence.transaction_id, NULL);
        }
    }

    if(persistence.global_buffer != NULL) {
        memcpy(persistence.global_buffer + persistence.buffer_offset, ptr, length);
        persistence.buffer_offset += length;

        if(persistence.left_bytes > 0) {
            persistence.left_bytes -= length;
        }

        FURI_LOG_I("MTP", "Buffer offset: %ld", persistence.buffer_offset);
        FURI_LOG_I("MTP", "Left bytes: %ld", persistence.left_bytes);

        if(persistence.left_bytes == 0) {
            handle_mtp_data_complete(mtp);
        }
    }
}

void handle_mtp_data_complete(AppMTP* mtp) {
    // dump the buffer
    print_bytes("MTP Data", persistence.global_buffer, persistence.buffer_offset);

    switch(persistence.op) {
    case MTP_OP_SEND_OBJECT_INFO: {
        // parse the object info - persistence.global_buffer doesn't contain MTPHeader
        struct ObjectInfoHeader* info = (struct ObjectInfoHeader*)(persistence.global_buffer);

        uint8_t* ptr = persistence.global_buffer + sizeof(struct ObjectInfoHeader);
        ptr += 12;

        // dump the ptr
        print_bytes(
            "MTP FileName", ptr, persistence.buffer_offset - sizeof(struct ObjectInfoHeader));

        bool is_dir = info->format == MTP_FORMAT_ASSOCIATION;
        char* name = NULL;
        if(CheckMTPStringHasUnicode(ptr)) {
            // unicode isn't supported
            name = is_dir ? "New Folder" : "New File";
        } else {
            name = ReadMTPString(ptr);
        }

        // if the name is blank, generate random name
        if(*name == 0) {
            char* random_name = malloc(sizeof(char) * 10);
            strcpy(random_name, "file_");
            int random = rand() % 1000;
            char random_str[4];
            itoa(random, random_str, 10);
            strcpy(random_name + 5, random_str);
            name = random_name;
        }

        FURI_LOG_I("MTP", "Creating object: %s", name);

        uint32_t storage_id = persistence.params[0];
        uint32_t parent = persistence.params[1];

        char* base_path = get_base_path_from_storage_id(storage_id);
        if(base_path == NULL) {
            FURI_LOG_E("MTP", "Invalid storage ID: %ld", storage_id);
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_STORAGE_ID, persistence.transaction_id, NULL);
            break;
        }

        if(parent != 0xffffffff) {
            base_path = get_path_from_handle(mtp, parent);
            if(base_path == NULL) {
                FURI_LOG_E("MTP", "Invalid parent handle: %ld", parent);
                send_mtp_response(
                    mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, persistence.transaction_id, NULL);
                break;
            }
        }

        char* full_path = malloc(sizeof(char) * MTP_PATH_SIZE);
        merge_path(full_path, base_path, name);

        FURI_LOG_I("MTP", "Format: %04x", info->format);

        if(is_dir) {
            if(!storage_dir_exists(mtp->storage, full_path)) {
                if(!storage_simply_mkdir(mtp->storage, full_path)) {
                    FURI_LOG_E("MTP", "Failed to create directory: %s", full_path);
                    send_mtp_response(
                        mtp, 3, MTP_RESP_GENERAL_ERROR, persistence.transaction_id, NULL);
                    free(full_path);
                    break;
                }
            }
        } else {
            if(!storage_file_exists(mtp->storage, full_path)) {
                // create file
                File* file = storage_file_alloc(mtp->storage);
                if(!storage_file_open(file, full_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    FURI_LOG_E("MTP", "Failed to create file: %s", full_path);
                    send_mtp_response(
                        mtp, 3, MTP_RESP_GENERAL_ERROR, persistence.transaction_id, NULL);
                    storage_file_free(file);
                    free(full_path);
                    break;
                }

                storage_file_close(file);
                storage_file_free(file);

                persistence.prev_handle = issue_object_handle(mtp, full_path);
            }
        }

        uint32_t handle = issue_object_handle(mtp, full_path);
        persistence.params[2] = handle;

        free(name);
        free(full_path);
        send_mtp_response(mtp, 3, MTP_RESP_OK, persistence.transaction_id, persistence.params);

        break;
    }
    default:
        FURI_LOG_W("MTP", "Unsupported MTP operation in bulk transfer: 0x%04x", persistence.op);
        send_mtp_response(mtp, 3, MTP_RESP_UNKNOWN, persistence.transaction_id, NULL);
        break;
    }

    free(persistence.global_buffer);
    persistence.global_buffer = NULL;
}

void handle_mtp_response(AppMTP* mtp, struct MTPHeader* header) {
    UNUSED(mtp);
    FURI_LOG_I("MTP", "Handling MTP response: 0x%04x", header->op);
    FURI_LOG_I("MTP", "Transaction ID: %ld", header->transaction_id);
    FURI_LOG_I("MTP", "Has Data: %d", persistence.global_buffer != NULL);
    FURI_LOG_I("MTP", "Data length: %ld", persistence.buffer_offset);
}

void handle_mtp_command(AppMTP* mtp, struct MTPContainer* container) {
    uint16_t mtp_op = container->header.op;
    FURI_LOG_I("MTP", "Handling MTP operation: 0x%04x", mtp_op);

    switch(mtp_op) {
    case MTP_OP_GET_DEVICE_INFO:
        FURI_LOG_I("MTP", "GetDeviceInfo operation");
        send_device_info(mtp, container->header.transaction_id);
        // Process the GetDeviceInfo operation
        break;
    case MTP_OP_OPEN_SESSION:
    case MTP_OP_CLOSE_SESSION:
        //FURI_LOG_I("MTP", "Open/CloseSession operation (STUB)");
        send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);
        break;
    case MTP_OP_GET_STORAGE_IDS:
        FURI_LOG_I("MTP", "GetStorageIDs operation");
        send_storage_ids(mtp, container->header.transaction_id);
        break;
    case MTP_OP_GET_STORAGE_INFO: {
        FURI_LOG_I("MTP", "GetStorageInfo operation");
        uint8_t* info = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
        int length = GetStorageInfo(mtp, container->params[0], info);
        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_STORAGE_INFO,
            container->header.transaction_id,
            info,
            length);
        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        free(info);
        break;
    }
    case MTP_OP_GET_OBJECT_HANDLES:
        FURI_LOG_I("MTP", "GetObjectHandles operation");
        if(container->params[1]) {
            send_mtp_response_buffer(
                mtp,
                MTP_TYPE_RESPONSE,
                MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED,
                container->header.transaction_id,
                NULL,
                0);
            break;
        } else {
            uint8_t* buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
            int length = GetObjectHandles(mtp, container->params[0], container->params[2], buffer);
            send_mtp_response_buffer(
                mtp,
                MTP_TYPE_DATA,
                MTP_OP_GET_OBJECT_HANDLES,
                container->header.transaction_id,
                buffer,
                length);

            send_mtp_response_buffer(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
            free(buffer);
        }
        break;
    case MTP_OP_GET_OBJECT_INFO: {
        FURI_LOG_I("MTP", "GetObjectInfo operation");
        uint8_t* buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
        int length = GetObjectInfo(mtp, container->params[0], buffer);

        if(length < 0) {
            send_mtp_response(
                mtp,
                MTP_TYPE_RESPONSE,
                MTP_RESP_INVALID_OBJECT_HANDLE,
                container->header.transaction_id,
                NULL);
            break;
        }

        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_OBJECT_INFO,
            container->header.transaction_id,
            buffer,
            length);
        free(buffer);

        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        break;
    }
    case MTP_OP_GET_OBJECT_PROPS_SUPPORTED: {
        FURI_LOG_I("MTP", "GetObjectPropsSupported operation");
        uint8_t* buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
        uint32_t count = sizeof(supported_object_props) / sizeof(uint16_t);
        memcpy(buffer, &count, sizeof(uint32_t));
        memcpy(buffer + sizeof(uint32_t), supported_object_props, sizeof(uint16_t) * count);

        int length = sizeof(uint32_t) + sizeof(uint16_t) * count;
        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_OBJECT_PROPS_SUPPORTED,
            container->header.transaction_id,
            buffer,
            length);
        free(buffer);
        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        break;
    }
    case MTP_OP_DELETE_OBJECT:
        FURI_LOG_I("MTP", "DeleteObject operation");
        if(DeleteObject(mtp, container->params[0])) {
            send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);
        } else {
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, container->header.transaction_id, NULL);
        }
        break;

    case MTP_OP_GET_DEVICE_PROP_VALUE:
        FURI_LOG_I("MTP", "GetDevicePropValue operation");
        GetDevicePropValue(mtp, container->header.transaction_id, container->params[0]);
        // Process the GetDevicePropValue operation
        break;
    case MTP_OP_GET_DEVICE_PROP_DESC:
        FURI_LOG_I("MTP", "GetDevicePropDesc operation");
        GetDevicePropDesc(mtp, container->header.transaction_id, container->params[0]);
        // Process the GetDevicePropDesc operation
        break;
    // Handle bulk transfer specific operations
    case MTP_OP_SEND_OBJECT_INFO:
        FURI_LOG_I("MTP", "SendObjectInfo operation");
        setup_persistence(container);
        break;
    case MTP_OP_SEND_OBJECT:
        FURI_LOG_I("MTP", "SendObject operation");
        setup_persistence(container);
        break;
    case MTP_OP_GET_OBJECT: {
        FURI_LOG_I("MTP", "GetObject operation");
        GetObject(mtp, container->header.transaction_id, container->params[0]);

        break;
    }
    case MTP_OP_MOVE_OBJECT: {
        FURI_LOG_I("MTP", "MoveObject operation");
        // Process the MoveObject operation
        MoveObject(
            mtp,
            container->header.transaction_id,
            container->params[0],
            container->params[1],
            container->params[2]);
        break;
    }
    case MTP_OP_POWER_DOWN: {
        FURI_LOG_I("MTP", "PowerDown operation");

        FURI_LOG_I("MTP", "Good night.");
        send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);

        furi_hal_power_off();
        break;
    }
    // Handle other bulk operations here...
    case MTP_OP_GET_OBJECT_PROP_VALUE: {
        FURI_LOG_I("MTP", "GetObjectPropValue operation");
        // Process the GetObjectPropValue operation
        GetObjectPropValue(
            mtp, container->header.transaction_id, container->params[0], container->params[1]);
        break;
    }
    case MTP_OP_SET_OBJECT_PROP_VALUE: {
        FURI_LOG_I("MTP", "SetObjectPropValue operation");
        setup_persistence(container);
        break;
    }
    default:
        FURI_LOG_W("MTP", "Unsupported MTP operation in bulk transfer: 0x%04x", mtp_op);
        send_mtp_response(mtp, 3, MTP_RESP_UNKNOWN, container->header.transaction_id, NULL);
        break;
    }
}

void send_storage_ids(AppMTP* mtp, uint32_t transaction_id) {
    uint32_t count;
    uint32_t storage_ids[3];
    GetStorageIDs(mtp, storage_ids, &count);

    FURI_LOG_I("MTP", "Sending storage IDs: %ld storages", count);
    uint32_t payload[3] = {count, storage_ids[0], storage_ids[1]};
    send_mtp_response_buffer(
        mtp,
        MTP_TYPE_DATA,
        MTP_OP_GET_STORAGE_IDS,
        transaction_id,
        (uint8_t*)payload,
        sizeof(uint32_t) * (count + 1));
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
}

void send_device_info(AppMTP* mtp, uint32_t transaction_id) {
    uint8_t* response = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
    int length = BuildDeviceInfo(response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_INFO, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
    free(response);
}

int GetNumObjects(AppMTP* mtp, uint32_t storage_id, uint32_t association) {
    return list_and_issue_handles(mtp, storage_id, association, NULL);
}
void send_mtp_response_stream(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    void* callback_context,
    int (*callback)(void* ctx, uint8_t* buffer, int length),
    uint32_t length) {
    int chunk_idx = 0;

    size_t buffer_available = MTP_MAX_PACKET_SIZE;
    uint8_t* buffer = malloc(sizeof(uint8_t) * buffer_available);
    uint8_t* ptr = buffer;

    uint32_t sent_length = 0;
    FURI_LOG_I("MTP", "Sending MTP response stream: %ld bytes", length);

    do {
        buffer_available = MTP_MAX_PACKET_SIZE;
        ptr = buffer; // reset the pointer

        if(chunk_idx == 0) {
            struct MTPHeader* hdr = (struct MTPHeader*)buffer;
            hdr->len = length + sizeof(*hdr);
            hdr->type = resp_type;
            hdr->op = resp_code;
            hdr->transaction_id = transaction_id;

            ptr += sizeof(*hdr);
            buffer_available -= sizeof(*hdr);
        }

        FURI_LOG_I("MTP", "Remaining bytes for packet: %d", buffer_available);

        int read_bytes = callback(callback_context, ptr, buffer_available);
        uint32_t usb_bytes = (ptr - buffer) + read_bytes;
        FURI_LOG_I("MTP", "USB packet size: %ld", usb_bytes);

        usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, buffer, usb_bytes);

        sent_length += usb_bytes;
        chunk_idx++;

        FURI_LOG_I(
            "MTP",
            "Sent chunk %d (currently sent: %ld/%ld)",
            chunk_idx,
            sent_length - sizeof(struct MTPHeader),
            length);
    } while(sent_length < length + sizeof(struct MTPHeader));

    free(buffer);
}

int send_mtp_response_buffer_callback(void* ctx, uint8_t* buffer, int size) {
    struct MTPResponseBufferContext* context = (struct MTPResponseBufferContext*)ctx;
    if(context->buffer == NULL || context->size == 0) {
        return 0;
    }

    uint32_t remaining = context->size - context->sent;
    uint32_t to_send = size;
    if(remaining < to_send) {
        to_send = remaining;
    }

    memcpy(buffer, context->buffer + context->sent, to_send);
    context->sent += to_send;

    return to_send;
}

void send_mtp_response_buffer(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint8_t* buffer,
    uint32_t size) {
    struct MTPResponseBufferContext* ctx = malloc(sizeof(struct MTPResponseBufferContext));
    ctx->buffer = buffer;
    ctx->size = size;
    ctx->sent = 0;

    send_mtp_response_stream(
        mtp, resp_type, resp_code, transaction_id, ctx, send_mtp_response_buffer_callback, size);
    free(ctx);
}

void send_mtp_response(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint32_t* params) {
    uint32_t response[5] = {0};

    if(params != NULL) {
        memcpy(response, params, sizeof(uint32_t) * 5);
    }

    send_mtp_response_buffer(
        mtp, resp_type, resp_code, transaction_id, (uint8_t*)response, sizeof(response));
}

int mtp_handle_class_control(AppMTP* mtp, usbd_device* dev, usbd_ctlreq* req) {
    UNUSED(dev);

    int value = -1;
    uint32_t handle = req->wIndex;
    uint8_t* buffer = req->data;
    uint32_t size = req->wLength;

    UNUSED(handle);
    UNUSED(buffer);
    UNUSED(size);

    switch(req->bRequest) {
    case MTP_REQ_CANCEL:
        // Handle Cancel request
        value = 0;
        break;
    case MTP_REQ_GET_EXT_EVENT_DATA:
        // Handle GetExtEventData request
        value = 0;
        break;
    case MTP_REQ_RESET:
        // Handle Reset request
        mtp->state = MTPStateReady;
        value = 0;
        break;
    case MTP_REQ_GET_DEVICE_STATUS:
        // Handle GetDeviceStatus request
        struct mtp_device_status* status = (struct mtp_device_status*)req->data;
        status->wLength = sizeof(*status);
        status->wCode = (mtp->state == MTPStateBusy) ? MTP_RESP_DEVICE_BUSY : MTP_RESP_OK;
        value = status->wLength;
        break;
    default:
        // Unsupported request
        break;
    }

    return value;
}
