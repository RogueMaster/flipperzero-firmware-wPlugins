#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include <furi.h>
#include <bit_lib.h>
#include <lib/nfc/protocols/nfc_protocol.h>
#include "../api/metroflip/metroflip_api.h"
#include "../api/suica/suica_loading.h"
#define TAG "Metroflip:Scene:Load"
#include "keys.h"
#include <nfc/protocols/mf_classic/mf_classic.h>

void metroflip_scene_load_on_enter(void* context) {
    Metroflip* app = (Metroflip*)context;

    // Initial state
    app->data_loaded = false;
    app->card_type = "unknown"; // Default card type

    // Buffers for reading data
    FuriString* card_type_str = furi_string_alloc();
    FuriString* device_type = furi_string_alloc();

    // File browser setup
    DialogsFileBrowserOptions browser_options;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    dialog_file_browser_set_basic_options(&browser_options, METROFLIP_FILE_EXTENSION, &I_icon);
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;
    FuriString* file_path = furi_string_alloc_set(browser_options.base_path);

    if(dialog_file_browser_show(app->dialogs, file_path, file_path, &browser_options)) {
        FlipperFormat* format = flipper_format_file_alloc(storage);

        do {
            if(!flipper_format_file_open_existing(format, furi_string_get_cstr(file_path))) break;

            bool read_device_type = false;
            do {
                read_device_type = flipper_format_read_string(format, "Device Type", device_type);
                if(read_device_type) break;

                flipper_format_file_close(format);
                flipper_format_file_open_existing(format, furi_string_get_cstr(file_path));
                // Reopen file because Flipper Format likes to clip stream if key was invalid first try
                read_device_type = flipper_format_read_string(format, "Device type", device_type);
            } while(0);

            if(!read_device_type) {
                // Try to assume it's a Mifare Classic card and proceed
                furi_string_set_str(device_type, "Mifare Classic");
            }

            const char* protocol_name = furi_string_get_cstr(device_type);

            if(!flipper_format_read_string(format, "Card Type", card_type_str)) {
                // Try to detect card type dynamically
                flipper_format_file_close(format);
                flipper_format_file_open_existing(format, furi_string_get_cstr(file_path));

                if(strcmp(protocol_name, "Mifare Classic") == 0) {
                    MfClassicData* mfc_data = mf_classic_alloc();
                    if(!mf_classic_load(mfc_data, format, 2)) {
                        mf_classic_free(mfc_data);
                        break;
                    }

                    CardType card_type = determine_card_type(app->nfc, mfc_data, true);
                    app->mfc_card_type = card_type;
                    app->data_loaded = true;
                    app->is_desfire = false;

                    switch(card_type) {
                    case CARD_TYPE_METROMONEY:
                        app->card_type = "metromoney";
                        FURI_LOG_I(TAG, "Detected: Metromoney");
                        break;
                    case CARD_TYPE_CHARLIECARD:
                        app->card_type = "charliecard";
                        break;
                    case CARD_TYPE_SMARTRIDER:
                        app->card_type = "smartrider";
                        break;
                    case CARD_TYPE_TROIKA:
                        app->card_type = "troika";
                        break;
                    case CARD_TYPE_RENFE_SUM10:
                        app->card_type = "renfe_sum10";
                        break;
                    case CARD_TYPE_RENFE_REGULAR:
                        app->card_type = "renfe_regular";
                        FURI_LOG_I(TAG, "Detected: RENFE Regular");
                        break;
                    case CARD_TYPE_GOCARD:
                        app->card_type = "gocard";
                        break;
                    case CARD_TYPE_TWO_CITIES:
                        app->card_type = "two_cities";
                        break;
                    case CARD_TYPE_UNKNOWN:
                    default:
                        app->card_type = "unknown";
                        break;
                    }

                } else if(strcmp(protocol_name, "Mifare DESFire") == 0) {
                    MfDesfireData* data = mf_desfire_alloc();
                    if(!mf_desfire_load(data, format, 2)) {
                        mf_desfire_free(data);
                        break;
                    }

                    app->is_desfire = true;
                    app->data_loaded = true;
                    app->card_type =
                        desfire_type(data); // This must return a static literal string

                    mf_desfire_free(data);
                } else if(strcmp(protocol_name, "FeliCa") == 0) {
                    do {
                        uint32_t data_format_version = 0;
                        bool read_success = flipper_format_read_uint32(
                            format, "Data format version", &data_format_version, 1);
                        if(!read_success || data_format_version != 2) break;
                        // data format version 2 => post API 87.0 i.e. OFW #4254
                        // If we didn't find a format version, it should be saved by previous Metroflip version
                        // So we let it fall through to the Japan Transit IC logic below
                        app->card_type = "suica";
                        app->is_desfire = false;
                        app->data_loaded = true;
                        load_suica_data(app, format, false);
                        FURI_LOG_I(TAG, "Detected: FeliCa (API 87.0+)");
                    } while(false);
                } else if(strcmp(protocol_name, "ST25TB") == 0) {
                    app->card_type = "intertic";
                    app->is_desfire = false;
                    app->data_loaded = true;
                }
            } else {
                const char* card_str = furi_string_get_cstr(card_type_str);

                if(strcmp(card_str, "Japan Transit IC") == 0) {
                    FURI_LOG_I(TAG, "Detected: Japan Transit IC");
                    app->card_type = "suica";
                    app->is_desfire = false;
                    app->data_loaded = true;
                    load_suica_data(app, format, true);
                    // This format is deprecated after OFW #4254 but kept for backward compatibility
                } else if(strcmp(card_str, "calypso") == 0) {
                    app->card_type = "calypso";
                    app->is_desfire = false;
                    app->data_loaded = true;
                } else if(strcmp(card_str, "renfe_sum10") == 0) {
                    // For RENFE cards, we need to load the MFC data and set it up properly
                    flipper_format_file_close(format);
                    flipper_format_file_open_existing(format, furi_string_get_cstr(file_path));

                    MfClassicData* mfc_data = mf_classic_alloc();
                    if(!mf_classic_load(mfc_data, format, 2)) {
                        mf_classic_free(mfc_data);
                        break;
                    }

                    app->card_type = "renfe_sum10";
                    app->mfc_card_type = CARD_TYPE_RENFE_SUM10;
                    app->data_loaded = true;
                    app->is_desfire = false;
                } else {
                    app->card_type = "unknown";
                    app->data_loaded = false;
                }
            }

            // Set file path
            strncpy(app->file_path, furi_string_get_cstr(file_path), sizeof(app->file_path) - 1);
            app->file_path[sizeof(app->file_path) - 1] = '\0';
            strncpy(
                app->delete_file_path,
                furi_string_get_cstr(file_path),
                sizeof(app->delete_file_path) - 1);
            app->delete_file_path[sizeof(app->delete_file_path) - 1] = '\0';

        } while(0);

        flipper_format_free(format);
    }

    // Scene transitions
    if(app->data_loaded) {
        FURI_LOG_I(TAG, "Data loaded successfully, transitioning to parse scene");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
    } else {
        FURI_LOG_I(TAG, "Data loading failed, returning to start");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
    }

    // Cleanup
    furi_string_free(file_path);
    furi_string_free(card_type_str);
    furi_string_free(device_type);
    furi_record_close(RECORD_STORAGE);
}

bool metroflip_scene_load_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    UNUSED(event);
    bool consumed = false;
    // If they don't select any file in the brwoser and press back button,
    // the data is not loaded
    if(!app->data_loaded) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
    }
    consumed = true;

    return consumed;
}

void metroflip_scene_load_on_exit(void* context) {
    Metroflip* app = context;
    UNUSED(app);
}
