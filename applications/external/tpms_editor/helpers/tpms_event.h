#pragma once

typedef enum {
    //TPMSCustomEvent
    TPMSCustomEventStartId = 100,

    TPMSCustomEventSceneSettingLock,
    TPMSCustomEventSceneOpenRelearn,
    TPMSCustomEventSceneRelearnStart,

    TPMSCustomEventViewReceiverOK,
    TPMSCustomEventViewReceiverConfig,
    TPMSCustomEventViewReceiverBack,
    TPMSCustomEventViewReceiverOffDisplay,
    TPMSCustomEventViewReceiverUnlock,
    TPMSCustomEventViewReceiverRelearn,
    TPMSCustomEventViewReceiverInfoEdit,
} TPMSCustomEvent;
