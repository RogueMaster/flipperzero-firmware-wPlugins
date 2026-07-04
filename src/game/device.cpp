#include "game.h"
#include "../plugin_api.h"

#include <furi.h>
#include <flipper_application/flipper_application.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <input/input.h>

#include <new>
#include <string.h>

namespace flipcraft {
namespace device {

static void delayMs(uint32_t ms) {
    furi_delay_ms(ms);
}

enum KeyIndex { B_UP = 0, B_DOWN, B_LEFT, B_RIGHT, B_OK, B_BACK, KEY_COUNT };

static constexpr uint8_t kUp = 1u << B_UP;
static constexpr uint8_t kDown = 1u << B_DOWN;
static constexpr uint8_t kLeft = 1u << B_LEFT;
static constexpr uint8_t kRight = 1u << B_RIGHT;
static constexpr uint8_t kOk = 1u << B_OK;
static constexpr uint8_t kBack = 1u << B_BACK;
static constexpr uint8_t kDpad = kUp | kDown | kLeft | kRight;

static constexpr uint32_t LONG_PRESS_MS = 300;
static constexpr uint32_t REPEAT_DELAY_MS = 260;
static constexpr uint32_t REPEAT_RATE_MS = 110;

static constexpr uint32_t GAME_TICK_MS = 80;

struct AppState {
    Game* game = nullptr;
    FuriMutex* mutex = nullptr;    
    FuriMutex* inputMutex = nullptr;
    ViewPort* view_port = nullptr;

    uint8_t present[SCREEN_WIDTH * SCREEN_HEIGHT / 8] = {};
    ScreenId presentScreen = SCR_PLAY;
    const char* presentName = nullptr;

    uint8_t held = 0;
    uint8_t pressLatch = 0;
    uint8_t releaseLatch = 0;
    uint32_t downTick[KEY_COUNT] = {};
    uint32_t holdDur[KEY_COUNT] = {};

    bool okConsumed = false;
    bool okLongFired = false;
    bool backConsumed = false;
    bool backLongFired = false;
    bool exitFired = false;
    uint32_t dirNextRepeat[4] = {};

    bool ev_exit = false;
};

// Pack the framebuffer into the Flipper canvas buffer: 8 vertical pixels per
// byte. Only bit 0 of each source byte is colour (bits 1-7 hold z-depth).
static void packFramebuffer(const Framebuffer& fb, uint8_t* dst) {
    for(int page = 0; page < (SCREEN_HEIGHT / 8); ++page) {
        const uint8_t* r0 = fb.px[page * 8 + 0];
        const uint8_t* r1 = fb.px[page * 8 + 1];
        const uint8_t* r2 = fb.px[page * 8 + 2];
        const uint8_t* r3 = fb.px[page * 8 + 3];
        const uint8_t* r4 = fb.px[page * 8 + 4];
        const uint8_t* r5 = fb.px[page * 8 + 5];
        const uint8_t* r6 = fb.px[page * 8 + 6];
        const uint8_t* r7 = fb.px[page * 8 + 7];
        uint8_t* out = dst + page * SCREEN_WIDTH;
        for(int col = 0; col < SCREEN_WIDTH; ++col) {
            out[col] = static_cast<uint8_t>(
                (r0[col] & 1)        | ((r1[col] & 1) << 1) |
                ((r2[col] & 1) << 2) | ((r3[col] & 1) << 3) |
                ((r4[col] & 1) << 4) | ((r5[col] & 1) << 5) |
                ((r6[col] & 1) << 6) | ((r7[col] & 1) << 7));
        }
    }
}

// Game thread, after a finished render: publish the packed frame and the
// overlay snapshot. The only writer of present[]/presentScreen/presentName.
static void presentFrame(AppState* st) {
    Game* g = st->game;
    const char* name = (g->screenId == SCR_PLAY && g->hudItemTicks) ?
                           itemName(g->pl.inventory[g->pl.invSlot].type) :
                           nullptr;
    furi_mutex_acquire(st->mutex, FuriWaitForever);
    packFramebuffer(g->fb, st->present);
    st->presentScreen = g->screenId;
    st->presentName = name; // itemName returns static strings, the pointer is stable
    furi_mutex_release(st->mutex);
}

// GUI thread. Never touches the Game object: copies the last finished frame,
// so a redraw landing mid-tick shows the previous frame instead of the
// cleared canvas (the old one-frame white flash). The lock is held for
// microseconds on both sides, so waiting on it is safe here.
static void drawCb(Canvas* canvas, void* ctx) {
    AppState* st = reinterpret_cast<AppState*>(ctx);

    furi_mutex_acquire(st->mutex, FuriWaitForever);
    uint8_t* buf = canvas_get_buffer(canvas);
    if(buf) memcpy(buf, st->present, sizeof(st->present));
    ScreenId screen = st->presentScreen;
    const char* name = st->presentName;
    furi_mutex_release(st->mutex);

    if(screen == SCR_GAMEOVER) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "You died!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignBottom, "Press OK to respawn");
    }

