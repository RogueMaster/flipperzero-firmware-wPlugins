#include "List.hpp"

void FTasks::List::callback(void* context, const uint32_t index) noexcept
{
    SEND_CUSTOM_EVENT(static_cast<UFZ::Application*>(context), index);
}

void FTasks::List::viewInputEvent(UFZ::Application& application, const UFZ::View& view) noexcept
{
    UNUSED(view.setContext(&application).setInputCallback(viewInputEventCallback));
}

bool FTasks::List::viewInputEventCallback(InputEvent* event, void* context) noexcept
{
    if (event == nullptr || context == nullptr)
        return false;
    if (event->type == InputTypePress)
    {
        if (event->key == InputKeyLeft || event->key == InputKeyRight)
        {
            SEND_CUSTOM_EVENT(static_cast<UFZ::Application*>(context), Scenes::MAIN_MENU);
            return true;
        }
    }
    return false;
}