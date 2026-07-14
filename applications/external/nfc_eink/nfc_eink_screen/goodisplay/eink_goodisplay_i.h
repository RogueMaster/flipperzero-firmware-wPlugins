#pragma once

#include "../nfc_eink_screen_i.h"
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/protocols/nfc_generic_event.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_listener.h>

#define eink_goodisplay_on_done(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeFinish)

#define eink_goodisplay_on_config_received(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeConfigurationReceived)

#define eink_goodisplay_on_target_detected(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetDetected)

#define eink_goodisplay_on_target_lost(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetLost)

#define eink_goodisplay_on_block_processed(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeBlockProcessed)

#define eink_goodisplay_on_updating(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeUpdating)

typedef struct {
    uint8_t CLA_byte;
    uint8_t CMD_code;
    uint8_t P1;
    uint8_t P2;
} FURI_PACKED APDU_Header;

typedef struct {
    APDU_Header header;
    uint8_t data_length;
    uint8_t data[];
} FURI_PACKED APDU_Command;

typedef APDU_Command APDU_Command_Select;

typedef struct {
    APDU_Header header;
    uint8_t expected_length;
} FURI_PACKED APDU_Command_Read;

typedef struct {
    uint8_t command_code;
    uint8_t command_data[];
} FURI_PACKED NfcEinkScreenCommand;

typedef struct {
    uint16_t status;
} FURI_PACKED APDU_Response;

typedef struct {
    uint16_t data_length; ///TODO: remove this to CC_File struct
    uint8_t data[];
} FURI_PACKED APDU_Response_Read;

typedef struct {
    uint16_t length;
    uint8_t ndef_message[];
} FURI_PACKED NDEF_File;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t file_id;
    uint16_t file_size;
    uint8_t read_access_flags;
    uint8_t write_access_flags;
} FURI_PACKED NDEF_File_Ctrl_TLV;

typedef struct {
    ///TODO: uint16_t data_length; //maybe this should be here instead of APDU_Response_Read
    uint8_t mapping_version;
    uint16_t r_apdu_max_size;
    uint16_t c_apdu_max_size;
    NDEF_File_Ctrl_TLV ctrl_file;
} FURI_PACKED CC_File;

typedef struct {
    uint8_t response_code;
    union {
        APDU_Response apdu_response;
        APDU_Response_Read apdu_response_read;
    } apdu_resp;
} FURI_PACKED NfcEinkGoodisplayScreenResponse;

typedef enum {
    EinkGoodisplayPollerStateSendC2Cmd,
    EinkGoodisplayPollerStateSelectNDEFTagApp,
    EinkGoodisplayPollerStateSelectNDEFFile,
    EinkGoodisplayPollerStateReadFIDFileData,
    EinkGoodisplayPollerStateSelect0xE104File,
    EinkGoodisplayPollerStateRead0xE104FileData,
    EinkGoodisplayPollerStateSendConfigCmd,
    EinkGoodisplayPollerStateDuplicateC2Cmd,
    EinkGoodisplayPollerStateSendDataCmd,
    EinkGoodisplayPollerStateApplyImage,
    EinkGoodisplayPollerStateWaitUpdateDone,
    EinkGoodisplayPollerStateSendDataDone,

    EinkGoodisplayPollerStateError,
    EinkGoodisplayPollerStateNum
} EinkGoodisplayPollerState;

typedef enum {
    EinkGoodisplayListenerStateIdle,
    EinkGoodisplayListenerStateWaitingForConfig,
    EinkGoodisplayListenerStateReadingBlocks,
    EinkGoodisplayListenerStateUpdatedSuccefully,
} EinkGoodisplayListenerState;

/// -----------------------
typedef struct {
    EinkGoodisplayPollerState poller_state;
    EinkGoodisplayListenerState listener_state;
    bool was_update;
    uint8_t update_cnt;
    uint8_t response_cnt;

    uint16_t block_number;
    size_t data_index;
} NfcEinkScreenSpecificGoodisplayContext;

void eink_goodisplay_parse_config(NfcEinkScreen* screen, const uint8_t* data, uint8_t data_length);
NfcCommand eink_goodisplay_listener_callback(NfcGenericEvent event, void* context);
NfcCommand eink_goodisplay_poller_callback(NfcGenericEvent event, void* context);
