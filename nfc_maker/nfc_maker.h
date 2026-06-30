#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/modules/validators.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#ifdef FW_ORIGIN_Momentum
#include <assets_icons.h>
#else
extern const Icon I_DolphinDone_80x58;
extern const Icon I_WarningDolphinFlip_45x42;
#endif
#include <gui/modules/submenu.h>
#ifdef FW_ORIGIN_Momentum
#include <gui/modules/text_input.h>
#else
#include "dropin/text_input.h"
#endif
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include "scenes/nfc_maker_scene.h"
#include <lib/flipper_format/flipper_format.h>
#include <toolbox/name_generator.h>
#include <bit_lib/bit_lib.h>
#if defined(FW_ORIGIN_Momentum) && __has_include(<applications/main/nfc/nfc_app_i.h>)
#include <applications/main/nfc/nfc_app_i.h>
#else
#define NFC_APP_FOLDER    EXT_PATH("nfc")
#define NFC_APP_EXTENSION ".nfc"
#endif
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <lib/nfc/protocols/type_4_tag/type_4_tag.h>
#include <lib/nfc/protocols/mf_classic/mf_classic.h>
#include <lib/nfc/protocols/slix/slix.h>
#include <lib/nfc/helpers/nfc_data_generator.h>

#define MAC_INPUT_LEN   (GAP_MAC_ADDR_SIZE)
#define MAIL_INPUT_LEN  (128)
#define PHONE_INPUT_LEN (17)

#define BIG_INPUT_LEN   (248)
#define SMALL_INPUT_LEN (90)

#define NTAG_DATA_AREA_UNIT_SIZE (2 * MF_ULTRALIGHT_PAGE_SIZE)
typedef enum {
    // MfUltralight
    CardNtag203,
    CardNtag213,
    CardNtag215,
    CardNtag216,
    CardNtagI2C1K,
    CardNtagI2C2K,

    // Type4Tag
    CardNtag413DNA,
    CardNtag424DNA,
    CardMfDesfire,
    CardType4Generic,

    // MfClassic
    CardMfClassicMini,
    CardMfClassic1K4b,
    CardMfClassic1K7b,
    CardMfClassic4K4b,
    CardMfClassic4K7b,

    // Slix
    CardSlix,
    CardSlixS,
    CardSlixL,
    CardSlix2,

    CardMAX,
} Card;
typedef struct {
    const char* name;
    size_t size;
    NfcProtocol protocol;
    NfcDataGeneratorType generator;
} CardDef;
extern const CardDef cards[CardMAX];

typedef enum {
    WifiAuthenticationOpen = 0x01,
    WifiAuthenticationWpa2Personal = 0x20,
    WifiAuthenticationWpa2Enterprise = 0x10,
    WifiAuthenticationWpaPersonal = 0x02,
    WifiAuthenticationWpaEnterprise = 0x08,
    WifiAuthenticationShared = 0x04,
} WifiAuthentication;

typedef enum {
    WifiEncryptionAes = 0x08,
    WifiEncryptionWep = 0x02,
    WifiEncryptionTkip = 0x04,
    WifiEncryptionNone = 0x01,
} WifiEncryption;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    Popup* popup;

    NfcDevice* nfc_device;
    uint8_t* ndef_buffer;
    size_t ndef_size;

    uint8_t mac_buf[MAC_INPUT_LEN];
    char mail_buf[MAIL_INPUT_LEN];
    char phone_buf[PHONE_INPUT_LEN];

    char big_buf[BIG_INPUT_LEN];
    char small_buf1[SMALL_INPUT_LEN];
    char small_buf2[SMALL_INPUT_LEN];
    char save_buf[BIG_INPUT_LEN];

    uint8_t uid_buf[10];
} NfcMaker;

typedef enum {
    NfcMakerViewSubmenu,
    NfcMakerViewTextInput,
    NfcMakerViewByteInput,
    NfcMakerViewPopup,
} NfcMakerView;

#ifdef FW_ORIGIN_Official
#define submenu_add_lockable_item(                                             \
    submenu, label, index, callback, callback_context, locked, locked_message) \
    if(!(locked)) submenu_add_item(submenu, label, index, callback, callback_context)
#endif
