#include "about/about.hpp"

FlipTelegramAbout::FlipTelegramAbout(ViewDispatcher** viewDispatcher)
    : widget(nullptr)
    , viewDispatcherRef(viewDispatcher) {
    easy_flipper_set_widget(
        &widget,
        FlipTelegramViewAbout,
        "Flipper Zero Telegram Client\n\n\n\n\nwww.github.com/jblanked",
        callbackToSubmenu,
        viewDispatcherRef);
}

FlipTelegramAbout::~FlipTelegramAbout() {
    if(widget && viewDispatcherRef && *viewDispatcherRef) {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipTelegramViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t FlipTelegramAbout::callbackToSubmenu(void* context) {
    UNUSED(context);
    return FlipTelegramViewSubmenu;
}
