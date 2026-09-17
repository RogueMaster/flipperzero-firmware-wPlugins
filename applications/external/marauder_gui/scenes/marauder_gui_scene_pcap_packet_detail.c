#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Shows one packet's parsed header fields - reached by pressing Ok on a row in pcap_view.c. The
   index only stores a tiny summary (see marauder_gui_pcap_index.h), so this re-reads that one
   packet's bytes from the file on demand via its cached record_offset, rather than keeping every
   packet's full bytes resident.

   Only the common 3-address (non-WDS) 802.11 MAC header is decoded: Addr1 is the
   receiver/destination, Addr2 the transmitter/source, Addr3 is usually the BSSID (its exact
   meaning flips with the ToDS/FromDS bits, which isn't spelled out here - v1 just labels them
   A1/A2/A3 like a packet sniffer would rather than guessing wrong labels for every frame type). */

enum {
    PcapPacketDetailEventHex,
};

static void marauder_gui_format_mac(const uint8_t* mac, char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

/* Widget has no hook for a raw Right keypress (only registered button elements fire a callback -
   same limitation already worked around for log_scan's "Devam Et" button), so "view as hex" is a
   right-aligned button element instead of a scene on_event custom-key check. */
static void marauder_gui_pcap_packet_detail_hex_button_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    MarauderGuiApp* app = context;
    if(button_type != GuiButtonTypeRight || input_type != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, PcapPacketDetailEventHex);
}

void marauder_gui_scene_pcap_packet_detail_on_enter(void* context) {
    MarauderGuiApp* app = context;

    widget_reset(app->widget);

    if(app->pcap_view_selected >= app->pcap_index_count) {
        view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
        return;
    }
    const MarauderPcapIndexEntry* e = &app->pcap_index[app->pcap_view_selected];

    uint8_t buf[32];
    memset(buf, 0, sizeof(buf));
    size_t got = 0;

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MARAUDER_GUI_PCAP_DIR, app->pcap_view_filename);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_seek(file, e->record_offset + 16, true)) {
            got = storage_file_read(file, buf, sizeof(buf));
        }
        storage_file_close(file);
    }
    storage_file_free(file);

    char header[32];
    snprintf(
        header,
        sizeof(header),
        "#%u %s",
        (unsigned)app->pcap_view_selected,
        marauder_pcap_frame_type_label(
            (MarauderPcapFrameType)e->frame_type, app->language == MarauderLanguageEnglish));
    widget_add_string_element(app->widget, 2, 0, AlignLeft, AlignTop, FontPrimary, header);

    if(got < 22) {
        widget_add_string_element(
            app->widget,
            2,
            14,
            AlignLeft,
            AlignTop,
            FontSecondary,
            marauder_gui_text(app, "Cerceve cok kisa/okunamadi", "Frame too short/unreadable"));
        view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
        return;
    }

    char a1[18], a2[18], a3[18], line[40];
    marauder_gui_format_mac(buf + 4, a1, sizeof(a1));
    marauder_gui_format_mac(buf + 10, a2, sizeof(a2));
    marauder_gui_format_mac(buf + 16, a3, sizeof(a3));

    snprintf(line, sizeof(line), "A1: %s", a1);
    widget_add_string_element(app->widget, 2, 13, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "A2: %s", a2);
    widget_add_string_element(app->widget, 2, 23, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "A3: %s", a3);
    widget_add_string_element(app->widget, 2, 33, AlignLeft, AlignTop, FontSecondary, line);

    if(e->ssid[0]) {
        snprintf(line, sizeof(line), "SSID: %s", e->ssid);
    } else {
        snprintf(
            line,
            sizeof(line),
            marauder_gui_text(app, "Uzunluk: %u bayt", "Length: %u bytes"),
            (unsigned)e->incl_len);
    }
    widget_add_string_element(app->widget, 2, 43, AlignLeft, AlignTop, FontSecondary, line);

    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        marauder_gui_text(app, "Hex", "Hex"),
        marauder_gui_pcap_packet_detail_hex_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
}

bool marauder_gui_scene_pcap_packet_detail_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == PcapPacketDetailEventHex) {
        scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapPacketHex);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_packet_detail_on_exit(void* context) {
    MarauderGuiApp* app = context;
    widget_reset(app->widget);
}
