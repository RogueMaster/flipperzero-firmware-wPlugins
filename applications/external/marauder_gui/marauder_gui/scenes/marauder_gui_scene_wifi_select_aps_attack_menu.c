#include "../marauder_gui_app_i.h"

/* Reached from wifi_select_aps.c's Right key once at least one AP is checked there. Deliberately
   a SUBSET of wifi_attack_menu.c's list: "Hedefli Deauth" (deauth -c) needs one specific station
   picked on one specific AP, which doesn't make sense against a whole checked-off list, and Evil
   Portal targets whatever a nearby device is probing for rather than any scanned AP at all - both
   are left out here. Every item that remains genuinely applies to every checked AP at once: see
   marauder_gui_scene_wifi_attack.c's on_enter for how wifi_attack_multi_ap skips the normal
   single-AP select/channel prep, since selection is already set from the previous screen. */
static const MarauderMenuItem marauder_wifi_select_aps_attack_menu_items[] = {
    {"Deauth Attack",
     "Deauth Attack",
     "Secili TUM aglardaki cihazlarin baglantisini sureklilikle keser.",
     "Continuously disconnects devices on ALL selected networks."},
    {"Probe Flood",
     "Probe Flood",
     "Secili tum aglara yonelik sahte baglanti/kimlik denemesi (probe) paketleri yagdirir.",
     "Floods all selected networks with fake connection/identity probe request packets."},
    {"AP Klon Spam",
     "AP Clone Spam",
     "Secili tum aglarin adini/kimligini taklit eden sahte kopyalarini yayinlar.",
     "Broadcasts fake copies impersonating each selected network's name/identity."},
    {"SAE Commit Flood",
     "SAE Commit Flood",
     "Secili WPA3 aglarin SAE el sikismasini sahte isteklerle bogup yavaslatir/coker.",
     "Floods each selected WPA3 network's SAE handshake with fake requests, slowing or crashing it."},
    {"Kanal Degistirme (CSA)",
     "Channel Switch (CSA)",
     "Secili aglara bagli cihazlara sahte 'kanal degistiriyorum' sinyali gonderir, baglanti kopar.",
     "Sends devices on selected networks a fake 'switching channel' signal, dropping their connection."},
    {"Quiet Time",
     "Quiet Time",
     "Secili aglara sahte 'bu kanalda sessiz kal' sinyali yayinlayip veri gondermeyi engeller.",
     "Broadcasts a fake 'stay quiet on this channel' signal on selected networks, blocking data."},
    {"Bad Msg",
     "Bad Msg",
     "Secili tum aglara bozuk/hatali yonetim paketleri gonderip cihazlarda cokme/kopma tetikler.",
     "Sends malformed management packets to all selected networks, triggering device crashes/drops."},
    {"Assoc Sleep",
     "Assoc Sleep",
     "Secili aglardaki cihazlara sahte uyku sinyali gonderip trafiklerini askida birakir.",
     "Sends devices on selected networks a fake 'going to sleep' signal, suspending their traffic."},
};

/* Index into this smaller list -> the wifi_ap_attack_type value wifi_attack.c actually expects
   (its own marauder_wifi_ap_attack_types table, which still has the excluded "targeted deauth"
   sitting at index 3). */
static const int marauder_wifi_select_aps_attack_type_map[] = {0, 1, 2, 4, 5, 6, 7, 8};

#define WIFI_SELECT_APS_ATTACK_MENU_COUNT \
    (sizeof(marauder_wifi_select_aps_attack_menu_items) / \
     sizeof(marauder_wifi_select_aps_attack_menu_items[0]))

void marauder_gui_scene_wifi_select_aps_attack_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_wifi_select_aps_attack_menu_items,
        WIFI_SELECT_APS_ATTACK_MENU_COUNT,
        marauder_gui_text(app, "Tum Secililere", "To All Selected"));
}

bool marauder_gui_scene_wifi_select_aps_attack_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < WIFI_SELECT_APS_ATTACK_MENU_COUNT) {
        app->wifi_ap_attack_type = marauder_wifi_select_aps_attack_type_map[event.event];
        app->wifi_attack_multi_ap = true;
        scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiAttack);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_select_aps_attack_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
