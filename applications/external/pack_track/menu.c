#include "menu.h"

#include <furi.h>
#include <gui/view_holder.h>
#include <gui/modules/submenu.h>

typedef struct {
    FuriSemaphore* done;
    int32_t chosen;
} MenuCtx;

// Both callbacks run on the GUI thread; the app thread waits on the semaphore.
static void menu_item_cb(void* context, uint32_t index) {
    MenuCtx* ctx = context;
    ctx->chosen = (int32_t)index;
    furi_semaphore_release(ctx->done);
}

static void menu_back_cb(void* context) {
    MenuCtx* ctx = context;
    ctx->chosen = -1;
    furi_semaphore_release(ctx->done);
}

int32_t menu_pick(Gui* gui, const char* header, const char* const* items, size_t count) {
    MenuCtx ctx = {
        .done = furi_semaphore_alloc(1, 0),
        .chosen = -1,
    };

    Submenu* submenu = submenu_alloc();
    if(header) submenu_set_header(submenu, header);
    for(size_t i = 0; i < count; i++) {
        submenu_add_item(submenu, items[i], (uint32_t)i, menu_item_cb, &ctx);
    }

    ViewHolder* holder = view_holder_alloc();
    view_holder_attach_to_gui(holder, gui);
    view_holder_set_back_callback(holder, menu_back_cb, &ctx);
    view_holder_set_view(holder, submenu_get_view(submenu));

    furi_semaphore_acquire(ctx.done, FuriWaitForever);

    view_holder_set_view(holder, NULL);
    view_holder_free(holder);
    submenu_free(submenu);
    furi_semaphore_free(ctx.done);

    return ctx.chosen;
}
