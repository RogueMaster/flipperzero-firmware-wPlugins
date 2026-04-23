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
    ProtoPirateViewTextInput,
} ProtoPirateView;

typedef enum {
    // Custom events for views
    ProtoPirateCustomEventViewReceiverOK,
    ProtoPirateCustomEventViewReceiverConfig,
    ProtoPirateCustomEventViewReceiverBack,
    ProtoPirateCustomEventViewReceiverDeleteItem,
    ProtoPirateCustomEventViewReceiverUnlock,
    // Custom events for scenes
    ProtoPirateCustomEventSceneReceiverUpdate,
    ProtoPirateCustomEventReceiverDeferredRxStart,
    ProtoPirateCustomEventSceneSettingLock,
    // File management
    ProtoPirateCustomEventReceiverInfoSave,
    ProtoPirateCustomEventReceiverInfoSaveConfirm,
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
