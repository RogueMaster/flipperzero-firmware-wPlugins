#include "travel_list.h"
#include "../view_modules/elements.h"
#include "../view_modules/app_elements.h"
#include "t_union_master_icons.h"

struct TravelListView {
    View* view;
    TravelListViewCallback cb;
    void* ctx;

    FuriTimer* tips_timer;
};

typedef struct {
    TUnionMessage* message;
    TUnionMessageExt* message_ext;

    size_t position;
    size_t window_position;

    bool tips_visible;
} RecordListViewModel;

static const uint8_t item_height = 22;
static const size_t items_on_screen = 3;

void travel_list_process_up(TravelListView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->travel_cnt;

            if(model->position > 0) {
                model->position--;
                if((model->position == model->window_position) && (model->window_position > 0)) {
                    model->window_position--;
                }
            } else {
                model->position = items_size - 1;
                if(model->position > items_on_screen - 1) {
                    model->window_position = model->position - (items_on_screen - 1);
                }
            }
        },
        true);
}

void travel_list_process_down(TravelListView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->travel_cnt;

            if(model->position < items_size - 1) {
                model->position++;
                if((model->position - model->window_position > items_on_screen - 2) &&
                   (model->window_position < items_size - items_on_screen)) {
                    model->window_position++;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
        },
        true);
}

void travel_list_process_ok(TravelListView* instance) {
    size_t items_size = 0;
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        { items_size = model->message->travel_cnt; },
        false);
    if(items_size > 0) instance->cb(TUM_CustomEventSwitchToTravelDetail, instance->ctx);
}

void travel_list_process_right(TravelListView* instance) {
    instance->cb(TUM_CustomEventSwitchToTransaction, instance->ctx);
}

uint8_t travel_list_get_index(TravelListView* instance) {
    uint8_t index = 0;
    with_view_model(
        instance->view, RecordListViewModel * model, { index = model->position; }, false);
    return index;
}

void travel_list_set_index(TravelListView* instance, size_t index) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->travel_cnt;

            if(index >= items_size) {
                index = 0;
            }

            model->position = index;
            model->window_position = index;

            if(model->window_position > 0) {
                model->window_position -= 1;
            }

            if(items_size <= items_on_screen) {
                model->window_position = 0;
            } else {
                const size_t pos = items_size - items_on_screen;
                if(model->window_position > pos) {
                    model->window_position = pos;
                }
            }
        },
        true);
}

void travel_list_reset(TravelListView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            model->position = 0;
            model->window_position = 0;
            model->tips_visible = false;
        },
        false);
}

