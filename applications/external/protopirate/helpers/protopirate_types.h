// helpers/protopirate_types.h
#pragma once

#include <furi.h>
#include <furi_hal.h>

typedef enum {
    ProtoPirateViewVariableItemList,
    ProtoPirateViewSubmenu,
    ProtoPirateViewWidget,
    ProtoPirateViewReceiver,
    ProtoPirateViewReceiverInfo,
    ProtoPirateViewAbout,
    ProtoPirateViewFileBrowser,
} ProtoPirateView;

typedef enum {
    // Custom events for views
    ProtoPirateCustomEventViewReceiverOK,
    ProtoPirateCustomEventViewReceiverConfig,
    ProtoPirateCustomEventViewReceiverBack,
    ProtoPirateCustomEventViewReceiverUnlock,
    // Custom events for scenes
    ProtoPirateCustomEventSceneReceiverUpdate,
    ProtoPirateCustomEventSceneSettingLock,
    // File management
    ProtoPirateCustomEventReceiverInfoSave,
    ProtoPirateCustomEventReceiverInfoEmulate,
    ProtoPirateCustomEventReceiverInfoBruteforceStart,
    ProtoPirateCustomEventReceiverInfoBruteforceCancel,
    ProtoPirateCustomEventSavedInfoDelete,
    // Emulator
    ProtoPirateCustomEventSavedInfoEmulate,
    ProtoPirateCustomEventEmulateTransmit,
    ProtoPirateCustomEventEmulateStop,
    ProtoPirateCustomEventEmulateExit,
    // Sub decode
    ProtoPirateCustomEventSubDecodeUpdate,
    ProtoPirateCustomEventSubDecodeSave,
    ProtoPirateCustomEventSubDecodeBruteforceStart,
    ProtoPirateCustomEventPsaBruteforceComplete,
    // File Browser
    ProtoPirateCustomEventSavedFileSelected,
    // Need saving confirmation
    ProtoPirateCustomEventSceneStay,
    ProtoPirateCustomEventSceneExit,
} ProtoPirateCustomEvent;

typedef enum {
    ProtoPirateLockOff,
    ProtoPirateLockOn,
} ProtoPirateLock;

typedef enum {
    ProtoPirateTxRxStateIDLE,
    ProtoPirateTxRxStateRx,
    ProtoPirateTxRxStateTx,
    ProtoPirateTxRxStateSleep,
} ProtoPirateTxRxState;

typedef enum {
    ProtoPirateHopperStateOFF,
    ProtoPirateHopperStateRunning,
    ProtoPirateHopperStatePause,
    ProtoPirateHopperStateRSSITimeOut,
} ProtoPirateHopperState;

typedef enum {
    ProtoPirateRxKeyStateIDLE,
    ProtoPirateRxKeyStateBack,
    ProtoPirateRxKeyStateStart,
    ProtoPirateRxKeyStateAddKey,
} ProtoPirateRxKeyState;
