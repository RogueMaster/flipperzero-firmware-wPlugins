#include "../cogs_mikai.h"
#include <furi_hal_rtc.h>

enum {
    SetCreditSceneEventInput,
};

static bool cogs_mikai_scene_set_credit_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);

    if(strlen(text) == 0) {
        return true;
    }

    bool has_decimal = false;
    for(size_t i = 0; text[i] != '\0'; i++) {
        if(text[i] == '.' || text[i] == ',') {
            if(has_decimal) {
                furi_string_set(error, "Only one decimal point");
                return false;
            }
            has_decimal = true;
        } else if(text[i] < '0' || text[i] > '9') {
            furi_string_set(error, "Only numbers and '.'");
            return false;
        }
    }

    return true;
}


static bool parse_euros_to_cents(const char* text, uint16_t* cents) {
    if(!text || !cents) return false;

    uint32_t integer_part = 0;
    uint32_t decimal_part = 0;
    uint32_t decimal_digits = 0;
    bool in_decimal = false;

    for(size_t i = 0; text[i] != '\0'; i++) {
        if(text[i] == '.' || text[i] == ',') {
            in_decimal = true;
        } else if(text[i] >= '0' && text[i] <= '9') {
            if(in_decimal) {
                if(decimal_digits < 2) {
                    decimal_part = decimal_part * 10 + (text[i] - '0');
                    decimal_digits++;
                }
            } else {
                integer_part = integer_part * 10 + (text[i] - '0');
            }
        }
    }
    if(decimal_digits == 1) {
        decimal_part *= 10;
    }

    uint32_t total_cents = integer_part * 100 + decimal_part;
    if(total_cents > 99999) return false; // Max 999.99 EUR

    *cents = (uint16_t)total_cents;
    return true;
}

static void cogs_mikai_scene_set_credit_text_input_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SetCreditSceneEventInput);
}

static void cogs_mikai_scene_set_credit_popup_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void cogs_mikai_scene_set_credit_on_enter(void* context) {
    COGSMyKaiApp* app = context;

    if(!app->mykey.is_loaded) {
        Popup* popup = app->popup;
        popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "No card loaded\nRead a card first", 64, 25, AlignCenter, AlignTop);
        popup_set_callback(popup, cogs_mikai_scene_set_credit_popup_callback);
        popup_set_context(popup, app);
        popup_set_timeout(popup, 2000);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
        return;
    }

    TextInput* text_input = app->text_input;
    snprintf(app->text_buffer, sizeof(app->text_buffer), "10.00");

    text_input_set_header_text(text_input, "Set Credit (EUR)");
    text_input_set_validator(text_input, cogs_mikai_scene_set_credit_validator, NULL);
    text_input_set_result_callback(
        text_input,
        cogs_mikai_scene_set_credit_text_input_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewTextInput);
}

bool cogs_mikai_scene_set_credit_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SetCreditSceneEventInput) {
            if(app->text_buffer[0] == '\0') {
                FURI_LOG_W(TAG, "Ignoring empty text_buffer (already processed)");
                consumed = true;
                return consumed;
            }
            FURI_LOG_I(TAG, "Set credit input: '%s'", app->text_buffer);
            uint16_t cents = 0;
            bool parse_ok = parse_euros_to_cents(app->text_buffer, &cents);
            FURI_LOG_I(TAG, "Parse result: %d, cents: %d", parse_ok, cents);

            if(parse_ok) {
                FURI_LOG_I(TAG, "Valid amount: %d cents", cents);

                DateTime datetime;
                furi_hal_rtc_get_datetime(&datetime);

                bool success = mykey_set_cents(
                    &app->mykey,
                    cents,
                    datetime.day,
                    datetime.month,
                    datetime.year - 2000);

                if(success) {
                    app->mykey.current_credit = mykey_get_current_credit(&app->mykey);
                    text_input_reset(app->text_input);

                    memset(app->text_buffer, 0, sizeof(app->text_buffer));
                    Popup* popup = app->popup;
                    popup_set_header(popup, "Credit Set!", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(popup, "Saved in memory\nUse 'Write to Card'", 64, 25, AlignCenter, AlignTop);
                    popup_set_callback(popup, cogs_mikai_scene_set_credit_popup_callback);
                    popup_set_context(popup, app);
                    popup_set_timeout(popup, 2000);
                    popup_enable_timeout(popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
                    notification_message(app->notifications, &sequence_success);
                } else {
                    Popup* popup = app->popup;
                    popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
                    popup_set_text(popup, "Failed to set credit", 64, 25, AlignCenter, AlignTop);
                    popup_set_callback(popup, cogs_mikai_scene_set_credit_popup_callback);
                    popup_set_context(popup, app);
                    popup_set_timeout(popup, 2000);
                    popup_enable_timeout(popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
                    notification_message(app->notifications, &sequence_error);
                }
            } else {
                FURI_LOG_E(TAG, "Invalid amount: parse_ok=%d, cents=%d",
                    parse_ok, cents);
                Popup* popup = app->popup;
                popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    popup,
                    "Invalid amount\nEnter 0.00-999.99",
                    64,
                    25,
                    AlignCenter,
                    AlignTop);
                popup_set_callback(popup, cogs_mikai_scene_set_credit_popup_callback);
                popup_set_context(popup, app);
                popup_set_timeout(popup, 2000);
                popup_enable_timeout(popup);
                view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
            }
            consumed = true;
        } else {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, COGSMyKaiSceneStart);
            consumed = true;
        }
    }

    return consumed;
}

void cogs_mikai_scene_set_credit_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
