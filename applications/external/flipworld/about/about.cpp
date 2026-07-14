#include "about/about.hpp"

FlipWorldAbout::FlipWorldAbout() {
    // nothing to do
}

FlipWorldAbout::~FlipWorldAbout() {
    free();
}

uint32_t FlipWorldAbout::callbackToSubmenu(void* context) {
    UNUSED(context);
    return FlipWorldViewSubmenu;
}

void FlipWorldAbout::free() {
    if(widget && viewDispatcherRef && *viewDispatcherRef) {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipWorldViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

bool FlipWorldAbout::init(ViewDispatcher** viewDispatcher, void* appContext) {
    viewDispatcherRef = viewDispatcher;
    this->appContext = appContext;
    return easy_flipper_set_widget(
        &widget,
        FlipWorldViewAbout,
        "The first open world\nmultiplayer game on the\nFlipper Zero.\n\n\nwww.github.com/jblanked",
        callbackToSubmenu,
        viewDispatcherRef);
}
