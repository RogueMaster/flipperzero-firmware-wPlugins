#include "about/about.hpp"

FlipSocialAbout::FlipSocialAbout(ViewDispatcher **viewDispatcher) : widget(nullptr), viewDispatcherRef(viewDispatcher)
{
    easy_flipper_set_widget(&widget, FlipSocialViewAbout, "Welcome to FlipSocial\n---\nThe social media app for\nFlipper Zero, created by\nJBlanked\n\nwww.github.com/jblanked\n---\nPress BACK to return.", callbackToSubmenu, viewDispatcherRef);
}

FlipSocialAbout::~FlipSocialAbout()
{
    if (widget && viewDispatcherRef && *viewDispatcherRef)
    {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipSocialViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t FlipSocialAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipSocialViewSubmenu;
}
