/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_config.h"
#include "401_config.h"

static const char* TAG = "401_LightMsgConfig";
static uint32_t update_counter = 0;

const LightMsg_Orientation lightmsg_orientation_value[] = {
    LightMsg_OrientationWheelUp,
    LightMsg_OrientationWheelDown};

const char* const lightmsg_orientation_text[] = {"wheel Up", "wheel Down"};

const double lightmsg_brightness_value[] = {
    0.05,
    0.2,
    0.6,
    1,
};

const char* const lightmsg_brightness_text[] = {
    "Dim",
    "Moderate",
    "Bright",
    "Blinding",
};

const uint8_t lightmsg_sensitivity_value[] = {
    60, // Low sensitivity
    40, // High sensitivity
};

const char* const lightmsg_sensitivity_text[] = {
    "Low",
    "High",
};

const uint32_t lightmsg_colors_flat[] = {
    0xFF0000, // COLOR_RED
    0x7F0F00, // COLOR_ORANGE
    0x777700, // COLOR_YELLOW
    0x00FF00, // COLOR_GREEN
    0x007777, // COLOR_CYAN
    0x0000FF, // COLOR_BLUE
    0x770077, // COLOR_PURPLE
};

const char* const lightmsg_color_text[] = {
    "R3d", // flat color
    "0r4ng3", // flat color
    "Y3ll0w", // flat color
    "Gr33n", // flat color
    "Cy4n", // flat color
    "Blu3", // flat color
    "Purpl3", // flat color
    "Ny4nc4t", // animated colors
    "R4inb0w", // animated colors
    "Sp4rkl3", // animated colors
    "V4p0rw4v3", // animated colors
    "R3dBlu3", // alternate colors
};

// Animation values (callbacks)

color_animation_callback lightmsg_color_value[] = {
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_flat, // flat color
    LightMsg_color_cb_nyancat, // animated colors
    LightMsg_color_cb_rainbow, // animated colors
    LightMsg_color_cb_sparkle, // animated colors
    LightMsg_color_cb_vaporwave, // animated colors
    LightMsg_color_cb_directional, // alternate colors
};

static uint32_t LightMsg_color_pal_nyancat[] = {
    lightmsg_colors_flat[COLOR_RED],
    lightmsg_colors_flat[COLOR_RED],
    lightmsg_colors_flat[COLOR_RED],
    lightmsg_colors_flat[COLOR_ORANGE],
    lightmsg_colors_flat[COLOR_ORANGE],
    lightmsg_colors_flat[COLOR_ORANGE],
    lightmsg_colors_flat[COLOR_YELLOW],
    lightmsg_colors_flat[COLOR_YELLOW],
    lightmsg_colors_flat[COLOR_GREEN],
    lightmsg_colors_flat[COLOR_GREEN],
    lightmsg_colors_flat[COLOR_CYAN],
    lightmsg_colors_flat[COLOR_CYAN],
    lightmsg_colors_flat[COLOR_BLUE],
    lightmsg_colors_flat[COLOR_BLUE],
    lightmsg_colors_flat[COLOR_PURPLE],
    lightmsg_colors_flat[COLOR_PURPLE],
};

static uint32_t LightMsg_color_pal_vaporwave[] = {
    0xf7b900,
    0xf88900,
    0xf95c00,
    0xfa2616,
    0xfb0f33,
    0xfc0080,
    0xfd00a9,
    0xfe00d3,
    0xff00f0,
    0xd200f2,
    0xae00f4,
    0x7700f6,
    0x4720f7,
    0x213ff9,
    0x0074fb,
    0x00d3ff,
};

const LightMsg_Mirror lightmsg_mirror_value[] = {
    LightMsg_MirrorDisabled,
    LightMsg_MirrorEnabled,
};

const char* const lightmsg_mirror_text[] = {
    "Disabled",
    "Enabled",
};

// Speed (ms) to change text being display (0=never, 1 sec, 0.5 sec)
// ERRATA: Now represent a "swipe number"
const uint32_t lightmsg_speed_value[] = {
    UINT32_MAX,
    16,
    8,
    2,
};

const char* const lightmsg_speed_text[] = {"Off", "Slow", "Medium", "Fast"};

// Delay in microseconds (us) to wait after rendering a column
const uint32_t lightmsg_width_value[] = {
    250,
    600,
    1200,
};

const char* const lightmsg_width_text[] = {
    "Narrow",
    "Normal",
    "Wide",
};

