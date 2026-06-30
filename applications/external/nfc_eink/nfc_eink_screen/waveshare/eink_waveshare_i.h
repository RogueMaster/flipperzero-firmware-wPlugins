#pragma once
#include "../nfc_eink_screen_i.h"
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>

#define EINK_WAVESHARE_COMMAND_SPECIFIC       0xCD
#define EINK_WAVESHARE_COMMAND_READ_PAGE      0x30
#define EINK_WAVESHARE_COMMAND_WRITE_PAGE     0xA2
#define EINK_WAVESHARE_COMMAND_FF_FE          0xFF
#define EINK_WAVESHARE_COMMAND_SELECT_TYPE    0x00
#define EINK_WAVESHARE_COMMAND_INIT           0x0D
#define EINK_WAVESHARE_COMMAND_DATA_WRITE     0x08
#define EINK_WAVESHARE_COMMAND_POWER_ON_V2    0x18
#define EINK_WAVESHARE_COMMAND_DATA_WRITE_V2  0x19
#define EINK_WAVESHARE_COMMAND_NORMAL_MODE    0x01
#define EINK_WAVESHARE_COMMAND_SET_CONFIG_1   0x02
#define EINK_WAVESHARE_COMMAND_POWER_ON       0x03
#define EINK_WAVESHARE_COMMAND_POWER_OFF      0x04
#define EINK_WAVESHARE_COMMAND_SET_CONFIG_2   0x05
#define EINK_WAVESHARE_COMMAND_LOAD_TO_MAIN   0x06
#define EINK_WAVESHARE_COMMAND_PREPARE_DATA   0x07
#define EINK_WAVESHARE_COMMAND_REFRESH        0x09
#define EINK_WAVESHARE_COMMAND_WAIT_FOR_READY 0x0A

typedef enum {
    NfcEinkWaveshareListenerStateIdle,
    NfcEinkWaveshareListenerStateWaitingForConfig,
    NfcEinkWaveshareListenerStateReadingBlocks,
    NfcEinkWaveshareListenerStateUpdatedSuccefully,
} NfcEinkWaveshareListenerStates;

typedef enum {
    EinkWavesharePollerStateReadPages,
    EinkWavesharePollerStateInit,
    EinkWavesharePollerStateSelectType,
    EinkWavesharePollerStateSetNormalMode,
    EinkWavesharePollerStateSetConfig1,
    EinkWavesharePollerStatePowerOn,
    EinkWavesharePollerStateSetConfig2,
    EinkWavesharePollerStateLoadToMain,
    EinkWavesharePollerStatePrepareData,
    EinkWavesharePollerStateSendImageData,
    EinkWavesharePollerStatePowerOnV2,
    EinkWavesharePollerStateRefresh,
    EinkWavesharePollerStateWaitReady,
    EinkWavesharePollerStateRetry,
    EinkWavesharePollerStateFinish,
    EinkWavesharePollerStateError,
    EinkWavesharePollerStateNum
} EinkWavesharePollerState;

typedef struct {
    NfcEinkWaveshareListenerStates listener_state;
    EinkWavesharePollerState poller_state;
    uint8_t poller_retry_cnt;
    size_t data_index;
    uint16_t block_number; // TODO: try to remove this
    uint8_t buf[16 * 4];
} NfcEinkWaveshareSpecificContext;

#define eink_waveshare_on_done(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeFinish)

#define eink_waveshare_on_config_received(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeConfigurationReceived)

#define eink_waveshare_on_target_detected(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetDetected)

#define eink_waveshare_on_target_lost(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeTargetLost)

#define eink_waveshare_on_block_processed(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeBlockProcessed)

#define eink_waveshare_on_updating(instance) \
    nfc_eink_screen_vendor_callback(instance, NfcEinkScreenEventTypeUpdating)

NfcCommand eink_waveshare_listener_callback(NfcGenericEvent event, void* context);
NfcCommand eink_waveshare_poller_callback(NfcGenericEvent event, void* context);

void eink_waveshare_parse_config(NfcEinkScreen* screen, const uint8_t* data, uint8_t data_length);
