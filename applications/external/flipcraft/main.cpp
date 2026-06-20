
#include "game/game.h"
#include "platform/f7fz/platform.h"

#include <new>

using namespace flipcraft;

extern "C" int32_t flipcraft_app(void* p) {
    (void)p;
    Game* game = new(std::nothrow) Game();
    if(!game) return -1;

    int32_t result = platform::run(*game);
    delete game;
    return result;
}
