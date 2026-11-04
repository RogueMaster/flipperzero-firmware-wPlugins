#include "payto.h"

#include <furi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/canvas.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/helpers/nfc_data_generator.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define PAYTO_STORAGE_PATH      EXT_PATH("apps_data/flipper_payto/wallets.txt")
#define PAYTO_MAX_WALLETS       32
#define PAYTO_EVENT_SCANNED     1
#define PAYTO_MENU_ENTER_WALLET 1
#define PAYTO_MENU_SCAN         2
#define PAYTO_MENU_MANUAL       3
#define PAYTO_MENU_SAVE         4
#define PAYTO_MENU_NAME         5
#define PAYTO_MENU_DISCARD      6
#define PAYTO_MENU_WALLET       100
#define PAYTO_MENU_STREAM       200
#define PAYTO_MENU_PRINT        201
#define PAYTO_MENU_REMOVE       202
#define PAYTO_NDEF_OFFSET       (4 * MF_ULTRALIGHT_PAGE_SIZE)
#define PAYTO_WALLET_FILE_BUFFER_SIZE \
    (PAYTO_MAX_WALLETS * (PAYTO_MAX_URI_LEN + PAYTO_MAX_NAME_LEN + 2))

typedef enum {
    PaytoViewMenu,
    PaytoViewEnterWallet,
    PaytoViewName,
    PaytoViewScan,
    PaytoViewStream,
    PaytoViewWalletActions,
    PaytoViewPrint,
} PaytoView;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* menu;
    Submenu* enter_menu;
    Submenu* wallet_actions;
    TextInput* text_input;
    TextBox* wallet_text;
    View* scan_view;
    View* stream_view;
    Storage* storage;
    NotificationApp* notifications;
    Nfc* nfc;
    NfcPoller* poller;
    NfcListener* listener;
    NfcDevice* emulation;
    PaytoWallet wallets[PAYTO_MAX_WALLETS];
    size_t wallet_count;
    PaytoWallet pending;
    char pending_name[PAYTO_MAX_NAME_LEN + 1];
    char manual_entry[PAYTO_MAX_URI_LEN];
    char wallet_file_data[PAYTO_WALLET_FILE_BUFFER_SIZE];
    PaytoView text_input_back_view;
    PaytoView active_view;
    char streaming_name[PAYTO_SHORT_NAME_LEN];
    size_t selected_wallet;
} PaytoApp;

static char payto_streaming_name[PAYTO_SHORT_NAME_LEN];

static void payto_menu_callback(void* context, uint32_t index);
static void payto_show_pending_actions(PaytoApp* app);
static NfcCommand payto_poller_callback(NfcGenericEvent event, void* context);
static void payto_stop_stream(PaytoApp* app);

static void payto_switch_view(PaytoApp* app, PaytoView view) {
    if(app->active_view == PaytoViewStream && view != PaytoViewStream) {
        payto_stop_stream(app);
    }
    app->active_view = view;
    view_dispatcher_switch_to_view(app->dispatcher, view);
}

static void payto_stop_listener(PaytoApp* app) {
    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
}

static void payto_stop_stream(PaytoApp* app) {
    payto_stop_listener(app);
    if(app->notifications) notification_message(app->notifications, &sequence_blink_stop);
    if(app->emulation) {
        nfc_device_free(app->emulation);
        app->emulation = NULL;
    }
}

static bool payto_parse_ntag_ndef(const MfUltralightData* data, PaytoWallet* wallet) {
    const size_t byte_count = (size_t)data->pages_read * MF_ULTRALIGHT_PAGE_SIZE;
    const uint8_t* bytes = (const uint8_t*)data->page;
    for(size_t i = 4; i + 2 < byte_count; i++) {
        if(bytes[i] != 0x03) continue;
        size_t ndef_length = bytes[i + 1];
        size_t payload_offset = i + 2;
        if(ndef_length == 0xFF) {
            if(i + 4 > byte_count) return false;
            ndef_length = ((size_t)bytes[i + 2] << 8) | bytes[i + 3];
            payload_offset = i + 4;
        }
        if(payload_offset + ndef_length > byte_count) return false;
        return payto_parse_ndef(bytes + payload_offset, ndef_length, wallet);
    }
    return false;
}

