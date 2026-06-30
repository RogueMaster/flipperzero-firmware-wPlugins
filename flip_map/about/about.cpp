#include "about/about.hpp"

FlipMapAbout::FlipMapAbout(ViewDispatcher **viewDispatcher) : widget(nullptr), viewDispatcherRef(viewDispatcher)
{
    easy_flipper_set_widget(&widget, FlipMapViewAbout, "Find Flipper Zero Users\n\n\n\n\nwww.github.com/jblanked", callbackToSubmenu, viewDispatcherRef);
}

FlipMapAbout::~FlipMapAbout()
{
    if (widget && viewDispatcherRef && *viewDispatcherRef)
    {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipMapViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t FlipMapAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipMapViewSubmenu;
}
