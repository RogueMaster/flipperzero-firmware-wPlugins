#include "ui.h"

#define LOG_TAG "tile_scanner_scan_scene"

static void toggle_scanning_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    UNUSED(result);
    
    if(type != InputTypePress){
        return;
    }
    
    UI* ui = context;
    
    if(ui->is_scanning){
        tile_scanner_stop_scanning(ui->tile_scanner);
    }else{
        tile_scanner_start_scanning(ui->tile_scanner);
    }
}

static void ignored_macs_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    UNUSED(result);
    UNUSED(type);
    
    UI* ui = context;
    scene_manager_next_scene(ui->scene_manager, IgnoredMacsDisplay);
}

static void recent_macs_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    UNUSED(result);
    UNUSED(type);
    
    UI* ui = context;
    scene_manager_next_scene(ui->scene_manager, RecentMacsDisplay);
}

void scan_scene_update(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    
    char text[256];
    snprintf(text, sizeof(text), "Ignoring %d MACs\nLeft Arrow = Ignored MACs\nRight Arrow = Recent MACs", ui->known_mac_index);
    
    widget_reset(ui->scan_scene->scan_widget);
    widget_add_string_element(ui->scan_scene->scan_widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Tile Scanner v" FAP_VERSION);
    widget_add_string_multiline_element(ui->scan_scene->scan_widget, 0, 15, AlignLeft, AlignTop, FontSecondary, text);
    
    widget_add_button_element(ui->scan_scene->scan_widget, GuiButtonTypeCenter, ui->is_scanning ? "Stop Scanning" : "Start Scanning", toggle_scanning_callback, ui);
    widget_add_button_element(ui->scan_scene->scan_widget, GuiButtonTypeLeft, "", ignored_macs_callback, ui);
    widget_add_button_element(ui->scan_scene->scan_widget, GuiButtonTypeRight, "", recent_macs_callback, ui);
}

void scan_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    UI* ui = context;
    scan_scene_update(ui);
    view_dispatcher_switch_to_view(ui->view_dispatcher, View_ScanDisplay);
}

bool scan_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    if (event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    // Default event handler
    return false;
}

void scan_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    UI* ui = context;
    widget_reset(ui->scan_scene->scan_widget);
}

ScanScene* scan_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);
    
    ScanScene* scan_scene = malloc(sizeof(ScanScene));
    scan_scene->scan_widget = widget_alloc();
    
    return scan_scene;
}

void scan_scene_free(ScanScene* scan_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(scan_scene);
    
    widget_free(scan_scene->scan_widget);
    free(scan_scene);
}