static void payto_save_wallets(PaytoApp* app) {
    storage_common_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_common_mkdir(app->storage, EXT_PATH("apps_data/flipper_payto"));
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, PAYTO_STORAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(size_t i = 0; i < app->wallet_count; i++) {
            char line[PAYTO_MAX_URI_LEN + PAYTO_MAX_NAME_LEN + 4];
            int length = snprintf(
                line, sizeof(line), "%s\t%s\n", app->wallets[i].uri, app->wallets[i].name);
            storage_file_write(file, line, length);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
}

static void payto_load_wallets(PaytoApp* app) {
    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, PAYTO_STORAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return;
    }
    size_t length =
        storage_file_read(file, app->wallet_file_data, sizeof(app->wallet_file_data) - 1);
    app->wallet_file_data[length] = '\0';
    storage_file_close(file);
    storage_file_free(file);
    char* line = app->wallet_file_data;
    while(*line && app->wallet_count < PAYTO_MAX_WALLETS) {
        char* next_line = strchr(line, '\n');
        if(next_line) *next_line = '\0';
        char* separator = strchr(line, '\t');
        if(separator) {
            *separator++ = '\0';
            PaytoWallet* wallet = &app->wallets[app->wallet_count];
            if(payto_parse_uri(line, wallet)) {
                strncpy(wallet->name, separator, PAYTO_MAX_NAME_LEN);
                wallet->name[PAYTO_MAX_NAME_LEN] = '\0';
                app->wallet_count++;
            }
        }
        if(!next_line) break;
        line = next_line + 1;
    }
}

static void payto_rebuild_menu(PaytoApp* app) {
    submenu_reset(app->menu);
    submenu_set_header(app->menu, "PayTo Wallets");
    submenu_add_item(app->menu, "Enter wallet", PAYTO_MENU_ENTER_WALLET, payto_menu_callback, app);
    for(size_t i = 0; i < app->wallet_count; i++) {
        char label[PAYTO_SHORT_NAME_LEN + PAYTO_MAX_NAME_LEN + 4];
        if(app->wallets[i].name[0]) {
            size_t address_length = strlen(app->wallets[i].address);
            const char* suffix = address_length >= 4 ?
                                     app->wallets[i].address + address_length - 4 :
                                     app->wallets[i].address;
            snprintf(label, sizeof(label), "%s (%s)", app->wallets[i].name, suffix);
        } else {
            snprintf(label, sizeof(label), "%s", app->wallets[i].short_name);
        }
        submenu_add_item(app->menu, label, PAYTO_MENU_WALLET + i, payto_menu_callback, app);
    }
}

static void payto_rebuild_enter_menu(PaytoApp* app) {
    submenu_reset(app->enter_menu);
    submenu_set_header(app->enter_menu, "Enter wallet");
    submenu_add_item(app->enter_menu, "Scan NFC", PAYTO_MENU_SCAN, payto_menu_callback, app);
    submenu_add_item(app->enter_menu, "Manually", PAYTO_MENU_MANUAL, payto_menu_callback, app);
}

static void payto_stop_nfc(PaytoApp* app) {
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

static void payto_stop_scan(PaytoApp* app) {
    payto_stop_nfc(app);
    if(app->notifications) notification_message(app->notifications, &sequence_blink_stop);
}

static void payto_scan_draw(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Scanning NFC");
}

static void payto_stream_draw(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Streaming NFC:");
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, payto_streaming_name);
}

static NfcCommand payto_poller_callback(NfcGenericEvent event, void* context) {
    PaytoApp* app = context;
    MfUltralightPollerEvent* tag_event = event.event_data;
    if(tag_event->type == MfUltralightPollerEventTypeReadSuccess) {
        const MfUltralightData* data = nfc_poller_get_data(app->poller);
        if(payto_parse_ntag_ndef(data, &app->pending)) {
            view_dispatcher_send_custom_event(app->dispatcher, PAYTO_EVENT_SCANNED);
        }
        return NfcCommandStop;
    }
    return tag_event->type == MfUltralightPollerEventTypeReadFailed ? NfcCommandReset :
                                                                      NfcCommandContinue;
}

static void payto_start_scan(PaytoApp* app) {
    payto_stop_nfc(app);
    payto_switch_view(app, PaytoViewScan);
    if(!app->notifications) app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(app->notifications) notification_message(app->notifications, &sequence_blink_start_magenta);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    if(!app->poller) {
        payto_stop_scan(app);
        submenu_set_header(app->enter_menu, "NFC unavailable");
        payto_switch_view(app, PaytoViewEnterWallet);
        return;
    }
    nfc_poller_start(app->poller, payto_poller_callback, app);
}

