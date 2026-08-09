#include "../app_user.h"

// Thread for the about us
int32_t about_us_thread(void* context);

// Function for the testing scene on enter
void app_scene_about_us_on_enter(void* context) {
    App* app = (App*)context;

    // Allocate and start the thread
    app->thread_alternative = furi_thread_alloc_ex("About Us", 3 * 1024, about_us_thread, app);
    furi_thread_start(app->thread_alternative);

    // Reset the widget and switch view
    widget_reset(app->widget);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_about_us_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_about_us_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);

    // Join and free the thread
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
}

/**
 * Draws to display
 */

void draw_present(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_multiline_element(
        app->widget, 65, 40, AlignCenter, AlignCenter, FontPrimary, "ELECTRONIC CATS \n Presents:");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", NULL, NULL);
}

void draw_ethernet_app_view(App* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 65, 20, AlignCenter, AlignCenter, FontPrimary, APP_NAME);
    widget_add_string_element(
        app->widget, 65, 35, AlignCenter, AlignCenter, FontSecondary, APP_VERSION);

    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", NULL, NULL);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", NULL, NULL);
}

void draw_license_view(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 65, 20, AlignCenter, AlignCenter, FontPrimary, "MIT LICENSE");
    widget_add_string_element(
        app->widget, 65, 35, AlignCenter, AlignCenter, FontSecondary, "Copyright(c) 2025");
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", NULL, NULL);
}

void draw_text_one(App* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        64,
        25,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Hi! We are\nElectronic Cats Team\nEnjoy the\nEthernet App");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", NULL, NULL);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", NULL, NULL);
}

void draw_store(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 10, 5, &I_store);
    widget_add_string_multiline_element(
        app->widget, 85, 27, AlignCenter, AlignCenter, FontPrimary, "Visit us\n<--");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", NULL, NULL);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", NULL, NULL);
}

void draw_github(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 10, 5, &I_github);
    widget_add_string_multiline_element(
        app->widget, 85, 27, AlignCenter, AlignCenter, FontPrimary, "Repo in\n<--");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", NULL, NULL);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", NULL, NULL);
}

void draw_credits_to(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 10, 5, &I_credits_to);
    widget_add_string_multiline_element(
        app->widget,
        88,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Credits to\nElectronic Cats\n&\nAdonai Diaz");
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", NULL, NULL);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", NULL, NULL);
}

int32_t about_us_thread(void* context) {
    App* app = (App*)context;

    uint8_t counter = 0;
    uint8_t total_count = 7;

    UNUSED(app);

    bool write_once = true;

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(!furi_hal_gpio_read(&gpio_button_left)) {
            while(!furi_hal_gpio_read(&gpio_button_left))
                furi_delay_ms(1);

            if(counter == 0)
                counter = 0;
            else
                counter--;

            write_once = true;
        }

        if(!furi_hal_gpio_read(&gpio_button_right)) {
            while(!furi_hal_gpio_read(&gpio_button_right))
                furi_delay_ms(1);

            counter++;

            if(counter >= total_count - 1) counter = total_count - 1;

            write_once = true;
        }

        if(write_once) {
            switch(counter) {
            case 0:
                draw_present(app);
                break;

            case 1:
                draw_ethernet_app_view(app);
                break;

            case 2:
                draw_text_one(app);
                break;

            case 3:
                draw_store(app);
                break;

            case 4:
                draw_github(app);
                break;

            case 5:
                draw_credits_to(app);
                break;

            case 6:
                draw_license_view(app);

            default:
                break;
            }

            write_once = false;
        }

        furi_delay_ms(1);
    }

    return 0;
}
