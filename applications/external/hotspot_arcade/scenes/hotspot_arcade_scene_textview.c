#include "../hotspot_arcade_i.h"

static void ha_textview_render(HotspotArcadeApp* app) {
    widget_reset(app->widget);
    const char* text = furi_string_empty(app->console) ?
                           "No events yet.\nStart a session and\nlet players join." :
                           furi_string_get_cstr(app->console);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
}

void hotspot_arcade_scene_textview_on_enter(void* context) {
    HotspotArcadeApp* app = context;
    ha_textview_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HaViewWidget);
}

bool hotspot_arcade_scene_textview_on_event(void* context, SceneManagerEvent event) {
    HotspotArcadeApp* app = context;
    // Re-render on new events so the console stays live (resets scroll to top).
    if(event.type == SceneManagerEventTypeCustom && event.event == HaEventRefreshView) {
        ha_textview_render(app);
        return true;
    }
    return false;
}

void hotspot_arcade_scene_textview_on_exit(void* context) {
    HotspotArcadeApp* app = context;
    widget_reset(app->widget);
}
