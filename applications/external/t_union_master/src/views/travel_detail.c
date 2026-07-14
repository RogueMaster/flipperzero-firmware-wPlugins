#include "travel_detail.h"
#include "../view_modules/elements.h"
#include "../view_modules/app_elements.h"
#include "t_union_master_icons.h"

struct TravelDetailView {
    View* view;
    TravelDetailViewCallback cb;
    void* ctx;
};

typedef struct {
    TUnionMessage* message;
    TUnionMessageExt* message_ext;

    size_t index;
} RecordListViewModel;

void travel_detail_process_up(TravelDetailView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            if(model->index > 0) model->index--;
        },
        true);
}

void travel_detail_process_down(TravelDetailView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            if(model->index < model->message->travel_cnt - 1) model->index++;
        },
        true);
}

void travel_detail_process_ok(TravelDetailView* instance) {
    UNUSED(instance);
}

void travel_detail_process_left(TravelDetailView* instance) {
    UNUSED(instance);
}

void travel_detail_process_right(TravelDetailView* instance) {
    UNUSED(instance);
}

uint8_t travel_detail_get_index(TravelDetailView* instance) {
    uint8_t index = 0;
    with_view_model(instance->view, RecordListViewModel * model, { index = model->index; }, false);
    return index;
}

void travel_detail_set_index(TravelDetailView* instance, size_t index) {
    with_view_model(instance->view, RecordListViewModel * model, { model->index = index; }, true);
}

void travel_detail_reset(TravelDetailView* instance) {
    with_view_model(instance->view, RecordListViewModel * model, { model->index = 0; }, false);
}

static void travel_detail_view_draw_cb(Canvas* canvas, void* _model) {
    RecordListViewModel* model = _model;
    TUnionMessage* msg = model->message;
    TUnionMessageExt* msg_ext = model->message_ext;

    TUnionTravel* travel = &msg->travels[model->index];
    TUnionTravelExt* travel_ext = &msg_ext->travels_ext[model->index];

    FuriString* temp_str = furi_string_alloc();

    // 翻页箭头
    elements_draw_page_cursor(canvas, model->index, msg->travel_cnt);

    // 时间戳
    canvas_draw_box(canvas, 0, 0, 116, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    furi_string_printf(
        temp_str,
        "%04u-%02u-%02u %02u:%02u:%02u",
        travel->year,
        travel->month,
        travel->day,
        travel->hour,
        travel->month,
        travel->second);
    canvas_draw_str(canvas, 2, 9, furi_string_get_cstr(temp_str));
    furi_string_reset(temp_str);

    canvas_set_color(canvas, ColorBlack);

    // 城市
    elements_draw_str_utf8(canvas, 2, 24, furi_string_get_cstr(travel_ext->city_name));

    // 交通LOGO
    canvas_draw_rframe(canvas, 2, 26, 36, 36, 1);
    const Icon* logo;
    if(travel_ext->line_type == LineTypeMetro) {
        logo = get_mtr_logo_by_city_id(travel->city_id);
    } else {
        logo = &I_logo_bus_32x32;
    }
    canvas_draw_icon(canvas, 4, 26 + 2, logo);

    // 线路名
    FURI_LOG_D("T", "line_type=%d", travel_ext->line_type);
    if(travel_ext->line_type == LineTypeBUS) {
        furi_string_set(temp_str, "公交车");
    } else {
        furi_string_set(temp_str, travel_ext->line_name);
    }
    elements_draw_str_utf8(canvas, 40, 42, furi_string_get_cstr(temp_str));
    furi_string_reset(temp_str);

    // 站台名
    elements_draw_str_utf8(canvas, 40, 56, furi_string_get_cstr(travel_ext->station_name));

    // 行程属性图标
    elements_draw_travel_attr_icons(canvas, 40, 60, travel, travel_ext);

    furi_string_free(temp_str);
}

static bool travel_detail_view_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    TravelDetailView* instance = ctx;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            travel_detail_process_up(instance);
            break;
        case InputKeyDown:
            consumed = true;
            travel_detail_process_down(instance);
            break;
        case InputKeyOk:
            consumed = true;
            travel_detail_process_ok(instance);
            break;
        case InputKeyRight:
            consumed = true;
            travel_detail_process_right(instance);
            break;
        case InputKeyLeft:
            consumed = true;
            travel_detail_process_left(instance);
        default:
            break;
        }
    }
    return consumed;
}

TravelDetailView* travel_detail_alloc(TUnionMessage* message, TUnionMessageExt* message_ext) {
    furi_assert(message);
    furi_assert(message_ext);
    TravelDetailView* instance = malloc(sizeof(TravelDetailView));

    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(RecordListViewModel));
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            model->message = message;
            model->message_ext = message_ext;
            model->index = 0;
        },
        false);

    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, travel_detail_view_draw_cb);
    view_set_input_callback(instance->view, travel_detail_view_input_cb);

    return instance;
}

void travel_detail_free(TravelDetailView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* travel_detail_get_view(TravelDetailView* instance) {
    furi_assert(instance);
    return instance->view;
}

void travel_detail_set_callback(TravelDetailView* instance, TravelDetailViewCallback cb, void* ctx) {
    furi_assert(instance);
    furi_assert(cb);
    instance->cb = cb;
    instance->ctx = ctx;
}
