#include "../marauder_gui_app_i.h"
#include <stdio.h>

/* Channel picker for the PMKID "on channel" modes - the only extra step those need (no scan, no
   AP). Builds "<base> <channel>" (e.g. "sniffpmkid -d -c 6") from app->pcap_pmkid_base, which the
   options menu set, so backing in and out of here can never append the number twice. */

/* 2.4GHz is 1-14; 5GHz runs 36-165 (36..64, 100..144, 149..165) on dual-band boards like the
   ESP32-C5. The range is left contiguous rather than a fixed list of legal channels - Marauder
   just ignores a channel its radio/region can't use, and a digit-entry keypad makes typing 149
   as quick as 6. */
#define PMKID_CHANNEL_MIN     1
#define PMKID_CHANNEL_MAX     165
#define PMKID_CHANNEL_DEFAULT 1

static void marauder_gui_scene_pcap_pmkid_channel_input_callback(void* context, int32_t number) {
    MarauderGuiApp* app = context;
    app->pcap_pmkid_channel = (int)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void marauder_gui_scene_pcap_pmkid_channel_on_enter(void* context) {
    MarauderGuiApp* app = context;

    if(app->pcap_pmkid_channel < PMKID_CHANNEL_MIN ||
       app->pcap_pmkid_channel > PMKID_CHANNEL_MAX) {
        app->pcap_pmkid_channel = PMKID_CHANNEL_DEFAULT;
    }

    number_input_set_header_text(
        app->number_input, marauder_gui_text(app, "Kanal (2.4/5GHz)", "Channel (2.4/5GHz)"));
    number_input_set_result_callback(
        app->number_input,
        marauder_gui_scene_pcap_pmkid_channel_input_callback,
        app,
        app->pcap_pmkid_channel,
        PMKID_CHANNEL_MIN,
        PMKID_CHANNEL_MAX);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewNumberInput);
}

bool marauder_gui_scene_pcap_pmkid_channel_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        snprintf(
            app->pcap_cmd_buf,
            sizeof(app->pcap_cmd_buf),
            "%s %d",
            app->pcap_pmkid_base,
            app->pcap_pmkid_channel);
        app->pcap_sniff_cmd = app->pcap_cmd_buf;
        app->pcap_sniff_prefix = "sniffpmkid";
        app->pcap_ap_scoped = false;
        scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapSniff);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_pmkid_channel_on_exit(void* context) {
    UNUSED(context);
}
