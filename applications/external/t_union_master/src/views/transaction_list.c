#include "transaction_list.h"
#include "../view_modules/elements.h"
#include "t_union_master_icons.h"

struct TransactionListView {
    View* view;
    TransactionListViewCallback cb;
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

void transaction_list_process_up(TransactionListView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->transaction_cnt;

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

void transaction_list_process_down(TransactionListView* instance) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->transaction_cnt;

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

void transaction_list_process_ok(TransactionListView* instance) {
    size_t items_size = 0;
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        { items_size = model->message->transaction_cnt; },
        false);
    if(items_size > 0) instance->cb(TUM_CustomEventSwitchToTransactionDetail, instance->ctx);
}

void transaction_list_process_left(TransactionListView* instance) {
    instance->cb(TUM_CustomEventSwitchToTravel, instance->ctx);
}

void transaction_list_process_right(TransactionListView* instance) {
    instance->cb(TUM_CustomEventSwitchToBaseinfo, instance->ctx);
}

uint8_t transaction_list_get_index(TransactionListView* instance) {
    uint8_t index = 0;
    with_view_model(
        instance->view, RecordListViewModel * model, { index = model->position; }, false);
    return index;
}

void transaction_list_set_index(TransactionListView* instance, size_t index) {
    with_view_model(
        instance->view,
        RecordListViewModel * model,
        {
            const size_t items_size = model->message->transaction_cnt;

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

void transaction_list_reset(TransactionListView* instance) {
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

static void transaction_list_view_draw_cb(Canvas* canvas, void* _model) {
    RecordListViewModel* model = _model;
    TUnionMessage* msg = model->message;

    FuriString* temp_str = furi_string_alloc();
    uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);

    if(msg->transaction_cnt == 0) {
        elements_draw_str_aligned_utf8(canvas, 64, 32, AlignCenter, AlignCenter, "空空如也。");
    } else {
        for(size_t position = 0; position < msg->transaction_cnt; position++) {
            TUnionTransaction* transaction = &msg->transactions[position];
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

                // 交易类型
                if(transaction->type == 9) {
                    if(transaction->money != 0)
                        furi_string_set(temp_str, "消费");
                    else
                        furi_string_set(temp_str, "区间记账");
                } else if(transaction->type == 2)
                    furi_string_set(temp_str, "充值");
                else
                    furi_string_set(temp_str, "其他");
                elements_draw_str_utf8(
                    canvas,
                    2,
                    (item_position * item_height) + item_height - 6,
                    furi_string_get_cstr(temp_str));
                furi_string_reset(temp_str);

                // 交易金额
                if(transaction->money != 0) {
                    canvas_set_font(canvas, FontPrimary);
                    furi_string_printf(
                        temp_str,
                        "%c%lu.%lu",
                        (transaction->type == 2) ? '+' : '-',
                        transaction->money / 100,
                        transaction->money % 100);
                    canvas_draw_str(
                        canvas,
                        28,
                        (item_position * item_height) + item_height - 8,
                        furi_string_get_cstr(temp_str));
                    furi_string_reset(temp_str);
                }

                // 交易序号
                canvas_set_font(canvas, FontSecondary);
                furi_string_printf(temp_str, "#%u", transaction->seqense);
                canvas_draw_str(
                    canvas,
                    62,
                    (item_position * item_height) + item_height - 8,
                    furi_string_get_cstr(temp_str));
                furi_string_reset(temp_str);

                // 交易时间
                furi_string_printf(temp_str, "%02u-%02u", transaction->month, transaction->day);
                canvas_draw_str(
                    canvas,
                    92,
                    (item_position * item_height) + item_height - 13,
                    furi_string_get_cstr(temp_str));
                furi_string_reset(temp_str);

                furi_string_printf(temp_str, "%02u:%02u", transaction->hour, transaction->minute);
                canvas_draw_str(
                    canvas,
                    92,
                    (item_position * item_height) + item_height - 4,
                    furi_string_get_cstr(temp_str));
                furi_string_reset(temp_str);
            }
        }
        elements_scrollbar(canvas, model->position, msg->transaction_cnt);
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
        elements_draw_str_aligned_utf8(canvas, 64, 32, AlignCenter, AlignCenter, "交易记录");
        canvas_draw_icon(canvas, frame_x + 5, 28, &I_ButtonLeft_4x7);
        canvas_draw_icon(canvas, frame_x + frame_width - 5 - 4, 28, &I_ButtonRight_4x7);
    }

    furi_string_free(temp_str);
}

static bool transaction_list_view_input_cb(InputEvent* event, void* ctx) {
    furi_assert(ctx);
    TransactionListView* instance = ctx;
    bool consumed = false;

    if(event->type == InputTypeShort) {
        with_view_model(
            instance->view, RecordListViewModel * model, { model->tips_visible = false; }, false);
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            transaction_list_process_up(instance);
            break;
        case InputKeyDown:
            consumed = true;
            transaction_list_process_down(instance);
            break;
        case InputKeyOk:
            consumed = true;
            transaction_list_process_ok(instance);
            break;
        case InputKeyRight:
            consumed = true;
            transaction_list_process_right(instance);
            break;
        case InputKeyLeft:
            consumed = true;
            transaction_list_process_left(instance);
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            consumed = true;
            transaction_list_process_up(instance);
        } else if(event->key == InputKeyDown) {
            consumed = true;
            transaction_list_process_down(instance);
        }
    }

    return consumed;
}

static void transaction_list_enter_cb(void* ctx) {
    TransactionListView* instance = ctx;

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
    TransactionListView* instance = ctx;

    with_view_model(
        instance->view, RecordListViewModel * model, { model->tips_visible = false; }, true);
}

TransactionListView*
    transaction_list_alloc(TUnionMessage* message, TUnionMessageExt* message_ext) {
    furi_assert(message);
    furi_assert(message_ext);
    TransactionListView* instance = malloc(sizeof(TransactionListView));

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
    view_set_enter_callback(instance->view, transaction_list_enter_cb);
    view_set_draw_callback(instance->view, transaction_list_view_draw_cb);
    view_set_input_callback(instance->view, transaction_list_view_input_cb);

    return instance;
}

void transaction_list_free(TransactionListView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* transaction_list_get_view(TransactionListView* instance) {
    furi_assert(instance);
    return instance->view;
}

void transaction_list_set_callback(
    TransactionListView* instance,
    TransactionListViewCallback cb,
    void* ctx) {
    furi_assert(instance);
    furi_assert(cb);
    instance->cb = cb;
    instance->ctx = ctx;
}
