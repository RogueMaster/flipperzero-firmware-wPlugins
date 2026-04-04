#include "about/about.hpp"

VideoGameModuleConsoleAbout::VideoGameModuleConsoleAbout(ViewDispatcher *viewDispatcher) : widget(nullptr), viewDispatcherRef(viewDispatcher)
{
    this->widget = widget_alloc();
    if (!this->widget)
    {
        return;
    }
    widget_add_text_scroll_element(this->widget, 0, 0, 128, 64, "Flipper Zero console for the\nVideo Game Module\n\n\n\nwww.github.com/jblanked");
    view_set_previous_callback(widget_get_view(this->widget), callbackToSubmenu);
    view_dispatcher_add_view(viewDispatcher, VideoGameModuleConsoleViewAbout, widget_get_view(this->widget));
}

VideoGameModuleConsoleAbout::~VideoGameModuleConsoleAbout()
{
    if (widget && viewDispatcherRef)
    {
        view_dispatcher_remove_view(viewDispatcherRef, VideoGameModuleConsoleViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t VideoGameModuleConsoleAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return VideoGameModuleConsoleViewSubmenu;
}
