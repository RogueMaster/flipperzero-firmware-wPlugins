#include "flippass_editor_icons.h"
#include "../scenes/flippass_db_browser_view.h"
#include "../scenes/flippass_scene.h"
#include <stdio.h>

enum {
    FlipPassEditorEventIconSelect = 0x400U,
    FlipPassEditorEventIconCancel = 0x4FFU,
    FlipPassEditorIconPage0 = 0x20U,
    FlipPassEditorIconPage1,
    FlipPassEditorIconPage2,
    FlipPassEditorDialogNone = 0U,
};

#define FLIPPASS_EDITOR_ICONS_PER_PAGE 23U
_Static_assert(
    KDBX_ICON_ID_COUNT == 3U * FLIPPASS_EDITOR_ICONS_PER_PAGE,
    "Update the three editor icon pages when the catalog changes");

static bool flippass_editor_icon_picker_active(uint32_t state) {
    return state >= FlipPassEditorIconPage0 && state <= FlipPassEditorIconPage2;
}

static void flippass_editor_icon_picker_callback(FlipPassDbBrowserEvent event, void* context) {
    App* app = context;
    if(event == FlipPassDbBrowserEventEnter) {
        const uint32_t state =
            scene_manager_get_scene_state(app->scene_manager, FlipPassScene_Editor);
        if(!flippass_editor_icon_picker_active(state)) return;
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            FlipPassEditorEventIconSelect + (state - FlipPassEditorIconPage0) * 64U +
                flippass_db_browser_view_get_selected_item(app->db_browser));
    } else if(event == FlipPassDbBrowserEventBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassEditorEventIconCancel);
    }
}

static void flippass_editor_show_icon_page(App* app, uint8_t icon_id) {
    const uint8_t page = icon_id / FLIPPASS_EDITOR_ICONS_PER_PAGE;
    const uint8_t first = page * FLIPPASS_EDITOR_ICONS_PER_PAGE;
    char text[24];
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_Editor, FlipPassEditorIconPage0 + page);
    flippass_db_browser_view_reset(app->db_browser);
    flippass_db_browser_view_set_has_parent(app->db_browser, true);
    flippass_db_browser_view_set_callback(
        app->db_browser, flippass_editor_icon_picker_callback, app);
    flippass_db_browser_view_set_back_filter(app->db_browser, NULL);
    snprintf(
        text,
        sizeof(text),
        "Icons %u-%u",
        (unsigned int)first,
        (unsigned int)(first + FLIPPASS_EDITOR_ICONS_PER_PAGE - 1U));
    flippass_db_browser_view_set_header(app->db_browser, text);
    if(page != 0U) {
        flippass_db_browser_view_add_item(
            app->db_browser,
            FlipPassDbBrowserItemTypeUp,
            "Previous page",
            FLIPPASS_DB_BROWSER_ICON_NONE);
    }
    for(uint8_t id = first; id < first + FLIPPASS_EDITOR_ICONS_PER_PAGE; ++id) {
        snprintf(text, sizeof(text), "Icon %u", (unsigned int)id);
        flippass_db_browser_view_add_item(
            app->db_browser, FlipPassDbBrowserItemTypeGroup, text, id);
    }
    if(page != 2U) {
        flippass_db_browser_view_add_item(
            app->db_browser,
            FlipPassDbBrowserItemTypeGroup,
            "Next page",
            FLIPPASS_DB_BROWSER_ICON_NONE);
    }
    flippass_db_browser_view_set_selected_item(app->db_browser, icon_id - first + (page != 0U));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDbBrowser);
}

void flippass_editor_icons_open(App* app) {
    flippass_ui_ensure_db_browser(app);
    flippass_editor_show_icon_page(
        app, app->editor_icon_id < KDBX_ICON_ID_COUNT ? app->editor_icon_id : 0U);
}

void flippass_editor_icons_reset(App* app) {
    flippass_db_browser_view_set_callback(app->db_browser, NULL, NULL);
    flippass_db_browser_view_set_back_filter(app->db_browser, NULL);
    flippass_db_browser_view_reset(app->db_browser);
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_Editor, FlipPassEditorDialogNone);
}

static bool flippass_editor_handle_icon_selection(App* app, uint32_t index, uint32_t state) {
    const uint8_t page = state - FlipPassEditorIconPage0;
    if(page != 0U && index == 0U) {
        flippass_editor_show_icon_page(app, page * FLIPPASS_EDITOR_ICONS_PER_PAGE - 1U);
        return false;
    }
    if(page != 0U) --index;
    if(index == FLIPPASS_EDITOR_ICONS_PER_PAGE && page != 2U) {
        flippass_editor_show_icon_page(app, (page + 1U) * FLIPPASS_EDITOR_ICONS_PER_PAGE);
    } else if(index < FLIPPASS_EDITOR_ICONS_PER_PAGE) {
        app->editor_icon_id = page * FLIPPASS_EDITOR_ICONS_PER_PAGE + index;
        flippass_editor_icons_reset(app);
        return true;
    }
    return false;
}

FlipPassEditorIconsEventResult flippass_editor_icons_on_event(App* app, SceneManagerEvent event) {
    const uint32_t state = scene_manager_get_scene_state(app->scene_manager, FlipPassScene_Editor);
    if(!flippass_editor_icon_picker_active(state)) return FlipPassEditorIconsUnhandled;
    if(event.type == SceneManagerEventTypeBack || (event.type == SceneManagerEventTypeCustom &&
                                                   event.event == FlipPassEditorEventIconCancel)) {
        flippass_editor_icons_reset(app);
        return FlipPassEditorIconsClosed;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event >= FlipPassEditorEventIconSelect &&
       event.event < FlipPassEditorEventIconSelect + 3U * 64U) {
        const uint32_t selected = event.event - FlipPassEditorEventIconSelect;
        if(selected / 64U == state - FlipPassEditorIconPage0 &&
           flippass_editor_handle_icon_selection(app, selected % 64U, state)) {
            return FlipPassEditorIconsClosed;
        }
    }
    return FlipPassEditorIconsHandled;
}
