#include "about/about.hpp"

FlipDownloaderAbout::FlipDownloaderAbout()
{
    // nothing to do
}

FlipDownloaderAbout::~FlipDownloaderAbout()
{
    free();
}

uint32_t FlipDownloaderAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipDownloaderViewSubmenu;
}

bool FlipDownloaderAbout::init(ViewDispatcher **viewDispatcher, void *appContext)
{
    viewDispatcherRef = viewDispatcher;
    this->appContext = appContext;
    return easy_flipper_set_widget(&widget, FlipDownloaderViewAbout, "Welcome to FlipDownloader!\n------\nDownload apps and assets via WiFi\nand run them on your Flipper!\n------\nwww.github.com/jblanked", callbackToSubmenu, viewDispatcherRef);
}

void FlipDownloaderAbout::free()
{
    if (widget && viewDispatcherRef && *viewDispatcherRef)
    {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipDownloaderViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}
