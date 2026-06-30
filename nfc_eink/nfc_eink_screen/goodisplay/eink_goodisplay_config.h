#pragma once
#include "eink_goodisplay_i.h"

typedef enum {
    NfcEinkGoodisplayScreenResolution2n13inch = 0x00,
    NfcEinkGoodisplayScreenResolution2n9inch = 0x01,
    NfcEinkGoodisplayScreenResolution1n54inch = 0x12,
    NfcEinkGoodisplayScreenResolution3n71inch = 0x51,
} NfcEinkGoodisplayScreenResolution;

typedef enum {
    NfcEinkGoodisplayScreenChannelBlackWhite = 0x20,
    NfcEinkGoodisplayScreenChannelBlackWhiteRed = 0x30,
} NfcEinkGoodisplayScreenChannel;

typedef struct {
    NfcEinkGoodisplayScreenResolution screen_resolution;
    NfcEinkGoodisplayScreenChannel screen_channel;
    uint16_t height;
    uint16_t width;
} FURI_PACKED NfcEinkGoodisplayScreenTypeData;

typedef struct {
    uint8_t inner_cmd_code;
    uint8_t length;
} FURI_PACKED EinkGoodisplayConfigPackHeader;

typedef struct {
    EinkGoodisplayConfigPackHeader header;
    uint8_t display_cmd_code;
} FURI_PACKED EinkGoodisplayCommandPackBase;

typedef EinkGoodisplayCommandPackBase EinkGoodisplayCommandWaitBusy;
typedef EinkGoodisplayCommandPackBase EinkGoodisplayCommandSoftReset;
typedef EinkGoodisplayCommandPackBase EinkGoodisplayCommandWriteRam;
typedef EinkGoodisplayCommandPackBase EinkGoodisplayCommandMasterActivation;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint8_t value;
} FURI_PACKED EinkGoodisplayCommandWithOneArg;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint16_t MUX_gate;
    uint8_t gate_scan_direction;
} FURI_PACKED EinkGoodisplayCommandDriverOutputCtrl;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint8_t data_entry_mode;
} FURI_PACKED EinkGoodisplayCommandDataEntryMode;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint8_t start_address;
    uint8_t end_address;
} FURI_PACKED EinkGoodisplayCommandSetRamAddrBoundsX;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint8_t ram_bypass_inverse_option;
    uint8_t source_output_mode;
} FURI_PACKED EinkGoodisplayCommandDisplayUpdateCtrl1;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint16_t start_address;
    uint16_t end_address;
} FURI_PACKED EinkGoodisplayCommandSetRamAddrBoundsY;

typedef EinkGoodisplayCommandWithOneArg EinkGoodisplayCommandBorderWaveformCtrl;
typedef EinkGoodisplayCommandWithOneArg EinkGoodisplayCommandTempSensorCtrl;
typedef EinkGoodisplayCommandWithOneArg EinkGoodisplayCommandSetRamAddrX;
typedef EinkGoodisplayCommandWithOneArg EinkGoodisplayCommandDisplayUpdateCtrl2;
typedef EinkGoodisplayCommandWithOneArg EinkGoodisplayCommandDisplayDeepSleepMode;
typedef EinkGoodisplayCommandWithOneArg EinkGoodisplayCommandUnknown;

typedef struct {
    EinkGoodisplayCommandPackBase base;
    uint16_t value;
} FURI_PACKED EinkGoodisplayCommandSetRamAddrY;

typedef struct {
    EinkGoodisplayConfigPackHeader header;
    NfcEinkGoodisplayScreenTypeData screen_data;
} FURI_PACKED EinkGoodisplaySizeConfigPack;

typedef struct {
    EinkGoodisplayCommandWaitBusy wait;
    EinkGoodisplayCommandUnknown unknown;
} FURI_PACKED EinkGoodisplayWaitBusySequence;

typedef struct {
    EinkGoodisplayCommandWaitBusy wait_1;
    EinkGoodisplayCommandSoftReset reset;
    EinkGoodisplayCommandWaitBusy wait_2;
} FURI_PACKED EinkGoodisplayResetSequence;

typedef struct {
    APDU_Header apdu;
    uint8_t data_length;
    EinkGoodisplaySizeConfigPack eink_size_config;
    EinkGoodisplayWaitBusySequence wait_busy[3];
    EinkGoodisplayResetSequence reset_sequence;
    EinkGoodisplayCommandDriverOutputCtrl driver_output_ctrl;
    EinkGoodisplayCommandDataEntryMode data_entry_mode;
    EinkGoodisplayCommandSetRamAddrBoundsX x_address_bounds;
    EinkGoodisplayCommandSetRamAddrBoundsY y_address_bounds;
    EinkGoodisplayCommandBorderWaveformCtrl border_waveform_ctrl;

    EinkGoodisplayCommandDisplayUpdateCtrl1 update_ctrl1;

    EinkGoodisplayCommandTempSensorCtrl temp_sensor;
    EinkGoodisplayCommandSetRamAddrX x_address_current;
    EinkGoodisplayCommandSetRamAddrY y_address_current;
    EinkGoodisplayCommandWriteRam write_ram;
    EinkGoodisplayCommandDisplayUpdateCtrl2 update_ctrl2;
    EinkGoodisplayCommandMasterActivation master_activation;
    EinkGoodisplayCommandWaitBusy wait_after_update;
    EinkGoodisplayCommandDisplayDeepSleepMode deep_sleep_mode;
    EinkGoodisplayCommandUnknown unknown_cmd;
} FURI_PACKED EinkGoodisplayConfigPack;

EinkGoodisplayConfigPack* eink_goodisplay_config_pack_alloc(size_t* config_length);
void eink_goodisplay_config_pack_free(EinkGoodisplayConfigPack* pack);
void eink_goodisplay_config_pack_set_by_screen_info(
    EinkGoodisplayConfigPack* pack,
    const NfcEinkScreenInfo* info);
const char* eink_goodisplay_config_get_screen_name(const uint8_t* data, uint8_t data_length);
