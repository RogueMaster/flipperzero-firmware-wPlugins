#pragma once

#include <furi.h>
#include <furi_hal.h>

#define TPMS_VERSION_APP "4.5.3"
#define TPMS_DEVELOPED "ProtoView core + altruista86 UI + TxF bits"
#define TPMS_GITHUB "https://github.com/teiksef/TPMS-Editor-TxF"

/* LF wake profiles. TPMS 3.8 keeps Relearn inside the normal RX18 scanner.
   CW Relearn can use 125.0 or 134.2 kHz. EL-50448 stays at 125 kHz CW,
   while Ford EL-50449 uses the measured gated 125 kHz data telegram. */
#define TPMS_LF_CARRIER_HZ 125000U
#define TPMS_LF_CARRIER_1342_HZ 134200U
#define TPMS_RELEARN_COMMON_DURATION_MS 3000U
#define TPMS_RELEARN_COMMON_CARRIER_DUTY 0.5f
#define TPMS_EL50448_DURATION_MS 5000U
#define TPMS_EL50448_CARRIER_DUTY 0.125f
#define TPMS_FORD_EL50449_DURATION_MS 5000U
#define TPMS_FORD_EL50449_AUTO_DURATION_MS 2500U
#define TPMS_FORD_EL50449_HALF_BIT_US 128U
#define TPMS_FORD_EL50449_CARRIER_DUTY 0.125f
#define TPMS_FORD_EL50449_FRAME_GAP_MS 25U

#define TPMS_RELEARN_RX_WAIT_5_MS 5000U
#define TPMS_RELEARN_RX_WAIT_10_MS 10000U
#define TPMS_RELEARN_RX_WAIT_15_MS 15000U

// Up to 128 decoded bits covers all currently registered protocols. For saving unknown data, at least 256 is needed
#define TPMS_RAWBITS_BUF 128U

#define TPMS_KEY_FILE_VERSION 1
#define TPMS_KEY_FILE_TYPE "Flipper Tire Pressure Monitoring System Key File"

/** TPMSRxKeyState state */
typedef enum {
    TPMSRxKeyStateIDLE,
    TPMSRxKeyStateBack,
    TPMSRxKeyStateStart,
    TPMSRxKeyStateAddKey,
} TPMSRxKeyState;

/** TPMSHopperState state */
typedef enum {
    TPMSHopperStateOFF,
    TPMSHopperStateRunnig,
    TPMSHopperStatePause,
    TPMSHopperStateRSSITimeOut,
} TPMSHopperState;

typedef enum {
    TPMSLockOff,
    TPMSLockOn,
} TPMSLock;

typedef enum {
    TPMSRadioModeAuto = 0,
    TPMSRadioModeInternal,
    TPMSRadioModeExternal,
} TPMSRadioMode;

typedef enum {
    TPMSViewVariableItemList,
    TPMSViewSubmenu,
    TPMSViewReceiver,
    TPMSViewReceiverInfo,
    TPMSViewWidget,
    TPMSViewTextInput,
    TPMSViewNumberInput,
} TPMSView;

/** TPMSTxRx state */
typedef enum {
    TPMSTxRxStateIDLE,
    TPMSTxRxStateRx,
    TPMSTxRxStateTx,
    TPMSTxRxStateSleep,
} TPMSTxRxState;

typedef enum {
    TPMSRelearnOff,
    TPMSRelearnOn,
} TPMSRelearn;

typedef enum {
    TPMSRelearnTypeCommon = 0,
    TPMSRelearnTypeEL50448,
    TPMSRelearnTypeFordEL50449,
} TPMSRelearnType;

typedef enum {
    TPMSRelearnRXBand315 = 0,
    TPMSRelearnRXBand433,
    TPMSRelearnRXBandAuto,
    TPMSRelearnRXBandCount,
} TPMSRelearnRXBand;

typedef enum {
    TPMSRelearnModAuto = 0,
    TPMSRelearnModFSK,
    TPMSRelearnModOOK,
    TPMSRelearnModGFSK,
    TPMSRelearnModCount,
} TPMSRelearnModulation;


typedef enum {
    TPMSCWFrequency125 = 0,
    TPMSCWFrequency1342,
    TPMSCWFrequencyCount,
} TPMSCWFrequency;

typedef enum {
    TPMSRelearnRepeatManual = 0,
    TPMSRelearnRepeatAuto,
    TPMSRelearnRepeatCount,
} TPMSRelearnRepeatMode;

typedef enum {
    TPMSRelearnRXWait5 = 0,
    TPMSRelearnRXWait10,
    TPMSRelearnRXWait15,
    TPMSRelearnRXWaitCount,
} TPMSRelearnRXWait;

typedef enum {
    TPMSFordLFProfile5A5A = 0,
    TPMSFordLFProfileVDO,
    TPMSFordLFProfileAuto,
    TPMSFordLFProfileCount,
} TPMSFordLFProfile;
