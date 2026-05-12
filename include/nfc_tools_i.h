#ifndef NFC_TOOLS_I_H
#define NFC_TOOLS_I_H

#include "nfc_tools_strings.h"
#include "nfc_tools_social.h"
#include "nfc_tools_search.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/mf_desfire/mf_desfire.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/protocols/felica/felica_poller.h>
#include <nfc/helpers/felica_crc.h>

#include "scenes/nfc_tools_scene.h"
#include "views/email_input/nfc_tools_email_input.h"
#include "views/mime_input/nfc_tools_mime_input.h"
#include "views/special_input/nfc_tools_special_input.h"
#include "helpers/md5/nfc_tools_md5.h"

#define NFC_TOOLS_VERSION "1.2"

#define NFC_TOOLS_WORKER_FLAG_DETECTED (1u << 0)
#define NFC_TOOLS_WORKER_FLAG_STOP     (1u << 1)

// ── NDEF record structured storage ───────────────────────────────────────────
#define NFC_TOOLS_MAX_NDEF_RECORDS  8
#define NFC_TOOLS_NDEF_PAYLOAD_MAX  220  // raw payload truncated to 220 bytes

typedef enum {
    NfcToolsNdefTypeUnknown = 0,
    NfcToolsNdefTypeUri,
    NfcToolsNdefTypeText,
    NfcToolsNdefTypeWifi,
    NfcToolsNdefTypeVcard,
    NfcToolsNdefTypeSmartPoster,
    NfcToolsNdefTypeMime,
    NfcToolsNdefTypeEmpty,
    NfcToolsNdefTypeExternal,
} NfcToolsNdefType;

typedef struct {
    uint8_t           tnf;
    NfcToolsNdefType  type;
    char              type_str[32];   // raw type (e.g. "U", "T", "application/vnd.wfa.wsc")
    char              value[128];     // decoded value (URL, text…)
    char              summary[40];    // one-liner for the submenu
    uint8_t           payload[NFC_TOOLS_NDEF_PAYLOAD_MAX];
    uint16_t          payload_len;    // actual length (before truncation)
    bool              has_qr;         // true if a QR code is relevant
} NfcToolsNdefRecord;

#define NFC_TOOLS_NDEF_BUF1_SIZE 128 // URL / text / WiFi SSID / Mail To / Contact Name
#define NFC_TOOLS_NDEF_BUF2_SIZE 64  // WiFi password / Mail Subject / Contact Company
#define NFC_TOOLS_NDEF_BUF3_SIZE 128 // Mail Body / Contact Address
#define NFC_TOOLS_NDEF_BUF4_SIZE 64  // Contact Phone
#define NFC_TOOLS_NDEF_BUF5_SIZE 128 // Contact Email
#define NFC_TOOLS_NDEF_BUF6_SIZE 128 // Contact URL

typedef enum {
    NfcToolsViewMainMenu,
    NfcToolsViewPopup,
    NfcToolsViewTextBox,
    NfcToolsViewTextInput,
    NfcToolsViewEmailInput,
    NfcToolsViewMimeInput,
    NfcToolsViewSpecialInput,
    NfcToolsViewSubmenu2,  // second submenu (NDEF records list in tag_info)
    NfcToolsViewQrCode,    // custom QR code view
    NfcToolsViewWidget,    // widget for record detail with button
} NfcToolsView;

typedef enum {
    NfcToolsEventScanSuccess,
    NfcToolsEventScanTimeout,
    NfcToolsEventAnalysisDone,
    NfcToolsEventWriteSuccess,
    NfcToolsEventWriteFail,
} NfcToolsEvent;

