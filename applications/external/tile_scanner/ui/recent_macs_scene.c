#include "ui.h"

#define LOG_TAG "tile_scanner_recent_macs_scene"

#define VALID_RECENT_MAC_TIME_DIFFERENCE 60 // seconds

static void update_display(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);

    furi_string_set_str(ui->recent_macs_scene->display_text, "Recent MACs:");

    uint32_t current_timestamp = furi_hal_rtc_get_timestamp();

    for(int i = 0; i < MAX_RECENT_MACS; i++) {
        uint32_t time_diff = current_timestamp - ui->recent_macs_scene->recent_macs[i].timestamp;

        if(time_diff > VALID_RECENT_MAC_TIME_DIFFERENCE) {
            continue;
        }

        furi_string_cat_printf(
            ui->recent_macs_scene->display_text,
            "\n%02X%02X%02X%02X%02X%02X (%ld) %lds",
            ui->recent_macs_scene->recent_macs[i].mac_address[0],
            ui->recent_macs_scene->recent_macs[i].mac_address[1],
            ui->recent_macs_scene->recent_macs[i].mac_address[2],
            ui->recent_macs_scene->recent_macs[i].mac_address[3],
            ui->recent_macs_scene->recent_macs[i].mac_address[4],
            ui->recent_macs_scene->recent_macs[i].mac_address[5],
            ui->recent_macs_scene->recent_macs[i].rssi,
            time_diff);
    }

    text_box_set_text(
        ui->recent_macs_scene->recent_macs_text_box,
        furi_string_get_cstr(ui->recent_macs_scene->display_text));
    notification_message(ui->notifications, &sequence_display_backlight_on);
}

void recent_macs_handle_scanned_mac(TileScannerMacAddress* mac_address, UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(mac_address);
    furi_assert(ui);

    int first_invalid = -1;
    bool updated_existing = false;

    uint32_t current_timestamp = furi_hal_rtc_get_timestamp();

    for(int i = 0; i < MAX_RECENT_MACS; i++) {
        if(current_timestamp - ui->recent_macs_scene->recent_macs[i].timestamp >
           VALID_RECENT_MAC_TIME_DIFFERENCE) {
            if(first_invalid == -1) {
                first_invalid = i;
            }
            continue;
        }

        bool matched = true;

        for(int j = 0; j < MAC_ADDRESS_LENGTH; j++) {
            if(ui->recent_macs_scene->recent_macs[i].mac_address[j] !=
               mac_address->mac_address[j]) {
                matched = false;
                break;
            }
        }

        if(matched) {
            updated_existing = true;
            ui->recent_macs_scene->recent_macs[i].timestamp = current_timestamp;
            ui->recent_macs_scene->recent_macs[i].rssi = mac_address->rssi;
        }
    }

    if(!updated_existing) {
        if(first_invalid == -1) {
            FURI_LOG_W(LOG_TAG, "Too many recent mac address");
        } else {
            memcpy(
                ui->recent_macs_scene->recent_macs[first_invalid].mac_address,
                mac_address->mac_address,
                MAC_ADDRESS_LENGTH);
            ui->recent_macs_scene->recent_macs[first_invalid].timestamp = current_timestamp;
            ui->recent_macs_scene->recent_macs[first_invalid].rssi = mac_address->rssi;
        }
    }

    update_display(ui);
}

void recent_macs_scene_on_enter(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    update_display(ui);
    view_dispatcher_switch_to_view(ui->view_dispatcher, View_RecentMacsDisplay);
}

bool recent_macs_scene_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;

    if(event.type == SceneManagerEventTypeTick) {
        update_display(ui);
        return true;
    }

    // Default event handler
    return false;
}

void recent_macs_scene_on_exit(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    text_box_reset(ui->recent_macs_scene->recent_macs_text_box);
}

RecentMacsScene* recent_macs_scene_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    RecentMacsScene* recent_macs_scene = malloc(sizeof(RecentMacsScene));
    recent_macs_scene->display_text = furi_string_alloc();

    recent_macs_scene->recent_macs_text_box = text_box_alloc();

    for(int i = 0; i < MAX_RECENT_MACS; i++) {
        recent_macs_scene->recent_macs[i].timestamp = 0;
    }

    return recent_macs_scene;
}

void recent_macs_scene_free(RecentMacsScene* recent_macs_scene) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(recent_macs_scene);

    furi_string_free(recent_macs_scene->display_text);
    text_box_free(recent_macs_scene->recent_macs_text_box);
    free(recent_macs_scene);
}
