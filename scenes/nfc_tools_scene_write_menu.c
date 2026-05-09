#include "../include/nfc_tools_i.h"

typedef enum {
    NdefMenuItemUrl,
    NdefMenuItemCustomUri,
    NdefMenuItemText,
    NdefMenuItemWifi,
    NdefMenuItemUnitLink,
    NdefMenuItemMail,
    NdefMenuItemPhone,
    NdefMenuItemSms,
    NdefMenuItemFacetime,
    NdefMenuItemFacetimeAudio,
    NdefMenuItemBluetooth,
    NdefMenuItemCustomData,
    NdefMenuItemSocial,
    NdefMenuItemLocation,
    NdefMenuItemContact,
    NdefMenuItemSearch,
    NdefMenuItemBitcoin,
} NdefMenuItem;

// Reset all NDEF write buffers to their neutral default before a new write
// session starts.  Called from on_event each time the user picks a type from
// the menu so that returning from a failed write_scan never carries stale data
// from a *previous* type into a new one.
static void write_bufs_reset(NfcToolsApp* app) {
    app->ndef_buf1[0] = '\0';
    app->ndef_buf2[0] = '\0';
    app->ndef_buf3[0] = '\0';
    app->ndef_buf4[0] = '\0';
    app->ndef_buf5[0] = '\0';
    app->ndef_buf6[0] = '\0';
}

static void nfc_tools_scene_write_menu_callback(void* context, uint32_t index) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_tools_scene_write_menu_on_enter(void* context) {
    NfcToolsApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, NTS_HEADER_WRITE);
    submenu_add_item(
        submenu, NTS_WRITE_TEXT, NdefMenuItemText, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_URL, NdefMenuItemUrl, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_CUSTOM_URI, NdefMenuItemCustomUri, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_UNIT_LINK, NdefMenuItemUnitLink, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_SOCIAL_NETWORKS, NdefMenuItemSocial, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_SEARCH, NdefMenuItemSearch, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_MAIL, NdefMenuItemMail, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_CONTACT, NdefMenuItemContact, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_PHONE_NUMBER, NdefMenuItemPhone, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_SMS, NdefMenuItemSms, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_FACETIME, NdefMenuItemFacetime, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_FACETIME_AUDIO, NdefMenuItemFacetimeAudio, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_LOCATION, NdefMenuItemLocation, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_BITCOIN, NdefMenuItemBitcoin, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_BLUETOOTH, NdefMenuItemBluetooth, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_WIFI_NETWORK, NdefMenuItemWifi, nfc_tools_scene_write_menu_callback, app);
    submenu_add_item(
        submenu, NTS_WRITE_CUSTOM_DATA, NdefMenuItemCustomData, nfc_tools_scene_write_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewMainMenu);
}

bool nfc_tools_scene_write_menu_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NdefMenuItemUrl:
            app->ndef_type = NdefTypeUrl;
            write_bufs_reset(app);
            strlcpy(app->ndef_buf1, "https://", sizeof(app->ndef_buf1));
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteUrl);
            consumed = true;
            break;
        case NdefMenuItemCustomUri:
            app->ndef_type = NdefTypeCustomUri;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteCustomUri);
            consumed = true;
            break;
        case NdefMenuItemText:
            app->ndef_type = NdefTypeText;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteText);
            consumed = true;
            break;
        case NdefMenuItemWifi:
            app->ndef_type = NdefTypeWifi;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteWifiSsid);
            consumed = true;
            break;
        case NdefMenuItemUnitLink:
            app->ndef_type = NdefTypeUnitLink;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteUnitLink);
            consumed = true;
            break;
        case NdefMenuItemMail:
            app->ndef_type = NdefTypeMail;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteMailTo);
            consumed = true;
            break;
        case NdefMenuItemPhone:
            app->ndef_type = NdefTypePhone;
            write_bufs_reset(app);
            strlcpy(app->ndef_buf1, "+", sizeof(app->ndef_buf1));
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWritePhone);
            consumed = true;
            break;
        case NdefMenuItemSms:
            app->ndef_type = NdefTypeSms;
            write_bufs_reset(app);
            strlcpy(app->ndef_buf1, "+", sizeof(app->ndef_buf1));
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteSmsNumber);
            consumed = true;
            break;
        case NdefMenuItemFacetime:
            app->ndef_type = NdefTypeFacetime;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteFacetime);
            consumed = true;
            break;
        case NdefMenuItemFacetimeAudio:
            app->ndef_type = NdefTypeFacetimeAudio;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteFacetime);
            consumed = true;
            break;
        case NdefMenuItemBluetooth:
            app->ndef_type = NdefTypeBluetooth;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteBluetooth);
            consumed = true;
            break;
        case NdefMenuItemCustomData:
            app->ndef_type = NdefTypeCustomData;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteCustomMime);
            consumed = true;
            break;
        case NdefMenuItemSocial:
            app->ndef_type = NdefTypeSocial;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteSocialPick);
            consumed = true;
            break;
        case NdefMenuItemLocation:
            app->ndef_type = NdefTypeLocation;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteLocationLat);
            consumed = true;
            break;
        case NdefMenuItemContact:
            app->ndef_type = NdefTypeContact;
            write_bufs_reset(app);
            strlcpy(app->ndef_buf4, "+", sizeof(app->ndef_buf4));
            strlcpy(app->ndef_buf6, "https://", sizeof(app->ndef_buf6));
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteContactName);
            consumed = true;
            break;
        case NdefMenuItemSearch:
            app->ndef_type = NdefTypeSearch;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteSearchPick);
            consumed = true;
            break;
        case NdefMenuItemBitcoin:
            app->ndef_type = NdefTypeBitcoin;
            write_bufs_reset(app);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteBitcoinAddr);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void nfc_tools_scene_write_menu_on_exit(void* context) {
    NfcToolsApp* app = context;
    submenu_reset(app->submenu);
}
