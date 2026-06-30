#pragma once
#include "Scenes.hpp"

namespace FTasks::Description
{
    void enter(void* context) noexcept;
    bool event(void* context, SceneManagerEvent event) noexcept;
    void exit(void* context) noexcept;
}