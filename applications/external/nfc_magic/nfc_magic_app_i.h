#pragma once

#include "nfc_magic_app.h"
#include "helpers/nfc_magic_custom_events.h"
#include "helpers/mfc_key_cache.h"

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

#define NFC_APP_FOLDER           EXT_PATH("nfc")
#define NFC_APP_EXTENSION        ".nfc"
#define NFC_APP_SHADOW_EXTENSION ".shd"

#define NFC_APP_MF_CLASSIC_DICT_USER_PATH   (NFC_APP_FOLDER "/assets/mf_classic_dict_user.nfc")
#define NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH (NFC_APP_FOLDER "/assets/mf_classic_dict.nfc")

#define NFC_MAGIC_APP_NAME_SIZE             22
#define NFC_MAGIC_APP_TEXT_STORE_SIZE       128
#define NFC_MAGIC_APP_FOLDER                EXT_PATH("nfc")
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
    NfcMagicCustomEventIso15693NotGen2,
};

typedef struct {
    KeysDict* dict; // dictionary phases; NULL while the key cache phase runs
    MfcKeyCache* key_cache; // key cache phase only; NULL selects the dictionary above. Mirrors the
        // scene state -- prepare_view is the only place both are set
    uint8_t cache_key_index; // cache cursor within current_sector: 0 = key A, 1 = key B
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
    NfcMagicIso15693WriteFailReasonUnset, // 0, the scene manager's starting state, so a screen
        // entered without a reason cannot pass for NotMagic. Never set on purpose: the reason
        // switches crash on it.
    NfcMagicIso15693WriteFailReasonNotMagic, // card present, but the backdoor write was not accepted.
        // Defensive fallback only: a card that really isn't magic leaves the UID unchanged, which is
        // NotGen2 rather than Fail, and lands on the gen1 opt-in screen instead of here.
    NfcMagicIso15693WriteFailReasonCardLost, // no card in the field / card removed mid-write
    NfcMagicIso15693WriteFailReasonPartial, // clone: UID written but some data blocks failed
    // clone OK and nothing lost, but the card carries something the bare Success popup cannot hold.
    // Any of the survey's three findings -- readable blocks above the source still holding data from
    // before the clone, a card answering reads past the count it now reports, or a block count or IC
    // reference it goes on reporting that is not the source's -- or a gen1 clone whose file reached
    // 56/57/62/63 with nothing there, where the card has registers in place of the file's blocks.
    // None is a failure and none is Partial. Why the survey's are kept apart is at
    // iso15693_poller_survey_above_source.
    NfcMagicIso15693WriteFailReasonCloneComplete,
    NfcMagicIso15693WriteFailReasonOverCapacity, // clone OK, but the file is larger than the card
        // and the blocks past its end were empty, so nothing was lost -- a success with a note
    NfcMagicIso15693WriteFailReasonNothingWiped, // wipe: not one block accepted the zero-write
    NfcMagicIso15693WriteFailReasonEmptySource, // clone: the source image has no data blocks to write
    NfcMagicIso15693WriteFailReasonNothingCloned, // clone: the UID took but not one data block did, so
        // the card carries the source's UID and none of its data
    NfcMagicIso15693WriteFailReasonUidUnexpected, // the gen2 backdoor moved the UID, but to neither the
        // original nor the target. The card IS magic -- an inert tag cannot change its UID -- so it
        // must not share the "not a magic tag" screen. The UID it answers with is in the result.
        // A clone's re-read after a pass that reached 56/57 lands here the same way.
        // So does a gen1 sequence that leaves half a UID.
    NfcMagicIso15693WriteFailReasonGen1Failed, // the opt-in gen1 UID didn't verify. The sequence is
        // sent before anything is checked, so blocks 56/57/62/63 were overwritten regardless -- on what
        // is most likely an ordinary tag. Naming them is the point of the screen.
    NfcMagicIso15693WriteFailReasonUidUnverifiable, // Write UID: the requested UID is the card's own,
        // so nothing was written -- a read-back against a UID the card already has proves nothing
    NfcMagicIso15693WriteFailReasonWipeUidChanged, // wipe: the blocks cleared, but the UID read back
        // afterwards is not the one the card presented before. On gen1 that is the wipe zeroing blocks
        // 56/57, which ARE the UID registers (see ISO15693_MAGIC_BLK_UNLOCK in the poller).
    NfcMagicIso15693WriteFailReasonWipeComplete, // wipe: a clean success -- the sweep was not cut and
        // nothing it reached is known to still hold data. NOT "no block refused": a block that refuses
        // every write and then reads back empty is deliberately not counted, because nothing was lost
        // there. Here rather than on the bare Success popup so it can report the
        // range it measured beside the count the card advertises, since the two can differ without
        // either being a fault. A wipe whose UID check never reached an answer still lands here and
        // says so on the third line: the wipe finished, only the identity check did not run.
    NfcMagicIso15693WriteFailReasonWipeStopped, // wipe: the sweep hit its time limit. Nothing ties the
        // cut to the advertised count -- the check is a wall-clock test at the top of every iteration,
        // so a card that accepts some writes and then answers reads at every address CAN be cut with
        // every claimed block already attempted and the cut index above that count. A high claim is
        // cut below it instead -- both sides are live, which is why the details screen branches on
        // them. (NOT the refuses-every-write card: that one clears nothing, so the wipe short-circuits
        // to NothingWiped before the WipeStopped truncation reporting -- it is NothingWiped's screen
        // that states the cut.) A
        // partial outcome, not a qualified success -- the poller reports Partial for it -- so it
        // carries the error tone and offers Retry, since re-running is the correct action when
        // data may sit above the cut.
} NfcMagicIso15693WriteFailReason;

