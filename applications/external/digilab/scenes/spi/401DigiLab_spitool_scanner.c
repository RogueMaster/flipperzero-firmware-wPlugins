/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "scenes/spi/401DigiLab_spitool_scanner.h"
#include <401_gui.h>
#include <cJSON/cJSON_helpers.h>

static const char* TAG = "401_DigiLabSpiToolScanner";

/**
Compare register's red values against it's expected values
*/
static bool cmp_spi_reg_nbytes(uint8_t reg, uint8_t len, uint8_t* expected) {
    uint8_t tx_buffer[1] = {reg | 0x80}; // Read command
    uint8_t* rx_buffer = calloc(len, sizeof(uint8_t));

    if(rx_buffer == NULL) {
        FURI_LOG_E(__FUNCTION__, "Couldn't allocate %d bytes", len);
        return false;
    }

    FURI_LOG_E(__FUNCTION__, "Sending 0X%02X", reg);

    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_tx(
        &furi_hal_spi_bus_handle_external, tx_buffer, sizeof(tx_buffer), SPI_MEM_SPI_TIMEOUT);
    furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_external, rx_buffer, len, SPI_MEM_SPI_TIMEOUT);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);

    // Validate against expected values
    if(memcmp(rx_buffer, expected, len) == 0) {
        FURI_LOG_I(__FUNCTION__, "Read Multiple: Passed");
        return true;
    } else {
        FURI_LOG_E(__FUNCTION__, "Read Multiple: Failed.");
        return false;
    }
    free(rx_buffer);
}

/**
Compare a register's value with it's expected value
*/
static bool cmp_spi_reg_byte(uint8_t reg, uint8_t expected) {
    uint8_t tx_buffer[2] = {reg | 0x80, 0}; // Read command
    uint8_t rx_buffer[2] = {0};
    FURI_LOG_I(__FUNCTION__, "REG: %02x VAL: %02x", tx_buffer[0], tx_buffer[1]);
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    furi_hal_spi_bus_tx(
        &furi_hal_spi_bus_handle_external, tx_buffer, sizeof(tx_buffer), SPI_MEM_SPI_TIMEOUT);
    furi_hal_spi_bus_rx(
        &furi_hal_spi_bus_handle_external, rx_buffer, sizeof(rx_buffer), SPI_MEM_SPI_TIMEOUT);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    if(rx_buffer[1] == expected) {
        return true;
    } else {
        return false;
    }
}

// Test: Write value to a register
static void write_spi_reg(uint8_t reg, uint8_t value) {
    uint8_t tx_buffer[2] = {reg & 0x7F, value}; // Write command
    FURI_LOG_I(__FUNCTION__, "REG: %02x VAL: %02x", tx_buffer[0], tx_buffer[1]);
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);

    if(furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, tx_buffer, 2, SPI_MEM_SPI_TIMEOUT)) {
        FURI_LOG_I(__FUNCTION__, "Write Register 0x%02X: Passed (0x%02X)", reg, value);
    } else {
        FURI_LOG_E(__FUNCTION__, "Write Register 0x%02X: Failed", reg);
    }
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
}

static void app_spitool_free_devices(void* ctx) {
    SPIToolScannerModel* model = (SPIToolScannerModel*)ctx;
    model->device_selected = 0;
    model->devices = 0;
    for(uint8_t i = 0; i < model->devices; i++) {
        FURI_LOG_I(TAG, "....%s", model->devices_map[i].partno);
        free(model->devices_map[i].partno);
        free(model->devices_map[i].desc);
        free(model->devices_map[i].manufacturer);
        free(model->devices_map[i].date);
    }
    free(model->devices_map);
}

