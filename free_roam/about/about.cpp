#include "about/about.hpp"

FreeRoamAbout::FreeRoamAbout()
{
    // nothing to do
}

FreeRoamAbout::~FreeRoamAbout()
{
    free();
}

uint32_t FreeRoamAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FreeRoamViewSubmenu;
}

bool FreeRoamAbout::init(ViewDispatcher **viewDispatcher, void *appContext)
{
    viewDispatcherRef = viewDispatcher;
    this->appContext = appContext;
    return easy_flipper_set_widget(&widget, FreeRoamViewAbout, "3D Multiplayer Game for the\nFlipper Zero and Picoware\ncreated by JBlanked\n\n\nwww.github.com/jblanked", callbackToSubmenu, viewDispatcherRef);
}

void FreeRoamAbout::free()
{
    if (widget && viewDispatcherRef && *viewDispatcherRef)
    {
        view_dispatcher_remove_view(*viewDispatcherRef, FreeRoamViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}