// Which ISO15693 operation the shared write scene is running: a clone, a wipe or a Write-UID.
typedef enum {
    NfcMagicIso15693ModeClone, // write a saved .nfc image onto the card
    NfcMagicIso15693ModeWipe, // zero every data block on the card, 56/57/62/63 included
    NfcMagicIso15693ModeWriteUid, // write a hand-entered UID and nothing else
} NfcMagicIso15693Mode;

// Which flow reached the gen1 opt-in screen. The two consent to different things (a clone writes every
// data block, a Write-UID writes only the UID registers) and return to different write scenes. Stored
// as the opt-in scene's state by whichever scene routes to it.
typedef enum {
    NfcMagicIso15693Gen1OptinFromClone,
    NfcMagicIso15693Gen1OptinFromWriteUid,
} NfcMagicIso15693Gen1OptinSource;

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
    uint8_t gen1_uid_len; // Gen1 UID length class (4/7) derived from card_uid_len; 0 if not Gen1
    uint8_t card_uid[ISO14443_3A_MAX_UID_SIZE]; // scanned card's UID; names its key cache entry
    uint8_t card_uid_len; // 0 when the card never activated (backdoor-only)
    UscuidUlData uscuid_ul_data;
    uint16_t write_progress_current; // USCUID-UL & ISO15693: units written so far
    uint16_t write_progress_total; // USCUID-UL & ISO15693: total units to write
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
    Iso15693_3Data*
        iso15693_data; // last read result, kept so the info scene survives the poller free
    uint8_t
        iso15693_target_uid[ISO15693_3_UID_SIZE]; // MSB-first UID to write to a magic ISO15693 card
    NfcMagicIso15693Mode iso15693_mode; // which ISO15693 operation the shared write scene is running
    bool iso15693_force_gen1; // ISO15693 clone / Write-UID: run the opt-in gen1 attempt (set by the
        // gen1 opt-in scene, cleared when a fresh clone or Write-UID is started from the menu)
    // Outcome of the last ISO15693 clone or wipe, fetched on every WriteProgress event and once per
    // terminal event -- mid-run it carries the live progress figures, see blocks_total. See
    // Iso15693PollerResult in iso15693_poller.h for the per-field meaning and its mode dependence.
    Iso15693PollerResult iso15693_result;

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

void nfc_magic_app_free_dict_attack_keys(NfcMagicApp* instance);

void nfc_magic_app_blink_start(NfcMagicApp* nfc_magic);

void nfc_magic_app_blink_stop(NfcMagicApp* nfc_magic);

void nfc_magic_app_show_loading_popup(void* context, bool show);

bool nfc_magic_load_from_file_select(NfcMagicApp* instance);
