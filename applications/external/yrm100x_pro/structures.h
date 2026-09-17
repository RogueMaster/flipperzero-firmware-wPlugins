#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>
#include <gui/elements.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <toolbox/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers/uhf_reader_settings.h"
#include "helpers/yrm100x_uart.h"
#include "helpers/yrm100x_buffer.h"
#include "helpers/yrm100x_worker.h"
#include "helpers/small_submenu.h"
#include <YRM100X_PRO_icons.h>
//Submenu enums for different screens
typedef enum {
    UHFReaderSubmenuIndexRead,
    UHFReaderSubmenuIndexDelete,
    UHFReaderSubmenuIndexTagInfo,
    UHFReaderSubmenuIndexTagSave,
    UHFReaderSubmenuIndexTagRename,
    UHFReaderSubmenuIndexEpcLock,
    UHFReaderSubmenuIndexTidLock,
    UHFReaderSubmenuIndexUserLock,
    UHFReaderSubmenuIndexApLock,
    UHFReaderSubmenuIndexKillLock,
    UHFReaderSubmenuIndexTagWrite,
    UHFReaderSubmenuIndexTagLock,
    UHFReaderSubmenuIndexTagKill,
    UHFReaderSubmenuIndexSetKillPwd,
    UHFReaderSubmenuIndexKillTag,
    UHFReaderSubmenuIndexTagDelete,
    UHFReaderSubmenuIndexSaved,
    UHFReaderSubmenuIndexConfig,
    UHFReaderSubmenuIndexAbout,
    UHFReaderSubmenuIndexStartReading,
    UHFReaderSubmenuIndexStartWriting,
    UHFReaderSubmenuIndexSelectTag,
    UHFReaderSubmenuIndexSetPower,
    UHFReaderSubmenuIndexTagAction,
    UHFReaderSubmenuIndexSetAccessPwd,
    UHFReaderSubmenuIndexTagClone,
    UHFReaderSubmenuIndexTagClonePc3400,
    UHFReaderSubmenuIndexUpdateAP,
    UHFReaderSubmenuIndexUpdateKP,
    UHFReaderSubmenuIndexReadSingle,
    UHFReaderSubmenuIndexReadFullSingle,
    UHFReaderSubmenuIndexReadFullMulti,
    UHFReaderSubmenuIndexReadForever,
    UHFReaderSubmenuIndexGetSizeBank,
    UHFReaderSubmenuIndexTestUmi,
    UHFReaderSubmenuIndexBankTools,
    UHFReaderSubmenuIndexCheckTagRewritable,
} UHFReaderSubmenuIndex;

//Defining views for the application
typedef enum {
    UHFReaderViewSubmenu,
    UHFReaderViewDeleteSuccess,
    UHFReaderViewConfigure,
    UHFReaderViewEpcDump,
    UHFReaderViewEpcInfo,
    UHFReaderViewBankMem,
    UHFReaderViewDelete,
    UHFReaderViewAbout,
    UHFReaderViewRead,
    UHFReaderViewSaveInput,
    UHFReaderViewRenameInput,
    UHFReaderViewEpcWriteInput,
    UHFReaderViewWrite,
    UHFReaderViewLock,
    UHFReaderViewKill,
    UHFReaderViewSaved,
    UHFReaderViewTagAction,
    UHFReaderViewSelectSavedTag,
    UHFReaderViewSetPower,
    UHFReaderViewSetReadAp,
    UHFReaderViewSetKillPwd,
    UHFReaderViewSetAccessPwd,
    UHFReaderViewKillConfirm,
    UHFReaderViewLockPopup,
    UHFReaderViewCloneBanks,
    UHFReaderViewClonePc3400Info,
    UHFReaderViewClone,
    UHFReaderViewCurrentApInput,
    UHFReaderViewNewApInput,
    UHFReaderViewCurrentKpInput,
    UHFReaderViewNewKpInput,
    UHFReaderViewDeleteAllConfirm,
    UHFReaderViewUmiTest,
    UHFReaderViewBankSize,
    UHFReaderViewBankTools,
    UHFReaderViewRewritableConfirm,
    UHFReaderViewRewritable,
    UHFReaderViewAntennaInfo,
} UHFReaderView;

