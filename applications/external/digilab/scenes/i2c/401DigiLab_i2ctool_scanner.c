/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */

#include "scenes/i2c/401DigiLab_i2ctool_scanner.h"
static const char* TAG = "401_DigiLabI2CToolScanner";
#include <401_gui.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_rtc.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <storage/storage.h>
#include <toolbox/path.h>

#include <401_err.h>
#include <devicehelpers.h>

#include "cJSON/cJSON.h"
#include <cJSON/cJSON_helpers.h>

/**
 * @brief Checks if an address is known
 *
 * @param addr address to check
 */
bool i2cdevice_is_known(uint8_t addr) {
    bool found = false;
    char addr_filename[64] = {0};
    Storage* storage = furi_record_open(RECORD_STORAGE);
    snprintf(addr_filename, 63, DIGILABCONF_I2CDEV_ADDR_FILE_FMT, addr);
    FURI_LOG_I(TAG, "Testing for %s...", addr_filename);
    if(storage_common_stat(storage, addr_filename, NULL) == FSE_OK) {
        FURI_LOG_I(TAG, "Found!");
        found = true;
    }
    furi_record_close(RECORD_STORAGE);
    return found;
}

/**
 * @brief Scans for any i2C devices connected
 *
 * @param appI2CToolScanner scene context
 */
void i2ctoolscanner_scan(AppI2CToolScanner* appI2CToolScanner) {
    uint8_t addr = 0;

    i2CToolScannerModel* model = view_get_model(appI2CToolScanner->view);
    model->screenview = I2C_VIEW_SCAN;
    view_commit_model(appI2CToolScanner->view, true);

    // Free existing memory and allocate new memory
    if(model->devices_map != NULL) {
        free(model->devices_map);
    }
    model->devices_map = (i2cdevice_t*)malloc(I2CDEV_MAX_SCAN_LENGHT * sizeof(i2cdevice_t));
    if(model->devices_map == NULL) {
        FURI_LOG_E(TAG, "Memory allocation failed");
        return; // Exit if memory allocation fails
    }
    model->devices = 0;

    // Acquire I2C bus
    furi_hal_i2c_acquire(I2C_BUS);

    // Scan valid I2C addresses
    for(addr = I2CDEV_LOWEST_ADDR; addr <= I2CDEV_HIGHEST_ADDR; addr++) {
        int addr8b = addr << 1; // Convert 7-Bit Address into 8-Bit Address
        bool res = furi_hal_i2c_is_device_ready(I2C_BUS, addr8b, I2C_MEM_I2C_TIMEOUT);
        if(res) {
            FURI_LOG_I(TAG, "Scanned 0x%.02X", addr);
            model->devices_map[model->devices].addr = addr;
            model->devices_map[model->devices].known = i2cdevice_is_known(addr);
            model->devices++;
        } else {
            FURI_LOG_I(TAG, "Nothing on 0x%.02X", addr);
        }
    }
    furi_hal_i2c_release(I2C_BUS);

    FURI_LOG_I(TAG, "Found %d devices...", model->devices);
    model->screenview = I2C_VIEW_ADDR;
    view_commit_model(appI2CToolScanner->view, true);

    // Release I2C bus

    // Commit the updated model to the view
    with_view_model(
        appI2CToolScanner->view,
        i2CToolScannerModel * model,
        { model->screenview = I2C_VIEW_ADDR; },
        true);
}

/**
 * @brief Render I2C devices menu
 *
 * @param canvas canvas to draw on
 * @param appI2CToolScanner scene model
 */
static void app_i2ctool_render_i2cDeviceMenu(Canvas* canvas, void* ctx) {
    i2CToolScannerModel* model = (i2CToolScannerModel*)ctx;
    char str[32] = {0};
    uint8_t i = 0;
    uint8_t selected = model->device_selected;
    uint8_t devices = model->devices;
    i2cdevice_t* device = &model->devices_map[model->device_selected];
    if(devices > 0) {
        canvas_set_font(canvas, FontPrimary);
        //canvas_draw_str_aligned(canvas, 39, 27, AlignLeft, AlignBottom, "0x");

        snprintf(str, 31, "0x%.02X", device->addr);
        canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_HUGE_REGULAR);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, str);

        snprintf(str, 31, "%d/%d", selected + 1, devices);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 128, 0, AlignRight, AlignTop, str);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Found:");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "I2C");

        if(device->options > 0) {
            canvas_set_font(canvas, FontSecondary);
            elements_button_center(canvas, "Info");
            snprintf(
                str, 31, "%d prediction%c:", device->options, ((device->options > 1) ? 's' : ' '));

            canvas_draw_str_aligned(canvas, 0, 32, AlignLeft, AlignTop, str);
            canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
            uint8_t wt = 0;
            for(i = 0; i < device->types; i++) {
                device_type_t type = device->types_map[i];
                device_getTypeStr(type, str, 31);
                wt += 2 + l401Gui_draw_btn(canvas, 1 + wt, 43, 0, false, str);
            }
        } else {
            elements_button_center(canvas, "R/W");
            canvas_draw_str_aligned(canvas, 0, 32, AlignLeft, AlignTop, "Unknown :(");
        }
        canvas_set_font(canvas, FontSecondary);
        elements_button_right(canvas, "Prev");
        elements_button_left(canvas, "Next");
    }
}

