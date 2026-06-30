#include "view_hijacker.h"
#include <gui/view_stack.h>
#include <gui/view_dispatcher.h>
#include <m-dict.h>

//copied from gui/view_port_i.h

struct ViewPort {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;
    ViewPortOrientation orientation;

    uint8_t width;
    uint8_t height;

    ViewPortDrawCallback draw_callback;
    void* draw_callback_context;

    ViewPortInputCallback input_callback;
    void* input_callback_context;
};

//copied from gui/view_i.h

struct View {
    ViewDrawCallback draw_callback;
    ViewInputCallback input_callback;
    ViewCustomCallback custom_callback;

    ViewModelType model_type;
    ViewNavigationCallback previous_callback;
    ViewCallback enter_callback;
    ViewCallback exit_callback;
    ViewOrientation orientation;

    ViewUpdateCallback update_callback;
    void* update_callback_context;

    void* model;
    void* context;
};

// copied from gui/view_dispatcher_i.h

DICT_DEF2(ViewDict, uint32_t, M_DEFAULT_OPLIST, View*, M_PTR_OPLIST) // NOLINT

struct ViewDispatcher {
    bool is_event_loop_owned;
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* event_queue;

    Gui* gui;
    ViewPort* view_port;
    ViewDict_t views;

    View* current_view;

    View* ongoing_input_view;
    uint8_t ongoing_input;

    ViewDispatcherCustomEventCallback custom_event_callback;
    ViewDispatcherNavigationEventCallback navigation_event_callback;
    ViewDispatcherTickEventCallback tick_event_callback;
    uint32_t tick_period;
    void* event_context;
};

ViewHijacker* alloc_view_hijacker() {
    ViewHijacker* view_hijacker = malloc(sizeof(ViewHijacker));

    view_hijacker->view = NULL;

    view_hijacker->orig_previous_callback = NULL;
    view_hijacker->orig_enter_callback = NULL;
    view_hijacker->orig_exit_callback = NULL;    
    view_hijacker->orig_input_callback = NULL;
    view_hijacker->orig_custom_callback = NULL;
    view_hijacker->orig_context = NULL;

    view_hijacker->new_previous_callback = NULL;
    view_hijacker->new_enter_callback = NULL;
    view_hijacker->new_exit_callback = NULL;    
    view_hijacker->new_input_callback = NULL;
    view_hijacker->new_custom_callback = NULL;

    view_hijacker->new_previous_callback_context = NULL;
    view_hijacker->new_enter_callback_context = NULL;
    view_hijacker->new_exit_callback_context = NULL;
    view_hijacker->new_input_callback_context = NULL;
    view_hijacker->new_custom_callback_context = NULL;

    return view_hijacker;
}

void free_view_hijacker(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    free(view_hijacker);
}

static uint32_t hijacked_previous_callback(void* context) {
    furi_check(context);

    ViewHijacker* view_hijacker = context;

    if (view_hijacker->new_previous_callback)
        return view_hijacker->new_previous_callback(view_hijacker);

    if (view_hijacker->orig_previous_callback)
        return view_hijacker->orig_previous_callback(view_hijacker->orig_context);

    return VIEW_IGNORE;
}

static void hijacked_enter_callback(void* context) {
    furi_check(context);

    ViewHijacker* view_hijacker = context;

    if (view_hijacker->new_enter_callback)
        view_hijacker->new_enter_callback(view_hijacker);
    else
        if (view_hijacker->orig_enter_callback)
            view_hijacker->orig_enter_callback(view_hijacker->orig_context);
}

static void hijacked_exit_callback(void* context) {
    furi_check(context);

    ViewHijacker* view_hijacker = context;

    if (view_hijacker->new_exit_callback)
        view_hijacker->new_exit_callback(view_hijacker);
    else
        if (view_hijacker->orig_exit_callback)
            view_hijacker->orig_exit_callback(view_hijacker->orig_context);
}

static bool hijacked_input_callback(InputEvent *event, void *context) {
    furi_check(context);

    ViewHijacker* view_hijacker = context;

    if (view_hijacker->new_input_callback)
        return view_hijacker->new_input_callback(event, view_hijacker);

    if (view_hijacker->orig_input_callback)
        return view_hijacker->orig_input_callback(event, view_hijacker->orig_context);

    return false;
}