static void payto_text_input_done(void* context) {
    PaytoApp* app = context;
    strncpy(app->pending.name, app->pending_name, PAYTO_MAX_NAME_LEN);
    app->pending.name[PAYTO_MAX_NAME_LEN] = '\0';
    if(app->wallet_count < PAYTO_MAX_WALLETS) app->wallets[app->wallet_count++] = app->pending;
    payto_save_wallets(app);
    payto_rebuild_menu(app);
    payto_switch_view(app, PaytoViewMenu);
}

static void payto_manual_input_done(void* context) {
    PaytoApp* app = context;
    char uri[PAYTO_MAX_URI_LEN];
    const char* entry = app->manual_entry;
    if(strncasecmp(entry, "payto://", 8) == 0) {
        snprintf(uri, sizeof(uri), "%s", entry);
    } else {
        snprintf(uri, sizeof(uri), "payto://%.183s", entry);
    }
    if(payto_parse_uri(uri, &app->pending)) {
        payto_show_pending_actions(app);
        payto_switch_view(app, PaytoViewMenu);
    } else {
        payto_rebuild_enter_menu(app);
        submenu_set_header(app->enter_menu, "Invalid PayTo URI");
        payto_switch_view(app, PaytoViewEnterWallet);
    }
}

static void payto_save_pending(PaytoApp* app, bool ask_for_name) {
    if(ask_for_name) {
        app->text_input_back_view = PaytoViewMenu;
        memset(app->pending_name, 0, sizeof(app->pending_name));
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Wallet name (optional)");
        text_input_set_result_callback(
            app->text_input,
            payto_text_input_done,
            app,
            app->pending_name,
            sizeof(app->pending_name),
            true);
        payto_switch_view(app, PaytoViewName);
    } else {
        app->pending.name[0] = '\0';
        if(app->wallet_count < PAYTO_MAX_WALLETS) app->wallets[app->wallet_count++] = app->pending;
        payto_save_wallets(app);
        payto_rebuild_menu(app);
    }
}

static void payto_rebuild_wallet_actions(PaytoApp* app) {
    PaytoWallet* wallet = &app->wallets[app->selected_wallet];
    submenu_reset(app->wallet_actions);
    submenu_set_header(app->wallet_actions, wallet->name[0] ? wallet->name : wallet->short_name);
    submenu_add_item(
        app->wallet_actions, "Stream NFC", PAYTO_MENU_STREAM, payto_menu_callback, app);
    submenu_add_item(
        app->wallet_actions, "Show Wallet", PAYTO_MENU_PRINT, payto_menu_callback, app);
    submenu_add_item(app->wallet_actions, "Remove", PAYTO_MENU_REMOVE, payto_menu_callback, app);
}

