#include "app.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Prototypes /////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static struct gpio_explorer_app_struct * gpio_explorer_app_alloc();
static void gpio_explorer_app_free(struct gpio_explorer_app_struct * app);
static uint32_t gpio_explorer_navigation_exit_callback(void* _context);
static uint32_t gpio_explorer_navigation_submenu_callback(void* _context);
static void gpio_explorer_submenu_callback(void* context, uint32_t index);
static void gpio_explorer_rgb_setting_1_change(VariableItem* item);
static void gpio_explorer_rgb_setting_2_change(VariableItem* item);
static void gpio_explorer_rgb_setting_3_change(VariableItem* item);
static void gpio_explorer_led_setting_1_change(VariableItem* item);
static uint32_t gpio_explorer_rgb_navigation_submenu_callback(void* _context);
static void view_rgb_init(struct gpio_explorer_app_struct *);
static void gpio_explorer_view_rgb_draw_callback(Canvas* canvas, void* model);
static bool gpio_explorer_view_rgb_input_callback(InputEvent* event, void* context);
static uint32_t gpio_explorer_led_navigation_submenu_callback(void* _context);
static void view_led_init(struct gpio_explorer_app_struct * app);
static void gpio_explorer_view_led_draw_callback(Canvas* canvas, void* model);
static bool gpio_explorer_view_led_input_callback(InputEvent* event, void* context);
static uint32_t gpio_explorer_gpio_reader_navigation_submenu_callback(void* _context);
static void view_gpio_reader_init(struct gpio_explorer_gpio_reader_struct* gpio_reader_model);
static void gpio_explorer_view_gpio_reader_draw_callback(Canvas* canvas, void* model);
static bool gpio_explorer_view_gpio_reader_input_callback(InputEvent* event, void* context);
static void gpio_explorer_view_gpio_reader_timer_callback(void* context);
static void gpio_explorer_view_gpio_reader_enter_callback(void* context);
static void gpio_explorer_view_gpio_reader_exit_callback(void* context);
static void set_rgb_light_color(uint8_t color_index, struct gpio_explorer_app_struct * app);
static void gpio_reader_switch_pin(struct gpio_explorer_gpio_reader_struct* model);

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Main function //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