static bool hijacked_custom_callback(uint32_t event, void *context) {
    furi_check(context);

    ViewHijacker* view_hijacker = context;

    if (view_hijacker->new_custom_callback)
        return view_hijacker->new_custom_callback(event, view_hijacker);

    if (view_hijacker->orig_custom_callback)
        return view_hijacker->orig_custom_callback(event, view_hijacker->orig_context);

    return false;
}

void view_hijacker_attach_to_view(ViewHijacker* view_hijacker, View* view) {
    furi_check(view_hijacker);
    furi_check(view);

    view_hijacker->view = view;
    view_hijacker->orig_previous_callback = view->previous_callback;
    view_hijacker->orig_enter_callback = view->enter_callback;
    view_hijacker->orig_exit_callback = view->exit_callback;
    view_hijacker->orig_input_callback = view->input_callback;
    view_hijacker->orig_custom_callback = view->custom_callback;
    view_hijacker->orig_context = view->context;

    view_set_previous_callback(view, hijacked_previous_callback);
    view_set_enter_callback(view, hijacked_enter_callback);
    view_set_exit_callback(view, hijacked_exit_callback);
    view_set_input_callback(view, hijacked_input_callback);
    view_set_custom_callback(view, hijacked_custom_callback);

    view_set_context(view, view_hijacker);
}

void view_hijacker_attach_to_view_dispacher_current(ViewHijacker* view_hijacker, ViewDispatcher* view_dispatcher)
{
    furi_check(view_hijacker);
    furi_check(view_dispatcher);

    view_hijacker_attach_to_view(view_hijacker, view_dispatcher->current_view);
}

void view_hijacker_detach_from_view(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    if (view_hijacker->view) {
        view_hijacker->view->previous_callback = view_hijacker->orig_previous_callback;
        view_hijacker->view->enter_callback = view_hijacker->orig_enter_callback;
        view_hijacker->view->exit_callback = view_hijacker->orig_exit_callback;
        view_hijacker->view->input_callback = view_hijacker->orig_input_callback;
        view_hijacker->view->custom_callback = view_hijacker->orig_custom_callback;

        view_hijacker->view->context = view_hijacker->orig_context;

        view_hijacker->view = NULL;
    }
}

void view_hijacker_hijack_previous_callback(ViewHijacker* view_hijacker, ViewNavigationCallback previous_callback, void* context) {
    furi_check(view_hijacker);

    view_hijacker->new_previous_callback = previous_callback;
    view_hijacker->new_previous_callback_context = context;
}

void view_hijacker_hijack_enter_callback(ViewHijacker* view_hijacker, ViewCallback enter_callback, void* context) {
    furi_check(view_hijacker);

    view_hijacker->new_enter_callback = enter_callback;
    view_hijacker->new_enter_callback_context = context;
}

void view_hijacker_hijack_exit_callback(ViewHijacker* view_hijacker, ViewCallback exit_callback, void* context) {
    furi_check(view_hijacker);

    view_hijacker->new_exit_callback = exit_callback;
    view_hijacker->new_exit_callback_context = context;
}

void view_hijacker_hijack_input_callback(ViewHijacker* view_hijacker, ViewInputCallback input_callback, void* context) {
    furi_check(view_hijacker);

    view_hijacker->new_input_callback = input_callback;
    view_hijacker->new_input_callback_context = context;
}

void view_hijacker_hijack_custom_callback(ViewHijacker* view_hijacker, ViewCustomCallback custom_callback, void* context) {
    furi_check(view_hijacker);

    view_hijacker->new_custom_callback = custom_callback;
    view_hijacker->new_custom_callback_context = context;
}

void view_hijacker_restore_previous_callback(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    view_hijacker->new_previous_callback = NULL;
    view_hijacker->new_previous_callback_context = NULL;
}

void view_hijacker_restore_enter_callback(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    view_hijacker->new_enter_callback = NULL;
    view_hijacker->new_enter_callback_context = NULL;
}

void view_hijacker_restore_exit_callback(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    view_hijacker->new_exit_callback = NULL;
    view_hijacker->new_exit_callback_context = NULL;
}

void view_hijacker_restore_input_callback(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    view_hijacker->new_input_callback = NULL;
    view_hijacker->new_input_callback_context = NULL;
}

void view_hijacker_restore_custom_callback(ViewHijacker* view_hijacker) {
    furi_check(view_hijacker);

    view_hijacker->new_custom_callback = NULL;
    view_hijacker->new_custom_callback_context = NULL;
}