/**
Test SPI Device based on it's Json definition
*/
static bool spitoolscanner_tests(cJSON* json_device) {
    bool res = true;
    cJSON* json_partno = cJSON_GetObjectItem(json_device, "partno");
    cJSON* json_tests = cJSON_GetObjectItem(json_device, "tests");

    if(!json_partno || !json_tests) {
        FURI_LOG_E(__FUNCTION__, "Malformed json_device in JSON");
        return false;
    }

    FURI_LOG_I(__FUNCTION__, "Testing Component: %s", json_partno->valuestring);

    cJSON* json_test = NULL;
    cJSON_ArrayForEach(json_test, json_tests) {
        if(!json_test) {
            FURI_LOG_E(__FUNCTION__, "Invalid test entry in JSON.");
            continue;
        }

        cJSON* json_type = cJSON_GetObjectItem(json_test, "type");
        cJSON* json_reg = cJSON_GetObjectItem(json_test, "reg");

        if(!cJSON_IsString(json_type) || !cJSON_IsString(json_reg)) {
            FURI_LOG_E(__FUNCTION__, "Malformed test object. Skipping.");
            continue;
        }

        char* type = json_type->valuestring;
        uint8_t reg = (uint8_t)strtoul(json_reg->valuestring, NULL, 16);
        // Detect test
        if(strncmp(json_type->valuestring, "READ", strlen(json_type->valuestring)) == 0) {
            cJSON* json_expect_item = cJSON_GetObjectItem(json_test, "expect");
            if(!cJSON_IsString(json_expect_item)) {
                FURI_LOG_E(__FUNCTION__, "Missing or invalid 'expected_value'.");
                continue;
            }
            uint8_t expected = (uint8_t)strtoul(json_expect_item->valuestring, NULL, 16);
            if(cmp_spi_reg_byte(reg, expected) == false) {
                res = false;
            }

        } else if(strncmp(json_type->valuestring, "WRITE", strlen(json_type->valuestring)) == 0) {
            cJSON* value_item = cJSON_GetObjectItem(json_test, "value");
            if(!cJSON_IsString(value_item)) {
                FURI_LOG_E(__FUNCTION__, "Missing or invalid 'value'.");
                continue;
            }
            uint8_t value = 0;
            if(value_item && value_item->valuestring) {
                value = (uint8_t)strtoul(value_item->valuestring, NULL, 16);
            }
            write_spi_reg(reg, value);
        } else if(strncmp(type, "READN", strlen(json_type->valuestring)) == 0) {
            cJSON* json_expect_item = cJSON_GetObjectItem(json_test, "expect");
            if(!cJSON_IsArray(json_expect_item)) {
                FURI_LOG_E(__FUNCTION__, "Missing or invalid 'expected_value' field.");
                continue;
            }

            size_t len = 0;
            uint8_t* expect = NULL;
            l401_err l401res = json_read_hex_array(json_expect_item, &expect, &len);
            if(l401res != L401_OK || expect == NULL) {
                printf("Error: Failed to read hex array, error code: %d\n", l401res);
                cJSON_Delete(json_expect_item); // Free cJSON object
                return false;
            }
            if(cmp_spi_reg_nbytes(reg, len, expect) == false) {
                res = false;
            }
        } else {
            return false;
        }
    }

    return res;
}

