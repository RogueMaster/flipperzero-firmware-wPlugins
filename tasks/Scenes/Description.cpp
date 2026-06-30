#include "Description.hpp"

void FTasks::Description::enter(void* context) noexcept
{
    auto* popup = GET_WIDGET_P(context, UFZ::Popup, Scenes::DESCRIPTION);
    const auto ctx = CTX(popup->application->getUserPointer());
    popup->reset();
    UNUSED(popup->setContext(popup->application)
            .setHeader("Description:", 64, 4, AlignCenter, AlignTop)
            .setIcon(-1, -1, nullptr)
            .setText((*ctx->currentContainer)[ctx->currentNoteIndex].second.c_str(), 4, 16, AlignLeft, AlignTop));
    RENDER_VIEW(popup->application, Scenes::DESCRIPTION);
}

bool FTasks::Description::event(void* context, const SceneManagerEvent event) noexcept
{
    UNUSED(context); UNUSED(event);
    return false;
}

void FTasks::Description::exit(void* context) noexcept
{
    GET_WIDGET_P(context, UFZ::Popup, Scenes::DESCRIPTION)->reset();
}