//Event IDs for the app
typedef enum {
    UHFReaderEventIdRedrawScreen = 0,
    UHFReaderEventIdOkPressed = 42,
    UHFCustomEventReserved = 100,
    UHFCustomEventWorkerExit = 105,
    UHFCustomEventWorkerExitAborted = 106,
    UHFCustomEventWorkerExitAccessDenied = 107,
    UHFCustomEventWorkerExitWrongPassword = 108,
    UHFCustomEventDeepReadDone = 108,
    UHFCustomEventDeepReadAborted = 109,
    UHFCustomEventSingleReadDone = 110,
    UHFCustomEventSingleReadAborted = 111,
    UHFCustomEventCloneScanDone = 112,
    UHFCustomEventCloneWriteDone = 113,
    UHFCustomEventCloneWriteFail = 114,
    UHFCustomEventCloneAccessDenied = 115,
    UHFCustomEventCloneScanTimeout = 116,
    UHFCustomEventMultiInventoryDone = 117,
    UHFCustomEventMultiFullDumpDone = 118,
    UHFCustomEventUmiTestDone = 119,
    UHFCustomEventUmiTestFail = 120,
    UHFCustomEventBankSizeDone = 121,
    UHFCustomEventBankSizeFail = 122,
    UHFCustomEventRewritableDone = 123,
    UHFCustomEventRewritableFail = 124,
    UHFCustomEventCloneProgress = 125,
    UHFCustomEventCloneModeMismatch = 126,
    UHFCustomEventCloneMarkerUnknown = 127,
} UHFReaderEventId;

//LED blinking notification sequence
static const NotificationSequence uhf_sequence_blink_start_cyan = {
    &message_blink_start_10,
    &message_blink_set_color_cyan,
    &message_do_not_reset,
    NULL,
};

//LED blinking notification sequence for stopping
static const NotificationSequence uhf_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

/*
 * Read Full(Multi) uses two deliberately different standard sounds:
 * - EPC acquired: sequence_semi_success
 * - full bank dump completed: sequence_success
 *
 * Keeping both as firmware-provided sequences avoids custom-audio lifetime issues.
 */

//Structure to track the state of a variable item in a list and if it is locked
typedef struct VariableItemLock {
    bool locked;
} VariableItemLock;

//Context describing which path opened the shared Tag Action menu.
//ActionFromLive  -> unsaved, in-memory scanned tag (Update/Lock/Kill, targeted by EPC)
//ActionFromSaved -> saved tag from Saved_EPCs.txt (full menu, single-poll writes)
typedef enum {
    ActionFromLive,
    ActionFromSaved,
} UHFActionContext;

