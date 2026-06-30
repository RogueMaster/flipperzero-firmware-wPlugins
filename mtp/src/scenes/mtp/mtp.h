#pragma once

#include "main.h"

// Block size for storage operations
#define BLOCK_SIZE 65536

// MTP Device Serial
#define MTP_DEVICE_FALLBACK_SERIAL \
    "HakureiReimu" // If you found this, Thank you for checking my (shitty) code!

#define MTP_STANDARD_VERSION         100
#define MTP_VENDOR_EXTENSION_ID      0x6
#define MTP_VENDOR_EXTENSION_VERSION 100
#define MTP_FUNCTIONAL_MODE          0x0

#define MTP_TYPE_COMMAND  0x1
#define MTP_TYPE_DATA     0x2
#define MTP_TYPE_RESPONSE 0x3

#define MTP_REQ_CANCEL             0x64
#define MTP_REQ_GET_EXT_EVENT_DATA 0x65
#define MTP_REQ_RESET              0x66
#define MTP_REQ_GET_DEVICE_STATUS  0x67

// MTP Operation Codes
#define MTP_OP_GET_DEVICE_INFO    0x1001
#define MTP_OP_OPEN_SESSION       0x1002
#define MTP_OP_CLOSE_SESSION      0x1003
#define MTP_OP_GET_STORAGE_IDS    0x1004
#define MTP_OP_GET_STORAGE_INFO   0x1005
#define MTP_OP_GET_NUM_OBJECTS    0x1006
#define MTP_OP_GET_OBJECT_HANDLES 0x1007
#define MTP_OP_GET_OBJECT_INFO    0x1008
#define MTP_OP_GET_OBJECT         0x1009

#define MTP_OP_DELETE_OBJECT         0x100B
#define MTP_OP_SEND_OBJECT_INFO      0x100C
#define MTP_OP_SEND_OBJECT           0x100D
#define MTP_OP_POWER_DOWN            0x1013
#define MTP_OP_GET_DEVICE_PROP_DESC  0x1014
#define MTP_OP_GET_DEVICE_PROP_VALUE 0x1015

#define MTP_OP_MOVE_OBJECT 0x1019

#define MTP_OP_GET_OBJECT_PROPS_SUPPORTED 0x9801
#define MTP_OP_GET_OBJECT_PROP_DESC       0x9802
#define MTP_OP_GET_OBJECT_PROP_VALUE      0x9803
#define MTP_OP_SET_OBJECT_PROP_VALUE      0x9804

// MTP Object Props
#define MTP_PROP_STORAGE_ID       0xDC01
#define MTP_PROP_OBJECT_FORMAT    0xDC02
#define MTP_PROP_OBJECT_FILE_NAME 0xDC07

// MTP Response Codes
#define MTP_RESP_UNKNOWN                    0x2000
#define MTP_RESP_OK                         0x2001
#define MTP_RESP_GENERAL_ERROR              0x2002
#define MTP_RESP_SESSION_NOT_OPEN           0x2003
#define MTP_RESP_INVALID_TRANSACTION_ID     0x2004
#define MTP_RESP_OPERATION_NOT_SUPPORTED    0x2005
#define MTP_RESP_PARAMETER_NOT_SUPPORTED    0x2006
#define MTP_RESP_INCOMPLETE_TRANSFER        0x2007
#define MTP_RESP_INVALID_STORAGE_ID         0x2008
#define MTP_RESP_INVALID_OBJECT_HANDLE      0x2009
#define MTP_RESP_DEVICE_PROP_NOT_SUPPORTED  0x200A
#define MTP_RESP_STORE_FULL                 0x200C
#define MTP_RESP_OBJECT_WRITE_PROTECTED     0x200D
#define MTP_RESP_STORE_READ_ONLY            0x200E
#define MTP_RESP_ACCESS_DENIED              0x200F
#define MTP_RESP_NO_THUMBNAIL_PRESENT       0x2010
#define MTP_RESP_SELF_TEST_FAILED           0x2011
#define MTP_RESP_PARTIAL_DELETION           0x2012
#define MTP_RESP_STORE_NOT_AVAILABLE        0x2013
#define MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED 0x2014
#define MTP_RESP_NO_VALID_OBJECT_INFO       0x2015
#define MTP_RESP_INVALID_CODE_FORMAT        0x2016
#define MTP_RESP_UNKNOWN_VENDOR_CODE        0x2017
#define MTP_RESP_CAPTURE_ALREADY_TERMINATED 0x2018
#define MTP_RESP_DEVICE_BUSY                0x2019
#define MTP_RESP_INVALID_OBJECT_PROP_CODE   0xA801

