#include "about/about.hpp"

FlipGeminiAbout::FlipGeminiAbout(ViewDispatcher** viewDispatcher)
    : widget(nullptr)
    , viewDispatcherRef(viewDispatcher) {
    easy_flipper_set_widget(
        &widget,
        FlipGeminiViewAbout,
        "Chat with Google's Gemini AI\non your Flipper Zero\n\n\n\nwww.github.com/jblanked",
        callbackToSubmenu,
        viewDispatcherRef);
}

FlipGeminiAbout::~FlipGeminiAbout() {
    if(widget && viewDispatcherRef && *viewDispatcherRef) {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipGeminiViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t FlipGeminiAbout::callbackToSubmenu(void* context) {
    UNUSED(context);
    return FlipGeminiViewSubmenu;
}
