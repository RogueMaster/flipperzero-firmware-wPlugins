#include "transaction_detail.h"
#include "../view_modules/elements.h"
#include "../view_modules/app_elements.h"

struct TransactionDetailView {
    View* view;
    TransactionDetailViewCallback cb;
    void* ctx;
};

typedef struct {
    TUnionMessage* message;
    TUnionMessageExt* message_ext;

    size_t index;
} RecordListViewModel;

void transaction_detail_process_up(TransactionDetailView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            if(model->index > 0) model->index--;
        },
        true);
}

void transaction_detail_process_down(TransactionDetailView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            if(model->index < model->message->transaction_cnt - 1) model->index++;
        },
        true);
}

void transaction_detail_process_left(TransactionDetailView* instance) {
    UNUSED(instance);
}

void transaction_detail_process_right(TransactionDetailView* instance) {
    UNUSED(instance);
}

uint8_t transaction_detail_get_index(TransactionDetailView* instance) {
    uint8_t index = 0;
    with_view_model(instance->view, RecordListViewModel * model, { index = model->index; }, false);
    return index;
}

void transaction_detail_set_index(TransactionDetailView* instance, size_t index) {
    with_view_model(instance->view, RecordListViewModel * model, { model->index = index; }, true);
}

void transaction_detail_reset(TransactionDetailView* instance) {
    with_view_model(instance->view, RecordListViewModel * model, { model->index = 0; }, false);
}

static void transaction_detail_view_draw_cb(Canvas* canvas, void* _model) {
    RecordListViewModel* model = _model;
    TUnionMessage* msg = model->message;
    TUnionTransaction* transcation = &msg->transactions[model->index];

    FuriString* temp_str = furi_string_alloc();

    // 翻页箭头
    elements_draw_page_cursor(canvas, model->index, msg->transaction_cnt);

    canvas_draw_box(canvas, 0, 0, 116, 16);
    canvas_set_color(canvas, ColorWhite);
    // 交易类型
    if(transcation->type == 9) {
        if(transcation->money != 0)
            furi_string_set(temp_str, "消费");
        else
            furi_string_set(temp_str, "区间记账");
    } else if(transcation->type == 2)
        furi_string_set(temp_str, "充值");
    else
        furi_string_set(temp_str, "其他");
    elements_draw_str_utf8(canvas, 2, 14, furi_string_get_cstr(temp_str));
    furi_string_reset(temp_str);

    // 交易金额
    canvas_set_font(canvas, FontPrimary);
    furi_string_printf(
        temp_str,
        "%c%lu.%lu",
        (transcation->type == 2) ? '+' : '-',
        transcation->money / 100,
        transcation->money % 100);
    canvas_draw_str(canvas, 52, 12, furi_string_get_cstr(temp_str));
    furi_string_reset(temp_str);

    // 交易序号
    furi_string_printf(temp_str, "#%u", transcation->seqense);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 80, 12, furi_string_get_cstr(temp_str));
    furi_string_reset(temp_str);

    canvas_set_color(canvas, ColorBlack);

    // 时间戳
    furi_string_printf(
        temp_str,
        "%04u-%02u-%02u %02u:%02u:%02u",
        transcation->year,
        transcation->month,
        transcation->day,
        transcation->hour,
        transcation->month,
        transcation->second);
    canvas_draw_str(canvas, 2, 26, furi_string_get_cstr(temp_str));
    furi_string_reset(temp_str);

    // 终端id
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 40, "Terminal ID");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 50, transcation->terminal_id);

    // type值
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 80, 40, "Type");
    canvas_set_font(canvas, FontSecondary);
    furi_string_printf(temp_str, "%02u", transcation->type);
    canvas_draw_str(canvas, 80, 50, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);
}

static bool transaction_detail_view_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    TransactionDetailView* instance = ctx;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            transaction_detail_process_up(instance);
            break;
        case InputKeyDown:
            consumed = true;
            transaction_detail_process_down(instance);
            break;
        case InputKeyRight:
            consumed = true;
            transaction_detail_process_right(instance);
            break;
        case InputKeyLeft:
            consumed = true;
            transaction_detail_process_left(instance);
        default:
            break;
        }
    }
    return consumed;
}

TransactionDetailView*
    transaction_detail_alloc(TUnionMessage* message, TUnionMessageExt* message_ext) {
    furi_assert(message);
    furi_assert(message_ext);
    TransactionDetailView* instance = malloc(sizeof(TransactionDetailView));

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
    view_set_draw_callback(instance->view, transaction_detail_view_draw_cb);
    view_set_input_callback(instance->view, transaction_detail_view_input_cb);

    return instance;
}

void transaction_detail_free(TransactionDetailView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* transaction_detail_get_view(TransactionDetailView* instance) {
    furi_assert(instance);
    return instance->view;
}

void transaction_detail_set_callback(
    TransactionDetailView* instance,
    TransactionDetailViewCallback cb,
    void* ctx) {
    furi_assert(instance);
    furi_assert(cb);
    instance->cb = cb;
    instance->ctx = ctx;
}
