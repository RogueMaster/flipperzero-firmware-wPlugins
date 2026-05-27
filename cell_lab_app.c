#include "cell_lab_screens.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>

// RU: Скорость симуляции хранится в миллисекундах между тиками.
// EN: Simulation speed is stored as milliseconds between ticks.
#define CELL_LAB_DEFAULT_DELAY_MS 70U
#define CELL_LAB_MIN_DELAY_MS 15U
#define CELL_LAB_MAX_DELAY_MS 220U
#define CELL_LAB_DELAY_STEP_MS 10U

typedef struct {
    // RU: Системные объекты Flipper: GUI, ViewPort, очередь ввода и mutex мира.
    // EN: Flipper system objects: GUI, ViewPort, input queue, and world mutex.
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriMutex* world_mutex;

    // RU: Состояние приложения, которое не принадлежит самой модели мира.
    // EN: App state that is not owned by the world model itself.
    bool running;
    bool exit_requested;
    CellLabScreen screen;
    uint8_t selected_world_setting;
    uint8_t selected_app_setting;
    uint8_t tick_delay_ms;
    uint32_t hud_until_tick;

    // RU: Модель мира и отдельное состояние анимации страниц.
    // EN: World model and separate page animation state.
    CellLabWorld* world;
    CellLabTransition transition;
} CellLabApp;

// RU: Большие буферы мира держим в static-памяти, а не на стеке приложения.
// EN: Large world buffers are kept in static memory, not on the app stack.
static CellLabWorld cell_lab_world_storage;

// RU: Показывает HUD на короткое время после действия пользователя.
// EN: Shows the HUD briefly after a user action.
static void cell_lab_show_hud(CellLabApp* app) {
    app->hud_until_tick = furi_get_tick() + CELL_LAB_HUD_VISIBLE_MS;
}

static CellLabScreen cell_lab_next_screen(CellLabScreen screen, int8_t direction) {
    int8_t next = (int8_t)screen + direction;

    if(next < 0) {
        next = CellLabScreenCount - 1;
    } else if(next >= CellLabScreenCount) {
        next = 0;
    }

    return (CellLabScreen)next;
}

// RU: Меняет страницу и запускает отдельную анимацию прокрутки.
// EN: Changes the page and starts the separate scroll animation.
static void cell_lab_change_screen(CellLabApp* app, int8_t direction) {
    const CellLabScreen from_screen = app->screen;
    const CellLabScreen to_screen = cell_lab_next_screen(app->screen, direction);

    app->screen = to_screen;
    cell_lab_transition_start(&app->transition, from_screen, to_screen, direction, furi_get_tick());
    cell_lab_show_hud(app);
}

static void cell_lab_change_speed(CellLabApp* app) {
    // RU: В App Settings скорость переключается по трем удобным пресетам.
    // EN: In App Settings, speed cycles through three convenient presets.
    if(app->tick_delay_ms < 45U) {
        app->tick_delay_ms = 70U;
    } else if(app->tick_delay_ms < 120U) {
        app->tick_delay_ms = 150U;
    } else {
        app->tick_delay_ms = 30U;
    }
}

static void cell_lab_apply_world_setting(CellLabApp* app) {
    // RU: Изменение закона мира пересоздает эпоху, чтобы старые буферы не конфликтовали с размером.
    // EN: Changing a world law recreates the epoch so old buffers do not conflict with size.
    CellLabWorldConfig config = app->world->config;

    cell_lab_config_change(&config, (CellLabWorldSetting)app->selected_world_setting);
    cell_lab_world_apply_config(app->world, &config);
    cell_lab_show_hud(app);
}

static void cell_lab_apply_app_setting(CellLabApp* app) {
    // RU: Настройки приложения не трогают модель мира и применяются без перезапуска эпохи.
    // EN: App settings do not touch the world model and apply without restarting the epoch.
    if(app->selected_app_setting == CellLabAppSettingSpeed) {
        cell_lab_change_speed(app);
    } else if(app->selected_app_setting == CellLabAppSettingTransitionEnabled) {
        cell_lab_transition_cycle_enabled(&app->transition);
    } else if(app->selected_app_setting == CellLabAppSettingTransitionDuration) {
        cell_lab_transition_cycle_duration(&app->transition);
    }

    cell_lab_show_hud(app);
}

