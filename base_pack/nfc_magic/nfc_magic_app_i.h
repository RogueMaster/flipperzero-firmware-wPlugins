#pragma once

#include "nfc_magic_app.h"
#include "helpers/nfc_magic_custom_events.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>

#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <views/dict_attack.h>
#include <views/write_problems.h>

#include <input/input.h>

#include "scenes/nfc_magic_scene.h"

#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/toolbox/path.h>
#include <dolphin/dolphin.h>

#include "nfc_magic_icons.h"

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <toolbox/keys_dict.h>

#include "magic/nfc_magic_scanner.h"
#include "magic/protocols/nfc_magic_protocols.h"
#include "magic/protocols/gen1a/gen1a_poller.h"
#include "magic/protocols/gen2/gen2_poller.h"
#include "magic/protocols/gen4/gen4_poller.h"
#include "magic/protocols/iso15693/iso15693_poller.h"

#include "lib/nfc/protocols/mf_classic/mf_classic_poller.h"

#define NFC_APP_FOLDER           ANY_PATH("nfc")
#define NFC_APP_EXTENSION        ".nfc"
#define NFC_APP_SHADOW_EXTENSION ".shd"

#define NFC_APP_MF_CLASSIC_DICT_USER_PATH   (NFC_APP_FOLDER "/assets/mf_classic_dict_user.nfc")
#define NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH (NFC_APP_FOLDER "/assets/mf_classic_dict.nfc")

#define NFC_MAGIC_APP_NAME_SIZE             22
#define NFC_MAGIC_APP_TEXT_STORE_SIZE       128
#define NFC_MAGIC_APP_FOLDER                ANY_PATH("nfc")
#define NFC_MAGIC_APP_EXTENSION             ".nfc"
#define NFC_MAGIC_APP_FILENAME_PREFIX       "NFC"
#define NFC_MAGIC_APP_BYTE_INPUT_STORE_SIZE (4)

enum NfcMagicAppCustomEvent {
    // Reserve first 100 events for button types and indexes, starting from 0
    NfcMagicAppCustomEventReserved = 100,

    NfcMagicAppCustomEventViewExit,
    NfcMagicAppCustomEventWorkerExit,
    NfcMagicAppCustomEventByteInputDone,
    NfcMagicAppCustomEventTextInputDone,
    NfcMagicAppCustomEventCardDetected,
    NfcMagicAppCustomEventCardLost,
    NfcMagicAppCustomEventDictAttackDataUpdate,
    NfcMagicAppCustomEventDictAttackComplete,
    NfcMagicAppCustomEventDictAttackSkip,
    NfcMagicCustomEventTextInputDone,
    NfcMagicCustomEventIso15693CardDetected,
    NfcMagicCustomEventIso15693CardDetectFailed,
};

typedef struct {
    KeysDict* dict;
    uint8_t sectors_total;
    uint8_t sectors_read;
    uint8_t current_sector;
    uint8_t keys_found;
    size_t dict_keys_total;
    size_t dict_keys_current;
    bool is_key_attack;
    uint8_t key_attack_current_sector;
    bool is_card_present;
} NfcMagicAppMfClassicDictAttackContext;

typedef struct {
    uint8_t problem_index;
    uint8_t problem_index_abs;
    uint8_t problems_total;
    Gen2PollerWriteProblems problems;
} NfcMagicAppWriteProblemsContext;

// Reason passed to the WipeFail scene via its scene state so it can explain the failure.
typedef enum {
    NfcMagicWipeFailReasonGeneric, // an error occurred mid-wipe
    NfcMagicWipeFailReasonNoKeys, // no sector keys found, so the wipe never started
} NfcMagicWipeFailReason;

// Reason passed to the Iso15693WriteFail scene via its scene state so it can explain the outcome.
typedef enum {
    NfcMagicIso15693WriteFailReasonNotMagic, // card present, but the backdoor write was not accepted
    NfcMagicIso15693WriteFailReasonCardLost, // no card in the field / card removed mid-write
    NfcMagicIso15693WriteFailReasonPartial, // clone: UID written but some data blocks failed
    NfcMagicIso15693WriteFailReasonOverCapacity, // clone OK, but the card now advertises more blocks
        // than it physically holds (the extra were empty, so nothing was lost) -- a success with a note
} NfcMagicIso15693WriteFailReason;

struct NfcMagicApp {
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    Storage* storage;

