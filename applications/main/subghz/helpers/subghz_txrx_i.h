#pragma once

#include "subghz_txrx.h"

struct SubGhzTxRx {
    SubGhzWorker* worker;

    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzProtocolFlag receiver_filter;
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
    // Keep the requested external preference when a missing module falls back to internal.
    bool radio_device_external_wanted;
    // Last probe of any kind; a failed module is not searched for again immediately.
    uint32_t radio_device_probe_tick;
    // Do not disable a rail that was already enabled before this helper acquired it.
    bool radio_device_otg_owned;

    SubGhzTxRxNeedSaveCallback need_save_callback;
    void* need_save_context;
    //True when the last TX transmitted the internal fff_data buffer (i.e. a
    //signal bound to subghz->file_path), as opposed to a history/RX signal.
    //Used to gate the post-TX dynamic save-back so it never writes an
    //unrelated file
    bool tx_from_internal_fff;

    size_t tx_min_heap_required;

    bool debug_pin_state;

    //Total duration of every sample handed to the decoders, in microseconds.
    //This is a clock that only advances while a signal is actually being
    //decoded, so it measures the air between two decoded frames rather than
    //the wall time between the moments the app was told about them
    uint64_t air_time_us;
};