static uint8_t sine_wave[256] = {
    0x80, 0x83, 0x86, 0x89, 0x8C, 0x90, 0x93, 0x96, 0x99, 0x9C, 0x9F, 0xA2, 0xA5, 0xA8, 0xAB,
    0xAE, 0xB1, 0xB3, 0xB6, 0xB9, 0xBC, 0xBF, 0xC1, 0xC4, 0xC7, 0xC9, 0xCC, 0xCE, 0xD1, 0xD3,
    0xD5, 0xD8, 0xDA, 0xDC, 0xDE, 0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEB, 0xED, 0xEF, 0xF0,
    0xF1, 0xF3, 0xF4, 0xF5, 0xF6, 0xF8, 0xF9, 0xFA, 0xFA, 0xFB, 0xFC, 0xFD, 0xFD, 0xFE, 0xFE,
    0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFE, 0xFD, 0xFD, 0xFC, 0xFB,
    0xFA, 0xFA, 0xF9, 0xF8, 0xF6, 0xF5, 0xF4, 0xF3, 0xF1, 0xF0, 0xEF, 0xED, 0xEB, 0xEA, 0xE8,
    0xE6, 0xE4, 0xE2, 0xE0, 0xDE, 0xDC, 0xDA, 0xD8, 0xD5, 0xD3, 0xD1, 0xCE, 0xCC, 0xC9, 0xC7,
    0xC4, 0xC1, 0xBF, 0xBC, 0xB9, 0xB6, 0xB3, 0xB1, 0xAE, 0xAB, 0xA8, 0xA5, 0xA2, 0x9F, 0x9C,
    0x99, 0x96, 0x93, 0x90, 0x8C, 0x89, 0x86, 0x83, 0x80, 0x7D, 0x7A, 0x77, 0x74, 0x70, 0x6D,
    0x6A, 0x67, 0x64, 0x61, 0x5E, 0x5B, 0x58, 0x55, 0x52, 0x4F, 0x4D, 0x4A, 0x47, 0x44, 0x41,
    0x3F, 0x3C, 0x39, 0x37, 0x34, 0x32, 0x2F, 0x2D, 0x2B, 0x28, 0x26, 0x24, 0x22, 0x20, 0x1E,
    0x1C, 0x1A, 0x18, 0x16, 0x15, 0x13, 0x11, 0x10, 0x0F, 0x0D, 0x0C, 0x0B, 0x0A, 0x08, 0x07,
    0x06, 0x06, 0x05, 0x04, 0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x02, 0x02, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0F, 0x10, 0x11, 0x13, 0x15, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26,
    0x28, 0x2B, 0x2D, 0x2F, 0x32, 0x34, 0x37, 0x39, 0x3C, 0x3F, 0x41, 0x44, 0x47, 0x4A, 0x4D,
    0x4F, 0x52, 0x55, 0x58, 0x5B, 0x5E, 0x61, 0x64, 0x67, 0x6A, 0x6D, 0x70, 0x74, 0x77, 0x7A,
    0x7D};

// No assert to avoid loosing cycles.
void LightMsg_color_cb_flat(uint16_t tick, bool direction, uint32_t* result, void* ctx) {
    UNUSED(tick);
    UNUSED(direction);
    AppContext* app = ctx;
    // AppData *appData = (AppData*)app->data;
    Configuration* light_msg_data = (Configuration*)app->data->config;
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        result[row] = lightmsg_colors_flat[light_msg_data->color];
    }
}

// No assert to avoid loosing cycles.
void LightMsg_color_cb_directional(uint16_t tick, bool direction, uint32_t* result, void* ctx) {
    UNUSED(ctx);
    UNUSED(tick);
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        result[row] = direction ? lightmsg_colors_flat[COLOR_RED] :
                                  lightmsg_colors_flat[COLOR_BLUE];
    }
}

// No assert to avoid loosing cycles.
void LightMsg_color_cb_nyancat(uint16_t tick, bool direction, uint32_t* result, void* ctx) {
    UNUSED(ctx);
    UNUSED(direction);
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        result[row] = LightMsg_color_pal_nyancat[(row + (tick >> 6)) % LIGHTMSG_LED_ROWS];
    }
}

// No assert to avoid loosing cycles.
void LightMsg_color_cb_vaporwave(uint16_t tick, bool direction, uint32_t* result, void* ctx) {
    UNUSED(ctx);
    UNUSED(tick);
    UNUSED(direction);
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        result[row] = LightMsg_color_pal_vaporwave[row];
    }
}

// No assert to avoid loosing cycles.
void LightMsg_color_cb_sparkle(uint16_t tick, bool direction, uint32_t* result, void* ctx) {
    UNUSED(ctx);
    UNUSED(direction);
    // XorShift randomness
    uint8_t rand = tick;
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        rand ^= (rand << 3);
        rand ^= rand >> 5;
        rand ^= rand << 4;
        result[row] = rand + (rand << 8) + (rand << 16);
    }
}

