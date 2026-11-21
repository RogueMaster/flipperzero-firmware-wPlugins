#include "rfs_remote_view.h"

#include <gui/elements.h>

#define ROWS       4
#define ROW_HEIGHT 10
#define LIST_TOP   13

struct RfsRemoteView {
    View* view;
    RfsRemoteViewCallback callback;
    void* context;
};

typedef struct {
    const RfsRemote* remote;
    const RfsSignalIndex* index;
    int32_t selected;
    int32_t top;
    char status[28];
    RfsStatusKind status_kind;
} RfsRemoteViewModel;

/* Draw text clipped to max_width, ending in "..." when it does not fit. */
static void
    rfs_draw_fit(Canvas* canvas, int32_t x, int32_t y, const char* text, size_t max_width) {
    char buffer[RFS_NAME_LEN + 4];
    strlcpy(buffer, text, sizeof(buffer));
    if(canvas_string_width(canvas, buffer) <= max_width) {
        canvas_draw_str(canvas, x, y, buffer);
        return;
    }
    size_t len = strlen(buffer);
    while(len > 0) {
        len--;
        buffer[len] = '\0';
        char trial[RFS_NAME_LEN + 8];
        snprintf(trial, sizeof(trial), "%s...", buffer);
        if(canvas_string_width(canvas, trial) <= max_width) {
            canvas_draw_str(canvas, x, y, trial);
            return;
        }
    }
}

static bool rfs_button_missing(const RfsRemoteViewModel* model, const RfsButton* button) {
    if(!button->signal_id[0]) return true;
    if(!model->index) return false;
    for(size_t i = 0; i < model->index->count; i++) {
        if(strcmp(model->index->items[i].id, button->signal_id) == 0) return false;
    }
    return true;
}

static void rfs_remote_view_draw(Canvas* canvas, void* model_ptr) {
    RfsRemoteViewModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(!model->remote) return;

    canvas_set_font(canvas, FontPrimary);
    rfs_draw_fit(canvas, 0, 9, model->remote->name, 127);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    canvas_set_font(canvas, FontSecondary);
    int32_t count = model->remote->count;
    if(count == 0) {
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignBottom, "No buttons yet.");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, "Hold OK to add one.");
    } else {
        for(int32_t row = 0; row < ROWS; row++) {
            int32_t index = model->top + row;
            if(index >= count) break;
            const RfsButton* button = &model->remote->buttons[index];
            int32_t y = LIST_TOP + row * ROW_HEIGHT;
            bool selected = index == model->selected;
            if(selected) {
                canvas_draw_box(canvas, 0, y, 123, ROW_HEIGHT);
                canvas_set_color(canvas, ColorWhite);
            }
            int32_t x = 3;
            if(rfs_button_missing(model, button)) {
                canvas_draw_str(canvas, x, y + 8, "!");
                x += 6;
            }
            rfs_draw_fit(canvas, x, y + 8, button->name, 118 - x);
            canvas_set_color(canvas, ColorBlack);
        }
        elements_scrollbar_pos(canvas, 128, 12, ROWS * ROW_HEIGHT, model->selected, count);
    }

    canvas_draw_line(canvas, 0, 53, 127, 53);
    char position[28];
    if(count > 0) {
        snprintf(
            position, sizeof(position), "%ld / %ld", (long)(model->selected + 1), (long)count);
    } else {
        strlcpy(position, "0 / 0", sizeof(position));
    }
    canvas_draw_str(canvas, 0, 63, position);
    if(model->status[0]) {
        if(model->status_kind == RfsStatusError) canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, model->status);
    }
}

