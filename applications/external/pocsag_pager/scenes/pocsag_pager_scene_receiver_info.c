#include "../pocsag_pager_app_i.h"

static void pocsag_pager_scene_receiver_info_populate(POCSAGPagerApp* app) {
    widget_reset(app->widget);

    FuriString* header = furi_string_alloc();
    FuriString* body = furi_string_alloc();
    FuriString* title = furi_string_alloc();
    FuriString* subtitle = furi_string_alloc();
    FuriString* scroll_text = furi_string_alloc();

    pcsg_history_load_message(app->txrx->history, app->txrx->idx_menu_chosen, header, body);

    // Split header "[P1200] RIC: 123456 | Msg  439.9875 MHz" into banner (left) + subtitle (right)
    size_t pipe_pos = furi_string_search_char(header, '|');
    if(pipe_pos != FURI_STRING_FAILURE) {
        furi_string_set_n(title, header, 0, pipe_pos);
        furi_string_trim(title);
        furi_string_set_n(
            subtitle, header, pipe_pos + 1, furi_string_size(header) - (pipe_pos + 1));
        furi_string_trim(subtitle);
    } else {
        furi_string_set(title, header);
        furi_string_trim(title);
    }

    if(furi_string_size(title) > 21) {
        furi_string_left(title, 21);
    }

    widget_add_text_box_element(
        app->widget,
        0,
        0,
        128,
        14,
        AlignCenter,
        AlignBottom,
        "\e#\e!                                                      \e!\n",
        false);

    FuriString* banner = furi_string_alloc_printf("\e#\e! %s \e!", furi_string_get_cstr(title));
    widget_add_text_box_element(
        app->widget, 0, 2, 128, 12, AlignCenter, AlignTop, furi_string_get_cstr(banner), false);

    furi_string_trim(body);
    const char* body_cstr = furi_string_size(body) > 0 ? furi_string_get_cstr(body) :
                                                         "(Tone only / empty)";

    if(furi_string_size(subtitle) > 0) {
        furi_string_printf(scroll_text, "\e#%s\e#\n%s", furi_string_get_cstr(subtitle), body_cstr);
    } else {
        furi_string_set_str(scroll_text, body_cstr);
    }

    widget_add_text_scroll_element(app->widget, 0, 15, 128, 49, furi_string_get_cstr(scroll_text));

    furi_string_free(banner);
    furi_string_free(scroll_text);
    furi_string_free(subtitle);
    furi_string_free(title);
    furi_string_free(body);
    furi_string_free(header);
}

static void pocsag_pager_scene_receiver_info_add_to_history_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    furi_assert(context);
    POCSAGPagerApp* app = context;

    if(pcsg_history_add_to_history(app->txrx->history, decoder_base, app->txrx->preset) ==
       PCSGHistoryStateAddKeyNewDada) {
        subghz_receiver_reset(receiver);
        notification_message(app->notifications, &sequence_blink_green_10);
        app->txrx->rx_key_state = PCSGRxKeyStateAddKey;
    }
}

void pocsag_pager_scene_receiver_info_on_enter(void* context) {
    POCSAGPagerApp* app = context;

    subghz_receiver_set_rx_callback(
        app->txrx->receiver, pocsag_pager_scene_receiver_info_add_to_history_callback, app);
    pocsag_pager_scene_receiver_info_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, POCSAGPagerViewWidget);
}

bool pocsag_pager_scene_receiver_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pocsag_pager_scene_receiver_info_on_exit(void* context) {
    POCSAGPagerApp* app = context;
    widget_reset(app->widget);
}