    if(name) {
        canvas_set_font(canvas, FontSecondary);
        int bw = (int)canvas_string_width(canvas, name) + 6, bh = 11;
        int bx = 64 - bw / 2, by = 40;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, bx, by, bw, bh);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, bx, by, bw, bh);
        canvas_draw_str_aligned(canvas, 64, by + bh - 2, AlignCenter, AlignBottom, name);
    }
}

static int keyIndex(InputKey key) {
    switch(key) {
    case InputKeyUp: return B_UP;
    case InputKeyDown: return B_DOWN;
    case InputKeyLeft: return B_LEFT;
    case InputKeyRight: return B_RIGHT;
    case InputKeyOk: return B_OK;
    case InputKeyBack: return B_BACK;
    default: return -1;
    }
}

static void inputCb(InputEvent* ev, void* ctx) {
    AppState* st = reinterpret_cast<AppState*>(ctx);
    int idx = keyIndex(ev->key);
    if(idx < 0) return;

    uint8_t bit = static_cast<uint8_t>(1u << idx);
    uint32_t now = furi_get_tick();

    if(ev->type == InputTypePress) {
        furi_mutex_acquire(st->inputMutex, FuriWaitForever);
        st->held |= bit;
        st->pressLatch |= bit;
        st->downTick[idx] = now;
        furi_mutex_release(st->inputMutex);
    } else if(ev->type == InputTypeRelease) {
        furi_mutex_acquire(st->inputMutex, FuriWaitForever);
        st->held &= ~bit;
        st->releaseLatch |= bit;
        st->holdDur[idx] = now - st->downTick[idx];
        furi_mutex_release(st->inputMutex);
    }
}

static Input pollInput(AppState* st) {
    uint32_t now = furi_get_tick();

    furi_mutex_acquire(st->inputMutex, FuriWaitForever);
    uint8_t held = st->held;
    uint8_t pressLatch = st->pressLatch;
    uint8_t releaseLatch = st->releaseLatch;
    uint32_t downTick[KEY_COUNT];
    uint32_t holdDur[KEY_COUNT];
    for(int i = 0; i < KEY_COUNT; ++i) {
        downTick[i] = st->downTick[i];
        holdDur[i] = st->holdDur[i];
    }
    st->pressLatch = 0;
    st->releaseLatch = 0;
    furi_mutex_release(st->inputMutex);

    const bool okHeld = held & kOk;
    const bool backHeld = held & kBack;
    const bool play = st->game->screenId == SCR_PLAY;

    if(pressLatch & kOk) {
        st->okConsumed = false;
        st->okLongFired = false;
    }
    if(pressLatch & kBack) {
        st->backConsumed = false;
        st->backLongFired = false;
    }

    if(okHeld && backHeld) {
        if(!st->exitFired) {
            st->ev_exit = true;
            st->exitFired = true;
        }
        st->okConsumed = true;
        st->backConsumed = true;
    } else if(!okHeld && !backHeld) {
        st->exitFired = false;
    }

    if(okHeld && (held & kDpad)) st->okConsumed = true;

    auto dirFires = [&](int i, bool active) -> bool {
        uint8_t bit = static_cast<uint8_t>(1u << i);
        if(!active) {
            st->dirNextRepeat[i] = 0;
            return false;
        }
        if(pressLatch & bit) {
            st->dirNextRepeat[i] = now + REPEAT_DELAY_MS;
            return true;
        }
        if(!(held & bit)) {
            st->dirNextRepeat[i] = 0;
            return false;
        }
        if(st->dirNextRepeat[i] == 0) {
            st->dirNextRepeat[i] = now + REPEAT_DELAY_MS;
            return true;
        }
        if(static_cast<int32_t>(now - st->dirNextRepeat[i]) >= 0) {
            st->dirNextRepeat[i] = now + REPEAT_RATE_MS;
            return true;
        }
        return false;
    };

    Input in{};

    if(play) {
        if(okHeld) {
            if(dirFires(B_UP, true)) in.pitch = 1;
            if(dirFires(B_DOWN, true)) in.pitch = -1;
            if(dirFires(B_LEFT, true)) in.slotScroll = -1;
            if(dirFires(B_RIGHT, true)) in.slotScroll = 1;
        } else {
            if(held & kUp) in.forward = 8;
            if(held & kDown) in.forward = -8;
            if(held & kLeft) in.turn = 1;
            if(held & kRight) in.turn = -1;
            for(int i = 0; i < 4; ++i) st->dirNextRepeat[i] = 0;
        }

        if(okHeld && !st->okConsumed && !st->okLongFired &&
           static_cast<int32_t>(now - downTick[B_OK]) >= (int32_t)LONG_PRESS_MS) {
            in.breakPressed = true;
            st->okLongFired = true;
            st->okConsumed = true;
        }
        if((releaseLatch & kOk) && !st->okConsumed && !st->okLongFired &&
           holdDur[B_OK] < LONG_PRESS_MS) {
            in.placePressed = true;
        }

        if(backHeld && !st->backConsumed && !st->backLongFired &&
           static_cast<int32_t>(now - downTick[B_BACK]) >= (int32_t)LONG_PRESS_MS) {
            in.openInventory = true;
            st->backLongFired = true;
            st->backConsumed = true;
        }
        if((releaseLatch & kBack) && !st->backConsumed && !st->backLongFired &&
           holdDur[B_BACK] < LONG_PRESS_MS) {
            in.jump = true;
        }
    } else {
        const bool distribute = okHeld;
        if(dirFires(B_UP, true)) {
            in.navY = 1;
            in.distribute = distribute;
        }
        if(dirFires(B_DOWN, true)) {
            in.navY = -1;
            in.distribute = distribute;
        }
        if(dirFires(B_LEFT, true)) {
            in.navX = -1;
            in.distribute = distribute;
        }
        if(dirFires(B_RIGHT, true)) {
            in.navX = 1;
            in.distribute = distribute;
        }

        if((releaseLatch & kOk) && !st->okConsumed) in.menuSelect = true;
        if((releaseLatch & kBack) && !st->backConsumed) in.openInventory = true;
    }

    return in;
}