static void spitoolscanner_powercycle() {
    furi_hal_gpio_init_simple(&gpio_ext_pa4, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pa4, false);
    furi_delay_tick(100);
    furi_hal_gpio_write(&gpio_ext_pa4, true);
    furi_delay_tick(100);
}
/**
Scan for SPI Devices based on DIGILABCONF_SPIDEV_TESTS test file
*/
static bool spitoolscanner_scan(AppSPIToolScanner* appSpitoolScanner) {
    SPIToolScannerModel* model = view_get_model(appSpitoolScanner->view);
    cJSON* json = NULL;
    l401_err err = L401_OK;
    spidevice_t* device = NULL;
    //uint8_t i = 0;

    // PowerCycle CS Pin to kinda Reset
    spitoolscanner_powercycle();

    // Load JSON File into a cJSON Structure
    err = json_read((const char*)DIGILABCONF_SPIDEV_TESTS, &json);
    if(err != L401_OK) {
        FURI_LOG_E(TAG, "Could not read %s", DIGILABCONF_SPIDEV_TESTS);
        return false;
    }
    // Check Json result
    if(json == NULL || !cJSON_IsObject(json)) {
        FURI_LOG_E(__FUNCTION__, "Json is NULL");
        return false;
    }
    // Get component list
    cJSON* json_devices = cJSON_GetObjectItem(json, "components");
    if(!json_devices || !cJSON_IsArray(json_devices)) {
        cJSON_Delete(json);
        FURI_LOG_E(__FUNCTION__, "JSON DEVICE ISN'T JSON");
        return false;
    }

    cJSON* json_device = NULL;
    // Free existing memory and allocate new memory
    if(model->devices_map != NULL) {
        app_spitool_free_devices(model);
        FURI_LOG_E(__FUNCTION__, "JSON DEVICE ALREADY ALLOCATED");
    }

    model->devices_map = (spidevice_t*)malloc(SPIDEV_MAX_OPTIONS * sizeof(spidevice_t));

    if(model->devices_map == NULL) {
        FURI_LOG_E(TAG, "Memory allocation failed");
        cJSON_Delete(json);
        return false; // Exit if memory allocation fails
    }

    model->devices = 0;
    cJSON_ArrayForEach(json_device, json_devices) {
        // Parse and execute tests
        if(spitoolscanner_tests(json_device)) {
            device = &model->devices_map[model->devices];
            cJSON* json_partno = cJSON_GetObjectItemCaseSensitive(json_device, "partno");
            cJSON* json_desc = cJSON_GetObjectItemCaseSensitive(json_device, "desc");
            cJSON* json_manufacturer =
                cJSON_GetObjectItemCaseSensitive(json_device, "manufacturer");
            cJSON* json_date = cJSON_GetObjectItemCaseSensitive(json_device, "date");
            cJSON* json_type = cJSON_GetObjectItemCaseSensitive(json_device, "type");

            if(cJSON_IsString(json_partno) && (json_partno->valuestring != NULL) &&
               cJSON_IsString(json_desc) && (json_desc->valuestring != NULL) &&
               cJSON_IsString(json_manufacturer) && (json_manufacturer->valuestring != NULL) &&
               cJSON_IsNumber(json_type) && cJSON_IsString(json_date) &&
               (json_date->valuestring != NULL)) {
                device->partno = strdup(json_partno->valuestring);
                device->desc = strdup(json_desc->valuestring);
                device->manufacturer = strdup(json_manufacturer->valuestring);
                device->type = (device_type_t)json_type->valueint;
                device->date = strdup(json_date->valuestring);
                model->devices++;
            }
        }
    }
    /*
    if(model->devices > 0) {
        FURI_LOG_I(TAG, "Found %d potential SPI devices", model->devices);
        for(i = 0; i < model->devices; i++) {
            device = &model->devices_map[i];
            FURI_LOG_W(__FUNCTION__, "...device->partno:\t %s", device->partno);
            FURI_LOG_W(__FUNCTION__, "...device->desc:\t %s", device->desc);
            FURI_LOG_W(__FUNCTION__, "...device->manufacturer:\t %s", device->manufacturer);
            FURI_LOG_W(__FUNCTION__, "...device->type:\t %d", device->type);
            FURI_LOG_W(__FUNCTION__, "...device->date:\t %s", device->date);
        }
    } else {
        FURI_LOG_I(TAG, "No potential SPI devices found");
    }
    */
    cJSON_Delete(json);
    // Commit the updated model to the view
    with_view_model(
        appSpitoolScanner->view,
        SPIToolScannerModel * model,
        { model->screenview = model->devices ? SPI_VIEW_DETAIL : SPI_VIEW_UNKNOWN; },
        true);
    return true;
}

static void app_spitool_render_spiDeviceInfo(Canvas* canvas, void* ctx) {
    SPIToolScannerModel* model = (SPIToolScannerModel*)ctx;
    char str[64] = {0};
    spidevice_t* device = &model->devices_map[model->device_selected];

    canvas_set_font(canvas, FontSecondary);
    snprintf(str, 32, "Predictions:");
    canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, str);
    snprintf(str, 63, "%d/%d", model->device_selected + 1, model->devices);
    canvas_draw_str_aligned(canvas, 128, 1, AlignRight, AlignTop, str);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "SPI");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, device->partno);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 128, 10, AlignRight, AlignTop, device->manufacturer);
    device_type_t type = device->type;
    device_getTypeStr(type, str, 31);
    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
    l401Gui_draw_btn(canvas, 2, 43, 0, false, str);
    elements_text_box(canvas, 0, 20, 128, 20, AlignLeft, AlignTop, device->desc, true);

    if(model->devices > 1) {
        elements_button_right(canvas, "Next");
        elements_button_left(canvas, "Prev");
    }
}

/**
 * Callback function for the main menu. Handles menu item selection and
 * switches to the appropriate scene based on the selected index.
 *
 * @param ctx The application ctx.
 * @param index The index of the selected menu item.
 */
void app_spitoolscanner_callback(void* ctx, uint32_t index) {
    UNUSED(ctx);
    UNUSED(index);
}

/**
 * @brief Render callback for the app osc.
 *
 * @param canvas The canvas to be used for rendering.
 * @param model The model passed to the callback.
 */

void app_spitoolscanner_render_callback(Canvas* canvas, void* ctx) {
    l401_err err = L401_OK;
    UNUSED(err);
    SPIToolScannerModel* model = (SPIToolScannerModel*)ctx;
    canvas_clear(canvas);
    switch(model->screenview) {
    case SPI_VIEW_SCAN:
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Scanning...");
        break;
    case SPI_VIEW_DETAIL:
        app_spitool_render_spiDeviceInfo(canvas, ctx);
        break;
    case SPI_VIEW_UNKNOWN:

        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "No devices found.");

        break;
    case SPI_VIEW_COUNT:
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "SPI_VIEW_COUNT");
        break;
    }
}