int32_t main_gpio_explorer_app(void* _p) {
    UNUSED(_p);

    struct gpio_explorer_app_struct * app = gpio_explorer_app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    gpio_explorer_app_free(app);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Memory functions ///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static struct gpio_explorer_app_struct * gpio_explorer_app_alloc() {
    struct gpio_explorer_app_struct * app = malloc(sizeof(struct gpio_explorer_app_struct ));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    ////////////////////////////////////////////////////////////////
    ////////Allocates memory for Submenu view //////////////////////
    ////////////////////////////////////////////////////////////////

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu,
        "Config",
        GPIOExplorerSubmenuIndexConfigure,
        gpio_explorer_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "LED", GPIOExplorerSubmenuIndexLED, gpio_explorer_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "RGB Light",
        GPIOExplorerSubmenuIndexRGBLignt,
        gpio_explorer_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "GPIO Reader",
        GPIOExplorerSubmenuIndexGPIORearder,
        gpio_explorer_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", GPIOExplorerSubmenuIndexAbout, gpio_explorer_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), gpio_explorer_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, GPIOExplorerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, GPIOExplorerViewSubmenu);

    ////////////////////////////////////////////////////////////////
    ////////Allocates memory for Configuration view ////////////////
    ////////////////////////////////////////////////////////////////

    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    VariableItem* rgb_r_item = variable_item_list_add(
        app->variable_item_list_config,
        PIN_R_CONFIG_LABEL,
        COUNT_OF(rgb_setting_pins),
        gpio_explorer_rgb_setting_1_change,
        app);
    VariableItem* rgb_g_item = variable_item_list_add(
        app->variable_item_list_config,
        PIN_G_CONFIG_LABEL,
        COUNT_OF(rgb_setting_pins),
        gpio_explorer_rgb_setting_2_change,
        app);
    VariableItem* rgb_b_item = variable_item_list_add(
        app->variable_item_list_config,
        PIN_B_CONFIG_LABEL,
        COUNT_OF(rgb_setting_pins),
        gpio_explorer_rgb_setting_3_change,
        app);
    VariableItem* led_pin_item = variable_item_list_add(
        app->variable_item_list_config,
        PIN_LED_CONFIG_LABEL,
        COUNT_OF(rgb_setting_pins),
        gpio_explorer_led_setting_1_change,
        app);

        app->configuration_settings = malloc(sizeof(struct gpio_explorer_configure_struct));

    if(configuration_file_init()){
        if(!configuration_file_read(app->configuration_settings)) assert(0);
    }else{  
        app->configuration_settings->rgb_pin_r_index = DEFAULT_RGB_R_PIN;
        app->configuration_settings->rgb_pin_g_index = DEFAULT_RGB_G_PIN;
        app->configuration_settings->rgb_pin_b_index = DEFAULT_RGB_B_PIN;
        app->configuration_settings->led_pin_index = DEFAULT_LED_PIN;
        if(!configuration_file_write(app->configuration_settings)) assert(0);
    }
    variable_item_set_current_value_index(rgb_r_item, app->configuration_settings->rgb_pin_r_index);
    variable_item_set_current_value_text(rgb_r_item, rgb_setting_pins[app->configuration_settings->rgb_pin_r_index]);
    variable_item_set_current_value_index(rgb_g_item, app->configuration_settings->rgb_pin_g_index);
    variable_item_set_current_value_text(rgb_g_item, rgb_setting_pins[app->configuration_settings->rgb_pin_g_index]);
    variable_item_set_current_value_index(rgb_b_item, app->configuration_settings->rgb_pin_b_index);
    variable_item_set_current_value_text(rgb_b_item, rgb_setting_pins[app->configuration_settings->rgb_pin_b_index]);
    variable_item_set_current_value_index(led_pin_item, app->configuration_settings->led_pin_index);
    variable_item_set_current_value_text(led_pin_item, rgb_setting_pins[app->configuration_settings->led_pin_index]);

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        gpio_explorer_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        GPIOExplorerViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    ////////////////////////////////////////////////////////////////
    ////////Allocates memory for RGB view //////////////////////////
    ////////////////////////////////////////////////////////////////

    app->view_rgb = view_alloc();
    view_set_draw_callback(app->view_rgb, gpio_explorer_view_rgb_draw_callback);
    view_set_input_callback(app->view_rgb, gpio_explorer_view_rgb_input_callback);
    view_set_previous_callback(app->view_rgb, gpio_explorer_rgb_navigation_submenu_callback);
    view_set_context(app->view_rgb, app);
    view_allocate_model(app->view_rgb, ViewModelTypeLockFree, sizeof(struct gpio_explorer_rgb_struct));
    struct gpio_explorer_rgb_struct* rgb_model = view_get_model(app->view_rgb);
    rgb_model->rgb_pins_state = OFF;
    rgb_model->color_index = 0;
    view_dispatcher_add_view(app->view_dispatcher, GPIOExplorerViewRGBLight, app->view_rgb);

    ////////////////////////////////////////////////////////////////
    ////////Allocates memory for LED view //////////////////////////
    ////////////////////////////////////////////////////////////////

    app->view_led = view_alloc();
    view_set_draw_callback(app->view_led, gpio_explorer_view_led_draw_callback);
    view_set_input_callback(app->view_led, gpio_explorer_view_led_input_callback);
    view_set_previous_callback(app->view_led, gpio_explorer_led_navigation_submenu_callback);
    view_set_context(app->view_led, app);
    view_allocate_model(app->view_led, ViewModelTypeLockFree, sizeof(struct gpio_explorer_led_struct));
    struct gpio_explorer_led_struct* led_model = view_get_model(app->view_led);
    led_model->led_pin_state = OFF;
    led_model->led_pin_index = app->configuration_settings->led_pin_index;
    view_dispatcher_add_view(app->view_dispatcher, GPIOExplorerViewLED, app->view_led);

    ////////////////////////////////////////////////////////////////
    ////////Allocates memory for GPIO Reader view //////////////////
    ////////////////////////////////////////////////////////////////

    app->view_gpio_reader = view_alloc();
    view_set_draw_callback(app->view_gpio_reader, gpio_explorer_view_gpio_reader_draw_callback);
    view_set_input_callback(app->view_gpio_reader, gpio_explorer_view_gpio_reader_input_callback);
    view_set_previous_callback(
        app->view_gpio_reader, gpio_explorer_gpio_reader_navigation_submenu_callback);
    view_set_enter_callback(app->view_gpio_reader, gpio_explorer_view_gpio_reader_enter_callback);
    view_set_exit_callback(app->view_gpio_reader, gpio_explorer_view_gpio_reader_exit_callback);
    view_set_context(app->view_gpio_reader, app);
    view_allocate_model(
        app->view_gpio_reader, ViewModelTypeLockFree, sizeof(struct gpio_explorer_gpio_reader_struct));
    struct gpio_explorer_gpio_reader_struct* gpio_reader_model = view_get_model(app->view_gpio_reader);
    gpio_reader_model->curr_pin_index = 0;
    view_dispatcher_add_view(
        app->view_dispatcher, GPIOExplorerViewGPIOReader, app->view_gpio_reader);

    ////////////////////////////////////////////////////////////////
    ////////Allocates memory for About view ////////////////////////
    ////////////////////////////////////////////////////////////////

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "GPIO Explorer\n---\nThis is the most complete\napp to start exploring the\nGPIO functionalities.\n---\nYou can configure the pins\nthat you want to use from\nthe configure menu.\n---\nNOTE: ALWAYS PUT\nAT LEAST 100 Ohm\nRESISTORS BEFORE\nYOU CONNECT THE LEDS.\n---\nauthor: dun-crop\nGitHub: https://github.com/EvgeniGenchev07/\ngpio_explorer\nDiscord: @dun-crop");
    view_set_previous_callback(
        widget_get_view(app->widget_about), gpio_explorer_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, GPIOExplorerViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    return app;
}