/**
 * @brief Free I2C device
 *
 * @param device allocated device
 */
static void app_i2ctool_free_options(i2cdevice_t* device) {
    FURI_LOG_I(TAG, "Setting params to 0");
    device->option_selected = 0;
    for(uint8_t i = 0; i < device->options; i++) {
        FURI_LOG_I(TAG, "Freeing %s", device->options_map[i].partno);
        free(device->options_map[i].partno);
        free(device->options_map[i].desc);
        free(device->options_map[i].manufacturer);
        free(device->options_map[i].date);
    }
    device->options = 0;
    device->loaded = false;
}

/**
 * @brief Load every gesses from an address.
 *
 * @param device allocated device
 */
static l401_err app_i2ctool_load_options(i2cdevice_t* device) {
    l401_err err = L401_OK;
    char addr_filename[64] = {0};
    if(device->options > 0) {
        app_i2ctool_free_options(device);
    }

    if(device->known && !device->loaded) {
        snprintf(addr_filename, 64, DIGILABCONF_I2CDEV_ADDR_FILE_FMT, device->addr);
        cJSON* json = NULL;

        err = json_read(addr_filename, &json);
        if(err != L401_OK) {
            FURI_LOG_E(TAG, "Could not read %s", addr_filename);
            return err;
        }

        // Check COMPONENTS
        cJSON* json_devices = cJSON_GetObjectItemCaseSensitive(json, "components");
        FURI_LOG_I(TAG, "Json cJSON_IsArray...");
        if(!cJSON_IsArray(json_devices)) {
            FURI_LOG_E(TAG, "Components is not a JSON array\n");
            cJSON_Delete(json);
            return L401_ERR_MALFORMED;
        }

        // Check UNIQUE TYPES
        cJSON* json_unique_types = cJSON_GetObjectItemCaseSensitive(json, "unique_types");
        if(!cJSON_IsArray(json_unique_types)) {
            FURI_LOG_E(TAG, "unique_types is not a JSON array\n");
            cJSON_Delete(json);
            return L401_ERR_MALFORMED;
        }

        // Get unique types from JSON
        device->types = cJSON_GetArraySize(json_unique_types);
        device->types_map = (uint8_t*)malloc((device->types) * sizeof(uint8_t));
        if(device->types_map == NULL) {
            cJSON_Delete(json);
            return L401_ERR_INTERNAL;
        }

        cJSON* json_item = NULL;
        int index = 0;
        // For each Unique type in the JSON file
        cJSON_ArrayForEach(json_item, json_unique_types) {
            if(cJSON_IsNumber(json_item)) {
                device->types_map[index] = (uint8_t)json_item->valueint;
                index++;
            } else {
                free(device->types_map);
                cJSON_Delete(json);
                return L401_ERR_MALFORMED;
            }
        }

        cJSON_ArrayForEach(json_item, json_devices) {
            FURI_LOG_I(TAG, "device n° %d...", device->options);
            cJSON* json_partno = cJSON_GetObjectItemCaseSensitive(json_item, "partno");
            cJSON* json_desc = cJSON_GetObjectItemCaseSensitive(json_item, "desc");
            cJSON* json_manufacturer = cJSON_GetObjectItemCaseSensitive(json_item, "manufacturer");
            cJSON* json_date = cJSON_GetObjectItemCaseSensitive(json_item, "date");
            cJSON* json_type = cJSON_GetObjectItemCaseSensitive(json_item, "type");

            i2cdevice_option_t* option = &device->options_map[device->options];
            if(cJSON_IsString(json_partno) && (json_partno->valuestring != NULL) &&
               cJSON_IsString(json_desc) && (json_desc->valuestring != NULL) &&
               cJSON_IsString(json_manufacturer) && (json_manufacturer->valuestring != NULL) &&
               cJSON_IsNumber(json_type) && cJSON_IsString(json_date) &&
               (json_date->valuestring != NULL)) {
                option->partno = strdup(json_partno->valuestring);
                option->desc = strdup(json_desc->valuestring);
                option->manufacturer = strdup(json_manufacturer->valuestring);
                option->type = (device_type_t)json_type->valueint;
                option->date = strdup(json_date->valuestring);
                device->options++;
                if(device->options >= I2CDEV_MAX_OPTIONS) break;
            }
        }
        cJSON_Delete(json);
    } else {
        return L401_ERR_FILE_DOESNT_EXISTS;
    }
    for(uint8_t i = 0; i < device->options; i++) {
        FURI_LOG_I(TAG, "Could be %s", device->options_map[i].partno);
    }
    return L401_OK;
}

