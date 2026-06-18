#include "platform.h"

#include "../../game/game.h"
#include "../../menu/menu.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#include <new>
#include <string.h>

namespace flipcraft {
namespace platform {

static constexpr int DISP_W = 128;
static constexpr int DISP_H = 64;
static constexpr int SSD_SZ = DISP_W * DISP_H / 8;
static_assert(SCREEN_WIDTH == DISP_W && SCREEN_HEIGHT == DISP_H);

struct F7File {
    Storage* storage = nullptr;
    ::File* file = nullptr;
};

static void* fsOpen(void*, const char* path, FileMode mode) {
    Storage* storage = reinterpret_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    ::File* file = storage_file_alloc(storage);

    FS_AccessMode access = FSAM_READ;
    FS_OpenMode open = FSOM_OPEN_EXISTING;
    switch(mode) {
    case FileMode::Read:
        access = FSAM_READ;
        open = FSOM_OPEN_EXISTING;
        break;
    case FileMode::WriteTruncate:
        access = FSAM_WRITE;
        open = FSOM_CREATE_ALWAYS;
        break;
    case FileMode::ReadWriteExisting:
        access = FSAM_READ_WRITE;
        open = FSOM_OPEN_EXISTING;
        break;
    }

    if(!storage_file_open(file, path, access, open)) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return nullptr;
    }

    F7File* out = new(std::nothrow) F7File();
    if(!out) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return nullptr;
    }

    out->storage = storage;
    out->file = file;
    return out;
}

static void fsClose(void*, void* handle) {
    F7File* file = reinterpret_cast<F7File*>(handle);
    if(!file) return;
    storage_file_close(file->file);
    storage_file_free(file->file);
    furi_record_close(RECORD_STORAGE);
    delete file;
}

static bool fsSeek(void*, void* handle, uint32_t offset) {
    F7File* file = reinterpret_cast<F7File*>(handle);
    return file && storage_file_seek(file->file, offset, true);
}

static size_t fsRead(void*, void* handle, void* data, size_t size) {
    F7File* file = reinterpret_cast<F7File*>(handle);
    if(!file) return 0;
    return storage_file_read(file->file, data, size);
}

static size_t fsWrite(void*, void* handle, const void* data, size_t size) {
    F7File* file = reinterpret_cast<F7File*>(handle);
    if(!file) return 0;
    return storage_file_write(file->file, data, size);
}

static uint32_t fsSize(void*, void* handle) {
    F7File* file = reinterpret_cast<F7File*>(handle);
    if(!file) return 0;
    return storage_file_size(file->file);
}

static void fsSync(void*, void* handle) {
    F7File* file = reinterpret_cast<F7File*>(handle);
    if(file) storage_file_sync(file->file);
}

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

struct AppState {
    Game* game = nullptr;
    FuriMutex* mutex = nullptr;
    FuriMutex* inputMutex = nullptr;
    ViewPort* view_port = nullptr;

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

static void packSsd(const Framebuffer& fb, uint8_t* ssd) {
    memset(ssd, 0, SSD_SZ);
    for(int page = 0; page < (SCREEN_HEIGHT / 8); ++page) {
        for(int col = 0; col < SCREEN_WIDTH; ++col) {
            uint8_t byte = 0;
            for(int bit = 0; bit < 8; ++bit) {
                if(fb.px[page * 8 + bit][col]) byte |= static_cast<uint8_t>(1u << bit);
            }
            ssd[page * DISP_W + col] = byte;
        }
    }
}

static void drawCb(Canvas* canvas, void* ctx) {
    AppState* st = reinterpret_cast<AppState*>(ctx);

    if(furi_mutex_acquire(st->mutex, 0) != FuriStatusOk) return;

    uint8_t* ssd = canvas_get_buffer(canvas);
    if(ssd) packSsd(st->game->fb, ssd);

    furi_mutex_release(st->mutex);
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

static const FileSystem g_files = {
    nullptr,
    fsOpen,
    fsClose,
    fsSeek,
    fsRead,
    fsWrite,
    fsSize,
    fsSync,
};

// Run a single game session for the save at `path`. Returns to the caller (the
// menu loop) when the player quits. Opening a save that fails to load is not
// fatal: we simply return so the menu can be shown again.
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

    GameConfig config = {&g_files, path};
    if(!game.setup(config)) {
        furi_mutex_free(st->mutex);
        furi_mutex_free(st->inputMutex);
        delete st;
        return;
    }

    st->view_port = view_port_alloc();
    view_port_draw_callback_set(st->view_port, drawCb, st);
    view_port_input_callback_set(st->view_port, inputCb, st);
    gui_add_view_port(gui, st->view_port, GuiLayerFullscreen);

    furi_mutex_acquire(st->mutex, FuriWaitForever);
    game.frame(Input{});
    furi_mutex_release(st->mutex);
    view_port_update(st->view_port);

    while(true) {
        Input in = pollInput(st);
        if(st->ev_exit) break;

        furi_mutex_acquire(st->mutex, FuriWaitForever);
        game.frame(in);
        furi_mutex_release(st->mutex);

        view_port_update(st->view_port);
        delayMs(33);
    }

    furi_mutex_acquire(st->mutex, FuriWaitForever);
    game.shutdown();
    furi_mutex_release(st->mutex);

    view_port_enabled_set(st->view_port, false);
    gui_remove_view_port(gui, st->view_port);
    view_port_free(st->view_port);
    furi_mutex_free(st->mutex);
    furi_mutex_free(st->inputMutex);
    delete st;
}

int32_t run(Game& game) {
    Storage* storage = reinterpret_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    Gui* gui = reinterpret_cast<Gui*>(furi_record_open(RECORD_GUI));

    // Show the save selector, play the chosen world, then return to the menu
    // until the player leaves it.
    while(true) {
        menu::Result choice = menu::run(gui, storage);
        if(!choice.launch) break;
        runGame(game, gui, choice.path);
    }

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    return 0;
}

}
}
