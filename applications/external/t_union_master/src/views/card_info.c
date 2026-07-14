#include "card_info.h"
#include "../view_modules/elements.h"

struct CardInfoView {
    View* view;
    CardInfoViewCallback cb;
    void* ctx;
};

typedef struct {
    bool more_screen; // 是否更多信息界面

    TUnionMessage* message;
    TUnionMessageExt* message_ext;
} CardInfoViewModel;

static void card_info_view_draw_cb(Canvas* canvas, void* _model) {
    CardInfoViewModel* model = _model;
    FuriString* temp_str = furi_string_alloc();

    if(model->more_screen == true) {
        // 详情界面
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_line(canvas, 39, 0, 39, 48);
        elements_draw_str_utf8(canvas, 2, 14, "签发日");
        furi_string_printf(
            temp_str,
            "%04d-%02d-%02d",
            model->message->iss_year,
            model->message->iss_month,
            model->message->iss_day);
        canvas_draw_str(canvas, 42, 13, furi_string_get_cstr(temp_str));
        furi_string_reset(temp_str);
        canvas_draw_line(canvas, 6, 16, 122, 16);

        elements_draw_str_utf8(canvas, 2, 30, "到期日");
        furi_string_printf(
            temp_str,
            "%04d-%02d-%02d",
            model->message->exp_year,
            model->message->exp_month,
            model->message->exp_day);
        canvas_draw_str(canvas, 42, 29, furi_string_get_cstr(temp_str));
        furi_string_reset(temp_str);
        canvas_draw_line(canvas, 6, 32, 122, 32);

        elements_draw_str_utf8(canvas, 2, 46, "发卡地");
        furi_string_printf(
            temp_str,
            "%s(%s)",
            furi_string_get_cstr(model->message_ext->city_name),
            model->message->city_id);
        elements_draw_str_utf8(canvas, 42, 46, furi_string_get_cstr(temp_str));
        furi_string_reset(temp_str);
        canvas_draw_line(canvas, 6, 48, 122, 48);

        elements_draw_str_utf8(canvas, 42, 62, "卡种");
        canvas_draw_line(canvas, 66, 48, 66, 62);
        furi_string_printf(
            temp_str,
            "%s(%d)",
            furi_string_get_cstr(model->message_ext->type_name),
            model->message->type);
        elements_draw_str_utf8(canvas, 68, 62, furi_string_get_cstr(temp_str));

        elements_button_left_utf8(canvas, "返回");
    } else {
        // 基本界面
        elements_draw_str_utf8(canvas, 2, 20, "余额");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 28, 2, 84, 18, 2);
        canvas_set_color(canvas, ColorWhite);

        furi_string_printf(
            temp_str, "%lu.%02lu", model->message->balance / 100, model->message->balance % 100);
        canvas_draw_str(canvas, 30, 18, furi_string_get_cstr(temp_str));

        canvas_set_color(canvas, ColorBlack);
        elements_draw_str_utf8(canvas, 114, 20, "元");

        canvas_draw_line(canvas, 0, 22, 127, 22);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 34, model->message->card_number);
        if(furi_string_size(model->message_ext->card_name) != 0) {
            furi_string_set(temp_str, model->message_ext->card_name);
        } else {
            furi_string_set(temp_str, "未知");
        }
        elements_draw_str_utf8(canvas, 2, 48, furi_string_get_cstr(temp_str));
        elements_button_left_utf8(canvas, "记录查询");
        elements_button_right_utf8(canvas, "更多");
    }
    furi_string_free(temp_str);
}

static bool card_info_view_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    CardInfoView* instance = ctx;
    bool consumed = false;

    if(event->type == InputTypeShort &&
       (event->key == InputKeyLeft || event->key == InputKeyRight || event->key == InputKeyBack)) {
        with_view_model(
            instance->view,
            CardInfoViewModel * model,
            {
                if(model->more_screen == true) {
                    // 在更多信息界面下
                    if(event->key == InputKeyLeft || event->key == InputKeyBack) {
                        model->more_screen = false;
                        consumed = true;
                    }
                } else {
                    // 在主信息界面下
                    if(event->key == InputKeyRight) {
                        model->more_screen = true;
                        consumed = true;
                    }
                    if(event->key == InputKeyLeft) {
                        if(instance->cb)
                            instance->cb(TUM_CustomEventSwitchToTransaction, instance->ctx);
                        consumed = true;
                    }
                }
            },
            true);
    }

    return consumed;
}

CardInfoView* card_info_alloc(TUnionMessage* message, TUnionMessageExt* message_ext) {
    furi_assert(message);
    furi_assert(message_ext);
    CardInfoView* instance = malloc(sizeof(CardInfoView));

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(CardInfoViewModel));
    with_view_model(
        instance->view,
        CardInfoViewModel * model,
        {
            model->message = message;
            model->message_ext = message_ext;
            model->more_screen = false;
        },
        false);

    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, card_info_view_draw_cb);
    view_set_input_callback(instance->view, card_info_view_input_cb);

    return instance;
}

void card_info_free(CardInfoView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* card_info_get_view(CardInfoView* instance) {
    furi_assert(instance);
    return instance->view;
}

void card_info_set_callback(CardInfoView* instance, CardInfoViewCallback cb, void* ctx) {
    furi_assert(instance);
    furi_assert(cb);
    instance->cb = cb;
    instance->ctx = ctx;
}
