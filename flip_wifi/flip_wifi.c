#include "flip_wifi.h"
#include <callback/free.h>
WiFiPlaylist *wifi_playlist = NULL;
// Function to free the resources used by FlipWiFiApp
void flip_wifi_app_free(FlipWiFiApp *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWiFiApp is NULL");
        return;
    }

    // Free Submenu(s)
    if (app->submenu_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        submenu_free(app->submenu_main);
    }

    free_all(app);

    // free the view dispatcher
    if (app->view_dispatcher)
        view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    // free the app
    if (app)
        free(app);
}
char *ssid_list[64];
uint32_t ssid_index = 0;
char current_ssid[64];
char current_password[64];
bool back_from_ap = false;