static void gpio_explorer_app_free(struct gpio_explorer_app_struct * app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, GPIOExplorerViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, GPIOExplorerViewRGBLight);
    view_free(app->view_rgb);
    view_dispatcher_remove_view(app->view_dispatcher, GPIOExplorerViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, GPIOExplorerViewLED);
    view_free(app->view_led);
    view_dispatcher_remove_view(app->view_dispatcher, GPIOExplorerViewGPIOReader);
    view_free(app->view_gpio_reader);
    view_dispatcher_remove_view(app->view_dispatcher, GPIOExplorerViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free_storage_manager();
    free(app);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Navigation functions ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t gpio_explorer_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

static uint32_t gpio_explorer_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return GPIOExplorerViewSubmenu;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Submenu functions /////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static void gpio_explorer_submenu_callback(void* context, uint32_t index) {
    struct gpio_explorer_app_struct * app = context;
    switch(index) {
    case GPIOExplorerSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, GPIOExplorerViewConfigure);
        break;
    case GPIOExplorerSubmenuIndexRGBLignt:
        view_rgb_init(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, GPIOExplorerViewRGBLight);
        break;
    case GPIOExplorerSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, GPIOExplorerViewAbout);
        break;
    case GPIOExplorerSubmenuIndexLED:
        view_led_init(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, GPIOExplorerViewLED);
        break;
    case GPIOExplorerSubmenuIndexGPIORearder:
        view_gpio_reader_init(view_get_model(app->view_gpio_reader));
        view_dispatcher_switch_to_view(app->view_dispatcher, GPIOExplorerViewGPIOReader);
        break;
    default:
        break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Configuration functions ////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static void gpio_explorer_rgb_setting_1_change(VariableItem* item) {
    struct gpio_explorer_app_struct * app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_setting_pins[index]);
    uint8_t pin_r_index = app->configuration_settings->rgb_pin_r_index;
    if(pin_r_index != app->configuration_settings->rgb_pin_b_index &&
       pin_r_index != app->configuration_settings->rgb_pin_g_index) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], OFF);
        furi_hal_gpio_init_simple(
            pins[app->configuration_settings->rgb_pin_r_index], GpioModeAnalog);
    }
    app->configuration_settings->rgb_pin_r_index = index;
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->rgb_pin_r_index], GpioModeOutputPushPull);
    configuration_file_write(app->configuration_settings);
}

