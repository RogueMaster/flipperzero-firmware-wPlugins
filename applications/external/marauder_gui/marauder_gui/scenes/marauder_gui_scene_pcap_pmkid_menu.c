#include "../marauder_gui_app_i.h"
#include <string.h>

/* PMKID capture modes, matching the WiFi Marauder companion's "Sniff PMKID" options menu.
   Marauder's syntax is "sniffpmkid [-c <channel>][-d][-l]":
     -d  also deauth, to force a handshake instead of waiting for one ("active")
     -l  only run against Marauder's selected-AP list ("targeted")
     -c  lock to one channel
   So each mode needs different setup: passive/active start immediately, the targeted modes scan
   and let you pick an AP first (we then "select -a" it), and the on-channel modes just ask for a
   channel number. Order must match marauder_pcap_pmkid_menu_items below. */
enum {
    PmkidMenuIndexPassive,
    PmkidMenuIndexActive,
    PmkidMenuIndexTargetedPassive,
    PmkidMenuIndexTargetedActive,
    PmkidMenuIndexChannelPassive,
    PmkidMenuIndexChannelActive,
};

static const MarauderMenuItem marauder_pcap_pmkid_menu_items[] = {
    {"Pasif",
     "Passive",
     "Sadece dinler, hicbir sey sormaz. El sikismasi kendiliginden olusana kadar bekler.",
     "Just listens, asks nothing. Waits for a handshake to happen on its own."},
    {"Aktif (Deauth)",
     "Active (Force Deauth)",
     "Deauth gonderip el sikismasini zorlar, sonra yakalar. Hedef secmez, genel yayin.",
     "Sends deauth to force a handshake, then captures it. No target picked, broadcast."},
    {"Hedefli Pasif",
     "Targeted Passive",
     "Once AP taratip birini secersin; sadece o AP dinlenir (deauth yok).",
     "Scans and lets you pick an AP first; only that AP is listened to (no deauth)."},
    {"Hedefli Aktif",
     "Targeted Active",
     "Once AP secersin, sonra o AP'ye deauth atip el sikismasini yakalar. En etkili mod.",
     "Pick an AP first, then deauths it and captures the handshake. The most effective mode."},
    {"Kanalda - Pasif",
     "On Channel - Passive",
     "Bir kanal numarasi girersin, sadece o kanalda pasif dinler.",
     "You enter a channel number, then passively listens on just that channel."},
    {"Kanalda - Aktif",
     "On Channel - Active",
     "Bir kanal numarasi girersin, o kanalda deauth atip el sikismasini yakalar.",
     "You enter a channel number, then deauths and captures the handshake on that channel."},
};

void marauder_gui_scene_pcap_pmkid_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_pcap_pmkid_menu_items,
        sizeof(marauder_pcap_pmkid_menu_items) / sizeof(marauder_pcap_pmkid_menu_items[0]),
        "PMKID");
}

bool marauder_gui_scene_pcap_pmkid_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type != SceneManagerEventTypeCustom) return false;

    app->pcap_sniff_prefix = "sniffpmkid";
    app->pcap_ap_scoped = false;

    switch(event.event) {
    case PmkidMenuIndexPassive:
        app->pcap_sniff_cmd = "sniffpmkid";
        app->pcap_sniff_label = "PMKID";
        scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapSniff);
        consumed = true;
        break;

    case PmkidMenuIndexActive:
        app->pcap_sniff_cmd = "sniffpmkid -d";
        app->pcap_sniff_label = marauder_gui_text(app, "PMKID Aktif", "PMKID Active");
        scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapSniff);
        consumed = true;
        break;

    case PmkidMenuIndexTargetedPassive:
        app->pcap_sniff_cmd = "sniffpmkid -l";
        app->pcap_sniff_label = marauder_gui_text(app, "PMKID Hedefli", "PMKID Targeted");
        app->pcap_ap_scoped = true;
        /* Scan first; wifi_scanning routes attack-type 20 to the capture scene once an AP is
           picked, and pcap_sniff.c "select -a"s it so "-l" has a target. */
        app->wifi_ap_attack_type = 20;
        scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
        consumed = true;
        break;

    case PmkidMenuIndexTargetedActive:
        app->pcap_sniff_cmd = "sniffpmkid -d -l";
        app->pcap_sniff_label = marauder_gui_text(app, "PMKID Hedefli+", "PMKID Targeted+");
        app->pcap_ap_scoped = true;
        app->wifi_ap_attack_type = 20;
        scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
        consumed = true;
        break;

    case PmkidMenuIndexChannelPassive:
        /* Stash the command up to "-c"; the channel picker appends the number. */
        app->pcap_pmkid_base = "sniffpmkid -c";
        app->pcap_sniff_label = marauder_gui_text(app, "PMKID Kanal", "PMKID Channel");
        scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapPmkidChannel);
        consumed = true;
        break;

    case PmkidMenuIndexChannelActive:
        app->pcap_pmkid_base = "sniffpmkid -d -c";
        app->pcap_sniff_label = marauder_gui_text(app, "PMKID Kanal+", "PMKID Channel+");
        scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapPmkidChannel);
        consumed = true;
        break;

    default:
        break;
    }

    return consumed;
}

void marauder_gui_scene_pcap_pmkid_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
