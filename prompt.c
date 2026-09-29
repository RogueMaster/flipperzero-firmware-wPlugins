#include "prompt.h"

#include <furi.h>
#include <gui/view_holder.h>
#include <gui/modules/text_input.h>

typedef struct {
    FuriSemaphore* done;
    bool confirmed;
} PromptCtx;

// Both callbacks run on the GUI thread; the app thread waits on the semaphore.
static void prompt_accept_cb(void* context) {
    PromptCtx* ctx = context;
    ctx->confirmed = true;
    furi_semaphore_release(ctx->done);
}

static void prompt_back_cb(void* context) {
    PromptCtx* ctx = context;
    ctx->confirmed = false;
    furi_semaphore_release(ctx->done);
}

bool prompt_text(Gui* gui, const char* header, char* buf, size_t cap, size_t min_len) {
    PromptCtx ctx = {
        .done = furi_semaphore_alloc(1, 0),
        .confirmed = false,
    };

    TextInput* text_input = text_input_alloc();
    text_input_set_header_text(text_input, header);
    text_input_set_minimum_length(text_input, min_len);
    text_input_set_result_callback(text_input, prompt_accept_cb, &ctx, buf, cap, false);

    ViewHolder* holder = view_holder_alloc();
    view_holder_attach_to_gui(holder, gui);
    view_holder_set_back_callback(holder, prompt_back_cb, &ctx);
    view_holder_set_view(holder, text_input_get_view(text_input));

    furi_semaphore_acquire(ctx.done, FuriWaitForever);

    view_holder_set_view(holder, NULL);
    view_holder_free(holder);
    text_input_free(text_input);
    furi_semaphore_free(ctx.done);

    return ctx.confirmed;
}