static void gpio_explorer_rgb_setting_2_change(VariableItem* item) {
    struct gpio_explorer_app_struct * app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_setting_pins[index]);
    uint8_t pin_g_index = app->configuration_settings->rgb_pin_g_index;
    if(pin_g_index != app->configuration_settings->rgb_pin_b_index &&
       pin_g_index != app->configuration_settings->rgb_pin_r_index) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], OFF);
        furi_hal_gpio_init_simple(
            pins[app->configuration_settings->rgb_pin_g_index], GpioModeAnalog);
    }
    app->configuration_settings->rgb_pin_g_index = index;
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->rgb_pin_g_index], GpioModeOutputPushPull);
    configuration_file_write(app->configuration_settings);
}

static void gpio_explorer_rgb_setting_3_change(VariableItem* item) {
    struct gpio_explorer_app_struct * app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_setting_pins[index]);
    uint8_t pin_b_index = app->configuration_settings->rgb_pin_b_index;
    if(pin_b_index != app->configuration_settings->rgb_pin_g_index &&
       pin_b_index != app->configuration_settings->rgb_pin_r_index) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], OFF);
        furi_hal_gpio_init_simple(
            pins[app->configuration_settings->rgb_pin_b_index], GpioModeAnalog);
    }
    app->configuration_settings->rgb_pin_b_index = index;
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->rgb_pin_b_index], GpioModeOutputPushPull);
    configuration_file_write(app->configuration_settings);
}

static void gpio_explorer_led_setting_1_change(VariableItem* item) {
    struct gpio_explorer_app_struct * app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_setting_pins[index]);
    furi_hal_gpio_write(pins[app->configuration_settings->led_pin_index], OFF);
    furi_hal_gpio_init_simple(pins[app->configuration_settings->led_pin_index], GpioModeAnalog);
    app->configuration_settings->led_pin_index = index;
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->led_pin_index], GpioModeOutputPushPull);
    ((struct gpio_explorer_led_struct*)view_get_model(app->view_led))->led_pin_index = index;
    configuration_file_write(app->configuration_settings);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// RGB functions //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t gpio_explorer_rgb_navigation_submenu_callback(void* _context) {
    struct gpio_explorer_app_struct * app = _context;
    furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], OFF);
    furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], OFF);
    furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], OFF);
    struct gpio_explorer_rgb_struct* rgb_struct = view_get_model(app->view_rgb);
    rgb_struct->rgb_pins_state = OFF;
    furi_hal_gpio_init_simple(pins[app->configuration_settings->rgb_pin_r_index], GpioModeAnalog);
    furi_hal_gpio_init_simple(pins[app->configuration_settings->rgb_pin_g_index], GpioModeAnalog);
    furi_hal_gpio_init_simple(pins[app->configuration_settings->rgb_pin_b_index], GpioModeAnalog);
    return gpio_explorer_navigation_submenu_callback(_context);
}

static void view_rgb_init(struct gpio_explorer_app_struct * app) {
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->rgb_pin_r_index], GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->rgb_pin_g_index], GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->rgb_pin_b_index], GpioModeOutputPushPull);
}

static void gpio_explorer_view_rgb_draw_callback(Canvas* canvas, void* model) {
    struct gpio_explorer_rgb_struct* my_model = model;
    canvas_draw_str(canvas, 1, 25, "Press < or > to change the color");
    FuriString* xstr = furi_string_alloc();
    furi_string_printf(xstr, "%s", rgb_colors[my_model->color_index]);
    canvas_draw_str(canvas, 50, 40, furi_string_get_cstr(xstr));
    furi_string_printf(xstr, "Press Ok to %s", my_model->rgb_pins_state == 0 ? "Start" : "Stop");
    canvas_draw_str(canvas, 25, 60, furi_string_get_cstr(xstr));
    furi_string_free(xstr);
}

