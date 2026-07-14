#pragma once

static const uint16_t supported_operations[] = {
    MTP_OP_GET_DEVICE_INFO,
    MTP_OP_OPEN_SESSION,
    MTP_OP_CLOSE_SESSION,
    MTP_OP_GET_STORAGE_IDS,
    MTP_OP_GET_STORAGE_INFO,
    MTP_OP_GET_NUM_OBJECTS,
    MTP_OP_GET_OBJECT_HANDLES,
    MTP_OP_GET_OBJECT_INFO,
    MTP_OP_GET_OBJECT,
    MTP_OP_DELETE_OBJECT,
    MTP_OP_SEND_OBJECT_INFO,
    MTP_OP_SEND_OBJECT,
    MTP_OP_GET_DEVICE_PROP_DESC,
    MTP_OP_GET_DEVICE_PROP_VALUE,
    MTP_OP_MOVE_OBJECT};

static const uint16_t supported_object_props[] = {
    MTP_PROP_STORAGE_ID,
    MTP_PROP_OBJECT_FORMAT,
    MTP_PROP_OBJECT_FILE_NAME,
};

static const uint16_t supported_device_properties[] = {
    0xd402, // Device Friendly Name
    0x5001 // Battery Level
};

static const uint16_t supported_playback_formats[] = {MTP_FORMAT_UNDEFINED, MTP_FORMAT_ASSOCIATION};
