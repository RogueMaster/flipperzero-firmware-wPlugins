
#include "ui.h"

#include <furi_hal.h>

#define LOG_TAG "tile_scanner_ui"

#define KNOWN_MACS_FOLDER "/ext/apps_data/tile_scanner/"
#define KNOWN_MACS_FILE   "/ext/apps_data/tile_scanner/known_macs.txt"

const NotificationSequence FOUND_TILE_NOTIFICATION_SEQUENCE = {
    &message_display_backlight_on, // Turn backlight on
    &message_force_speaker_volume_setting_1f, // Turn on sound
    
    &message_vibro_on,
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_50,
    
    // Clean up
    &message_sound_off,
    &message_vibro_off,
    NULL,
};

/// collection of all scene on_enter handlers - in the same order as their enum
void (*const scene_on_enter_handlers[])(void*) = {
    scan_scene_on_enter,
    ignored_macs_scene_on_enter,
    recent_macs_scene_on_enter,
};

/// collection of all scene on event handlers - in the same order as their enum
bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    scan_scene_on_event,
    ignored_macs_scene_on_event,
    recent_macs_scene_on_event,
};

/// collection of all scene on exit handlers - in the same order as their enum
void (*const scene_on_exit_handlers[])(void*) = {
    scan_scene_on_exit,
    ignored_macs_scene_on_exit,
    recent_macs_scene_on_exit,
};

static const SceneManagerHandlers scene_event_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = ui_count
};

static bool scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    UI* ui = context;
    return scene_manager_handle_back_event(ui->scene_manager);
}

static bool scene_manager_custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    UI* ui = context;
    return scene_manager_handle_custom_event(ui->scene_manager, event);
}

static void tile_scanner_callback(TileScannerEvent event, void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    UI* ui = context;
    
    if(event.type == TileScannerEventHeartbeat) {
        TileScannerHeartbeat* heartbeat = event.data;
        bool changed = ui->is_scanning ^ heartbeat->scanning;
        ui->is_scanning = heartbeat->scanning;
        
        if(changed){
            scan_scene_update(ui);
        }
    }else if(event.type == TileScannerEventMacAddress) {
        TileScannerMacAddress* mac_address = event.data;
        recent_macs_handle_scanned_mac(mac_address, ui);
        
        for(int i = 0; i < ui->known_mac_index; i++){
            bool matched = true;
            
            for(int j = 0; j < MAC_ADDRESS_LENGTH; j++){
                matched &= mac_address->mac_address[j] == ui->known_macs[i].mac_address[j];
            }
            
            if(matched){
                return;
            }
        }
        
        notification_message(ui->notifications, &FOUND_TILE_NOTIFICATION_SEQUENCE);
    }
}

static void view_dispatcher_tick_callback(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);
    
    UI* ui = context;
    scene_manager_handle_tick_event(ui->scene_manager);
}

