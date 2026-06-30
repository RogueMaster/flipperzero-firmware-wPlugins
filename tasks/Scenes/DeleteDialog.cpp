#include "DeleteDialog.hpp"

void FTasks::DeleteDialog::callback(const DialogExResult result, void* context) noexcept
{
    const auto* app = static_cast<UFZ::Application*>(context);
    if (result == DialogExResultRight)
    {
        const auto* ctx = CTX(app->getUserPointer());
        ctx->currentContainer->erase(ctx->currentContainer->begin() + static_cast<NoteContainer::difference_type>(ctx->currentNoteIndex));
    }
    SEND_CUSTOM_EVENT(app, Scenes::MAIN_MENU);
}

void FTasks::DeleteDialog::enter(void* context) noexcept
{
    auto* popup = GET_WIDGET_P(context, UFZ::DialogEx, Scenes::DELETE);
    auto* ctx = CTX(popup->application->getUserPointer());

    ctx->tmpBuffer = R"(Do you want to delete the following note: ")";
    ctx->tmpBuffer += (*ctx->currentContainer)[ctx->currentNoteIndex].first.c_str(); // Call c_str() because appending doesn't work I guess??
    ctx->tmpBuffer += R"("?)";

    popup->reset();
    popup->setContext(popup->application)
            .setHeader("Delete note?", 64, 4, AlignCenter, AlignTop)
            .setIcon(-1, -1, nullptr)
            .setText(ctx->tmpBuffer.c_str(), 4, 16, AlignLeft, AlignTop)
            .setLeftButtonText("No")
            .setCenterButtonText(nullptr)
            .setRightButtonText("Yes")
            .setResultCallback(callback);
    RENDER_VIEW(popup->application, Scenes::DELETE);
}

bool FTasks::DeleteDialog::event(void* context, const SceneManagerEvent event) noexcept
{
    if (event.type == SceneManagerEventTypeCustom)
    {
        FORCE_NEXT_SCENE(static_cast<UFZ::Application*>(context), event.event);
        return true;
    }
    return false;
}

void FTasks::DeleteDialog::exit(void* context)
{
    GET_WIDGET_P(context, UFZ::DialogEx, Scenes::DELETE)->reset();
}