typedef enum {
    NdefTypeUrl,
    NdefTypeCustomUri,   // raw URI without prefix
    NdefTypeText,
    NdefTypeWifi,
    NdefTypeUnitLink, // https://unit.link/<alias>
    NdefTypeMail,     // mailto:to[?subject=...][&body=...]
    NdefTypePhone,    // tel:<number>
    NdefTypeSms,            // sms:<number>[?body=<message>]
    NdefTypeFacetime,       // facetime:<number or email>
    NdefTypeFacetimeAudio,  // facetime-audio:<number or email>
    NdefTypeBluetooth,  // Bluetooth Classic OOB (MAC address)
    NdefTypeCustomData, // MIME arbitraire : Content-Type + Data
    NdefTypeSocial,     // Social network: URL built from index + username
    NdefTypeLocation,   // geo:latitude,longitude
    NdefTypeContact,    // vCard 3.0: Name, Company, Address, Phone, Email, URL
    NdefTypeSearch,     // Web search: engine + URL-encoded keyword
    NdefTypeBitcoin,    // BIP-21: bitcoin:ADDRESS[?amount=...][&message=...]
    NdefTypeEmpty,      // Erase: empty NDEF record
} NdefType;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    Submenu* submenu2;  // for the NDEF records list
    View*    qr_view;   // custom QR code view
    Widget*  widget;    // widget for record detail (text + button)
    Popup* popup;
    TextBox* text_box;
    TextInput* text_input;
    EmailInput*   email_input;
    MimeInput*    mime_input;
    SpecialInput* special_input;

    NotificationApp* notifications;
    Nfc* nfc;

    FuriThread* worker_thread;
    FuriEventFlag* worker_flags;

    NfcToolsSceneId scan_destination;

    uint8_t social_network_index; // index into nfc_tools_social_networks[]
    uint8_t search_engine_index;  // index into nfc_tools_search_engines[]

    NfcProtocol detected_protocol;
    uint8_t uid[10];
    size_t uid_len;
    uint8_t sak;
    uint8_t atqa[2];
    MfClassicType mfc_type;
    MfUltralightType    mful_type;
    MfUltralightVersion mful_version;       // raw GET_VERSION (0x60) response
    bool                mful_version_valid; // true if GET_VERSION responded

    // ISO 15693 (ICODE SLI / SLIX / SLIX2 et compatibles)
    uint16_t iso15693_block_count;
    uint8_t  iso15693_block_size; // actual size in bytes (SDK already adds +1)
    uint8_t  iso15693_ic_ref;     // IC Reference byte (GET_SYSTEM_INFORMATION)

    // DESFire EV1 / EV2 / EV3
    uint8_t  desfire_hw_major;         // 0x01=EV1, 0x12=EV2, 0x22=EV2XL, 0x33=EV3
    uint32_t desfire_app_count;
    bool     desfire_has_free_memory;
    uint32_t desfire_free_memory;

    // FeliCa (ISO 18092 / NFC-F)
    uint8_t            felica_pmm[FELICA_PMM_SIZE]; // 8 bytes PMm
    char               felica_ic_name[32];           // IC type string (e.g. "RC-S960")
    uint8_t            felica_blocks_read;
    uint8_t            felica_blocks_total;
    FelicaWorkflowType felica_workflow_type;
    uint8_t            felica_write_block;           // block number for write (0-27)

    FuriString* info_str;
    FuriString* ndef_str; // Parsed NDEF content during read (text)

    // Structured NDEF records
    NfcToolsNdefRecord ndef_records[NFC_TOOLS_MAX_NDEF_RECORDS];
    uint8_t            ndef_record_count;
    uint8_t            ndef_selected_record; // index selected for the detail view

    // NDEF write
    NdefType ndef_type;
    char ndef_buf1[NFC_TOOLS_NDEF_BUF1_SIZE]; // URL / text / WiFi SSID / Mail To / Contact Name
    char ndef_buf2[NFC_TOOLS_NDEF_BUF2_SIZE]; // WiFi password / Mail Subject / Contact Company
    char ndef_buf3[NFC_TOOLS_NDEF_BUF3_SIZE]; // Mail Body / Contact Address
    char ndef_buf4[NFC_TOOLS_NDEF_BUF4_SIZE]; // Contact Phone
    char ndef_buf5[NFC_TOOLS_NDEF_BUF5_SIZE]; // Contact Email
    char ndef_buf6[NFC_TOOLS_NDEF_BUF6_SIZE]; // Contact URL
} NfcToolsApp;

// QR view helpers
View* nfc_tools_qr_view_alloc(NfcToolsApp* app);
void  nfc_tools_qr_view_free(View* view);

#endif /* NFC_TOOLS_I_H */