UI* ui_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);
    
    UI* ui = malloc(sizeof(UI));
    ui->tile_scanner = tile_scanner_alloc();
    tile_scanner_set_callback(ui->tile_scanner, tile_scanner_callback, ui);
    
    ui->scan_scene = scan_scene_alloc();
    ui->ignored_macs_scene = ignored_macs_scene_alloc();
    ui->recent_macs_scene = recent_macs_scene_alloc();
    
    ui->scene_manager = scene_manager_alloc(&scene_event_handlers, ui);
    ui->view_dispatcher = view_dispatcher_alloc();
    ui->notifications = furi_record_open(RECORD_NOTIFICATION);
    ui->gui = furi_record_open(RECORD_GUI);
    
    FURI_LOG_D(LOG_TAG, "Setting Up View Dispatcher");
    view_dispatcher_set_event_callback_context(ui->view_dispatcher, ui);
    view_dispatcher_set_navigation_event_callback(ui->view_dispatcher, scene_manager_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(ui->view_dispatcher, scene_manager_custom_event_callback);
    
    FURI_LOG_D(LOG_TAG, "Adding Scenes");
    view_dispatcher_add_view(ui->view_dispatcher, View_ScanDisplay, widget_get_view(ui->scan_scene->scan_widget));
    view_dispatcher_add_view(ui->view_dispatcher, View_IgnoredMacsDisplay, text_box_get_view(ui->ignored_macs_scene->ignored_macs_text_box));
    view_dispatcher_add_view(ui->view_dispatcher, View_RecentMacsDisplay, text_box_get_view(ui->recent_macs_scene->recent_macs_text_box));
    
    view_dispatcher_set_event_callback_context(ui->view_dispatcher, ui);
    view_dispatcher_set_tick_event_callback(
        ui->view_dispatcher,
        view_dispatcher_tick_callback,
        furi_ms_to_ticks(1000)
    );
    
    FURI_LOG_D(LOG_TAG, "Loading Known MACs file from %s", KNOWN_MACS_FILE);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    
    if(!storage_dir_exists(storage, KNOWN_MACS_FOLDER)){
        FURI_LOG_D(LOG_TAG, "Creating folder %s", KNOWN_MACS_FOLDER);
        if(!storage_simply_mkdir(storage, KNOWN_MACS_FOLDER)) {
            FURI_LOG_E(LOG_TAG, "Failed to create %s", KNOWN_MACS_FOLDER);
        }
    }
    
    File* file = storage_file_alloc(storage);
    
    if(storage_file_open(file, KNOWN_MACS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)){
        uint8_t index = 0;
        uint8_t data;
        size_t amount_read;
        uint8_t mac_from_file[3 * MAC_ADDRESS_LENGTH] = {0};
        
        while(true){
            if(ui->known_mac_index >= MAX_KNOWN_MACS) {
                FURI_LOG_W(LOG_TAG, "Too many MAC addresses ignored");
                break;
            }
            
            index = 0;
            
            while((amount_read = storage_file_read(file, &data, 1)) == 1 && data != '\n'){
                if(index < 3 * MAC_ADDRESS_LENGTH - 1) {
                    mac_from_file[index] = data;
                    index++;
                }
            }
            
            if(amount_read == 0 && index == 0){
                // End of file
                break;
            }
            
            FURI_LOG_D(LOG_TAG, "Read: %s", mac_from_file);
            
            if(index < 3 * MAC_ADDRESS_LENGTH - 1) {
                continue;
            }
            
            // Check that the spacers are correct
            if(
                mac_from_file[2] == ':' &&
                mac_from_file[5] == ':' &&
                mac_from_file[8] == ':' &&
                mac_from_file[11] == ':' &&
                mac_from_file[14] == ':'
            ) {
                bool parsed = true;
                
                for(int i = 0; i < MAC_ADDRESS_LENGTH; i++){
                    int index = i * 3;
                    ui->known_macs[ui->known_mac_index].mac_address[i] = 0;
                    
                    if('0' <= mac_from_file[index] && mac_from_file[index] <= '9') {
                        ui->known_macs[ui->known_mac_index].mac_address[i] = mac_from_file[index] - '0';
                    }else if('a' <= mac_from_file[index] && mac_from_file[index] <= 'f') {
                        ui->known_macs[ui->known_mac_index].mac_address[i] = mac_from_file[index] - 'a' + 10;
                    }else if('A' <= mac_from_file[index] && mac_from_file[index] <= 'F') {
                        ui->known_macs[ui->known_mac_index].mac_address[i] = mac_from_file[index] - 'A' + 10;
                    }else{
                        parsed = false;
                        break;
                    }
                    
                    ui->known_macs[ui->known_mac_index].mac_address[i] = ui->known_macs[ui->known_mac_index].mac_address[i] << 4;
                    index++;
                    
                    if('0' <= mac_from_file[index] && mac_from_file[index] <= '9') {
                        ui->known_macs[ui->known_mac_index].mac_address[i] |= mac_from_file[index] - '0';
                    }else if('a' <= mac_from_file[index] && mac_from_file[index] <= 'f') {
                        ui->known_macs[ui->known_mac_index].mac_address[i] |= mac_from_file[index] - 'a' + 10;
                    }else if('A' <= mac_from_file[index] && mac_from_file[index] <= 'F') {
                        ui->known_macs[ui->known_mac_index].mac_address[i] |= mac_from_file[index] - 'A' + 10;
                    }else{
                        parsed = false;
                        break;
                    }
                }
                
                if(parsed) {
                    FURI_LOG_D(
                        LOG_TAG,
                        "Parsed MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
                        ui->known_macs[ui->known_mac_index].mac_address[0],
                        ui->known_macs[ui->known_mac_index].mac_address[1],
                        ui->known_macs[ui->known_mac_index].mac_address[2],
                        ui->known_macs[ui->known_mac_index].mac_address[3],
                        ui->known_macs[ui->known_mac_index].mac_address[4],
                        ui->known_macs[ui->known_mac_index].mac_address[5]
                    );
                    
                    ui->known_mac_index++;
                }else{
                    FURI_LOG_D(LOG_TAG, "Addres was not hex");
                }
            }else{
                FURI_LOG_D(LOG_TAG, "Not the correct spacers");
            }
        }
        
        FURI_LOG_D(LOG_TAG, "Loaded %d MACs", ui->known_mac_index);
    }else{
        FURI_LOG_W(LOG_TAG, "Known MACs file does not exist");
    }
    
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    return ui;
}

void ui_start(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);
    
    view_dispatcher_attach_to_gui(ui->view_dispatcher, ui->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(ui->scene_manager, ScanDisplay);
    
    FURI_LOG_D(LOG_TAG, "Starting dispatcher");
    view_dispatcher_run(ui->view_dispatcher);
}

void ui_free(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);
    
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    
    view_dispatcher_remove_view(ui->view_dispatcher, View_ScanDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_IgnoredMacsDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_RecentMacsDisplay);
    view_dispatcher_free(ui->view_dispatcher);
    
    scene_manager_free(ui->scene_manager);
    scan_scene_free(ui->scan_scene);
    ignored_macs_scene_free(ui->ignored_macs_scene);
    recent_macs_scene_free(ui->recent_macs_scene);
    
    tile_scanner_free(ui->tile_scanner);
    free(ui);
}
