#include "marauder_gui_app_i.h"
#include <string.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <furi_hal_power.h>

#define MARAUDER_TICK_PERIOD_MS 100

/* One byte, persisted across reboots so the user only has to pick a language once. Missing file
   (first run, or an older version without this feature) leaves the English default selected. */
void marauder_gui_load_language(MarauderGuiApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("language.bin"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t value = MarauderLanguageTurkish;
        if(storage_file_read(file, &value, sizeof(value)) == sizeof(value)) {
            app->language = (value == MarauderLanguageEnglish) ? MarauderLanguageEnglish :
                                                                 MarauderLanguageTurkish;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void marauder_gui_save_language(MarauderGuiApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("language.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t value = (uint8_t)app->language;
        storage_file_write(file, &value, sizeof(value));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static bool marauder_gui_custom_event_callback(void* context, uint32_t event) {
    MarauderGuiApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool marauder_gui_navigation_event_callback(void* context) {
    MarauderGuiApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* Attributes one live-captured frame (peeked bytes from MarauderPcapLiveParser) to an AP row in
   app->pcap_live_aps, grouped by BSSID rather than SSID so frame types that carry no SSID of
   their own (Data, Auth, Deauth, ...) still land on the right row. BSSID is read straight out of
   the 802.11 MAC header rather than reusing marauder_pcap_classify_frame()'s SSID-only output,
   since that function (shared with the saved-file indexer, which has no need for addressing)
   never looks at addr1/addr2/addr3.

   For every Management subtype (Beacon, Probe, Auth, Deauth, Assoc, ...) addr3 is always the
   BSSID per the 802.11 spec - no per-subtype special-casing needed. For Data frames the BSSID
   depends on which of ToDS/FromDS is set (station->AP: addr1, AP->station: addr2, IBSS: addr3);
   the rare ToDS+FromDS (WDS, 4-address) case has no single BSSID field and is left unattributed,
   same as marauder_pcap_classify_frame() already does for it. */
static void marauder_gui_pcap_live_frame_callback(void* ctx, const uint8_t* data, size_t len) {
    MarauderGuiApp* app = ctx;
    if(len < 24) return; /* no full MAC header - nothing to resolve an address out of */

    char ssid[16];
    MarauderPcapFrameType type = marauder_pcap_classify_frame(data, len, ssid, sizeof(ssid));

    uint8_t frame_type = (data[0] >> 2) & 0x3;
    const uint8_t* bssid = NULL;
    if(frame_type == 0) {
        bssid = data + 16; /* Management - addr3 */
    } else if(frame_type == 2) {
        bool to_ds = data[1] & 0x1;
        bool from_ds = (data[1] >> 1) & 0x1;
        if(to_ds && !from_ds) {
            bssid = data + 4; /* Data, station -> AP: addr1 */
        } else if(!to_ds && from_ds) {
            bssid = data + 10; /* Data, AP -> station: addr2 */
        } else if(!to_ds && !from_ds) {
            bssid = data + 16; /* Data, IBSS: addr3 */
        }
    }
    if(!bssid) return;

    static const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if(memcmp(bssid, broadcast, 6) == 0) return; /* undirected probe etc. - nothing to attribute */

    size_t row = app->pcap_live_ap_count;
    for(size_t i = 0; i < app->pcap_live_ap_count; i++) {
        if(memcmp(app->pcap_live_aps[i].bssid, bssid, 6) == 0) {
            row = i;
            break;
        }
    }
    if(row == app->pcap_live_ap_count) {
        if(app->pcap_live_ap_count >= MARAUDER_AP_LIST_MAX)
            return; /* dashboard full - keep tracking what we already have */
        memcpy(app->pcap_live_aps[row].bssid, bssid, 6);
        app->pcap_live_aps[row].ssid[0] = '\0';
        memset(app->pcap_live_aps[row].counts, 0, sizeof(app->pcap_live_aps[row].counts));
        app->pcap_live_ap_count++;
    }
    if(ssid[0]) {
        strncpy(app->pcap_live_aps[row].ssid, ssid, sizeof(app->pcap_live_aps[row].ssid) - 1);
        app->pcap_live_aps[row].ssid[sizeof(app->pcap_live_aps[row].ssid) - 1] = '\0';
    }
    app->pcap_live_aps[row].counts[type]++;
    app->pcap_live_dirty = true;
}

/* Drains bytes received over UART since the last tick, reassembles them into lines, and
   forwards each complete line to whichever scene currently wants them (if any). Runs on the
   ViewDispatcher's own thread, so it is safe to touch view/model state directly here. */
static void marauder_gui_tick_event_callback(void* context) {
    MarauderGuiApp* app = context;

    uint8_t buf[64];
    size_t received;

    while((received = marauder_uart_receive(app->uart, buf, sizeof(buf))) > 0) {
        for(size_t i = 0; i < received; i++) {
            char c = (char)buf[i];

            if(c == '\r') continue;

            if(c == '\n') {
                app->rx_line_buf[app->rx_line_len] = '\0';
                if(app->rx_line_len > 0 && app->uart_line_handler) {
                    app->uart_line_handler(app, app->rx_line_buf);
                }
                app->rx_line_len = 0;
            } else if(app->rx_line_len < MARAUDER_LINE_MAX - 1) {
                app->rx_line_buf[app->rx_line_len++] = c;
            }
        }
    }

    /* Drain the demuxed PCAP byte stream every tick. Always drain (so it can't back up and
       stall the shared serial callback when nobody is capturing); only write to SD while a
       capture scene has opened a file and set is_writing_pcap. */
    uint8_t pcap_buf[128];
    size_t pcap_len;
    while((pcap_len = marauder_uart_receive_pcap(app->uart, pcap_buf, sizeof(pcap_buf))) > 0) {
        if(app->is_writing_pcap && app->capture_file) {
            storage_file_write(app->capture_file, pcap_buf, pcap_len);
            app->pcap_bytes += pcap_len;
            marauder_pcap_live_parser_feed(
                &app->pcap_live_parser,
                pcap_buf,
                pcap_len,
                marauder_gui_pcap_live_frame_callback,
                app);
        }
    }

    if(app->tick_handler) {
        app->tick_handler(app);
    }
}

static MarauderGuiApp* marauder_gui_app_alloc(void) {
    MarauderGuiApp* app = malloc(sizeof(MarauderGuiApp));
    memset(app, 0, sizeof(MarauderGuiApp));

    app->selected_ap_index = -1;
    app->selected_tracker_index = -1;
    app->selected_probe_index = -1;
    app->selected_ip_index = -1;

    app->language = MarauderLanguageEnglish;
    marauder_gui_load_language(app);

    app->uart = marauder_uart_alloc();

    /* Persistent storage handle + capture file, used by the PCAP sniff scene to write .pcap
       files to the Flipper SD card. */
    app->storage = furi_record_open(RECORD_STORAGE);
    app->capture_file = storage_file_alloc(app->storage);

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&marauder_gui_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, marauder_gui_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, marauder_gui_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, marauder_gui_tick_event_callback, MARAUDER_TICK_PERIOD_MS);

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MarauderGuiViewWidget, widget_get_view(app->widget));

    app->wifi_list_view = marauder_gui_wifi_list_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, MarauderGuiViewWifiList, app->wifi_list_view);

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MarauderGuiViewNumberInput,
        number_input_get_view(app->number_input));

    app->text_input = marauder_text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        MarauderGuiViewTextInput,
        marauder_text_input_get_view(app->text_input));

    app->menu_view = marauder_gui_menu_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, MarauderGuiViewMenu, app->menu_view);

    app->attack_status_view = marauder_gui_attack_view_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, MarauderGuiViewAttackStatus, app->attack_status_view);

    app->pcap_table_view = marauder_gui_pcap_table_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, MarauderGuiViewPcapTable, app->pcap_table_view);

    return app;
}