static bool gpio_explorer_view_rgb_input_callback(InputEvent* event, void* context) {
    struct gpio_explorer_app_struct * app = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            bool redraw = true;
            with_view_model(
                app->view_rgb,
                struct gpio_explorer_rgb_struct * model,
                {
                    if(model->color_index > 0)
                        model->color_index--;
                    else
                        model->color_index = COUNT_OF(rgb_colors) - 1;
                    if(model->rgb_pins_state) set_rgb_light_color(model->color_index, app);
                },
                redraw);
        } else if(event->key == InputKeyRight) {
            bool redraw = true;
            with_view_model(
                app->view_rgb,
                struct gpio_explorer_rgb_struct * model,
                {
                    if(COUNT_OF(rgb_colors) - 1 > model->color_index)
                        model->color_index++;
                    else
                        model->color_index = 0;
                    if(model->rgb_pins_state) set_rgb_light_color(model->color_index, app);
                },
                redraw);
        }
    } else if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            bool redraw = true;
            with_view_model(
                app->view_rgb,
                struct gpio_explorer_rgb_struct * _model,
                {
                    uint8_t rgb_pins_state = _model->rgb_pins_state;
                    if(rgb_pins_state == ON) {
                        _model->rgb_pins_state = OFF;
                        furi_hal_gpio_write(
                            pins[app->configuration_settings->rgb_pin_r_index], OFF);
                        furi_hal_gpio_write(
                            pins[app->configuration_settings->rgb_pin_g_index], OFF);
                        furi_hal_gpio_write(
                            pins[app->configuration_settings->rgb_pin_b_index], OFF);
                    } else {
                        _model->rgb_pins_state = ON;
                        uint8_t color_index = _model->color_index;
                        set_rgb_light_color(color_index, app);
                    }
                },
                redraw);
            return true;
        }
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// LED functions //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t gpio_explorer_led_navigation_submenu_callback(void* _context) {
    struct gpio_explorer_app_struct * app = _context;
    furi_hal_gpio_write(pins[app->configuration_settings->led_pin_index], OFF);
    struct gpio_explorer_led_struct* led_struct = view_get_model(app->view_led);
    led_struct->led_pin_state = OFF;
    furi_hal_gpio_init_simple(pins[app->configuration_settings->led_pin_index], GpioModeAnalog);
    return gpio_explorer_navigation_submenu_callback(_context);
}

static void view_led_init(struct gpio_explorer_app_struct * app) {
    furi_hal_gpio_init_simple(
        pins[app->configuration_settings->led_pin_index], GpioModeOutputPushPull);
}

static void gpio_explorer_view_led_draw_callback(Canvas* canvas, void* model) {
    struct gpio_explorer_led_struct* my_model = model;
    FuriString* xstr = furi_string_alloc();
    furi_string_printf(xstr, "Pin state: %s", my_model->led_pin_state == 0 ? "LOW" : "HIGH");
    canvas_draw_str(canvas, 30, 15, furi_string_get_cstr(xstr));
    furi_string_printf(xstr, "Pin: %s", rgb_setting_pins[my_model->led_pin_index]);
    canvas_draw_str(canvas, 43, 35, furi_string_get_cstr(xstr));
    furi_string_printf(xstr, "Press Ok to %s", my_model->led_pin_state == 0 ? "Start" : "Stop");
    canvas_draw_str(canvas, 25, 60, furi_string_get_cstr(xstr));
    furi_string_free(xstr);
}

static bool gpio_explorer_view_led_input_callback(InputEvent* event, void* context) {
    struct gpio_explorer_app_struct * app = context;
    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            bool redraw = true;
            with_view_model(
                app->view_led,
                struct gpio_explorer_led_struct * model,
                {
                    if(model->led_pin_state) {
                        furi_hal_gpio_write(pins[model->led_pin_index], OFF);
                        model->led_pin_state = OFF;
                    } else {
                        furi_hal_gpio_write(pins[model->led_pin_index], ON);
                        model->led_pin_state = ON;
                    }
                },
                redraw);
        }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// GPIO Reader functions //////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t gpio_explorer_gpio_reader_navigation_submenu_callback(void* _context) {
    struct gpio_explorer_app_struct * app = _context;
    furi_hal_gpio_init_simple(
        pins[((struct gpio_explorer_gpio_reader_struct*)view_get_model(app->view_gpio_reader))->curr_pin_index],
        GpioModeAnalog);

    return gpio_explorer_navigation_submenu_callback(_context);
}

