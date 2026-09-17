#include "../marauder_gui_app_i.h"

/* Every entry except the last needs an AP picked first (see marauder_gui_scene_wifi_attack.c's
   marauder_wifi_ap_attack_types table for why - all of them, not just deauth/probe/beacon, it
   turns out), so those items just set which attack type to run and send the user into
   wifi_scanning.c's AP list - the menu's array index IS the wifi_ap_attack_type to set (Deauth
   is type 0 and sits first). Evil Portal/Karma doesn't fit that pattern at all (it targets
   whatever a nearby device is probing for, not a scanned AP), so it's handled as a special case
   going straight to the probe-sniff flow instead. */
static const MarauderMenuItem marauder_wifi_attack_menu_items[] = {
    {"Deauth Attack",
     "Deauth Attack",
     "Secilen agdaki TUM cihazlarin baglantisini sureklilikle keser (klasik deauth saldirisi).",
     "Continuously disconnects ALL devices on the selected network (classic deauth attack)."},
    {"Probe Flood",
     "Probe Flood",
     "Secilen aga yonelik sahte baglanti/kimlik denemesi (probe) paketleri yagdirir.",
     "Floods the selected network with fake connection/identity probe request packets."},
    {"AP Klon Spam",
     "AP Clone Spam",
     "Secilen agin adini/kimligini taklit eden sahte kopyalarini yayinlar, kafa karistirir.",
     "Broadcasts fake copies impersonating the selected network's name/identity, causing confusion."},
    {"Hedefli Deauth",
     "Targeted Deauth",
     "Bir ag secip sonra o aga bagli belirli bir cihazi secersin - SADECE onun baglantisini keser.",
     "Pick a network, then a specific device connected to it - disconnects ONLY that device."},
    {"SAE Commit Flood",
     "SAE Commit Flood",
     "WPA3 aglarin SAE el sikismasini sahte isteklerle bogup yavaslatir/coker.",
     "Floods a WPA3 network's SAE handshake with fake requests, slowing or crashing it."},
    {"Kanal Degistirme (CSA)",
     "Channel Switch (CSA)",
     "Cihazlara sahte 'kanal degistiriyorum' sinyali gonderip yanlis kanala yonlendirir, baglanti kopar.",
     "Sends devices a fake 'switching channel' signal, redirecting them to the wrong channel and dropping their connection."},
    {"Quiet Time",
     "Quiet Time",
     "Sahte 'bu kanalda sessiz kal' sinyali yayinlayip cihazlarin veri gondermesini engeller.",
     "Broadcasts a fake 'stay quiet on this channel' signal, preventing devices from sending data."},
    {"Bad Msg",
     "Bad Msg",
     "Bozuk/hatali yonetim paketleri gonderip bazi cihaz surucularinde cokme/kopma tetikler.",
     "Sends malformed management packets, triggering crashes/drops in some device drivers."},
    {"Assoc Sleep",
     "Assoc Sleep",
     "Cihazlarin uyku moduna gectigine dair sahte sinyal gonderip trafiklerini askida birakir.",
     "Sends a fake 'going to sleep' signal, leaving devices' traffic suspended."},
    {"Evil Portal (Karma)",
     "Evil Portal (Karma)",
     "Cihazlarin aradigi ag isimlerini dinler, birini secip o isimde sahte bir ag (Evil Portal) kurar.",
     "Listens for network names devices are searching for; pick one to set up a fake network (Evil Portal) with that name."},
};

#define WIFI_ATTACK_MENU_EVIL_PORTAL_INDEX \
    (sizeof(marauder_wifi_attack_menu_items) / sizeof(marauder_wifi_attack_menu_items[0]) - 1)

void marauder_gui_scene_wifi_attack_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_wifi_attack_menu_items,
        sizeof(marauder_wifi_attack_menu_items) / sizeof(marauder_wifi_attack_menu_items[0]),
        marauder_gui_text(app, "Saldirilar", "Attacks"));
}

bool marauder_gui_scene_wifi_attack_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event < sizeof(marauder_wifi_attack_menu_items) / sizeof(marauder_wifi_attack_menu_items[0])) {
        if(event.event == WIFI_ATTACK_MENU_EVIL_PORTAL_INDEX) {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiProbeSniff);
        } else {
            app->wifi_ap_attack_type = (int)event.event;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiScanning);
        }
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_attack_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
