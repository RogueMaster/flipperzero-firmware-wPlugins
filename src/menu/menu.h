#pragma once

#include <gui/gui.h>
#include <storage/storage.h>

namespace flipcraft {
namespace menu {

enum class Action { Quit, Launch, Generate };

struct Result {
    Action action = Action::Quit;
    char path[256] = {0}; // full data path of the .fcw save to open or create
    uint8_t chunks = 16;  // requested world size for Action::Generate
    uint32_t seed = 0;    // parsed seed for Action::Generate
};

// Owns its own ViewDispatcher; the caller keeps ownership of gui and storage.
Result run(Gui* gui, Storage* storage);

}
}