static void view_gpio_reader_init(struct gpio_explorer_gpio_reader_struct* gpio_reader_model) {
    furi_hal_gpio_init(
        pins[gpio_reader_model->curr_pin_index], GpioModeInput, GpioPullDown, GpioSpeedHigh);
    gpio_reader_model->curr_pin_state =
        furi_hal_gpio_read(pins[gpio_reader_model->curr_pin_index]);
}

static void gpio_explorer_view_gpio_reader_draw_callback(Canvas* canvas, void* model) {
    struct gpio_explorer_gpio_reader_struct* my_model = model;
    FuriString* xstr = furi_string_alloc();
    furi_string_printf(xstr, "Pin state: %s", my_model->curr_pin_state == 0 ? "LOW" : "HIGH");
    canvas_draw_str(canvas, 30, 15, furi_string_get_cstr(xstr));
    furi_string_printf(xstr, "Pin: %s", rgb_setting_pins[my_model->curr_pin_index]);
    canvas_draw_str(canvas, 43, 35, furi_string_get_cstr(xstr));
    canvas_draw_str(canvas, 1, 60, "Press < or > to change the pin");
    furi_string_free(xstr);
}

static bool gpio_explorer_view_gpio_reader_input_callback(InputEvent* event, void* context) {
    struct gpio_explorer_app_struct * app = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            bool redraw = true;
            with_view_model(
                app->view_gpio_reader,
                struct gpio_explorer_gpio_reader_struct * model,
                {
                    if(model->curr_pin_index > 0)
                        model->curr_pin_index--;
                    else
                        model->curr_pin_index = COUNT_OF(rgb_setting_pins) - 1;
                    gpio_reader_switch_pin(model);
                },
                redraw);
        } else if(event->key == InputKeyRight) {
            bool redraw = true;
            with_view_model(
                app->view_gpio_reader,
                struct gpio_explorer_gpio_reader_struct * model,
                {
                    if(COUNT_OF(rgb_setting_pins) - 1 > model->curr_pin_index)
                        model->curr_pin_index++;
                    else
                        model->curr_pin_index = 0;
                    gpio_reader_switch_pin(model);
                },
                redraw);
        }
    }
    return false;
}

static void gpio_explorer_view_gpio_reader_timer_callback(void* context) {
    struct gpio_explorer_app_struct * app = context;
    bool redraw = true;
    with_view_model(
        app->view_gpio_reader,
        struct gpio_explorer_gpio_reader_struct * model,
        { model->curr_pin_state = furi_hal_gpio_read(pins[model->curr_pin_index]); },
        redraw);
}

static void gpio_explorer_view_gpio_reader_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(200);
    struct gpio_explorer_app_struct * app = context;
    furi_assert(app->timer == NULL);
    app->timer = furi_timer_alloc(
        gpio_explorer_view_gpio_reader_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, period);
}

static void gpio_explorer_view_gpio_reader_exit_callback(void* context) {
    struct gpio_explorer_app_struct * app = context;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// Helper functions ///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static void set_rgb_light_color(uint8_t color_index, struct gpio_explorer_app_struct * app) {
    if(color_index == 0) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], ON);
    } else if(color_index == 1) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], OFF);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], OFF);
    } else if(color_index == 2) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], OFF);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], OFF);
    } else if(color_index == 3) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], OFF);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], OFF);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], ON);
    } else if(color_index == 4) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], OFF);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], ON);
    } else if(color_index == 5) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], OFF);
    } else if(color_index == 6) {
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_r_index], OFF);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_g_index], ON);
        furi_hal_gpio_write(pins[app->configuration_settings->rgb_pin_b_index], ON);
    }
}

static void gpio_reader_switch_pin(struct gpio_explorer_gpio_reader_struct* model) {
    furi_hal_gpio_init_simple(pins[model->curr_pin_index], GpioModeAnalog);
    furi_hal_gpio_init(pins[model->curr_pin_index], GpioModeInput, GpioPullDown, GpioSpeedHigh);
    model->curr_pin_state = furi_hal_gpio_read(pins[model->curr_pin_index]);
}

