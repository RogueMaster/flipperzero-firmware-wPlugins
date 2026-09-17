#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Filter options for the .pcap table viewer (pcap_view.c) - a checkbox per frame type plus an
   SSID row that opens a picker over the SSIDs actually seen in the file (pcap_filter_ssid.c),
   and, below that, file-management rows (Rename/Delete) for the file currently open in the
   viewer. Nothing here touches app->pcap_index directly; the checkboxes only edit
   app->pcap_filter_type_mask/pcap_filter_ssids, which pcap_view.c re-applies on every redraw, so
   toggling a box here and backing out is effectively free. Rename/Delete are the exception - they
   hand off to their own scenes (pcap_rename.c/pcap_delete_confirm.c) that touch the file itself.

   When the file is empty/invalid (pcap_index_count == 0, e.g. a capture that never wrote a
   single valid record) the type/SSID rows have nothing to filter, so this scene shows ONLY the
   Rename/Delete rows - which is exactly what's needed to get rid of a bad capture, and previously
   couldn't be reached at all: pcap_view.c used to swallow every keypress (Right included) on an
   empty file, so this screen was simply unreachable for the one case a user most needs it. */

static const MarauderPcapFrameType marauder_pcap_filter_types[] = {
    MarauderPcapFrameBeacon,
    MarauderPcapFrameProbe,
    MarauderPcapFrameDeauth,
    MarauderPcapFrameAuth,
    MarauderPcapFrameAssoc,
    MarauderPcapFrameData,
    MarauderPcapFrameEapol,
    MarauderPcapFrameOther,
};
#define MARAUDER_PCAP_FILTER_TYPE_COUNT \
    (sizeof(marauder_pcap_filter_types) / sizeof(marauder_pcap_filter_types[0]))
#define PCAP_FILTER_SSID_ROW_INDEX   MARAUDER_PCAP_FILTER_TYPE_COUNT
#define PCAP_FILTER_RENAME_ROW_INDEX (MARAUDER_PCAP_FILTER_TYPE_COUNT + 1)
#define PCAP_FILTER_DELETE_ROW_INDEX (MARAUDER_PCAP_FILTER_TYPE_COUNT + 2)
#define PCAP_FILTER_ITEM_COUNT       (MARAUDER_PCAP_FILTER_TYPE_COUNT + 3)

static char marauder_pcap_filter_labels_tr[PCAP_FILTER_ITEM_COUNT][40];
static char marauder_pcap_filter_labels_en[PCAP_FILTER_ITEM_COUNT][40];
static MarauderMenuItem marauder_pcap_filter_items[PCAP_FILTER_ITEM_COUNT];

static const char* const marauder_pcap_filter_descriptions_tr[] = {
    "Beacon cerceveleri (AP'lerin kendini duyurmasi).",
    "Probe request/response cerceveleri.",
    "Deauth (baglanti kesme) cerceveleri.",
    "Authentication cerceveleri.",
    "(Re)Association request/response cerceveleri.",
    "Sifreli veri cerceveleri (EAPOL haric).",
    "WPA el sikismasi (EAPOL) cerceveleri - handshake yakalamak icin en onemlisi.",
    "Yukaridakilerin disinda kalan/taninmayan cerceveler.",
    "Bu dosyada gorulen SSID'lerden birini/birkacini secip sadece onlari gosterir. Hicbiri secili degilse hepsi gosterilir.",
    "Bu .pcap dosyasinin adini degistirir.",
    "Bu .pcap dosyasini SD karttan kalici olarak siler.",
};
static const char* const marauder_pcap_filter_descriptions_en[] = {
    "Beacon frames (APs announcing themselves).",
    "Probe request/response frames.",
    "Deauth (disconnect) frames.",
    "Authentication frames.",
    "(Re)Association request/response frames.",
    "Encrypted data frames (excluding EAPOL).",
    "WPA handshake (EAPOL) frames - the most important one for capturing handshakes.",
    "Frames that don't fall into any of the above/unrecognized.",
    "Pick one or more SSIDs seen in this file to show only those. None selected = show everything.",
    "Renames this .pcap file.",
    "Permanently deletes this .pcap file from the SD card.",
};

