#pragma once

#include <furi.h>
#include <furi_hal.h>

/** SubGhzTxRx state */
typedef enum {
    SubGhzTxRxStateIDLE,
    SubGhzTxRxStateRx,
    SubGhzTxRxStateTx,
    SubGhzTxRxStateSleep,
} SubGhzTxRxState;

/** SubGhzHopperState state */
typedef enum {
    SubGhzHopperStateOFF,
    SubGhzHopperStateRunning,
    SubGhzHopperStatePause,
    SubGhzHopperStateRSSITimeOut,
} SubGhzHopperState;

/** SubGhzSpeakerState state */
typedef enum {
    SubGhzSpeakerStateDisable,
    SubGhzSpeakerStateShutdown,
    SubGhzSpeakerStateEnable,
} SubGhzSpeakerState;

/** SubGhzRadioDeviceType */
typedef enum {
    SubGhzRadioDeviceTypeAuto,
    SubGhzRadioDeviceTypeInternal,
    SubGhzRadioDeviceTypeExternalCC1101,
} SubGhzRadioDeviceType;

typedef enum {
    SubGhzViewIdFrequencyAnalyzer,

} SubGhzViewId;
