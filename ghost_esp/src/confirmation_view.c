#include "confirmation_view.h"
#include <gui/elements.h>
#include <furi.h>

struct ConfirmationView {
    View* view;
    ConfirmationViewCallback ok_callback;
    void* ok_callback_context;
    ConfirmationViewCallback cancel_callback;
    void* cancel_callback_context;
};

typedef struct {
    const char* header;
    const char* text;
    uint8_t scroll_position;
    uint8_t total_lines;
    bool can_scroll;
    // Simplified easter egg tracking
    bool easter_egg_active;
    uint8_t sequence_position; // Track position in sequence
} ConfirmationViewModel;

static void confirmation_view_draw_callback(Canvas* canvas, void* _model) {
    if(!canvas || !_model) return;

    ConfirmationViewModel* model = (ConfirmationViewModel*)_model;

    // Border
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 2);

    // Header
    if(model->header) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 5, AlignCenter, AlignTop, model->header);
    }

    // Text area
    canvas_set_font(canvas, FontSecondary);

    // Calculate visible area for text (leave space for header and OK button)
    uint8_t text_y = 20;

    if(model->text) {
        // Create a temporary buffer for visible text
        const size_t max_visible_chars = 128;
        char visible_text[max_visible_chars];
        uint8_t current_line = 0;
        uint8_t visible_lines = 0;
        const char* p = model->text;
        size_t buffer_pos = 0;

        // Skip lines before scroll position
        while(*p && current_line < model->scroll_position) {
            if(*p == '\n') current_line++;
            p++;
        }

        // Copy only the visible portion of text
        while(*p && visible_lines < 4 && buffer_pos < (max_visible_chars - 1)) {
            if(*p == '\n') {
                visible_lines++;
                if(visible_lines >= 4) break;
            }
            visible_text[buffer_pos++] = *p++;
        }
        visible_text[buffer_pos] = '\0';

        // Draw visible portion
        elements_multiline_text_aligned(canvas, 63, text_y, AlignCenter, AlignTop, visible_text);

        // Draw scroll indicators if needed
        if(model->can_scroll) {
            if(model->scroll_position > 0) {
                canvas_draw_str(canvas, 120, text_y, "^");
            }
            if(model->scroll_position < model->total_lines - 4) {
                canvas_draw_str(canvas, 120, 52, "v");
            }
        }
    }

    // Draw OK button at bottom
    canvas_set_font(canvas, FontSecondary);
    elements_button_center(canvas, "OK");
}

static bool confirmation_view_input_callback(InputEvent* event, void* context) {
    if(!event || !context) return false;

    ConfirmationView* instance = (ConfirmationView*)context;
    ConfirmationViewModel* model = view_get_model(instance->view);
    bool consumed = false;

    // Handle regular scroll behavior
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp && model->scroll_position > 0) {
            model->scroll_position--;
            consumed = true;
        } else if(
            event->key == InputKeyDown && model->can_scroll &&
            model->scroll_position < model->total_lines - 4) {
            model->scroll_position++;
            consumed = true;
        } else if(event->key == InputKeyOk) {
            if(instance->ok_callback) {
                instance->ok_callback(instance->ok_callback_context);
            }
            consumed = true;
        } else if(event->key == InputKeyBack) {
            if(instance->cancel_callback) {
                instance->cancel_callback(instance->cancel_callback_context);
            }
            consumed = true;
        }
    }

    view_commit_model(instance->view, consumed);
    return consumed;
}
__attribute__((used)) ConfirmationView* confirmation_view_alloc(void) {
    ConfirmationView* instance = malloc(sizeof(ConfirmationView));
    if(!instance) return NULL;

    instance->view = view_alloc();
    if(!instance->view) {
        free(instance);
        return NULL;
    }

    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, confirmation_view_draw_callback);
    view_set_input_callback(instance->view, confirmation_view_input_callback);

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ConfirmationViewModel));

    with_view_model(
        instance->view,
        ConfirmationViewModel * model,
        {
            model->header = NULL;
            model->text = NULL;
            model->scroll_position = 0;
            model->total_lines = 0;
            model->can_scroll = false;
            model->easter_egg_active = false;
            model->sequence_position = 0;
        },
        true);

    return instance;
}

__attribute__((used)) void confirmation_view_free(ConfirmationView* instance) {
    if(!instance) return;
    if(instance->view) view_free(instance->view);
    free(instance);
}

__attribute__((used)) View* confirmation_view_get_view(ConfirmationView* instance) {
    return instance ? instance->view : NULL;
}

__attribute__((used)) void
    confirmation_view_set_header(ConfirmationView* instance, const char* text) {
    if(!instance || !instance->view) return;
    with_view_model(
        instance->view, ConfirmationViewModel * model, { model->header = text; }, true);
}

__attribute__((used)) void
    confirmation_view_set_text(ConfirmationView* instance, const char* text) {
    if(!instance || !instance->view) return;
    with_view_model(
        instance->view,
        ConfirmationViewModel * model,
        {
            model->text = text;
            model->scroll_position = 0;

            if(text) {
                uint8_t lines = 1;
                const char* p = text;
                while(*p) {
                    if(*p == '\n') lines++;
                    p++;
                }
                model->total_lines = lines;
                model->can_scroll = (lines > 4);
            } else {
                model->total_lines = 0;
                model->can_scroll = false;
            }
        },
        true);
}

__attribute__((used)) void confirmation_view_set_ok_callback(
    ConfirmationView* instance,
    ConfirmationViewCallback callback,
    void* context) {
    if(!instance) {
        FURI_LOG_E("ConfView", "Null instance in set_ok_callback");
        return;
    }
    FURI_LOG_D("ConfView", "Setting OK callback: %p with context: %p", callback, context);
    instance->ok_callback = callback;
    instance->ok_callback_context = context;
}

__attribute__((used)) void confirmation_view_set_cancel_callback(
    ConfirmationView* instance,
    ConfirmationViewCallback callback,
    void* context) {
    if(!instance) {
        FURI_LOG_E("ConfView", "Null instance in set_cancel_callback");
        return;
    }
    FURI_LOG_D("ConfView", "Setting Cancel callback: %p with context: %p", callback, context);
    instance->cancel_callback = callback;
    instance->cancel_callback_context = context;
}
