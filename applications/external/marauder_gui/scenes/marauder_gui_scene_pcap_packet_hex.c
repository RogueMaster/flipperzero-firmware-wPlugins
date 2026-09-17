#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Wireshark-style hex+ASCII dump of one packet's raw bytes - reached via the "Hex" button on
   pcap_packet_detail.c. Re-reads the packet from the file via its cached record_offset (same as
   the detail screen), same reasoning as there: the index only keeps a tiny summary per packet,
   not the full bytes, so there's nothing to dump without going back to the file.

   widget_add_text_scroll_element()'s "\e*...\e*" escape switches to Widget's built-in monospaced
   font for the rest of that line - without it hex columns wouldn't line up, since the normal UI
   font is proportional. Bytes-per-row was tried at 8 then 6 and both still wrapped the line on
   the 128px-wide screen - the monospace overlay glyph is wider than assumed. 4 bytes/row keeps
   the whole line ("XX "*4 + gap + ascii(4) = 17 chars) comfortably inside even a generous
   per-glyph width estimate, so hex (left) and its ascii (right) stay on one physical line and
   line up as two clean columns down the screen instead of getting torn apart by a mid-row wrap. */

#define PCAP_HEX_BYTES_PER_ROW 4
#define PCAP_HEX_MAX_BYTES     252

void marauder_gui_scene_pcap_packet_hex_on_enter(void* context) {
    MarauderGuiApp* app = context;

    widget_reset(app->widget);

    if(app->pcap_view_selected >= app->pcap_index_count) {
        view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
        return;
    }
    const MarauderPcapIndexEntry* e = &app->pcap_index[app->pcap_view_selected];

    uint8_t buf[PCAP_HEX_MAX_BYTES];
    size_t got = 0;

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MARAUDER_GUI_PCAP_DIR, app->pcap_view_filename);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_seek(file, e->record_offset + 16, true)) {
            size_t want = (e->incl_len < PCAP_HEX_MAX_BYTES) ? e->incl_len : PCAP_HEX_MAX_BYTES;
            got = storage_file_read(file, buf, want);
        }
        storage_file_close(file);
    }
    storage_file_free(file);

    FuriString* text = furi_string_alloc();
    for(size_t off = 0; off < got; off += PCAP_HEX_BYTES_PER_ROW) {
        char ascii[PCAP_HEX_BYTES_PER_ROW + 1];
        furi_string_cat_printf(text, "\e*");
        for(size_t i = 0; i < PCAP_HEX_BYTES_PER_ROW; i++) {
            if(off + i < got) {
                uint8_t b = buf[off + i];
                furi_string_cat_printf(text, "%02X ", b);
                ascii[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
            } else {
                furi_string_cat_printf(text, "   ");
                ascii[i] = ' ';
            }
        }
        ascii[PCAP_HEX_BYTES_PER_ROW] = '\0';
        furi_string_cat_printf(text, " %s\n", ascii);
    }
    if(got == 0) {
        furi_string_set(text, marauder_gui_text(app, "Okunamadi", "Could not read"));
    } else if(e->incl_len > got) {
        furi_string_cat_printf(
            text,
            marauder_gui_text(app, "\n(+%u bayt daha)", "\n(+%u more bytes)"),
            (unsigned)(e->incl_len - got));
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
}

bool marauder_gui_scene_pcap_packet_hex_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_packet_hex_on_exit(void* context) {
    MarauderGuiApp* app = context;
    widget_reset(app->widget);
}