static void travel_list_view_draw_cb(Canvas* canvas, void* _model) {
    RecordListViewModel* model = _model;
    TUnionMessage* msg = model->message;
    TUnionMessageExt* msg_ext = model->message_ext;

    FuriString* temp_str = furi_string_alloc();
    uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);

    if(msg->travel_cnt == 0) {
        elements_draw_str_aligned_utf8(canvas, 64, 32, AlignCenter, AlignCenter, "空空如也。");
    } else {
        for(size_t position = 0; position < msg->travel_cnt; position++) {
            TUnionTravel* travel = &msg->travels[position];
            TUnionTravelExt* travel_ext = &msg_ext->travels_ext[position];
            const size_t item_position = position - model->window_position;

            if(item_position < items_on_screen) {
                if(position == model->position) {
                    canvas_set_color(canvas, ColorBlack);
                    elements_slightly_rounded_box(
                        canvas, 0, item_position * item_height, item_width, item_height - 2);
                    canvas_set_color(canvas, ColorWhite);
                } else {
                    canvas_set_color(canvas, ColorBlack);
                }

                // 行程属性图标
                elements_draw_travel_attr_icons(
                    canvas, 2, (item_position * item_height) + item_height - 7, travel, travel_ext);

                // 线路名+站台名
                if(furi_string_size(travel_ext->line_name) != 0) {
                    // 线路名有效
                    if(furi_string_size(travel_ext->station_name) != 0) {
                        //站台名有效
                        furi_string_set(temp_str, travel_ext->station_name);
                    } else {
                        //站台名无效
                        furi_string_set(temp_str, travel_ext->line_name);
                        furi_string_cat_str(temp_str, "(未知站台)");
                    }
                } else {
                    // 线路名无效
                    furi_string_printf(
                        temp_str, "%s(未知线路)", furi_string_get_cstr(travel_ext->city_name));
                }
                elements_draw_str_utf8(
                    canvas,
                    2,
                    (item_position * item_height) + item_height - 9,
                    furi_string_get_cstr(temp_str));

                // 行程时间
                canvas_set_font(canvas, FontSecondary);
                furi_string_printf(temp_str, "%02u-%02u", travel->month, travel->day);
                canvas_draw_str(
                    canvas,
                    92,
                    (item_position * item_height) + item_height - 13,
                    furi_string_get_cstr(temp_str));
                furi_string_reset(temp_str);

                furi_string_printf(temp_str, "%02u:%02u", travel->hour, travel->minute);
                canvas_draw_str(
                    canvas,
                    92,
                    (item_position * item_height) + item_height - 4,
                    furi_string_get_cstr(temp_str));
                furi_string_reset(temp_str);
            }
        }
        elements_scrollbar(canvas, model->position, msg->travel_cnt);
    }

    if(model->tips_visible) {
        const uint8_t frame_x = 26;
        const uint8_t frame_width = canvas_width(canvas) - frame_x * 2;
        const uint8_t frame_y = 22;
        const uint8_t frame_height = canvas_height(canvas) - frame_y * 2;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, frame_x + 1, frame_y + 1, frame_width - 2, frame_height - 2);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, frame_x, frame_y, frame_width, frame_height);
        canvas_draw_line(
            canvas,
            frame_x + 1,
            frame_y + frame_height,
            frame_x + frame_width,
            frame_y + frame_height);
        canvas_draw_line(
            canvas,
            frame_x + frame_width,
            frame_y + 1,
            frame_x + frame_width,
            frame_y + frame_height);
        elements_draw_str_aligned_utf8(canvas, 64, 32, AlignCenter, AlignCenter, "行程记录");
        canvas_draw_icon(canvas, frame_x + frame_width - 5 - 4, 28, &I_ButtonRight_4x7);
    }

    furi_string_free(temp_str);
}

static bool travel_list_view_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    TravelListView* instance = ctx;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        with_view_model(
            instance->view, RecordListViewModel * model, { model->tips_visible = false; }, false);
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            travel_list_process_up(instance);
            break;
        case InputKeyDown:
            consumed = true;
            travel_list_process_down(instance);
            break;
        case InputKeyOk:
            consumed = true;
            travel_list_process_ok(instance);
            break;
        case InputKeyRight:
            consumed = true;
            travel_list_process_right(instance);
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            consumed = true;
            travel_list_process_up(instance);
        } else if(event->key == InputKeyDown) {
            consumed = true;
            travel_list_process_down(instance);
        }
    }

    return consumed;
}

static void travel_list_enter_cb(void* ctx) {
    TravelListView* instance = ctx;

    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            model->tips_visible = true;
            furi_timer_start(instance->tips_timer, 800);
        },
        true);
}

static void tips_timer_cb(void* ctx) {
    TravelListView* instance = ctx;

    with_view_model(
        instance->view, RecordListViewModel * model, { model->tips_visible = false; }, true);
}

TravelListView* travel_list_alloc(TUnionMessage* message, TUnionMessageExt* message_ext) {
    furi_assert(message);
    furi_assert(message_ext);
    TravelListView* instance = malloc(sizeof(TravelListView));

    instance->tips_timer = furi_timer_alloc(tips_timer_cb, FuriTimerTypeOnce, instance);

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(RecordListViewModel));
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            model->message = message;
            model->message_ext = message_ext;
            model->position = 0;
            model->window_position = 0;
        },
        false);

    view_set_context(instance->view, instance);
    view_set_enter_callback(instance->view, travel_list_enter_cb);
    view_set_draw_callback(instance->view, travel_list_view_draw_cb);
    view_set_input_callback(instance->view, travel_list_view_input_cb);

    return instance;
}

void travel_list_free(TravelListView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* travel_list_get_view(TravelListView* instance) {
    furi_assert(instance);
    return instance->view;
}

void travel_list_set_callback(TravelListView* instance, TravelListViewCallback cb, void* ctx) {
    furi_assert(instance);
    furi_assert(cb);
    instance->cb = cb;
    instance->ctx = ctx;
}
