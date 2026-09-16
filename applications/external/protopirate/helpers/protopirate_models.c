#include "protopirate_models.h"

#define TAG "ProtoPirateModels"

#define PROTOPIRATE_MODELS_FILE APP_ASSETS_PATH("models.txt")

//Models File Format.
#define MODELS_FILE_HEADER              "ProtoPirate Car Models Database"
#define MODELS_FILE_VERSION             2
#define MODELS_FIELD_MODEL_COUNT        "ModelCount"
#define MODELS_FIELD_MODEL_NAME         "ModelName"
#define MODELS_FIELD_FREQUENCY          "Frequency"
#define MODELS_FIELD_PRESET_NAME        "PresetName"
#define MODELS_FIELD_CUSTOM_PRESET_DATA "CustomPresetData"

bool car_model_get_by_index(
    ProtoPirateCarModel* car_model,
    uint16_t index,
    uint16_t car_models_count,
    SubGhzSetting* app_settings) {
    //Temp variables
    uint32_t version = 0;
    uint32_t frequency;
    uint8_t* preset_data = NULL;
    uint32_t preset_data_size = 0;
    bool error = true;
    bool custom_preset = false;

    //Objects that need allocating.
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* preset_name = furi_string_alloc();
    FuriString* model_name = furi_string_alloc();
    FuriString* header = furi_string_alloc();

    //Index 0 is None.
    switch(index) {
    case 0:
        //Leave as same as error (none)
        break;
    default: {
        //Open Models File, check the Header, check Version
        if(!flipper_format_file_open_existing(ff, PROTOPIRATE_MODELS_FILE)) {
            FURI_LOG_I(TAG, "models.txt file not found");
            break;
        } else if(!flipper_format_read_header(ff, header, &version)) {
            FURI_LOG_W(TAG, "Failed to read models.txt header");
            break;
        } else if(furi_string_cmp_str(header, MODELS_FILE_HEADER) != 0) {
            FURI_LOG_W(TAG, "Invalid models.txt file header");
            break;
        } else if(version != MODELS_FILE_VERSION) {
            FURI_LOG_W(TAG, "Unsupported models.txt version %lu", (unsigned long)version);
            break;
        }

        //Fields we add the number to for searching the file.
        char model_name_index[16]; //Frequency65535 + NULL
        char frequency_index[16]; //Frequency65535 + NULL
        char preset_name_index[18]; //PresetName65535 + NULL
        char preset_data_index[23]; //CustomPresetData65535 + NULL

        //Build the values we need to grab.
        snprintf(model_name_index, sizeof(model_name_index), MODELS_FIELD_MODEL_NAME "%u", index);
        snprintf(frequency_index, sizeof(frequency_index), MODELS_FIELD_FREQUENCY "%u", index);
        snprintf(
            preset_name_index, sizeof(preset_name_index), MODELS_FIELD_PRESET_NAME "%u", index);
        snprintf(
            preset_data_index,
            sizeof(preset_data_index),
            MODELS_FIELD_CUSTOM_PRESET_DATA "%u",
            index);

        //Read the Model Name.
        if(!flipper_format_read_string(ff, model_name_index, model_name)) {
            FURI_LOG_E("ProtoPirate", "Failed to read %s", model_name_index);
            break;
        }

        // Read frequency
        if(!flipper_format_read_uint32(ff, frequency_index, &frequency, 1)) {
            FURI_LOG_W(TAG, "Failed to read frequency.");
            frequency = 0;
            break;
        }

        //Read the Modulation Preset.
        if(!flipper_format_read_string(ff, preset_name_index, preset_name)) {
            //Set the Custom preset name.
            furi_string_set_str(preset_name, "Custom");
            custom_preset = true;
        } else {
            //Get the built in preset if it exists
            const char* preset_name_long = furi_string_get_cstr(preset_name);
            const char* preset_name_short = preset_name_long;

            //Which Preset have we selected?
            if(!strcmp(preset_name_long, "FuriHalSubGhzPresetOok270Async")) {
                preset_name_short = "AM270";
            } else if(!strcmp(preset_name_long, "FuriHalSubGhzPresetOok650Async")) {
                preset_name_short = "AM650";
            } else if(!strcmp(preset_name_long, "FuriHalSubGhzPreset2FSKDev238Async")) {
                preset_name_short = "FM238";
            } else if(!strcmp(preset_name_long, "FuriHalSubGhzPreset2FSKDev12KAsync")) {
                preset_name_short = "FM12K";
            } else if(!strcmp(preset_name_long, "FuriHalSubGhzPreset2FSKDev476Async")) {
                preset_name_short = "FM476";
            } else if(!strcmp(preset_name_long, "FuriHalSubGhzPresetCustom")) {
                preset_name_short = "Custom";
                custom_preset = true;
            }

            //Set the preset_name
            furi_string_set_str(preset_name, preset_name_short);

            if(!custom_preset) {
                size_t preset_index = subghz_setting_get_preset_count(app_settings);
                for(size_t i = 0; i < subghz_setting_get_preset_count(app_settings); i++) {
                    if(!strcmp(
                           subghz_setting_get_preset_name(app_settings, i), preset_name_short)) {
                        preset_index = i;
                        break;
                    }
                }
                if(preset_index >= subghz_setting_get_preset_count(app_settings)) {
                    preset_name_short = "AM650";
                    for(size_t i = 0; i < subghz_setting_get_preset_count(app_settings); i++) {
                        if(!strcmp(
                               subghz_setting_get_preset_name(app_settings, i),
                               preset_name_short)) {
                            preset_index = i;
                            break;
                        }
                    }
                    if(preset_index >= subghz_setting_get_preset_count(app_settings)) {
                        FURI_LOG_E(TAG, "Failed to get preset index!");
                        break;
                    }
                }

                //Get the preset data into a buffer to copy it.
                uint8_t* temp_preset_data = NULL;
                uint32_t temp_preset_data_size = 0;
                temp_preset_data = subghz_setting_get_preset_data(app_settings, preset_index);
                temp_preset_data_size =
                    subghz_setting_get_preset_data_size(app_settings, preset_index);

                //Error fallback
                if(temp_preset_data == NULL) {
                    FURI_LOG_E(TAG, "Failed to get preset data!");
                    break;
                } else {
                    //Copy the preset data, so we can initialize it.
                    preset_data = malloc(temp_preset_data_size);
                    memcpy(preset_data, temp_preset_data, temp_preset_data_size);
                    preset_data_size = temp_preset_data_size;

                    //Yay, we made it...
                    error = false;
                }
            }
        }

        /* Custom_preset_data (only if present) */
        if(custom_preset &&
           flipper_format_get_value_count(ff, preset_data_index, &preset_data_size) &&
           preset_data_size > 0) {
            if(preset_data_size >= 1024) {
                FURI_LOG_E(
                    "ProtoPirate",
                    "%s too large: %lu",
                    preset_data_index,
                    (unsigned long)preset_data_size);
                break;
            }

            preset_data = malloc(preset_data_size);
            if(!preset_data) {
                FURI_LOG_E(
                    "ProtoPirate",
                    "Malloc failed: %s (%lu bytes)",
                    preset_data_index,
                    (unsigned long)preset_data_size);
                break;
            }

            flipper_format_rewind(ff);
            if(!flipper_format_read_hex(ff, preset_data_index, preset_data, preset_data_size)) {
                break;
            }

            //Yay, we made it...
            error = false;
        }
    }
    }

    if(!error) {
        //Set the Car Model name and index.
        furi_string_set((car_model)->name, model_name);
        car_model->index = index;

        //Alloc a preset if none, or recycle.
        if(!car_model->preset) {
            car_model->preset = malloc(sizeof(SubGhzRadioPreset));
            car_model->preset->name = furi_string_alloc();
        } else {
            //Free old data..
            if(car_model->preset->data) {
                free(car_model->preset->data);
                car_model->preset->data = NULL;
            }
        }
        furi_string_set(car_model->preset->name, preset_name);
        car_model->preset->data = preset_data;
        car_model->preset->data_size = preset_data_size;
        car_model->preset->frequency = frequency;
    } else {
        car_model->index = 0;
        furi_string_set_str(
            car_model->name,
            (car_models_count) ? "< Select a Car Model >" : "No Models in Database");

        if(car_model->preset) {
            if(car_model->preset->data) {
                free(car_model->preset->data);
                car_model->preset->data = NULL;
            }

            car_model->preset->data_size = 0;

            if(car_model->preset->name) {
                furi_string_free(car_model->preset->name);
                car_model->preset->name = NULL;
            }

            free(car_model->preset);
            car_model->preset = NULL;
        }

        // If we allocated preset_data but never assigned it, free it
        if(custom_preset && preset_data_size) {
            free(preset_data);
        }
    }

    //Close up and return.
    flipper_format_free(ff);
    furi_string_free(header);
    furi_string_free(model_name);
    furi_string_free(preset_name);
    furi_record_close(RECORD_STORAGE);
    return (!error);
}

uint16_t car_model_get_count(void) {
    uint32_t version = 0;
    uint32_t model_count = 0;

    //Set up storage, init temp variables
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    //Open file, read header, check version, and grab the model count.
    if(!flipper_format_file_open_existing(ff, PROTOPIRATE_MODELS_FILE)) {
        FURI_LOG_W(TAG, "Failed to open " PROTOPIRATE_MODELS_FILE);
    } else if(!flipper_format_read_header(ff, header, &version)) {
        FURI_LOG_W(TAG, "Failed to read models file header.");
    } else if(version != MODELS_FILE_VERSION || furi_string_cmp_str(header, MODELS_FILE_HEADER) != 0) {
        FURI_LOG_W(TAG, "Models file version is invalid.");
    } else if(!flipper_format_read_uint32(ff, MODELS_FIELD_MODEL_COUNT, &model_count, 1)) {
        FURI_LOG_W(TAG, "Failed to read " MODELS_FIELD_MODEL_COUNT ".");
    }

    //Free temp variables, close storage.
    furi_string_free(header);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    //Finished, return the count or 0 if error.
    return (uint16_t)model_count;
}