static void payto_stream_wallet(PaytoApp* app, PaytoWallet* wallet) {
    FURI_LOG_I("PayTo", "stream begin");
    const size_t uri_length = strlen(wallet->uri);
    const size_t ndef_length = uri_length + 5;
    if(ndef_length > 255) {
        submenu_set_header(app->wallet_actions, "URI too long for NFC");
        return;
    }
    payto_stop_nfc(app);
    payto_stop_stream(app);
    app->emulation = nfc_device_alloc();
    FURI_LOG_I("PayTo", "device allocated");
    if(!app->emulation) {
        submenu_set_header(app->wallet_actions, "NFC unavailable");
        return;
    }
    nfc_data_generator_fill_data(NfcDataGeneratorTypeNTAG215, app->emulation);
    FURI_LOG_I("PayTo", "device data generated");
    const MfUltralightData* original =
        nfc_device_get_data(app->emulation, NfcProtocolMfUltralight);
    if(!original || !original->iso14443_3a_data) {
        nfc_device_free(app->emulation);
        app->emulation = NULL;
        submenu_set_header(app->wallet_actions, "NFC data unavailable");
        return;
    }
    FURI_LOG_I("PayTo", "source data valid");
    MfUltralightData* data = malloc(sizeof(MfUltralightData));
    Iso14443_3aData* iso_data = malloc(sizeof(Iso14443_3aData));
    if(!data || !iso_data) {
        free(data);
        free(iso_data);
        nfc_device_free(app->emulation);
        app->emulation = NULL;
        submenu_set_header(app->wallet_actions, "Not enough memory");
        return;
    }
    memcpy(data, original, sizeof(MfUltralightData));
    memcpy(iso_data, original->iso14443_3a_data, sizeof(Iso14443_3aData));
    data->iso14443_3a_data = iso_data;
    FURI_LOG_I("PayTo", "data copied");
    const size_t emulation_size = (size_t)data->pages_total * MF_ULTRALIGHT_PAGE_SIZE;
    if(PAYTO_NDEF_OFFSET + 2 + ndef_length + 1 > emulation_size) {
        free(iso_data);
        free(data);
        nfc_device_free(app->emulation);
        app->emulation = NULL;
        submenu_set_header(app->wallet_actions, "URI too long for NFC");
        return;
    }
    uint8_t* ndef = (uint8_t*)data->page;
    ndef[PAYTO_NDEF_OFFSET] = 0x03;
    ndef[PAYTO_NDEF_OFFSET + 1] = (uint8_t)ndef_length;
    ndef[PAYTO_NDEF_OFFSET + 2] = 0xD1;
    ndef[PAYTO_NDEF_OFFSET + 3] = 0x01;
    ndef[PAYTO_NDEF_OFFSET + 4] = (uint8_t)(uri_length + 1);
    ndef[PAYTO_NDEF_OFFSET + 5] = 'U';
    ndef[PAYTO_NDEF_OFFSET + 6] = 0;
    memcpy(&ndef[PAYTO_NDEF_OFFSET + 7], wallet->uri, uri_length);
    ndef[PAYTO_NDEF_OFFSET + ndef_length + 2] = 0xFE;
    data->pages_read = data->pages_total;
    nfc_device_set_data(app->emulation, NfcProtocolMfUltralight, data);
    FURI_LOG_I("PayTo", "data installed");
    free(iso_data);
    free(data);
    const MfUltralightData* listener_data =
        nfc_device_get_data(app->emulation, NfcProtocolMfUltralight);
    if(!listener_data) {
        nfc_device_free(app->emulation);
        app->emulation = NULL;
        submenu_set_header(app->wallet_actions, "NFC data unavailable");
        return;
    }
    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, listener_data);
    FURI_LOG_I("PayTo", "listener allocated");
    if(!app->listener) {
        nfc_device_free(app->emulation);
        app->emulation = NULL;
        submenu_set_header(app->wallet_actions, "NFC listener unavailable");
        return;
    }
    nfc_listener_start(app->listener, NULL, NULL);
    FURI_LOG_I("PayTo", "listener started");
    if(!app->notifications) app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(app->notifications) notification_message(app->notifications, &sequence_blink_start_magenta);
    if(wallet->name[0]) {
        const size_t address_length = strlen(wallet->address);
        const char* suffix = address_length >= 4 ? wallet->address + address_length - 4 :
                                                   wallet->address;
        snprintf(
            payto_streaming_name, sizeof(payto_streaming_name), "%s (%s)", wallet->name, suffix);
    } else {
        snprintf(payto_streaming_name, sizeof(payto_streaming_name), "%s", wallet->short_name);
    }
    payto_switch_view(app, PaytoViewStream);
}

static void payto_menu_callback(void* context, uint32_t index) {
    PaytoApp* app = context;
    if(index == PAYTO_MENU_ENTER_WALLET)
        payto_switch_view(app, PaytoViewEnterWallet);
    else if(index == PAYTO_MENU_SCAN)
        payto_start_scan(app);
    else if(index == PAYTO_MENU_MANUAL) {
        app->text_input_back_view = PaytoViewEnterWallet;
        memset(app->manual_entry, 0, sizeof(app->manual_entry));
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Enter currency/address");
        text_input_set_result_callback(
            app->text_input,
            payto_manual_input_done,
            app,
            app->manual_entry,
            sizeof(app->manual_entry),
            true);
        text_input_set_minimum_length(app->text_input, 3);
        payto_switch_view(app, PaytoViewName);
    } else if(index == PAYTO_MENU_SAVE)
        payto_save_pending(app, false);
    else if(index == PAYTO_MENU_NAME)
        payto_save_pending(app, true);
    else if(index == PAYTO_MENU_DISCARD)
        payto_rebuild_menu(app);
    else if(index == PAYTO_MENU_STREAM) {
        payto_stream_wallet(app, &app->wallets[app->selected_wallet]);
    } else if(index == PAYTO_MENU_PRINT) {
        text_box_set_text(app->wallet_text, app->wallets[app->selected_wallet].uri);
        payto_switch_view(app, PaytoViewPrint);
    } else if(index == PAYTO_MENU_REMOVE) {
        for(size_t i = app->selected_wallet; i + 1 < app->wallet_count; i++) {
            app->wallets[i] = app->wallets[i + 1];
        }
        if(app->wallet_count) app->wallet_count--;
        payto_save_wallets(app);
        payto_rebuild_menu(app);
        payto_switch_view(app, PaytoViewMenu);
    } else if(index >= PAYTO_MENU_WALLET && index < PAYTO_MENU_WALLET + app->wallet_count) {
        app->selected_wallet = index - PAYTO_MENU_WALLET;
        payto_rebuild_wallet_actions(app);
        payto_switch_view(app, PaytoViewWalletActions);
    }
}