/**
 * Handles the on-enter event for the main menu scene. Switches the view to
 * the main menu.
 *
 * @param ctx The application ctx.
 */
void app_scene_spitoolscanner_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppSPIToolScanner* appSpiToolScanner = (AppSPIToolScanner*)app->sceneSPIToolScanner;
    with_view_model(
        appSpiToolScanner->view,
        SPIToolScannerModel * model,
        { model->screenview = SPI_VIEW_SCAN; },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSPIToolScanner);
    if(!spitoolscanner_scan(appSpiToolScanner)) {
        with_view_model(
            appSpiToolScanner->view,
            SPIToolScannerModel * model,
            { model->screenview = SPI_VIEW_UNKNOWN; },
            true);
    }
}
/**
 * @brief Callback function to handle input events for the app osc.
 *
 * @param input_event The input event.
 * @param ctx The context passed to the callback.
 * @return Returns true if the event was handled, otherwise false.
 */
bool app_spitoolscanner_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppSPIToolScanner* appSPIToolScanner = (AppSPIToolScanner*)app->sceneSPIToolScanner;
    SPIToolScannerModel* model = view_get_model(appSPIToolScanner->view);
    bool consumed = false;

    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyUp:
            break;
        case InputKeyDown:
            break;
        case InputKeyLeft:
            switch(model->screenview) {
            case SPI_VIEW_DETAIL:
                consumed = true;
                if(model->device_selected > 0)
                    model->device_selected--;
                else
                    model->device_selected = model->devices - 1;
                view_commit_model(appSPIToolScanner->view, true);
                break;
            default:
                break;
            }
            break;
        case InputKeyRight:
            switch(model->screenview) {
            case SPI_VIEW_DETAIL:
                consumed = true;
                if(model->device_selected < model->devices - 1)
                    model->device_selected++;
                else
                    model->device_selected = 0;
                view_commit_model(appSPIToolScanner->view, true);
                break;
            default:
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
 * Handles events for the main menu scene. Logs specific events and switches
 * to the appropriate scene.
 *
 * @param ctx The application ctx.
 * @param event The scene manager event.
 * @return true if the event was consumed, false otherwise.
 */
bool app_scene_spitoolscanner_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

/**
 * Handles the on-exit event for the main menu scene.
 *
 * @param ctx The application ctx.
 */
void app_scene_spitoolscanner_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppSPIToolScanner* appSPIToolScanner = (AppSPIToolScanner*)app->sceneSPIToolScanner;
    SPIToolScannerModel* model = view_get_model(appSPIToolScanner->view);
    if(model->devices_map != NULL) {
        app_spitool_free_devices(model);
    }
}

/**
 * Allocates and initializes an instance of SpiToolScanner. (Currently returns
 * NULL)
 *
 * @return A pointer to the allocated SpiToolScanner or NULL.
 */
AppSPIToolScanner* app_spitoolscanner_alloc(void* ctx) {
    furi_assert(ctx);
    // Ensures the 5V is ON
    if(!furi_hal_power_is_otg_enabled()) furi_hal_power_enable_otg();
    AppContext* app = (AppContext*)ctx;

    AppSPIToolScanner* appSpiToolScanner = malloc(sizeof(AppSPIToolScanner));
    appSpiToolScanner->view = view_alloc();
    view_allocate_model(
        appSpiToolScanner->view, ViewModelTypeLockFree, sizeof(SPIToolScannerModel));

    view_set_context(appSpiToolScanner->view, app);
    view_set_draw_callback(appSpiToolScanner->view, app_spitoolscanner_render_callback);
    view_set_input_callback(appSpiToolScanner->view, app_spitoolscanner_input_callback);

    return appSpiToolScanner;
}

/**
 * @brief Get the view associated with the app osc.
 *
 * @param appSpiToolScanner The AppSPIToolScanner for which the view is to be
 * fetched.
 * @return Returns a pointer to the View.
 */
View* app_spitoolscanner_get_view(AppSPIToolScanner* appSpitoolScanner) {
    furi_assert(appSpitoolScanner);
    return appSpitoolScanner->view;
}

/**
 * @brief Free the memory occupied by the app probe.
 *
 * @param appProbe The AppProbe to be freed.
 */
void app_spitoolscanner_free(AppSPIToolScanner* appSpitoolScanner) {
    furi_assert(appSpitoolScanner);
    view_free_model(appSpitoolScanner->view);
    view_free(appSpitoolScanner->view);
    free(appSpitoolScanner);
}
