#include "../marauder_gui_app_i.h"
#include <string.h>

/* Marauder GPS/NMEA commands. Order must match marauder_gps_menu_items below (the menu fires the
   row index as its custom event). Each row just runs the command and shows its output via the
   shared cmd_output scene - GPS features only do anything with a GPS module wired to the ESP32. */
static const char* const marauder_gps_commands[] = {
    "gpsdata", /* GPS Data (stream) */
    "gps -t", /* Tracker toggle */
    "gps -g fix", /* Fix status */
    "gps -g sat", /* Satellites */
    "gps -g accuracy", /* Accuracy - added in companion v0.7.10 */
    "gps -g text", /* Human-readable location - added in companion v0.7.10 */
    "gps -g nmea", /* One NMEA sentence - added in companion v0.7.10 */
    "nmea", /* Raw NMEA stream */
    "gpspoi -m", /* Mark point of interest */
};

static const MarauderMenuItem marauder_gps_menu_items[] = {
    {"GPS Verisi",
     "GPS Data",
     "GPS modulunun cozdugu konum/hiz/zaman verisini canli akitir.",
     "Streams the parsed location/speed/time data from the GPS module."},
    {"Takip",
     "Tracker",
     "GPS takibini ac/kapat. (Eski gpstracker komutu firmware'den kaldirildi.)",
     "Toggle GPS tracking. (The old gpstracker command was dropped from the firmware.)"},
    {"Fix Durumu",
     "Fix Status",
     "GPS'in konum kilidi (fix) durumunu gosterir.",
     "Shows the GPS position lock (fix) status."},
    {"Uydular",
     "Satellites",
     "Gorunen/kullanilan uydu sayisini gosterir.",
     "Shows the number of satellites in view/used."},
    {"Dogruluk",
     "Accuracy",
     "Mevcut konum dogrulugunu gosterir.",
     "Shows the current position accuracy."},
    {"Konum Metni",
     "Location Text",
     "Konumu okunabilir metin olarak gosterir.",
     "Shows the location as human-readable text."},
    {"NMEA Cumlesi",
     "NMEA Sentence",
     "Tek bir NMEA cumlesi gosterir.",
     "Shows a single NMEA sentence."},
    {"NMEA Akisi",
     "NMEA Stream",
     "GPS modulunun ham NMEA cumlelerini akitir.",
     "Streams the raw NMEA sentences from the GPS module."},
    {"POI Isaretle",
     "Mark POI",
     "Mevcut konumu ilgi noktasi (POI) olarak isaretler.",
     "Marks the current location as a point of interest (POI)."},
};

void marauder_gui_scene_gps_menu_on_enter(void* context) {
    MarauderGuiApp* app = context;
    marauder_gui_menu_set_items(
        app,
        marauder_gps_menu_items,
        sizeof(marauder_gps_menu_items) / sizeof(marauder_gps_menu_items[0]),
        "GPS");
}

bool marauder_gui_scene_gps_menu_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        size_t count = sizeof(marauder_gps_commands) / sizeof(marauder_gps_commands[0]);
        if(event.event < count) {
            strncpy(
                app->terminal_cmd,
                marauder_gps_commands[event.event],
                sizeof(app->terminal_cmd) - 1);
            app->terminal_cmd[sizeof(app->terminal_cmd) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneCmdOutput);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_gps_menu_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