// No assert to avoid loosing cycles.
void LightMsg_color_cb_rainbow(uint16_t tick, bool direction, uint32_t* result, void* ctx) {
    UNUSED(ctx);
    UNUSED(direction);
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        uint8_t r = (uint8_t)(sine_wave[(tick) % 255]);
        uint8_t b = (uint8_t)(sine_wave[(tick + 85) % 255]);
        uint8_t g = (uint8_t)(sine_wave[(tick + 170) % 255]);
        result[row] = (r << 16) | (g << 8) | b;
    }
}

/**
 * @brief Update leds based on the current shader selected
 *
 * @param ctx The context passed to the callback.
 */
static void update_led(void* ctx) {
    UNUSED(ctx);
    AppContext* app = ctx;
    AppData* appData = (AppData*)app->data;
    Configuration* light_msg_data = (Configuration*)appData->config;
    uint32_t leds[LIGHTMSG_LED_ROWS] = {0};
    color_animation_callback led_cb = (color_animation_callback)appData->shader;
    // Retrieve colors from the configured callback
    led_cb(update_counter << 2, update_counter % 16 > 7, leds, ctx);
    double brightness = lightmsg_brightness_value[light_msg_data->brightness];
    for(size_t row = 0; row < LIGHTMSG_LED_ROWS; ++row) {
        uint32_t color = leds[row];
        SK6805_set_led_color(
            row,
            (uint8_t)(((color >> 16) & 0xFF) * brightness),
            (uint8_t)(((color >> 8) & 0xFF) * brightness),
            (uint8_t)(((color) & 0xFF) * brightness));
    }
    SK6805_update();
}

/**
 * @brief Update led according to update_counter
 *
 * @param ctx The context passed to the callback.
 */
static void on_tick(void* ctx) {
    AppContext* app = ctx;
    update_counter++;
    update_led(app);
}

/**
 * @brief Callback function to handle orientation menu
 *
 * @param item Selected item
 */
void on_change_orientation(VariableItem* item) {
    // SystemSettings* app = variable_item_get_context(item);
    AppContext* app = variable_item_get_context(item);
    AppData* appData = (AppData*)app->data;
    Configuration* light_msg_data = (Configuration*)appData->config;

    uint8_t index = variable_item_get_current_value_index(item);
    light_msg_data->orientation = lightmsg_orientation_value[index];
    variable_item_set_current_value_text(item, lightmsg_orientation_text[index]);
    // light_msg_data->orientation = lightmsg_orientation_value[index];
}

/**
 * @brief Callback function to handle brightness menu
 *
 * @param item Selected item
 */
void on_change_brightness(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* light_msg_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    light_msg_data->brightness = index;

    variable_item_set_current_value_text(item, lightmsg_brightness_text[index]);
}

/**
 * @brief Callback function to handle sensitivity menu
 *
 * @param item Selected item
 */
void on_change_sensitivity(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* light_msg_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    light_msg_data->sensitivity = index;

    variable_item_set_current_value_text(item, lightmsg_sensitivity_text[index]);
}

/**
 * @brief Callback function to handle color menu
 *
 * @param item Selected item
 */
void on_change_color(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    AppData* appData = (AppData*)app->data;
    Configuration* light_msg_data = (Configuration*)app->data->config;

    uint8_t index = variable_item_get_current_value_index(item);

    light_msg_data->color = index;
    appData->shader = lightmsg_color_value[index];
    variable_item_set_current_value_text(item, lightmsg_color_text[index]);
}

void on_change_mirror(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* light_msg_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    light_msg_data->mirror = lightmsg_mirror_value[index] == LightMsg_MirrorEnabled;
    variable_item_set_current_value_text(item, lightmsg_mirror_text[index]);
}

void on_change_speed(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* light_msg_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    light_msg_data->speed = index;
    variable_item_set_current_value_text(item, lightmsg_speed_text[index]);
}

void on_change_width(VariableItem* item) {
    AppContext* app = variable_item_get_context(item);
    Configuration* light_msg_data = (Configuration*)app->data->config;
    uint8_t index = variable_item_get_current_value_index(item);
    light_msg_data->width = index;
    variable_item_set_current_value_text(item, lightmsg_width_text[index]);
}

/**
 * @brief Allocate memory and initialize the app configuration.
 * @param ctx The application context.
 * @return Returns a pointer to the allocated AppConfig.
 */