static void marauder_pcap_filter_relabel(MarauderGuiApp* app, size_t item_index) {
    if(item_index < MARAUDER_PCAP_FILTER_TYPE_COUNT) {
        MarauderPcapFrameType type = marauder_pcap_filter_types[item_index];
        bool on = (app->pcap_filter_type_mask & (1u << type)) != 0;
        const char* name_tr = marauder_pcap_frame_type_label(type, false);
        const char* name_en = marauder_pcap_frame_type_label(type, true);
        snprintf(
            marauder_pcap_filter_labels_tr[item_index],
            sizeof(marauder_pcap_filter_labels_tr[item_index]),
            "[%c] %s",
            on ? 'x' : ' ',
            name_tr);
        snprintf(
            marauder_pcap_filter_labels_en[item_index],
            sizeof(marauder_pcap_filter_labels_en[item_index]),
            "[%c] %s",
            on ? 'x' : ' ',
            name_en);
    } else if(item_index == PCAP_FILTER_SSID_ROW_INDEX) {
        if(app->pcap_filter_ssid_count == 0) {
            snprintf(
                marauder_pcap_filter_labels_tr[item_index],
                sizeof(marauder_pcap_filter_labels_tr[item_index]),
                "SSID: (hepsi)");
            snprintf(
                marauder_pcap_filter_labels_en[item_index],
                sizeof(marauder_pcap_filter_labels_en[item_index]),
                "SSID: (all)");
        } else {
            snprintf(
                marauder_pcap_filter_labels_tr[item_index],
                sizeof(marauder_pcap_filter_labels_tr[item_index]),
                "SSID: %u secili",
                (unsigned)app->pcap_filter_ssid_count);
            snprintf(
                marauder_pcap_filter_labels_en[item_index],
                sizeof(marauder_pcap_filter_labels_en[item_index]),
                "SSID: %u selected",
                (unsigned)app->pcap_filter_ssid_count);
        }
    } else if(item_index == PCAP_FILTER_RENAME_ROW_INDEX) {
        snprintf(
            marauder_pcap_filter_labels_tr[item_index],
            sizeof(marauder_pcap_filter_labels_tr[item_index]),
            "Yeniden Adlandir>");
        snprintf(
            marauder_pcap_filter_labels_en[item_index],
            sizeof(marauder_pcap_filter_labels_en[item_index]),
            "Rename>");
    } else {
        snprintf(
            marauder_pcap_filter_labels_tr[item_index],
            sizeof(marauder_pcap_filter_labels_tr[item_index]),
            "Dosyayi Sil>");
        snprintf(
            marauder_pcap_filter_labels_en[item_index],
            sizeof(marauder_pcap_filter_labels_en[item_index]),
            "Delete File>");
    }
}

/* An empty/invalid file (pcap_index_count == 0) hides the type/SSID rows entirely - see the file
   comment at the top - so only 2 rows are shown, at slots 0/1, instead of the usual
   PCAP_FILTER_ITEM_COUNT. marauder_pcap_filter_real_index() maps a displayed slot back to its
   real item index (identity in the normal case) so marauder_pcap_filter_relabel() and the
   descriptions/labels arrays above never need a second, duplicate set of entries for the
   empty-file case - Rename/Delete just get displayed at different slots. */
static bool marauder_pcap_filter_management_only(const MarauderGuiApp* app) {
    return app->pcap_index_count == 0;
}

static size_t marauder_pcap_filter_visible_count(const MarauderGuiApp* app) {
    return marauder_pcap_filter_management_only(app) ? 2 : PCAP_FILTER_ITEM_COUNT;
}

static size_t marauder_pcap_filter_real_index(const MarauderGuiApp* app, size_t slot) {
    if(marauder_pcap_filter_management_only(app)) {
        return (slot == 0) ? PCAP_FILTER_RENAME_ROW_INDEX : PCAP_FILTER_DELETE_ROW_INDEX;
    }
    return slot;
}

void marauder_gui_scene_pcap_filter_on_enter(void* context) {
    MarauderGuiApp* app = context;
    size_t count = marauder_pcap_filter_visible_count(app);

    for(size_t slot = 0; slot < count; slot++) {
        size_t real = marauder_pcap_filter_real_index(app, slot);
        marauder_pcap_filter_relabel(app, real);
        marauder_pcap_filter_items[slot].label_tr = marauder_pcap_filter_labels_tr[real];
        marauder_pcap_filter_items[slot].label_en = marauder_pcap_filter_labels_en[real];
        marauder_pcap_filter_items[slot].description_tr =
            marauder_pcap_filter_descriptions_tr[real];
        marauder_pcap_filter_items[slot].description_en =
            marauder_pcap_filter_descriptions_en[real];
    }

    marauder_gui_menu_set_items(
        app, marauder_pcap_filter_items, count, marauder_gui_text(app, "Filtre", "Filter"));
}

bool marauder_gui_scene_pcap_filter_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        size_t real = marauder_pcap_filter_real_index(app, event.event);
        if(real < MARAUDER_PCAP_FILTER_TYPE_COUNT) {
            MarauderPcapFrameType type = marauder_pcap_filter_types[real];
            app->pcap_filter_type_mask ^= (1u << type);
            marauder_pcap_filter_relabel(app, real);
            marauder_gui_menu_redraw(app);
            consumed = true;
        } else if(real == PCAP_FILTER_SSID_ROW_INDEX) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapFilterSsid);
            consumed = true;
        } else if(real == PCAP_FILTER_RENAME_ROW_INDEX) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapRename);
            consumed = true;
        } else if(real == PCAP_FILTER_DELETE_ROW_INDEX) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapDeleteConfirm);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_pcap_filter_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