    SceneManager* scene_manager;
    NfcDevice* source_dev;
    NfcDevice* target_dev;
    char text_store[NFC_MAGIC_APP_TEXT_STORE_SIZE + 1];
    FuriString* file_name;
    FuriString* file_path;
    MfClassicData* dump_data;

    Nfc* nfc;
    NfcMagicProtocol protocol;
    Gen2Type gen2_type;
    uint8_t gen1_uid_len;
    UscuidUlData uscuid_ul_data;
    uint16_t write_progress_current; // USCUID-UL: pages written so far (live progress)
    uint16_t write_progress_total; // USCUID-UL: total pages to write
    uint16_t write_failed_count; // USCUID-UL: pages that didn't take (partial clone)
    uint8_t write_failed_bitmap[USCUID_UL_FAILED_BITMAP_SIZE]; // bit N set = page N failed
    uint8_t uscuid_ul_password[USCUID_UL_PWD_SIZE]; // PWD-AUTH password (direct/ATS tags)
    bool uscuid_ul_password_set; // auth before writes is armed
    bool uscuid_ul_is_wipe_mode; // Wipe = write a synthesized factory-default dump
    bool source_uid_mismatch;
    NfcMagicScanner* scanner;
    NfcPoller* poller;
    Gen1aPoller* gen1a_poller;

    Gen2Poller* gen2_poller;
    bool gen2_poller_is_wipe_mode;
    uint16_t gen2_partial_blocks_total; // Gen2 wipe/clone partial: blocks on the card/dump
    uint16_t gen2_partial_failed_count; // blocks that couldn't be written (no key / read-only)
    uint8_t gen2_partial_failed_bitmap[GEN2_POLLER_BLOCK_BITMAP_SIZE]; // bit N = block N not written

    Gen4Poller* gen4_poller;
    UscuidUlPoller* uscuid_ul_poller;
    // Allocated per-scene (in the ISO15693 get-info / write scenes), NOT at app startup:
    // iso15693_poller_alloc -> nfc_poller_alloc(Iso15693_3) calls nfc_config() on the shared Nfc,
    // and holding that config would make the scanner's first nfc_config() furi_check-fail.
    Iso15693Poller* iso15693_poller;
    Iso15693Data* iso15693_data; // last read result, kept so the info scene survives the poller free
    uint8_t iso15693_target_uid[ISO15693_3_UID_SIZE]; // MSB-first UID to write to a magic ISO15693 card
    bool iso15693_is_wipe_mode; // ISO15693 write scene: wipe (zero blocks) vs clone (from a file)
    uint16_t iso15693_clone_blocks_total; // ISO15693 clone: data blocks on the source image
    uint16_t iso15693_clone_failed_count; // ISO15693 clone: in-range blocks that couldn't be written
    uint16_t iso15693_clone_over_capacity; // ISO15693 clone: source blocks past the target's capacity
    uint8_t iso15693_clone_failed_bitmap[ISO15693_POLLER_BLOCK_BITMAP_SIZE]; // bit N = source block N failed
    bool iso15693_clone_used_gen1; // ISO15693 clone: gen1 fallback set the UID (overwrote blocks 56/57/62/63)

    Gen4* gen4_data;

    Gen4Password gen4_password;
    Gen4Password gen4_password_new;

    NfcMagicAppMfClassicDictAttackContext nfc_dict_context;
    DictAttack* dict_attack;
    NfcMagicAppWriteProblemsContext write_problems_context;
    WriteProblems* write_problems;

    FuriString* text_box_store;
    uint8_t byte_input_store[NFC_MAGIC_APP_BYTE_INPUT_STORE_SIZE];

    // Common Views
    Submenu* submenu;
    Popup* popup;
    Loading* loading;
    TextInput* text_input;
    ByteInput* byte_input;
    Widget* widget;
};

typedef enum {
    NfcMagicAppViewMenu,
    NfcMagicAppViewPopup,
    NfcMagicAppViewLoading,
    NfcMagicAppViewTextInput,
    NfcMagicAppViewByteInput,
    NfcMagicAppViewWidget,
    NfcMagicAppViewDictAttack,
    NfcMagicAppViewWriteProblems,
} NfcMagicAppView;

void nfc_magic_app_blink_start(NfcMagicApp* nfc_magic);

void nfc_magic_app_blink_stop(NfcMagicApp* nfc_magic);

void nfc_magic_app_show_loading_popup(void* context, bool show);

bool nfc_magic_load_from_file_select(NfcMagicApp* instance);