//The main UHFReaderApp Struct
typedef struct {
    ViewDispatcher* ViewDispatcher;
    NotificationApp* Notifications;

    Submenu* Submenu;
    SmallSubmenu* SubmenuSaved;
    Submenu* SubmenuTagActions;
    Submenu* SubmenuLockActions;
    Submenu* SubmenuKillActions;
    Submenu* SubmenuBankTools;

    TextInput* TextInput;
    ByteInput* ApInput;
    ByteInput* KillInput;
    ByteInput* SetApInput;
    ByteInput* KillConfirmInput;
    ByteInput* CurrentApInput;
    ByteInput* NewApInput;
    ByteInput* CurrentKpInput;
    ByteInput* NewKpInput;

    TextInput* SaveInput;
    TextInput* RenameInput;
    TextInput* EpcWrite;

    Popup* LockPopup;
    DialogEx* DeleteAllDialog;
    DialogEx* RewritableConfirmDialog;

    VariableItemList* VariableItemListConfig;
    VariableItemList* VariableItemListLock;
    VariableItem* ConnectionItem;
    VariableItem* Setting2Item;
    VariableItem* SettingApPwdItem;
    VariableItem* SettingLockApPwdItem;
    VariableItem* SettingLockResultItem;
    VariableItem* WriteSettingApPwdItem;
    VariableItem* SavingSelectionItem;
    VariableItem* BaudSelection;
    VariableItem* RegionSelection;
    VariableItem* SessionSelection;
    VariableItem* TargetSelection;
    VariableItem* MultiFullDumpSelection;
    VariableItem* AutoSaveMultiSelection;
    VariableItem* TagProfileSelection;
    VariableItem* FullDumpAttemptsSelection;
    VariableItem* CloneAttemptsSelection;
    VariableItem* ForeverDelaySelection;
    VariableItem* SoundSelection;
    VariableItem* VibrationSelection;
    Widget* WidgetAbout;

    View* ViewRead;
    View* ViewWrite;
    View* ViewDelete;
    View* ViewLock;
    View* ViewDeleteSuccess;
    View* ViewEpc;
    View* ViewEpcInfo;
    View* ViewBankMem;
    View* ViewClone;
    View* ViewUmiTest;
    View* ViewBankSize;
    View* ViewRewritable;
    View* ViewAntennaInfo;
    VariableItemList* VariableItemListClone;
    VariableItem* CloneEpcItem;
    VariableItem* CloneTidItem;
    VariableItem* CloneUserItem;
    VariableItem* CloneResItem;
    VariableItem* CloneStartItem;
    DialogEx* ClonePc3400InfoDialog;
    UHFTag* CloneSourceTag;
    uint16_t CloneMask;
    bool ClonePc3400Mode;

    // Source view model + return view for the shared Up-key save flow
    View* SaveSourceView;
    uint32_t SaveReturnView;

    char* TempBuffer;
    uint8_t* ApTempBuffer;
    uint8_t* KillPwdTempBuffer;
    uint8_t* KillConfirmPwdTempBuffer;
    uint8_t* SetPwdTempBuffer;
    uint8_t* CurrentApBuffer;
    uint8_t* NewApBuffer;
    uint8_t* CurrentKpBuffer;
    uint8_t* NewKpBuffer;
    char* TempSaveBuffer;
    char* EpcToSave;
    char* Setting1ConfigLabel;
    char* Setting1Names[2];
    char* SettingLockBankConfigLabel;
    char* SettingLockBankNames[5];
    char* SettingLockActionConfigLabel;
    char* SettingLockActionNames[4];
    char* Setting2ConfigLabel;
    char* Setting2EntryText;
    char* Setting2DefaultValue;
    char* SettingSavingNames[2];
    char* SettingBaudNames[3];
    char* SettingRegionNames[5];
    char* SettingSessionNames[4];
    char* SettingTargetNames[2];
    char* SettingMultiFullNames[2];
    char* SettingAutoSaveMultiNames[2];
    char* SettingTagProfileNames[UHF_READER_TAG_PROFILE_COUNT];
    char* SettingSoundNames[2];
    char* SettingVibrationNames[2];
    char* SettingSavingConfigLabel;
    char* SettingBaudConfigLabel;
    char* SettingRegionConfigLabel;
    char* SettingSessionConfigLabel;
    char* SettingTargetConfigLabel;
    char* SettingMultiFullConfigLabel;
    char* SettingAutoSaveMultiConfigLabel;
    char* SettingTagProfileConfigLabel;
    char* SettingFullDumpAttemptsConfigLabel;
    char* SettingCloneAttemptsConfigLabel;
    char* SettingForeverDelayConfigLabel;
    char* SettingSoundConfigLabel;
    char* SettingVibrationConfigLabel;
    char* SettingLockExecuteConfigLabel;
    char* SettingLockExecuteResult;
    char MainMenuHeader[32];

    uint32_t TempBufferSize;
    uint8_t ApInputBufferSize;
    uint8_t KillPwdInputBufferSize;
    uint32_t TempBufferSaveSize;
    uint32_t SelectedTagIndex;
    uint32_t SavedPage;
    uint32_t NumberOfSavedTags;
    uint32_t NumberOfTidsToRead;
    uint32_t NumberOfResToRead;
    uint32_t NumberOfMemToRead;
    uint32_t CurEpcIndex;
    uint32_t CurTidIndex;
    uint32_t CurResIndex;
    uint32_t CurMemIndex;
    uint32_t UHFBaudRate;

    FuriString* EpcNameDelete;
    FuriString* EpcDelete;
    FuriString* EpcName;
    FuriString* Setting2PowerStr;
    FuriString* DefaultLockAccessPwdStr;
    FuriString* DefaultLockResultStr;
    FuriString* EpcToWrite;
    FuriString* DefaultAccessPwdStr;

    bool IsReading;
    bool IsWriting;
    bool ReaderConnected;
    bool DeepReading;
    bool DeepReadDone;
    bool DeepReadTimerExpired;
    // True once reader-owned defaults have been adopted or a saved profile was loaded.
    // A false profile may still persist app-only preferences such as Save on Write.
    bool SettingsInitialized;

    //True when the Read view is operating in single-tag live mode (Read (Single)),
    //false for the default simultaneous multi-tag scan (Read (Multi)).
    bool SingleReadMode;
    bool FullSingleReadMode;
    bool FullMultiReadMode;
    bool ForeverReadMode;

    // Multi mode options / state.
    bool MultiDumpInProgress;
    size_t LastMultiBeepCount;
    size_t LastFullMultiEpcBeepCount;
    size_t LastFullMultiDumpBeepCount;
    size_t LastFullMultiPartialBeepCount;

    /*
     * The periodic FuriTimer is NOT the GUI thread.
     * It may only enqueue one redraw/service event. All notifications,
     * storage writes and submenu mutations are then done by the dispatcher
     * callback on the GUI thread.
     */
    volatile bool FullMultiUiEventPending;

    //Which path opened the shared Tag Action menu (live vs saved).
    UHFActionContext ActionContext;

    //True on a fresh entry into the Write/Update view from the action menu, so
    //the enter callback only resets the bank selection once (not when returning
    //from the value-entry keyboard).
    bool WriteMenuFreshEntry;

    FuriTimer* Timer;

    Storage* TagStorage;
    FlipperFormat* EpcFile;
    FlipperFormat* EpcIndexFile;
    UHFUmiMarkerRegistry UmiMarkerRegistry;

    size_t NumberOfEpcsToRead;

    uint8_t SettingSavingIndex;
    uint8_t UHFSaveType;
    uint8_t SettingBaudIndex;
    uint8_t SettingRegionIndex;
    uint8_t SettingSessionIndex;
    uint8_t SettingTargetIndex;
    uint8_t SettingPowerIndex;
    uint8_t SettingMultiFullIndex;
    uint8_t SettingAutoSaveMultiIndex;
    uint8_t SettingTagProfileIndex;
    uint8_t SettingFullDumpAttempts;
    uint8_t SettingCloneAttempts;
    uint8_t SettingForeverDelaySeconds;
    uint8_t SettingSoundIndex;
    uint8_t SettingVibrationIndex;
    uint8_t SettingLockBankIndex;
    uint8_t SettingLockActionIndex;
    uint8_t Setting1Index;
    uint8_t Setting1Values[2];
    uint8_t SettingLockBankValues[5];
    uint8_t SettingLockActionValues[4];
    uint8_t SettingSavingValues[2];
    uint8_t SettingBaudValues[3];
    uint8_t SettingRegionValues[5];
    uint8_t UHFModuleType;
    uint8_t UHFRegionType;

    UHFWorker* YRM100XWorker;

    char* ReadAccessPasswordLabel;
    char* AccessPasswordPlaceHolder;
    char* DefaultAccessPassword;

    char* SettingApLabel;
    char* SettingApDefaultPassword;

    char* KillPasswordPlaceHolder;
    char* DefaultKillPassword;

    char* SetAccessPasswordPlaceHolder;
    char* KillConfirmPasswordPlaceHolder;

    BankType DefaultLockBank;
    LockType DefaultLockType;

    //Buffers for YRM100 functionality
    size_t EpcBytesLen;
    size_t ResBytesLen;
    size_t TidBytesLen;
    size_t UserBytesLen;
    size_t PcBytesLen;
    size_t CrcBytesLen;
    uint8_t* EpcBytes;
    uint8_t* ResBytes;
    uint8_t* TidBytes;
    uint8_t* UserBytes;
    uint16_t* PcBytes;
    uint16_t* CrcBytes;

    // Add tracking for locked items
    VariableItemLock* item_locks;
    size_t num_items;

} UHFReaderApp;