/**
 * @brief Render I2C devices informations
 *
 * @param canvas canvas to draw on
 * @param appI2CToolScanner scene model
 */
void app_i2ctool_render_i2cDeviceInfo(Canvas* canvas, void* ctx) {
    i2CToolScannerModel* model = (i2CToolScannerModel*)ctx;
    char str[64] = {0};
    i2cdevice_t* device = &model->devices_map[model->device_selected];

    canvas_set_font(canvas, FontSecondary);
    snprintf(str, 32, "Prediction for 0x%02X", device->addr);
    canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, str);
    snprintf(str, 63, "%d/%d", device->option_selected + 1, device->options);
    canvas_draw_str_aligned(canvas, 128, 1, AlignRight, AlignTop, str);
    if((device->known == true) && (device->loaded == true)) {
        i2cdevice_option_t* option = &device->options_map[device->option_selected];
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, option->partno);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 128, 10, AlignRight, AlignTop, option->manufacturer);
        device_type_t type = option->type;
        device_getTypeStr(type, str, 31);
        canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
        l401Gui_draw_btn(canvas, 2, 43, 0, false, str);
        elements_text_box(canvas, 0, 20, 128, 20, AlignLeft, AlignTop, option->desc, true);
        elements_button_center(canvas, "R/W");
    } else {
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, "I don't know much more.");
    }
    if(device->options > 1) {
        elements_button_right(canvas, "Next");
        elements_button_left(canvas, "Prev");
    }
}

/**
 * @brief Render callback for the app i2ctoolscanner.
 *
 * @param canvas The canvas to be used for rendering.
 * @param model The model passed to the callback.
 */

void app_i2ctoolscanner_render_callback(Canvas* canvas, void* ctx) {
    l401_err err = L401_OK;
    i2CToolScannerModel* model = (i2CToolScannerModel*)ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(model->screenview) {
    case I2C_VIEW_SCAN:
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Scanning...");
        break;
    case I2C_VIEW_ADDR:
        if(model->devices_map[model->device_selected].loaded == false) {
            err = app_i2ctool_load_options(&model->devices_map[model->device_selected]);
            if(err != L401_OK) {
                FURI_LOG_E(
                    TAG,
                    "Could not load device with addr %02x",
                    model->devices_map[model->device_selected].addr);
            }
            model->devices_map[model->device_selected].loaded = true;
        }
        app_i2ctool_render_i2cDeviceMenu(canvas, ctx);
        break;
    case I2C_VIEW_DETAIL:
        if(model->devices_map[model->device_selected].loaded == false) {
            err = app_i2ctool_load_options(&model->devices_map[model->device_selected]);
            if(err != L401_OK) {
                FURI_LOG_E(
                    TAG,
                    "Could not load device with addr %02x",
                    model->devices_map[model->device_selected].addr);
            }
            model->devices_map[model->device_selected].loaded = true;
        }
        app_i2ctool_render_i2cDeviceInfo(canvas, ctx);
        break;
    case I2C_VIEW_UNKNOWN:
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "No devices found.");
        elements_button_center(canvas, "R/W");
        break;
    case I2C_VIEW_COUNT:
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Nothing to count");
        break;
    }
}

/**
 * @brief Allocate memory and initialize the app i2ctoolscanner.
 *
 * @return Returns a pointer to the allocated AppI2CToolScanner.
 */
AppI2CToolScanner* app_i2ctoolscanner_alloc(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppI2CToolScanner* appI2CToolScanner = malloc(sizeof(AppI2CToolScanner));

    appI2CToolScanner->view = view_alloc();
    view_allocate_model(
        appI2CToolScanner->view, ViewModelTypeLockFree, sizeof(i2CToolScannerModel));

    i2CToolScannerModel* model = view_get_model(appI2CToolScanner->view);
    model->screenview = I2C_VIEW_ADDR;

    view_set_context(appI2CToolScanner->view, app);
    view_set_draw_callback(appI2CToolScanner->view, app_i2ctoolscanner_render_callback);
    view_set_input_callback(appI2CToolScanner->view, app_i2ctoolscanner_input_callback);

    return appI2CToolScanner;
}