static void rfs_remote_view_move(RfsRemoteViewModel* model, int32_t delta, bool wrap) {
    int32_t count = model->remote ? (int32_t)model->remote->count : 0;
    if(count == 0) return;
    int32_t next = model->selected + delta;
    if(wrap) {
        if(next < 0) next = count - 1;
        if(next >= count) next = 0;
    } else {
        if(next < 0) next = 0;
        if(next >= count) next = count - 1;
    }
    model->selected = next;
    if(model->selected < model->top) model->top = model->selected;
    if(model->selected >= model->top + ROWS) model->top = model->selected - ROWS + 1;
    /* Moving clears a stale status message. */
    if(model->status_kind != RfsStatusBusy) {
        model->status[0] = '\0';
        model->status_kind = RfsStatusNone;
    }
}

static bool rfs_remote_view_input(InputEvent* event, void* context) {
    RfsRemoteView* view = context;
    bool consumed = false;

    if(event->key == InputKeyUp || event->key == InputKeyDown || event->key == InputKeyLeft ||
       event->key == InputKeyRight) {
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            int32_t delta = 0;
            bool wrap = event->type == InputTypeShort;
            switch(event->key) {
            case InputKeyUp:
                delta = -1;
                break;
            case InputKeyDown:
                delta = 1;
                break;
            case InputKeyLeft:
                delta = -ROWS;
                wrap = false;
                break;
            default:
                delta = ROWS;
                wrap = false;
                break;
            }
            with_view_model(
                view->view,
                RfsRemoteViewModel * model,
                { rfs_remote_view_move(model, delta, wrap); },
                true);
        }
        consumed = true;
    } else if(event->key == InputKeyOk) {
        if(event->type == InputTypeShort) {
            if(view->callback) view->callback(RfsRemoteViewEventSend, view->context);
        } else if(event->type == InputTypeLong) {
            if(view->callback) view->callback(RfsRemoteViewEventMenu, view->context);
        }
        consumed = true;
    }
    return consumed;
}

RfsRemoteView* rfs_remote_view_alloc(void) {
    RfsRemoteView* view = malloc(sizeof(RfsRemoteView));
    view->callback = NULL;
    view->context = NULL;
    view->view = view_alloc();
    view_allocate_model(view->view, ViewModelTypeLocking, sizeof(RfsRemoteViewModel));
    view_set_context(view->view, view);
    view_set_draw_callback(view->view, rfs_remote_view_draw);
    view_set_input_callback(view->view, rfs_remote_view_input);
    with_view_model(
        view->view,
        RfsRemoteViewModel * model,
        { memset(model, 0, sizeof(RfsRemoteViewModel)); },
        false);
    return view;
}

void rfs_remote_view_free(RfsRemoteView* view) {
    view_free(view->view);
    free(view);
}

View* rfs_remote_view_get_view(RfsRemoteView* view) {
    return view->view;
}

void rfs_remote_view_set_callback(
    RfsRemoteView* view,
    RfsRemoteViewCallback callback,
    void* context) {
    view->callback = callback;
    view->context = context;
}

void rfs_remote_view_set_remote(
    RfsRemoteView* view,
    const RfsRemote* remote,
    const RfsSignalIndex* index,
    int32_t selected) {
    with_view_model(
        view->view,
        RfsRemoteViewModel * model,
        {
            model->remote = remote;
            model->index = index;
            int32_t count = remote ? (int32_t)remote->count : 0;
            if(selected >= count) selected = count - 1;
            if(selected < 0) selected = 0;
            model->selected = selected;
            if(model->top > model->selected) model->top = model->selected;
            if(model->selected >= model->top + ROWS) model->top = model->selected - ROWS + 1;
            if(model->top < 0) model->top = 0;
            model->status[0] = '\0';
            model->status_kind = RfsStatusNone;
        },
        true);
}

int32_t rfs_remote_view_get_selected(RfsRemoteView* view) {
    int32_t selected = 0;
    with_view_model(
        view->view, RfsRemoteViewModel * model, { selected = model->selected; }, false);
    return selected;
}

void rfs_remote_view_set_status(RfsRemoteView* view, const char* text, RfsStatusKind kind) {
    with_view_model(
        view->view,
        RfsRemoteViewModel * model,
        {
            strlcpy(model->status, text ? text : "", sizeof(model->status));
            model->status_kind = kind;
        },
        true);
}
