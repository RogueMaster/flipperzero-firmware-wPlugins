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
#include "helpers/uart_helper.h"
#include "helpers/yrm100x_uart.h"
#include "helpers/yrm100x_buffer.h"
#include "helpers/yrm100x_worker.h"
#include <simultaneous_rfid_reader_icons.h>
//Submenu enums for different screens
typedef enum {
    UHFReaderSubmenuIndexRead,
    UHFReaderSubmenuIndexDelete,
    UHFReaderSubmenuIndexTagInfo,
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
} UHFReaderSubmenuIndex;

//Defining views for the application
typedef enum {
    UHFReaderViewSubmenu,
    UHFReaderViewDeleteSuccess,
    UHFReaderViewConfigure,
    UHFReaderViewEpcDump,
    UHFReaderViewEpcInfo,
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
} UHFReaderView;

//Event IDs for the app
typedef enum {
    UHFReaderEventIdRedrawScreen = 0,
    UHFReaderEventIdOkPressed = 42,
    UHFCustomEventReserved = 100,
    UHFCustomEventWorkerExit = 105,
    UHFCustomEventWorkerExitAborted = 106,
} UHFReaderEventId;

//State of the reader app when communicating with raspberry pi zero over uart
typedef enum {
    UHFReaderStateIdle,
    UHFReaderStateWaitForNumber,
    UHFReaderStateCollectEPCs,
    UHFReaderStateDoneCollecting,
    UHFReaderStateWaitForTID,
    UHFReaderStateCollectTIDs,
    UHFReaderStateDoneCollectingTIDs,
    UHFReaderStateWaitForRES,
    UHFReaderStateCollectRESs,
    UHFReaderStateDoneCollectingRESs,
    UHFReaderStateWaitForMEM,
    UHFReaderStateCollectMEMs,
    UHFReaderStateDoneCollectingMEMs
} UHFReaderState;

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

//Structure to track the state of a variable item in a list and if it is locked
typedef struct VariableItemLock {
    bool locked;
} VariableItemLock;

//The main UHFReaderApp Struct
typedef struct {
    ViewDispatcher* ViewDispatcher;
    NotificationApp* Notifications;

    Submenu* Submenu;
    Submenu* SubmenuSaved;
    Submenu* SubmenuTagActions;
    Submenu* SubmenuLockActions;
    Submenu* SubmenuKillActions;
    
    TextInput* TextInput;
    ByteInput* ApInput;
    ByteInput* KillInput;
    ByteInput* SetApInput;
    ByteInput* KillConfirmInput;

    TextInput* SaveInput;
    TextInput* RenameInput;
    TextInput* EpcWrite;
    
    Popup* LockPopup;

    VariableItemList* VariableItemListConfig;
    VariableItemList* VariableItemListLock;
    VariableItem* Setting2Item;
    VariableItem* SettingApPwdItem;
    VariableItem* SettingLockApPwdItem;
    VariableItem* SettingLockResultItem;
    VariableItem* WriteSettingApPwdItem;
    VariableItem* ModuleSelectionItem;
    VariableItem* SavingSelectionItem;
    VariableItem* BaudSelection;
    VariableItem* RegionSelection;
    VariableItem* AntennaSelection;
    Widget* WidgetAbout;

    View* ViewRead;
    View* ViewWrite;
    View* ViewDelete;
    View* ViewLock;
    View* ViewDeleteSuccess;
    View* ViewEpc;
    View* ViewEpcInfo;

    char* TempBuffer;
    uint8_t* ApTempBuffer;
    uint8_t* KillPwdTempBuffer;
    uint8_t* KillConfirmPwdTempBuffer;
    uint8_t* SetPwdTempBuffer;
    char* TempSaveBuffer;
    char* FileName;
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
    char* Setting3ConfigLabel;
    char* Setting3Names[2];
    char* SettingModuleNames[3];
    char* SettingSavingNames[2];
    char* SettingBaudNames[3];
    char* SettingRegionNames[5];
    char* SettingModuleConfigLabel;
    char* SettingSavingConfigLabel;
    char* SettingBaudConfigLabel;
    char* SettingRegionConfigLabel;
    char* SettingLockExecuteConfigLabel;
    char* SettingLockExecuteResult;

    uint32_t TempBufferSize;
    uint8_t ApInputBufferSize;
    uint8_t KillPwdInputBufferSize;
    uint32_t TempBufferSaveSize;
    uint32_t NameSize;
    uint32_t SelectedTagIndex;
    uint32_t NumberOfSavedTags;
    uint32_t NumberOfTidsToRead;
    uint32_t NumberOfResToRead;
    uint32_t NumberOfMemToRead;
    uint32_t CurEpcIndex;
    uint32_t CurTidIndex;
    uint32_t CurResIndex;
    uint32_t CurMemIndex;
    uint32_t UHFBaudRate;

    UartHelper* UartHelper;

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

    FuriTimer* Timer;

    Storage* TagStorage;
    FlipperFormat* EpcFile;
    FlipperFormat* EpcIndexFile;

    UHFReaderState State;

    size_t NumberOfEpcsToRead;
    size_t NameSizeParse;

    uint8_t Setting3Index;
    uint8_t SettingModuleIndex;
    uint8_t SettingSavingIndex;
    uint8_t UHFSaveType;
    uint8_t SettingBaudIndex;
    uint8_t SettingRegionIndex;
    uint8_t SettingLockBankIndex;
    uint8_t SettingLockActionIndex;
    uint8_t Setting1Index;
    uint8_t Setting1Values[2];
    uint8_t SettingLockBankValues[5];
    uint8_t SettingLockActionValues[4];
    uint8_t Setting3Values[2];
    uint8_t SettingModuleValues[3];
    uint8_t SettingSavingValues[2];
    uint8_t SettingBaudValues[3];
    uint8_t SettingRegionValues[5];
    uint8_t UHFModuleType;
    uint8_t UHFRegionType;
    

    char** EpcValues;
    char** TidValues;
    char** ResValues;
    char** MemValues;
    
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
    
    uint32_t Setting3Index;
    bool IsReading;
    FuriString* EpcName;
    uint32_t CurEpcIndex;
    FuriString* EpcValue;
    uint32_t NumEpcsRead;
    FuriString* Setting1Value;
    FuriString* Setting3Value;
    FuriString* Pc;
    FuriString* Crc;
    uint32_t ScrollOffset;
    char* ScrollingText;
    bool IsScrolling;
} UHFReaderConfigModel;

//Model for the write screen
typedef struct {
    uint32_t Setting1Index;
    FuriString* Setting2Power;
    uint32_t Setting3Index;
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
    FuriString* Setting3Value;
    FuriString* Crc;
    FuriString* Pc;
    FuriString* SettingKillPwd;
} UHFReaderWriteModel;

//Model for the delete screen
typedef struct {
    uint32_t SelectedTagIndex;
    FuriString* SelectedTagName;
    FuriString* SelectedTagEpc;
    uint32_t ScrollOffset;
    char* ScrollingText;
} UHFReaderDeleteModel;

//Model use for handling UHF RFID tag data
typedef struct {
    FuriString* Reserved;
    FuriString* Epc;
    FuriString* Tid;
    FuriString* User;
    FuriString* Crc;
    FuriString* Pc;
    uint32_t CurEpcIndex;
    uint32_t ScrollOffsetEpc;
    char* ScrollingTextEpc;
    uint32_t ScrollOffsetTid;
    char* ScrollingTextTid;
    uint32_t ScrollOffsetRes;
    char* ScrollingTextRes;
    uint32_t ScrollOffsetMem;
    char* ScrollingTextMem;
} UHFRFIDTagModel;