/**
 * @brief Callback function to handle input events for the app i2ctoolscanner.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_i2ctoolscanner_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolScanner* appI2CToolScanner = (AppI2CToolScanner*)app->sceneI2CToolScanner;
    i2CToolScannerModel* model = view_get_model(appI2CToolScanner->view);
    bool consumed = false;

    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyUp:
            break;
        case InputKeyDown:
            break;
        case InputKeyLeft:
            switch(model->screenview) {
            case I2C_VIEW_ADDR:
                consumed = true;
                if(model->device_selected > 0)
                    model->device_selected--;
                else
                    model->device_selected = model->devices - 1;
                view_commit_model(appI2CToolScanner->view, true);
                break;
            case I2C_VIEW_DETAIL:
                consumed = true;
                i2cdevice_t* device = &model->devices_map[model->device_selected];
                if(device->option_selected > 0)
                    device->option_selected--;
                else
                    device->option_selected = device->options - 1;
                view_commit_model(appI2CToolScanner->view, true);
                break;
            default:
                break;
            }
            break;
        case InputKeyRight:
            switch(model->screenview) {
            case I2C_VIEW_ADDR:
                consumed = true;
                if(model->device_selected < model->devices - 1)
                    model->device_selected++;
                else
                    model->device_selected = 0;
                view_commit_model(appI2CToolScanner->view, true);
                break;
            case I2C_VIEW_DETAIL:
                consumed = true;
                i2cdevice_t* device = &model->devices_map[model->device_selected];
                if(device->option_selected < device->options - 1)
                    device->option_selected++;
                else
                    device->option_selected = 0;
                view_commit_model(appI2CToolScanner->view, true);
                break;
            default:
                break;
            }
            break;
        case InputKeyOk:
            i2cdevice_t* device = &model->devices_map[model->device_selected];
            switch(model->screenview) {
            case I2C_VIEW_ADDR:
                consumed = true;
                if(device->options > 0) {
                    model->screenview = I2C_VIEW_DETAIL;
                } else {
                    app->i2cToolDevice = device;
                    scene_manager_next_scene(app->scene_manager, AppSceneI2CToolReader);
                }
                view_commit_model(appI2CToolScanner->view, true);
                break;
            case I2C_VIEW_DETAIL:
                consumed = true;
                app->i2cToolDevice = device;
                scene_manager_next_scene(app->scene_manager, AppSceneI2CToolReader);
                break;
            default:
                consumed = true;
                if(device->options > 0) {
                    model->screenview = I2C_VIEW_ADDR;
                } else {
                    model->screenview = I2C_VIEW_UNKNOWN;
                }
                view_commit_model(appI2CToolScanner->view, true);

                break;
            }
            break;
        default:
            break;
        }
    }

    return consumed;
}

/**
 * @brief Get the view associated with the app i2ctoolscanner.
 *
 * @param appI2CToolScanner The AppI2CToolScanner for which the view is to be
 * fetched.
 * @return Returns a pointer to the View.
 */
View* app_i2ctoolscanner_get_view(AppI2CToolScanner* appI2CToolScanner) {
    furi_assert(appI2CToolScanner);
    return appI2CToolScanner->view;
}

/**
 * @brief Callback when the app i2ctoolscanner scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_i2ctoolscanner_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppI2CToolScanner* appI2CToolScanner = app->sceneI2CToolScanner;
    with_view_model(
        appI2CToolScanner->view,
        i2CToolScannerModel * model,
        { model->screenview = I2C_VIEW_SCAN; },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewI2CToolScanner);
    i2ctoolscanner_scan(appI2CToolScanner);
}

/**
 * @brief Handle scene manager events for the app i2ctoolscanner scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_i2ctoolscanner_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
 * @brief Callback when the app i2ctoolscanner scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_i2ctoolscanner_on_exit(void* ctx) {
    /*AppContext *app = (AppContext *)ctx;
  AppI2CToolScanner *appI2CToolScanner = app->sceneI2CToolScanner;
  i2CToolScannerModel *model = view_get_model(appI2CToolScanner->view);
  */
    UNUSED(ctx);
}

/**
 * @brief Free the memory occupied by the app i2ctoolscanner.
 *
 * @param appI2CToolScanner The AppI2CToolScanner to be freed.
 */
void app_i2ctoolscanner_free(AppI2CToolScanner* appI2CToolScanner) {
    i2CToolScannerModel* model = view_get_model(appI2CToolScanner->view);
    for(int i = 0; i < model->devices; i++) {
        FURI_LOG_I(
            TAG,
            "Freeing option %d for %02X",
            model->devices_map[i].options,
            model->devices_map[i].addr);
        app_i2ctool_free_options(&model->devices_map[i]);
    }
    free(model->devices_map);
    furi_assert(appI2CToolScanner);
    view_free_model(appI2CToolScanner->view);
    view_free(appI2CToolScanner->view);
    free(appI2CToolScanner);
}
