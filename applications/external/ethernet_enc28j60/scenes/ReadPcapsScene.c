#include "../app_user.h"

// Variables to save the positions and the count of packets.
// F0.7 — capacity exposed as a constant so pcap_scan can bounds-check.
#define PACKET_POSITIONS_MAX 2000
uint32_t packet_count = 0;
uint64_t packet_positions[PACKET_POSITIONS_MAX] = {0};

// Function for the thread
int32_t thread_read_pcaps(void* context);

// if the file couldnt be read.
void draw_could_not_be_read(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Couldnt open file");
}

// Function for the testing scene on enter
void app_scene_read_pcap_on_enter(void* context) {
    App* app = (App*)context;

    // watch if the pcap function works
    packet_count = pcap_scan(
        app->file, furi_string_get_cstr(app->path), packet_positions, PACKET_POSITIONS_MAX);

    // Allocate and start the thread
    app->thread_alternative =
        furi_thread_alloc_ex("PCAP reader", 10 * 1024, thread_read_pcaps, app);
    furi_thread_start(app->thread_alternative);

    // Reset the widget and switch view
    text_box_reset(app->text_box);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_font(app->text_box, TextBoxFontText);

    furi_string_reset(app->text);

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text));

    view_dispatcher_switch_to_view(app->view_dispatcher, TextBoxView);
}

// Function for the testing scene on event
bool app_scene_read_pcap_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 1:
            text_box_set_text(app->text_box, furi_string_get_cstr(app->text));
            consumed = true;
            break;

        default:
            break;
        }
    }

    return consumed;
}

// Function for the testing scene on exit
void app_scene_read_pcap_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);

    // Join and free the thread
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
}

/**
 * Thread for read the packets from pcap
 */

int32_t thread_read_pcaps(void* context) {
    App* app = (App*)context;

    uint8_t* buffer = app->ethernet->tx_buffer;
    const size_t buffer_capacity = MAX_FRAMELEN; // matches enc28j60_t.tx_buffer

    uint32_t len = 0;

    uint32_t counter = 0;

    bool write_once = true;

    // F0.5e — bail out early on empty/corrupt PCAPs. Pre-fix the loop
    // dereferenced packet_positions[counter] even when packet_count==0,
    // and `counter >= packet_count - 1` underflowed to a near-infinite
    // sentinel that disabled the right-arrow upper bound.
    if(packet_count == 0) {
        draw_could_not_be_read(app);
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        return 0;
    }

    bool start = storage_file_open(
        app->file, furi_string_get_cstr(app->path), FSAM_READ, FSOM_OPEN_EXISTING);

    while(furi_hal_gpio_read(&gpio_button_back) && start) {
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

            if(counter >= packet_count - 1) counter = packet_count - 1;

            write_once = true;
        }

        if(write_once) {
            len = pcap_get_specific_packet(
                app->file, buffer, buffer_capacity, packet_positions[counter]);

            furi_string_reset(app->text);

            furi_string_cat_printf(
                app->text, "<== Packet %lu of %lu ==>\n", counter + 1, packet_count);

            print_packet_info(app->text, buffer, len);

            view_dispatcher_send_custom_event(app->view_dispatcher, 1);

            write_once = false;
        }

        furi_delay_ms(1);
    }

    if(start) storage_file_close(app->file);

    return 0;
}
