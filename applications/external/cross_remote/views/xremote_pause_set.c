#include "xremote_pause_set.h"

struct XRemotePauseSet {
    View* view;
    XRemotePauseSetCallback callback;
    void* context;
};

typedef struct {
    int type;
    const char* name;
    int time; // total pause duration in SECONDS (0..XREMOTE_PAUSE_MAX_SECONDS)
    int field; // which field Up/Down edits: minutes or seconds
} XRemotePauseSetModel;

static void xremote_pause_set_model_init(XRemotePauseSetModel* const model) {
    model->type = XRemoteRemoteItemTypePause;
    model->time = 1;
    model->field = XREMOTE_PAUSE_FIELD_SECONDS;
}

// Adjust the active field by `dir` (+1/-1) steps. Held keys (InputTypeRepeat)
// move by XREMOTE_PAUSE_FAST_STEP units so large values are quick to dial in.
// Total is clamped to [0, XREMOTE_PAUSE_MAX_SECONDS].
static void xremote_pause_set_adjust(XRemotePauseSetModel* model, int dir, bool fast) {
    int unit = (model->field == XREMOTE_PAUSE_FIELD_MINUTES) ? 60 : 1;
    int mult = fast ? XREMOTE_PAUSE_FAST_STEP : 1;
    int t = model->time + dir * unit * mult;
    if(t < 0) t = 0;
    if(t > XREMOTE_PAUSE_MAX_SECONDS) t = XREMOTE_PAUSE_MAX_SECONDS;
    model->time = t;
}

void xremote_pause_set_set_callback(
    XRemotePauseSet* instance,
    XRemotePauseSetCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void xremote_pause_set_draw(Canvas* canvas, XRemotePauseSetModel* model) {
    int minutes = model->time / 60;
    int seconds = model->time % 60;
    // Buffers sized for the compiler's worst-case int range (format-truncation);
    // at runtime model->time is clamped to <= 60:00.
    char buf[24];
    char mm[12];
    snprintf(buf, sizeof(buf), PAUSE_TIME_FORMAT, minutes, seconds);
    snprintf(mm, sizeof(mm), "%02d", minutes);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Pause duration");
    canvas_draw_icon(canvas, 59, 22, &I_ButtonUp_10x5);
    canvas_draw_icon(canvas, 59, 44, &I_ButtonDown_10x5);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, buf);

    // Underline the field that Up/Down currently edits.
    int w = canvas_string_width(canvas, buf);
    int x0 = 64 - w / 2;
    int mm_w = canvas_string_width(canvas, mm);
    int colon_w = canvas_string_width(canvas, ":");
    int uy = 41;
    if(model->field == XREMOTE_PAUSE_FIELD_MINUTES) {
        canvas_draw_line(canvas, x0, uy, x0 + mm_w, uy);
    } else {
        int sx = x0 + mm_w + colon_w;
        canvas_draw_line(canvas, sx, uy, sx + canvas_string_width(canvas, "00"), uy);
    }

    // Left/Right arrows flank the time to hint at field selection.
    canvas_draw_icon(canvas, 30, 31, &I_ButtonLeft_4x7);
    canvas_draw_icon(canvas, 94, 31, &I_ButtonRight_4x7);
    elements_button_center(canvas, "Add");
}

bool xremote_pause_set_input(InputEvent* event, void* context) {
    furi_assert(context);
    XRemotePauseSet* instance = context;

    // Up/Down adjust the active field. A quick press (Short) steps by one unit;
    // holding the key (Repeat) accelerates so minute-level values dial in fast.
    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            int dir = (event->key == InputKeyUp) ? 1 : -1;
            bool fast = (event->type == InputTypeRepeat);
            with_view_model(
                instance->view,
                XRemotePauseSetModel * model,
                { xremote_pause_set_adjust(model, dir, fast); },
                true);
        }
        return true;
    }

    if(event->type != InputTypeShort) {
        return true;
    }

    switch(event->key) {
    case InputKeyBack:
        instance->callback(XRemoteCustomEventPauseSetBack, instance->context);
        break;
    case InputKeyLeft:
    case InputKeyRight:
        // Toggle which field (minutes/seconds) Up/Down edits.
        with_view_model(
            instance->view,
            XRemotePauseSetModel * model,
            {
                model->field = (model->field == XREMOTE_PAUSE_FIELD_MINUTES) ?
                                   XREMOTE_PAUSE_FIELD_SECONDS :
                                   XREMOTE_PAUSE_FIELD_MINUTES;
            },
            true);
        break;
    case InputKeyOk:
        with_view_model(
            instance->view,
            XRemotePauseSetModel * model,
            {
                XRemote* app = instance->context;
                xremote_cross_remote_add_pause(app->cross_remote, model->time);
            },
            true);

        instance->callback(XRemoteCustomEventPauseSetOk, instance->context);
        break;
    case InputKeyMAX:
        break;
    default:
        break;
    }
    return true;
}

XRemotePauseSet* xremote_pause_set_alloc() {
    XRemotePauseSet* instance = malloc(sizeof(XRemotePauseSet));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(XRemotePauseSetModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)xremote_pause_set_draw);
    view_set_enter_callback(instance->view, xremote_pause_set_enter);
    view_set_input_callback(instance->view, xremote_pause_set_input);

    with_view_model(
        instance->view,
        XRemotePauseSetModel * model,
        { xremote_pause_set_model_init(model); },
        true);

    return instance;
}

void xremote_pause_set_enter(void* context) {
    furi_assert(context);
    XRemotePauseSet* instance = (XRemotePauseSet*)context;
    with_view_model(
        instance->view,
        XRemotePauseSetModel * model,
        { xremote_pause_set_model_init(model); },
        true);
}

View* xremote_pause_set_get_view(XRemotePauseSet* instance) {
    furi_assert(instance);
    return instance->view;
}

void xremote_pause_set_free(XRemotePauseSet* instance) {
    furi_assert(instance);

    with_view_model(instance->view, XRemotePauseSetModel * model, { UNUSED(model); }, true);
    view_free(instance->view);
    free(instance);
}