static void marauder_gui_app_free(MarauderGuiApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewWifiList);
    view_free(app->wifi_list_view);

    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewTextInput);
    marauder_text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewMenu);
    view_free(app->menu_view);

    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewAttackStatus);
    view_free(app->attack_status_view);

    view_dispatcher_remove_view(app->view_dispatcher, MarauderGuiViewPcapTable);
    view_free(app->pcap_table_view);

    if(app->pcap_index) {
        free(app->pcap_index);
    }

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    marauder_uart_free(app->uart);

    if(app->capture_file) {
        if(storage_file_is_open(app->capture_file)) {
            storage_file_close(app->capture_file);
        }
        storage_file_free(app->capture_file);
    }
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t marauder_gui_app(void* p) {
    UNUSED(p);

    /* Power the ESP32 board over the GPIO 5V pin, matching the WiFi Marauder companion app:
       enable OTG on launch (retry a few times, it can briefly fail under load) and disable it
       again on exit - but only if we were the ones who turned it on. */
    uint8_t otg_attempts = 0;
    bool otg_was_enabled = furi_hal_power_is_otg_enabled();
    while(!furi_hal_power_is_otg_enabled() && otg_attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    furi_delay_ms(200);

    MarauderGuiApp* app = marauder_gui_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, MarauderGuiSceneMainMenu);

    view_dispatcher_run(app->view_dispatcher);

    marauder_gui_app_free(app);

    if(furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }

    return 0;
}
