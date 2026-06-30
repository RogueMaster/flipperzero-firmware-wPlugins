#include "Input.hpp"

bool FTasks::Input::event(void* context, const SceneManagerEvent event) noexcept
{
    if (event.type == SceneManagerEventTypeCustom)
    {
        FORCE_NEXT_SCENE(static_cast<UFZ::Application*>(context), event.event);
        return true;
    }
    return false;
}