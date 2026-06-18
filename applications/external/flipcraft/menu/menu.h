#pragma once

#include <gui/gui.h>
#include <storage/storage.h>

namespace flipcraft {
namespace menu {

// Result of one menu session.
struct Result {
    bool launch = false;          // true: open the world at `path`
    char path[256] = {0};         // full data path to the .fcw save to open
};

// Show the world-select menu and block until the user either picks a world to
// play (launch=true) or leaves the app (launch=false). Owns its own
// ViewDispatcher; the caller keeps ownership of `gui` and `storage`.
Result run(Gui* gui, Storage* storage);

}
}
