#pragma once
#include "Scenes.hpp"

namespace FTasks::DeleteDialog
{
    void callback(DialogExResult result, void* context) noexcept;

    void enter(void* context) noexcept;
    bool event(void* context, SceneManagerEvent event) noexcept;
    void exit(void* context);
}