//The model for the configure/read screen
typedef struct {
    uint32_t Setting1Index;
    FuriString* Setting2Power;
    FuriString* SettingReadAp;

    bool IsReading;
    FuriString* EpcName;
    uint32_t CurEpcIndex;
    FuriString* EpcValue;
    uint32_t NumEpcsRead;
    FuriString* Setting1Value;
    FuriString* Pc;
    FuriString* Crc;
    int32_t Rssi; // Live RSSI (dBm) of the currently shown tag; used as an aiming meter
    uint32_t ScrollOffset;
    char* ScrollingText;
    bool IsScrolling;
    bool IsDumping;
    uint32_t DumpCurrent;
    uint32_t DumpTotal;
    bool IsFullMultiMode;
    bool IsFullSingleMode;
    bool IsForeverMode;
    uint32_t ForeverTotalEpcs;
    bool ForeverWaiting;
    uint8_t ForeverWaitSeconds;
    bool IsSingleMode;
    bool SingleTidAvailable;
    bool SingleResAvailable;
    bool SingleUserAvailable;
    uint32_t FullMultiDumped;
    uint32_t FullMultiPartial;
} UHFReaderConfigModel;

//Model for the write screen
typedef struct {
    uint32_t Setting1Index;
    FuriString* Setting2Power;
    bool IsWriting;
    FuriString* EpcName;
    FuriString* WriteFunction;
    FuriString* EpcValue;
    FuriString* WriteStatus;
    FuriString* NewEpcValue;
    FuriString* TidValue;
    FuriString* NewTidValue;
    FuriString* ResValue;
    FuriString* NewResValue;
    FuriString* MemValue;
    FuriString* NewMemValue;
    FuriString* Setting1Value;
    FuriString* Crc;
    FuriString* Pc;
    FuriString* SettingKillPwd;
    //Bank availability for the Update menu (live tags grey out unread banks)
    bool TidAvail;
    bool UserAvail;
    bool ResAvail;
    //True once the user has picked a bank + entered a value (shows Write button)
    bool BankChosen;
    //True when this is a targeted "Update" of a specific scanned tag (live)
    bool IsUpdateMode;
    //True once the live-update AP prompt has been completed for the current session
    bool WriteApPromptDone;
    //Scratch banks live inside the view model. This avoids six independent
    //unchecked heap allocations and their allocator overhead/fragmentation.
    uint8_t EpcBuffer[EPC_MAX_BANK_SIZE];
    uint8_t ResBuffer[RESERVED_MAX_BANK_SIZE];
    uint8_t TidBuffer[TID_MAX_BANK_SIZE];
    uint8_t UserBuffer[USER_MAX_BANK_SIZE];
    uint16_t PcBuffer[2];
    uint16_t CrcBuffer[2];
} UHFReaderWriteModel;

