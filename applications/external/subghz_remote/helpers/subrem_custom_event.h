#pragma once

typedef enum {
    SubRemEditMenuStateUP = 0,
    SubRemEditMenuStateDOWN,
    SubRemEditMenuStateLEFT,
    SubRemEditMenuStateRIGHT,
    SubRemEditMenuStateOK,
} SubRemEditMenuState;

typedef enum {
    // StartSubmenuIndex
    SubmenuIndexSubRemOpenMapFile = 0,
    SubmenuIndexSubRemEditMapFile,
    SubmenuIndexSubRemNewMapFile,
    SubmenuIndexSubRemSetDefault,
    SubmenuIndexSubRemClearDefault,
#ifdef FURI_DEBUG
    SubmenuIndexSubRemRemoteView,
#endif
    // SubmenuIndexSubRemAbout,

    // Submenu enter selection
    SubRemCustomEventEnterEditLabel,
    SubRemCustomEventEnterEditFile,

    // SubRemCustomEvent
    SubRemCustomEventViewRemoteStartUP = 100,
    SubRemCustomEventViewRemoteStartDOWN,
    SubRemCustomEventViewRemoteStartLEFT,
    SubRemCustomEventViewRemoteStartRIGHT,
    SubRemCustomEventViewRemoteStartOK,
    SubRemCustomEventViewRemoteBack,
    SubRemCustomEventViewRemoteStop,
    SubRemCustomEventViewRemoteForcedStop,

    SubRemCustomEventViewEditMenuBack,
    SubRemCustomEventViewEditMenuUP,
    SubRemCustomEventViewEditMenuDOWN,
    SubRemCustomEventViewEditMenuEdit,
    SubRemCustomEventViewEditMenuSave,

    SubRemCustomEventSceneEditSubmenu,
    SubRemCustomEventSceneEditLabelInputDone,
    SubRemCustomEventSceneEditLabelWidgetAccess,
    SubRemCustomEventSceneEditLabelWidgetBack,
    SubRemCustomEventSceneEditButtonInputDone,
    SubRemCustomEventSceneEditButtonWidgetAccess,
    SubRemCustomEventSceneEditButtonWidgetBack,

    SubRemCustomEventSceneEditOpenSubErrorPopup,

    SubRemCustomEventSceneEditPreviewSaved,

    SubRemCustomEventSceneNewName,

#ifdef FW_ORIGIN_Official
    SubRemCustomEventSceneFwWarningExit,
    SubRemCustomEventSceneFwWarningNext,
    SubRemCustomEventSceneFwWarningContinue,
#endif

} SubRemCustomEvent;

typedef enum {
    EditSubmenuIndexEditLabel,
    EditSubmenuIndexEditFile,
    EditSubmenuIndexEditButton,
    EditSubmenuIndexClearSlot,
} SubRemEditSubmenuIndex;
