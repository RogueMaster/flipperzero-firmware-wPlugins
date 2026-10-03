#pragma once

#include "subghz_wardriving_txrx.h"

struct SubGhzWarDrivingTxRx {
    SubGhzWarDrivingWorker* worker;

    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzTransmitter* transmitter;
    SubGhzProtocolDecoderBase* decoder_result;
    FlipperFormat* fff_data;

    SubGhzRadioPreset* preset;
    SubGhzSetting* setting;

    uint8_t hopper_timeout;
    uint8_t hopper_idx_frequency;
    bool is_database_loaded;
    SubGhzHopperState hopper_state;

    SubGhzTxRxState txrx_state;
    SubGhzSpeakerState speaker_state;
    const SubGhzDevice* radio_device;
    SubGhzRadioDeviceType radio_device_type;

    SubGhzTxRxNeedSaveCallback need_save_callback;
    void* need_save_context;

    // Kept here so the lazily (re)allocated receiver can be restored to the
    // filter/callback the app last asked for.
    SubGhzProtocolFlag filter;
    SubGhzReceiverCallback rx_callback;
    void* rx_callback_context;

    bool debug_pin_state;

    float latitude;
    float longitude;
};