AppConfig* app_config_alloc(void* ctx) {
    FURI_LOG_I(TAG, "app_config_alloc");
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppConfig* appConfig = malloc(sizeof(AppConfig));
    Configuration* light_msg_data = (Configuration*)app->data->config;
    appConfig->timer = furi_timer_alloc(on_tick, FuriTimerTypePeriodic, app);
    appConfig->list = variable_item_list_alloc();
    variable_item_list_reset(appConfig->list);

    // debug_config(light_msg_data);
    VariableItem* item = variable_item_list_add(
        appConfig->list,
        "Orientation",
        COUNT_OF(lightmsg_orientation_text),
        on_change_orientation,
        app);
    variable_item_set_current_value_index(item, light_msg_data->orientation); // 0,10,20,30,...
    variable_item_set_current_value_text(
        item, lightmsg_orientation_text[light_msg_data->orientation]);
// Activate the brightness menu or not.
#if PARAM_BRIGHTNESS_CONFIG_EN == 1
    item = variable_item_list_add(
        appConfig->list,
        "Brightness",
        COUNT_OF(lightmsg_brightness_text),
        on_change_brightness,
        app);
    variable_item_set_current_value_index(item, light_msg_data->brightness); // 0,10,20,30,...
    variable_item_set_current_value_text(
        item, lightmsg_brightness_text[light_msg_data->brightness]);
#endif
    item = variable_item_list_add(
        appConfig->list,
        "Sensitivity",
        COUNT_OF(lightmsg_sensitivity_text),
        on_change_sensitivity,
        app);
    variable_item_set_current_value_index(item, light_msg_data->sensitivity); // 0,10,20,30,...
    variable_item_set_current_value_text(
        item, lightmsg_sensitivity_text[light_msg_data->sensitivity]);

    item = variable_item_list_add(
        appConfig->list, "Color", COUNT_OF(lightmsg_color_text), on_change_color, app);
    variable_item_set_current_value_index(item, light_msg_data->color); // 0,10,20,30,...
    variable_item_set_current_value_text(item, lightmsg_color_text[light_msg_data->color]);

    item = variable_item_list_add(
        appConfig->list, "Mirror", COUNT_OF(lightmsg_mirror_text), on_change_mirror, app);
    variable_item_set_current_value_index(item, light_msg_data->mirror ? 1 : 0);
    variable_item_set_current_value_text(
        item, lightmsg_mirror_text[light_msg_data->mirror ? 1 : 0]);

    item = variable_item_list_add(
        appConfig->list, "Letter width", COUNT_OF(lightmsg_width_text), on_change_width, app);
    variable_item_set_current_value_index(item, light_msg_data->width);
    variable_item_set_current_value_text(item, lightmsg_width_text[light_msg_data->width]);

    item = variable_item_list_add(
        appConfig->list, "Word by Word", COUNT_OF(lightmsg_speed_text), on_change_speed, app);
    variable_item_set_current_value_index(item, light_msg_data->speed);
    variable_item_set_current_value_text(item, lightmsg_speed_text[light_msg_data->speed]);

    return appConfig;
}

/**
 * @brief Free the memory occupied by the app configuration.
 *
 * @param appConfig The AppConfig to be freed.
 */
void app_config_free(AppConfig* appConfig) {
    furi_assert(appConfig);
    variable_item_list_free(appConfig->list);
    furi_timer_free(appConfig->timer);
    free(appConfig);
}

/**
 * @brief Get the view associated with the app configuration.
 *
 * @param appConfig The AppConfig for which the view is to be fetched.
 * @return Returns a pointer to the View.
 */
View* app_config_get_view(AppConfig* appConfig) {
    furi_assert(appConfig);
    return variable_item_list_get_view(appConfig->list);
}

/**
 * @brief Callback when the app configuration scene is entered.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_config_on_enter(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppConfig* appConfig = (AppConfig*)app->sceneConfig;
    furi_check(
        furi_timer_start(appConfig->timer, (furi_kernel_get_tick_frequency() / 100)) ==
        FuriStatusOk);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewConfig);
}

/**
 * @brief Handle scene manager events for the app configuration scene.
 *
 * @param context The context passed to the callback.
 * @param event The scene manager event.
 * @return Returns true if the event was consumed, otherwise false.
 */
bool app_scene_config_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

/**
 * @brief Callback when the app configuration scene is exited.
 *
 * @param ctx The context passed to the callback.
 */
void app_scene_config_on_exit(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppConfig* appConfig = (AppConfig*)app->sceneConfig;
    furi_timer_stop(appConfig->timer);
    SK6805_off();

    l401_err res = config_save_json(LIGHTMSGCONF_CONFIG_FILE, app->data->config);
    if(res == L401_OK) {
        FURI_LOG_I(TAG, "Successfully saved configuration to %s.", LIGHTMSGCONF_CONFIG_FILE);
    } else {
        FURI_LOG_E(TAG, "Error while saving to %s: %d", LIGHTMSGCONF_CONFIG_FILE, res);
    }

    SK6805_off();
}
