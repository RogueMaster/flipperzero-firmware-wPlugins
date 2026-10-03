// helpers/protopirate_types.h
#pragma once

#include <furi.h>
#include <furi_hal.h>

typedef enum {
    ProtoPirateViewVariableItemList,
    ProtoPirateViewSubmenu,
    ProtoPirateViewWidget,
    ProtoPirateViewReceiver,
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
    ProtoPirateCustomEventViewReceiverHopperUpdate,
    // Custom events for scenes
    ProtoPirateCustomEventSceneReceiverUpdate,
    ProtoPirateCustomEventReceiverDeferredRxStart,
    ProtoPirateCustomEventSceneSettingLock,
    // File management
    ProtoPirateCustomEventReceiverInfoSave,
    ProtoPirateCustomEventReceiverInfoSaveConfirm,
    ProtoPirateCustomEventReceiverInfoUpdate,
    ProtoPirateCustomEventReceiverInfoEmulate,
    ProtoPirateCustomEventSavedInfoDelete,
    //Bruteforcing PSA & Renault
    ProtoPirateCustomEventBruteforceStart,
    ProtoPirateCustomEventBruteforceComplete,
    // Emulator
    ProtoPirateCustomEventEmulateTransmit,
    ProtoPirateCustomEventEmulateStop,
    ProtoPirateCustomEventEmulateExit,
    ProtoPirateCustomEventSavedInfoEmulate,
    // Sub decode
    ProtoPirateCustomEventSubDecodeUpdate,
    ProtoPirateCustomEventSubDecodeSave,
    ProtoPirateCustomEventSubDecodeEmulate,
    ProtoPirateCustomEventSubDecodeSaveConfirm,
    // File Browser
    ProtoPirateCustomEventSavedFileSelected,
    // Need saving confirmation
    ProtoPirateCustomEventSceneStay,
    ProtoPirateCustomEventSceneExit,
    // About scene
    ProtoPirateCustomEventAboutToggleEmulate,
    //Plugin Scenes
    ProtoPirateCustomEventPluginNavigateBack,
    ProtoPirateCustomEventPluginNavigateEmulate,
    ProtoPirateCustomEventPluginNavigateConfig,
    ProtoPirateCustomEventPluginNavigateStopApp,
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