static void cell_lab_handle_world_settings_input(CellLabApp* app, const InputEvent* event) {
    // RU: Up/Down выбирают пункт, OK меняет значение, long OK пересоздает мир вручную.
    // EN: Up/Down select an item, OK changes it, long OK manually recreates the world.
    if(event->key == InputKeyUp) {
        if(app->selected_world_setting == 0U) {
            app->selected_world_setting = CellLabWorldSettingCount - 1U;
        } else {
            app->selected_world_setting--;
        }
    } else if(event->key == InputKeyDown) {
        app->selected_world_setting++;
        if(app->selected_world_setting >= CellLabWorldSettingCount) {
            app->selected_world_setting = 0U;
        }
    } else if((event->key == InputKeyOk) && (event->type == InputTypeShort)) {
        cell_lab_apply_world_setting(app);
    } else if((event->key == InputKeyOk) && (event->type == InputTypeLong)) {
        cell_lab_world_start_new_generation(app->world);
        cell_lab_show_hud(app);
    }
}

static void cell_lab_handle_app_settings_input(CellLabApp* app, const InputEvent* event) {
    // RU: Меню приложения устроено так же, но меняет только UI-параметры.
    // EN: The app menu behaves the same way but only changes UI parameters.
    if(event->key == InputKeyUp) {
        if(app->selected_app_setting == 0U) {
            app->selected_app_setting = CellLabAppSettingCount - 1U;
        } else {
            app->selected_app_setting--;
        }
    } else if(event->key == InputKeyDown) {
        app->selected_app_setting++;
        if(app->selected_app_setting >= CellLabAppSettingCount) {
            app->selected_app_setting = 0U;
        }
    } else if((event->key == InputKeyOk) && (event->type == InputTypeShort)) {
        cell_lab_apply_app_setting(app);
    }
}

static void cell_lab_handle_input(CellLabApp* app, const InputEvent* event) {
    // RU: Игнорируем отпускания кнопок, чтобы одно действие не срабатывало дважды.
    // EN: Ignore button releases so one action is not handled twice.
    if((event->type != InputTypeShort) && (event->type != InputTypeRepeat) &&
       (event->type != InputTypeLong)) {
        return;
    }

    if(event->key == InputKeyBack) {
        app->exit_requested = true;
        return;
    }

    if((event->key == InputKeyLeft) && (event->type == InputTypeShort)) {
        cell_lab_change_screen(app, -1);
        return;
    } else if((event->key == InputKeyRight) && (event->type == InputTypeShort)) {
        cell_lab_change_screen(app, 1);
        return;
    }

    if(app->screen == CellLabScreenWorldSettings) {
        cell_lab_handle_world_settings_input(app, event);
        return;
    } else if(app->screen == CellLabScreenAppSettings) {
        cell_lab_handle_app_settings_input(app, event);
        return;
    }

    if((event->key == InputKeyOk) && (event->type == InputTypeShort)) {
        app->running = !app->running;
        cell_lab_show_hud(app);
    } else if((event->key == InputKeyOk) && (event->type == InputTypeLong)) {
        cell_lab_world_start_new_generation(app->world);
        cell_lab_show_hud(app);
    } else if(event->key == InputKeyUp) {
        if(app->tick_delay_ms > CELL_LAB_MIN_DELAY_MS + CELL_LAB_DELAY_STEP_MS) {
            app->tick_delay_ms -= CELL_LAB_DELAY_STEP_MS;
        } else {
            app->tick_delay_ms = CELL_LAB_MIN_DELAY_MS;
        }
        cell_lab_show_hud(app);
    } else if(event->key == InputKeyDown) {
        if(app->tick_delay_ms < CELL_LAB_MAX_DELAY_MS - CELL_LAB_DELAY_STEP_MS) {
            app->tick_delay_ms += CELL_LAB_DELAY_STEP_MS;
        } else {
            app->tick_delay_ms = CELL_LAB_MAX_DELAY_MS;
        }
        cell_lab_show_hud(app);
    }
}

