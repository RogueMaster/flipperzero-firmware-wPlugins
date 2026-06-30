#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/modules/validators.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include "api/nfc/mf_classic_key_cache.h"
#include <flipper_application/plugins/composite_resolver.h>
#include <loader/firmware_api/firmware_api.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>
#include <flipper_application/flipper_application.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include "scenes/metroflip_scene.h"
#include <lib/flipper_format/flipper_format.h>
#include <toolbox/name_generator.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <lib/nfc/helpers/nfc_data_generator.h>
#include <furi_hal_bt.h>
#include <notification/notification_messages.h>

#include "scenes/desfire.h"
#include "scenes/nfc_detected_protocols.h"
#include "scenes/keys.h"
#include <lib/nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <datetime.h>
#include <dolphin/dolphin.h>
#include <locale/locale.h>
#include <stdio.h>
#include <strings.h>
#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>
#include <applications/services/storage/storage.h>
#include <applications/services/dialogs/dialogs.h>

#include "scenes/metroflip_scene.h"

#include "api/calypso/calypso_i.h"
#include "api/suica/suica_structs.h"


#define KEY_MASK_BIT_CHECK(key_mask_1, key_mask_2) (((key_mask_1) & (key_mask_2)) == (key_mask_1))
#define METROFLIP_FILE_EXTENSION                   ".nfc"
typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    TextInput* text_input;
    TextBox* text_box;
    ByteInput* byte_input;
    Popup* popup;
    uint8_t mac_buf[GAP_MAC_ADDR_SIZE];
    FuriString* text_box_store;
    Widget* widget;

    Nfc* nfc;
    NfcPoller* poller;
    NfcScanner* scanner;
    NfcDevice* nfc_device;
    MfClassicKeyCache* mfc_key_cache;
    NfcDetectedProtocols* detected_protocols;
    MfDesfireData* mfdes_data;
    MfClassicData* mfc_data;

    // save stuff
    char save_buf[248];

    //plugin manager
    PluginManager* plugin_manager;

    //api
    CompositeApiResolver* resolver;

    // card details:
    uint32_t balance_lari;
    uint8_t balance_tetri;
    uint32_t card_number;
    size_t sec_num;
    float value;
    char currency[4];
    const char* card_type;
    bool auto_mode;
    CardType mfc_card_type;
    NfcProtocol protocol;
    char file_path[256];
    char delete_file_path[256];
    bool is_desfire;
    uint8_t hist_bytes[32];
    uint32_t hist_bytes_count;


    // Calypso specific context
    CalypsoContext* calypso_context;
    FuriString* calypso_file_data;

    // Suica
    SuicaContext* suica_context;

    DialogsApp* dialogs;

    bool data_loaded;

} Metroflip;

enum MetroflipCustomEvent {
    // Reserve first 100 events for button types and indexes, starting from 0
    MetroflipCustomEventReserved = 100,

    MetroflipCustomEventViewExit,
    MetroflipCustomEventByteInputDone,
    MetroflipCustomEventTextInputDone,
    MetroflipCustomEventWorkerExit,

    MetroflipCustomEventPollerDetect,
    MetroflipCustomEventPollerSuccess,
    MetroflipCustomEventPollerFail,
    MetroflipCustomEventPollerSelectFailed,
    MetroflipCustomEventPollerFileNotFound,

    MetroflipCustomEventCardLost,
    MetroflipCustomEventCardDetected,
    MetroflipCustomEventWrongCard
};

typedef enum {
    MetroflipPollerEventTypeStart,
    MetroflipPollerEventTypeCardDetect,

    MetroflipPollerEventTypeSuccess,
    MetroflipPollerEventTypeFail,
} MetroflipPollerEventType;

typedef enum {
    MetroflipViewSubmenu,
    MetroflipViewTextInput,
    MetroflipViewByteInput,
    MetroflipViewPopup,
    MetroflipViewMenu,
    MetroflipViewLoading,
    MetroflipViewTextBox,
    MetroflipViewWidget,
    MetroflipViewUart,
    MetroflipViewCanvas,
} MetroflipView;

typedef enum {
    SUCCESSFUL,
    INCOMPLETE_KEYFILE,
    MISSING_KEYFILE
} KeyfileManager;

CardType determine_card_type(Nfc* nfc, MfClassicData* mfc_data, bool data_loaded);

#ifdef FW_ORIGIN_Official
#define submenu_add_lockable_item(                                             \
    submenu, label, index, callback, callback_context, locked, locked_message) \
    if(!(locked)) submenu_add_item(submenu, label, index, callback, callback_context)
#endif

char* bit_slice(const char* bit_representation, int start, int end);

void metroflip_plugin_manager_alloc(Metroflip* app);

///////////////////////////////// Calypso / EN1545 /////////////////////////////////

#define Metroflip_POLLER_MAX_BUFFER_SIZE 1024

#define epoch 852073200

void locale_format_datetime_cat(FuriString* out, const DateTime* dt, bool time);

int binary_to_decimal(const char binary[]);