static bool payto_custom_event(void* context, uint32_t event) {
    PaytoApp* app = context;
    if(event == PAYTO_EVENT_SCANNED) {
        payto_stop_scan(app);
        payto_show_pending_actions(app);
        payto_switch_view(app, PaytoViewMenu);
        return true;
    }
    return false;
}

static void payto_show_pending_actions(PaytoApp* app) {
    submenu_reset(app->menu);
    submenu_set_header(app->menu, app->pending.short_name);
    submenu_add_item(app->menu, "Save unnamed", PAYTO_MENU_SAVE, payto_menu_callback, app);
    submenu_add_item(app->menu, "Add name", PAYTO_MENU_NAME, payto_menu_callback, app);
    submenu_add_item(app->menu, "Discard", PAYTO_MENU_DISCARD, payto_menu_callback, app);
}

static bool payto_navigation_callback(void* context) {
    PaytoApp* app = context;
    if(app->active_view == PaytoViewName || app->active_view == PaytoViewEnterWallet) {
        payto_switch_view(
            app, app->active_view == PaytoViewName ? app->text_input_back_view : PaytoViewMenu);
        return true;
    }
    if(app->active_view == PaytoViewScan) {
        payto_stop_scan(app);
        payto_switch_view(app, PaytoViewEnterWallet);
        return true;
    }
    if(app->active_view == PaytoViewStream) {
        payto_switch_view(app, PaytoViewWalletActions);
        return true;
    }
    if(app->active_view == PaytoViewPrint) {
        payto_switch_view(app, PaytoViewWalletActions);
        return true;
    }
    if(app->active_view == PaytoViewWalletActions) {
        payto_switch_view(app, PaytoViewMenu);
        return true;
    }
    payto_stop_nfc(app);
    payto_stop_listener(app);
    view_dispatcher_stop(app->dispatcher);
    return true;
}

int32_t flipper_payto_app(void* context) {
    UNUSED(context);
    PaytoApp* app = malloc(sizeof(PaytoApp));
    if(!app) return -1;
    memset(app, 0, sizeof(PaytoApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->nfc = nfc_alloc();
    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, payto_custom_event);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, payto_navigation_callback);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    app->menu = submenu_alloc();
    app->enter_menu = submenu_alloc();
    app->wallet_actions = submenu_alloc();
    app->text_input = text_input_alloc();
    app->wallet_text = text_box_alloc();
    app->scan_view = view_alloc();
    view_set_draw_callback(app->scan_view, payto_scan_draw);
    app->stream_view = view_alloc();
    view_set_context(app->stream_view, app);
    view_set_draw_callback(app->stream_view, payto_stream_draw);
    view_dispatcher_add_view(app->dispatcher, PaytoViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(
        app->dispatcher, PaytoViewEnterWallet, submenu_get_view(app->enter_menu));
    view_dispatcher_add_view(app->dispatcher, PaytoViewName, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->dispatcher, PaytoViewScan, app->scan_view);
    view_dispatcher_add_view(app->dispatcher, PaytoViewStream, app->stream_view);
    view_dispatcher_add_view(
        app->dispatcher, PaytoViewWalletActions, submenu_get_view(app->wallet_actions));
    view_dispatcher_add_view(app->dispatcher, PaytoViewPrint, text_box_get_view(app->wallet_text));
    payto_load_wallets(app);
    payto_rebuild_menu(app);
    payto_rebuild_enter_menu(app);
    payto_switch_view(app, PaytoViewMenu);
    view_dispatcher_run(app->dispatcher);
    payto_stop_nfc(app);
    if(app->notifications) notification_message(app->notifications, &sequence_blink_stop);
    payto_stop_stream(app);
    nfc_free(app->nfc);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewMenu);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewEnterWallet);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewName);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewScan);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewStream);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewWalletActions);
    view_dispatcher_remove_view(app->dispatcher, PaytoViewPrint);
    submenu_free(app->menu);
    submenu_free(app->enter_menu);
    submenu_free(app->wallet_actions);
    text_input_free(app->text_input);
    text_box_free(app->wallet_text);
    view_free(app->scan_view);
    view_free(app->stream_view);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    if(app->notifications) furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}