static void cell_lab_draw_callback(Canvas* canvas, void* context) {
    // RU: Draw callback приходит из GUI-потока, поэтому мир читаем под mutex.
    // EN: The draw callback runs on the GUI thread, so the world is read under a mutex.
    CellLabApp* app = context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(furi_mutex_acquire(app->world_mutex, 0) == FuriStatusOk) {
        CellLabScreenContext screen_context = {
            .running = app->running,
            .tick_delay_ms = app->tick_delay_ms,
            .selected_world_setting = app->selected_world_setting,
            .selected_app_setting = app->selected_app_setting,
            .hud_until_tick = app->hud_until_tick,
            .transition = &app->transition,
        };
        int16_t from_offset;
        int16_t to_offset;

        // RU: Если переход активен, рисуем две страницы со смещениями; иначе одну текущую.
        // EN: If transition is active, draw two offset pages; otherwise draw the current page.
        if(cell_lab_transition_offsets(&app->transition, furi_get_tick(), &from_offset, &to_offset)) {
            cell_lab_screens_draw(
                canvas,
                app->world,
                &screen_context,
                app->transition.from_screen,
                from_offset);
            cell_lab_screens_draw(
                canvas,
                app->world,
                &screen_context,
                app->transition.to_screen,
                to_offset);
        } else {
            cell_lab_screens_draw(canvas, app->world, &screen_context, app->screen, 0);
        }

        furi_mutex_release(app->world_mutex);
    }
}

static void cell_lab_input_callback(InputEvent* event, void* context) {
    // RU: Input callback должен быть быстрым: он только кладет событие в очередь приложения.
    // EN: The input callback must stay fast: it only queues the event for the app loop.
    CellLabApp* app = context;
    furi_message_queue_put(app->input_queue, event, 0);
}

int32_t cell_lab_app(void* p) {
    UNUSED(p);

    CellLabWorldConfig config;
    // RU: Конфигурация создается до мира, потому что размер мира зависит от размера клетки.
    // EN: Config is created before the world because world size depends on cell size.
    cell_lab_config_default(&config);

    CellLabApp app = {
        .gui = NULL,
        .view_port = NULL,
        .input_queue = NULL,
        .world_mutex = NULL,
        .running = true,
        .exit_requested = false,
        .screen = CellLabScreenWorld,
        .selected_world_setting = 0,
        .selected_app_setting = 0,
        .tick_delay_ms = CELL_LAB_DEFAULT_DELAY_MS,
        .hud_until_tick = 0,
        .world = &cell_lab_world_storage,
    };

    cell_lab_transition_init(&app.transition);
    cell_lab_world_init(app.world, &config);
    cell_lab_show_hud(&app);

    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.world_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app.view_port = view_port_alloc();
    app.gui = furi_record_open(RECORD_GUI);

    view_port_draw_callback_set(app.view_port, cell_lab_draw_callback, &app);
    view_port_input_callback_set(app.view_port, cell_lab_input_callback, &app);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    uint32_t next_step_tick = furi_get_tick();

    while(!app.exit_requested) {
        InputEvent event;
        const uint32_t now = furi_get_tick();
        uint32_t queue_wait_ms = 100;

        // RU: В активном режиме ждем либо кнопку, либо ближайший тик симуляции.
        // EN: While running, wait either for input or for the nearest simulation tick.
        if(app.running) {
            if(cell_lab_time_reached(now, next_step_tick)) {
                queue_wait_ms = 0;
            } else {
                const uint32_t until_step = next_step_tick - now;
                queue_wait_ms = (until_step > 100U) ? 100U : until_step;
            }
        }

        if(furi_message_queue_get(app.input_queue, &event, queue_wait_ms) == FuriStatusOk) {
            // RU: Ввод может менять мир и настройки, поэтому обрабатываем его под mutex.
            // EN: Input can change world/settings, so handle it under the mutex.
            furi_mutex_acquire(app.world_mutex, CELL_LAB_WAIT_FOREVER);
            cell_lab_handle_input(&app, &event);

            while(furi_message_queue_get(app.input_queue, &event, 0) == FuriStatusOk) {
                cell_lab_handle_input(&app, &event);
            }

            furi_mutex_release(app.world_mutex);
            view_port_update(app.view_port);
        }

        if(app.running && cell_lab_time_reached(furi_get_tick(), next_step_tick)) {
            // RU: Один тик мира выполняется в основном потоке приложения, не в GUI callback.
            // EN: One world tick runs in the app main loop, not inside the GUI callback.
            furi_mutex_acquire(app.world_mutex, CELL_LAB_WAIT_FOREVER);
            cell_lab_world_step(app.world);
            furi_mutex_release(app.world_mutex);

            next_step_tick = furi_get_tick() + app.tick_delay_ms;
            view_port_update(app.view_port);
        } else if(!app.running || app.transition.active) {
            view_port_update(app.view_port);
        }
    }

    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app.world_mutex);
    furi_message_queue_free(app.input_queue);

    return 0;
}