//Model for the delete screen
typedef struct {
    uint32_t SelectedTagIndex;
    FuriString* SelectedTagName;
    FuriString* SelectedTagEpc;
    uint32_t ScrollOffset;
} UHFReaderDeleteModel;

//Model use for handling UHF RFID tag data
typedef struct {
    FuriString* Reserved;
    FuriString* Epc;
    FuriString* Tid;
    FuriString* User;
    FuriString* Crc;
    FuriString* Pc;
    int8_t Rssi; // Last RSSI reading from the YRM100 poll frame (signed dBm)
    uint32_t CurEpcIndex;
    uint32_t ScrollOffsetEpc;
    char* ScrollingTextEpc;
    uint32_t ScrollOffsetTid;
    char* ScrollingTextTid;
    uint32_t ScrollOffsetRes;
    char* ScrollingTextRes;
    uint32_t ScrollOffsetMem;
    char* ScrollingTextMem;
    bool IsDeepReading;
    bool DeepReadDone;
    // Per-bank memory screen: which bank is shown (0=TID, 1=Reserved, 2=User)
    // and the vertical scroll offset (in wrapped lines) for long content.
    uint32_t CurrentBank;
    uint32_t VScrollLine;
    // Whether each memory bank has been read for this live tag. Drives the
    // Update menu's grey-out (EPC is always available from the scan).
    bool TidBankRead;
    bool UserBankRead;
    bool ResBankRead;
} UHFRFIDTagModel;

