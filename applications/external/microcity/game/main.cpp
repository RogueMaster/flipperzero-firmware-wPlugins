#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>

#include "Defines.h"
#include "Draw.h"
#include "Interface.h"
#include "Game.h"
#include "Simulation.h"

#define MICROCITY_FRAME_RATE 25
#define MICROCITY_SAVE_PATH  APP_DATA_PATH("eeprom.bin")
#define MICROCITY_SAVE_MAGIC "CTY1"

#define SCREEN_STRIDE DISPLAY_WIDTH
#define SCREEN_PAGES  (DISPLAY_HEIGHT / 8)
#define SCREEN_SIZE   (SCREEN_STRIDE * SCREEN_PAGES)

typedef struct {
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;
    ViewPort* view_port;
    Gui* gui;
    bool running;
    uint8_t held;
    uint8_t latched;
    uint8_t input;
    bool back_long;
} MicroCityApp;

static uint8_t ScreenBuffer[SCREEN_SIZE];
static uint8_t PowerGrid[SCREEN_SIZE];

void PutPixel(uint8_t x, uint8_t y, uint8_t colour) {
    if(x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;

    uint8_t* dst = ScreenBuffer + (uint16_t)(y >> 3) * SCREEN_STRIDE + x;
    const uint8_t mask = (uint8_t)(1u << (y & 7));

    if(colour) {
        *dst = (uint8_t)(*dst & ~mask);
    } else {
        *dst = (uint8_t)(*dst | mask);
    }
}

void DrawBitmap(const uint8_t* bmp, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    const uint8_t pages = (uint8_t)((h + 7) >> 3);

    for(uint8_t page = 0; page < pages; page++) {
        for(uint8_t col = 0; col < w; col++) {
            uint8_t bits = bmp[(uint16_t)page * w + col];
            uint8_t row = (uint8_t)(page << 3);

            while(bits) {
                if((bits & 1) && row < h) PutPixel((uint8_t)(x + col), (uint8_t)(y + row), 1);
                bits >>= 1;
                row++;
            }
        }
    }
}

uint8_t* GetPowerGrid() {
    return PowerGrid;
}

void SaveCity() {
    Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, MICROCITY_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, MICROCITY_SAVE_MAGIC, 4);
        storage_file_write(file, &State, sizeof(GameState));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool LoadCity() {
    Storage* storage = (Storage*)furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool loaded = false;

    if(storage_file_open(file, MICROCITY_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char magic[4];
        if(storage_file_read(file, magic, sizeof(magic)) == sizeof(magic) &&
           memcmp(magic, MICROCITY_SAVE_MAGIC, sizeof(magic)) == 0) {
            loaded = storage_file_read(file, &State, sizeof(GameState)) == sizeof(GameState);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return loaded;
}

static MicroCityApp* app_instance = NULL;

uint8_t GetInput() {
    return app_instance ? app_instance->input : 0;
}

static uint8_t button_from_key(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return INPUT_UP;
    case InputKeyDown:
        return INPUT_DOWN;
    case InputKeyLeft:
        return INPUT_LEFT;
    case InputKeyRight:
        return INPUT_RIGHT;
    case InputKeyOk:
        return INPUT_B;
    default:
        return 0;
    }
}

static void input_callback(InputEvent* event, void* context) {
    MicroCityApp* app = (MicroCityApp*)context;
    furi_message_queue_put(app->input_queue, event, 0);
}

static void input_apply(MicroCityApp* app, const InputEvent* event) {
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            app->latched |= INPUT_A;
        } else if(event->type == InputTypeLong) {
            app->back_long = true;
        }
        return;
    }

    const uint8_t button = button_from_key(event->key);
    if(button == 0) return;

    switch(event->type) {
    case InputTypePress:
        app->held |= button;
        app->latched |= button;
        break;
    case InputTypeRelease:
        app->held = (uint8_t)(app->held & ~button);
        break;
    default:
        break;
    }
}

static void draw_callback(Canvas* canvas, void* context) {
    MicroCityApp* app = (MicroCityApp*)context;
    uint8_t* frame = canvas_get_buffer(canvas);
    if(!frame) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memcpy(frame, ScreenBuffer, SCREEN_SIZE);
    furi_mutex_release(app->mutex);
}

extern "C" int32_t microcity_app(void* p) {
    UNUSED(p);

    MicroCityApp* app = (MicroCityApp*)malloc(sizeof(MicroCityApp));
    memset(app, 0, sizeof(MicroCityApp));
    app->running = true;
    app_instance = app;

    memset(ScreenBuffer, 0, sizeof(ScreenBuffer));
    memset(PowerGrid, 0, sizeof(PowerGrid));

    app->input_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InitGame();

    const uint32_t frame_ticks = furi_kernel_get_tick_frequency() / MICROCITY_FRAME_RATE;
    uint32_t next_frame = furi_get_tick();

    while(app->running) {
        const uint32_t now = furi_get_tick();
        uint32_t wait = next_frame > now ? next_frame - now : 0;

        InputEvent event;
        if(furi_message_queue_get(app->input_queue, &event, wait) == FuriStatusOk) {
            input_apply(app, &event);
            continue;
        }

        next_frame += frame_ticks;
        if(next_frame < now) next_frame = now + frame_ticks;

        if(app->back_long) {
            app->back_long = false;
            if(UIState.state == StartScreen) {
                app->running = false;
                break;
            }
            UIState.state = StartScreen;
            UIState.selection = 0;
            app->held = 0;
            app->latched = 0;
        }

        app->input = (uint8_t)(app->held | app->latched);
        app->latched = 0;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        TickGame();
        furi_mutex_release(app->mutex);

        view_port_update(app->view_port);
    }

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->input_queue);

    app_instance = NULL;
    free(app);

    return 0;
}
