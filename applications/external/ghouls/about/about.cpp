#include "about/about.hpp"

GhoulsAbout::GhoulsAbout() {
    // nothing to do
}

GhoulsAbout::~GhoulsAbout() {
    free();
}

uint32_t GhoulsAbout::callbackToSubmenu(void* context) {
    UNUSED(context);
    return GhoulsViewSubmenu;
}

bool GhoulsAbout::init(ViewDispatcher** viewDispatcher, void* appContext) {
    viewDispatcherRef = viewDispatcher;
    this->appContext = appContext;
    return easy_flipper_set_widget(
        &widget,
        GhoulsViewAbout,
        "3D Multiplayer Game for the\nFlipper Zero and Picoware\ncreated by JBlanked\n\n\nwww.github.com/jblanked",
        callbackToSubmenu,
        viewDispatcherRef);
}

void GhoulsAbout::free() {
    if(widget && viewDispatcherRef && *viewDispatcherRef) {
        view_dispatcher_remove_view(*viewDispatcherRef, GhoulsViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}