// Storage IDs
#define INTERNAL_STORAGE_ID 0x00010001
#define EXTERNAL_STORAGE_ID 0x00020001

// MTP Playback formats
#define MTP_FORMAT_UNDEFINED   0x3000
#define MTP_FORMAT_ASSOCIATION 0x3001

// Buffer size configuration
#define MTP_BUFFER_SIZE 1024 // General purpose buffer size
#define MTP_PATH_SIZE   256 // Maximum path length
#define MTP_NAME_SIZE   256 // Maximum filename length

// File transfer configuration
#define MTP_FILE_SYNC_INTERVAL (1024 * 1024) // Sync interval in bytes (1MB)

typedef struct {
    uint32_t handle;
    char name[256];
    uint32_t size;
    uint32_t parent_handle;
    bool is_directory;
} MTPObject;

struct mtp_device_status {
    uint16_t wLength;
    uint16_t wCode;
};

// Storage information structure based on MTP specification
typedef struct {
    uint16_t storage_type;
    uint16_t filesystem_type;
    uint16_t access_capability;
    uint64_t max_capacity;
    uint64_t free_space_in_bytes;
    uint32_t free_space_in_objects;
} MTPStorageInfoHeader;

typedef struct ObjectInfoHeader {
    uint32_t storage_id;
    uint16_t format;
    uint16_t protection_status;
    uint32_t compressed_size;
    uint16_t thumb_format;
    uint32_t thumb_compressed_size;
    uint32_t thumb_pix_width;
    uint32_t thumb_pix_height;
    uint32_t image_pix_width;
    uint32_t image_pix_height;
    uint32_t image_bit_depth;
    uint32_t parent_object;
    uint16_t association_type;
    uint32_t association_desc;
} ObjectInfoHeader;

struct MTPHeader {
    uint32_t len; // 0
    uint16_t type; // 4
    uint16_t op; // 6
    uint32_t transaction_id; // 8
};

typedef struct MTPDataPersistence {
    uint32_t left_bytes;
    uint8_t* global_buffer;
    uint32_t buffer_offset;

    uint16_t op;
    uint32_t transaction_id;
    uint32_t params[5];

    uint32_t prev_handle;
    File* current_file;
} MTPDataPersistence;

struct MTPContainer {
    struct MTPHeader header; // 0
    uint32_t params[5]; // 12
};

void OpenSession(AppMTP* mtp, uint32_t session_id);
void CloseSession(AppMTP* mtp);

void mtp_handle_bulk(AppMTP* mtp, uint8_t* buffer, uint32_t length);
void handle_mtp_command(AppMTP* mtp, struct MTPContainer* container);
void handle_mtp_data_packet(AppMTP* mtp, uint8_t* buffer, int32_t length, bool cont);
void handle_mtp_response(AppMTP* mtp, struct MTPHeader* container);
int mtp_handle_class_control(AppMTP* mtp, usbd_device* dev, usbd_ctlreq* req);
void send_mtp_response(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint32_t* params);
void send_mtp_response_buffer(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint8_t* buffer,
    uint32_t size);
void send_mtp_response_stream(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    void* callback_context,
    int (*callback)(void* ctx, uint8_t* buffer, int length),
    uint32_t length);

void send_device_info(AppMTP* mtp, uint32_t transaction_id);
void send_storage_ids(AppMTP* mtp, uint32_t transaction_id);

void handle_mtp_data_complete(AppMTP* mtp);

struct MTPResponseBufferContext {
    uint8_t* buffer;
    uint32_t size;
    uint32_t sent;
};

// Object context for file operations
struct GetObjectContext {
    File* file;
};