// Phase of the clone scan/write screen
typedef enum {
    ClonePhaseScanning,
    ClonePhaseConfirm,
    ClonePhaseModeMismatch,
    ClonePhaseMarkerUnknown,
    ClonePhaseWriting,
    ClonePhaseSuccess,
    ClonePhaseFailed,
    ClonePhaseAccessDenied,
    ClonePhaseTimeout,
} ClonePhase;

// View model for the clone scan/write screen
typedef struct {
    ClonePhase phase;
    char target_epc_str[65]; // hex string of found target tag EPC (up to 32B = 64 hex chars + NUL)
    uint16_t clone_mask; // which banks are being written (copy of App->CloneMask)
    uint8_t clone_count; // number of successful clones this session
    uint8_t clone_attempt_current;
    uint8_t clone_attempt_total;
    uint16_t source_pc;
    uint16_t target_pc;
    bool mode_conversion_applied;
    bool pc3400_mode;
    bool pc3400_marker_applied;
    bool pc3400_verified;
    bool marker_known;
    uint8_t marker_model_id[UHF_UMI_MODEL_ID_SIZE];
    uint16_t marker_word;
    uint16_t marker_value;
    uint16_t final_pc;
    uint16_t final_crc;
    uint16_t reserved_source_bytes;
    uint16_t reserved_target_bytes;
    uint16_t reserved_written_bytes;
    uint16_t reserved_skipped_bytes;
} UHFReaderCloneModel;

typedef enum {
    UmiTestPhaseReady = 0,
    UmiTestPhaseRunning,
    UmiTestPhaseResult,
    UmiTestPhaseFailed,
    UmiTestPhaseRestoreFailed,
} UmiTestPhase;

typedef struct {
    UmiTestPhase phase;
    UHFUmiTestOutcome outcome;
    UHFUmiTestStage failure_stage;
    uint16_t pc_before;
    uint16_t pc_during;
    uint16_t pc_after;
    uint8_t user_before[2];
    uint8_t user_during[2];
    uint8_t user_after[2];
    uint16_t user_bytes;
    uint16_t best_word;
    uint16_t best_value;
    uint16_t best_mask;
    uint16_t pc3000_bits;
    uint16_t pc3400_bits;
    uint8_t model_id[UHF_UMI_MODEL_ID_SIZE];
    bool model_valid;
    bool marker_saved;
    uint16_t current_word;
    uint16_t current_value;
    uint16_t candidates_tested;
    uint16_t position_tests;
    uint16_t bit_tests;
    bool best_found;
    bool best_single_bit;
    bool temporary_write_done;
    bool restore_verified;
    M100ResponseType last_status;
} UHFReaderUmiTestModel;

typedef enum {
    BankSizePhaseReady = 0,
    BankSizePhaseRunning,
    BankSizePhaseResult,
    BankSizePhaseFailed,
} BankSizePhase;

typedef struct {
    BankSizePhase phase;
    uint16_t epc_bytes;
    uint16_t tid_bytes;
    uint16_t user_bytes;
    uint16_t reserved_bytes;
    UHFSizeBankStatus epc_status;
    UHFSizeBankStatus tid_status;
    UHFSizeBankStatus user_status;
    UHFSizeBankStatus reserved_status;
    uint16_t pc;
    uint16_t crc;
} UHFReaderBankSizeModel;

typedef enum {
    RewritablePhaseRunning = 0,
    RewritablePhaseResult,
    RewritablePhaseFailed,
} RewritablePhase;

typedef struct {
    RewritablePhase phase;
    UHFRewriteBankStatus epc_status;
    UHFRewriteBankStatus tid_status;
    UHFRewriteBankStatus user_status;
    UHFRewriteBankStatus reserved_status;
} UHFReaderRewritableModel;