static void runGame(Game& game, Gui* gui, const char* path) {
    AppState* st = new(std::nothrow) AppState();
    if(!st) return;

    st->game = &game;
    st->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    st->inputMutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!st->mutex || !st->inputMutex) {
        if(st->mutex) furi_mutex_free(st->mutex);
        if(st->inputMutex) furi_mutex_free(st->inputMutex);
        delete st;
        return;
    }

    GameConfig config = {path};
    if(!game.setup(config)) {
        furi_mutex_free(st->mutex);
        furi_mutex_free(st->inputMutex);
        delete st;
        return;
    }

    // First frame is presented before the viewport exists, so drawCb always
    // has a finished frame to copy.
    game.simulate(Input{});
    game.render();
    presentFrame(st);

    st->view_port = view_port_alloc();
    view_port_draw_callback_set(st->view_port, drawCb, st);
    view_port_input_callback_set(st->view_port, inputCb, st);
    gui_add_view_port(gui, st->view_port, GuiLayerFullscreen);
    view_port_update(st->view_port);

    const uint32_t TICK_MS = GAME_TICK_MS;
    const uint32_t MAX_ACC = TICK_MS * 5;
    uint32_t last = furi_get_tick();
    uint32_t acc = 0;

    while(true) {
        uint32_t now = furi_get_tick();
        acc += now - last;
        last = now;
        if(acc > MAX_ACC) acc = MAX_ACC;

        bool stepped = false;
        while(acc >= TICK_MS) {
            Input in = pollInput(st);
            if(st->ev_exit) break;
            game.simulate(in);
            acc -= TICK_MS;
            stepped = true;
        }
        if(st->ev_exit) break;

        if(stepped && game.render()) {
            presentFrame(st);
            view_port_update(st->view_port);
        }

        uint32_t ahead = (acc < TICK_MS) ? (TICK_MS - acc) : 1;
        delayMs(ahead);
    }

    game.shutdown(); // drawCb only reads the present buffer, no lock needed

    view_port_enabled_set(st->view_port, false);
    gui_remove_view_port(gui, st->view_port);
    view_port_free(st->view_port);
    furi_mutex_free(st->mutex);
    furi_mutex_free(st->inputMutex);
    delete st;
}

}
}

// The Game object (world window, framebuffer, z-buffer, chunk meshes) exists
// only for the duration of one session: while the menu is open none of it is
// allocated, and the whole game code segment itself is unmapped by the host.

static int32_t flipcraft_game_run(const char* world_path) {
    using namespace flipcraft;

    Game* game = new(std::nothrow) Game();
    if(!game) return -1;

    Gui* gui = reinterpret_cast<Gui*>(furi_record_open(RECORD_GUI));
    device::runGame(*game, gui, world_path);
    furi_record_close(RECORD_GUI);

    delete game;
    return 0;
}

static const FlipcraftGameApi flipcraft_game_api = {
    .run = flipcraft_game_run,
};

static const FlipperAppPluginDescriptor flipcraft_game_descriptor = {
    .appid = FLIPCRAFT_GAME_APP_ID,
    .ep_api_version = FLIPCRAFT_GAME_API_VERSION,
    .entry_point = &flipcraft_game_api,
};

extern "C" const FlipperAppPluginDescriptor* flipcraft_game_ep(void) {
    return &flipcraft_game_descriptor;
}
