#include "constants.h"
#include "player.h"
#include "render.h"
#include "game_logic.h"
#include "level_loader.h"
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <m-array.h>
#include <math.h>

// Function prototypes
static void input_callback(InputEvent* input_event, void* ctx);

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    if (event_queue && input_event) {
        furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    }
}

int32_t geometry_dash_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    if (!event_queue) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to allocate event queue");
        return -1; // Возвращаем код ошибки
    }

    GeometryDashApp* app = malloc(sizeof(GeometryDashApp));
    if (!app) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to allocate app memory");
        furi_message_queue_free(event_queue);
        return -1;
    }
    memset(app, 0, sizeof(GeometryDashApp));

    // Инициализируем начальное состояние приложения
    app->state = GameStateMenu;
    player_init(&app->player);
    app->scroll_offset = 0.0f;
    app->last_tick = furi_get_tick(); // Получаем текущее время
    app->input_pressed = false;
    app->selected_menu_item = 0;

    LevelFileArray_init(app->level_files);
    scan_level_files(app);

    ViewPort* view_port = view_port_alloc();
    if (!view_port) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to allocate view port");
        free_level_files(&app->level_files);
        LevelFileArray_clear(app->level_files);
        free(app);
        furi_message_queue_free(event_queue);
        return -1;
    }

    view_port_draw_callback_set(view_port, render_callback, app);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    if (!gui) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to open GUI record");
        view_port_free(view_port);
        free_level_files(&app->level_files);
        LevelFileArray_clear(app->level_files);
        free(app);
        furi_message_queue_free(event_queue);
        return -1;
    }

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Основной цикл приложения
    InputEvent event;
    bool running = true;

    while(running) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 50);
        if(event_status == FuriStatusOk) {
            // Обрабатываем нажатие или удержание кнопки
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if (app->state == GameStateMenu) {
                    // Логика навигации по меню
                    size_t file_count = LevelFileArray_size(app->level_files);
                    if(file_count > 0) { // Проверяем, есть ли файлы
                        if(event.key == InputKeyUp) {
                            // Перемещение вверх по списку (с учётом цикличности)
                            app->selected_menu_item = (app->selected_menu_item - 1 + file_count) % file_count;
                        } else if(event.key == InputKeyDown) {
                            // Перемещение вниз по списку (с учётом цикличности)
                            app->selected_menu_item = (app->selected_menu_item + 1) % file_count;
                        } else if(event.key == InputKeyOk) {
                            // Выбор текущего пункта меню
                            char** selected_filename_ptr = LevelFileArray_get(app->level_files, app->selected_menu_item);
                            // Проверка указателя перед использованием
                            if(selected_filename_ptr && *selected_filename_ptr) {
                                char* selected_filename = *selected_filename_ptr;
                                // Пытаемся загрузить выбранный уровень
                                if(load_level_from_file(app, selected_filename)) {
                                    // Если успешно, сбрасываем игру
                                    reset_game(app);
                                } else {
                                    // Если ошибка загрузки
                                    FURI_LOG_E(TAG, "Failed to load level: %s", selected_filename);
                                }
                            }
                        }
                    }
                    // Выход из приложения по кнопке Back
                    if(event.key == InputKeyBack) {
                        running = false;
                    }
                } else if (app->state == GameStatePlaying) {
                    // Логика управления игроком во время игры
                    if(event.key == InputKeyOk) {
                        // For ball mode, register input immediately for better responsiveness
                        // The actual flip will be handled by cooldown in player_handle_input
                        // For other modes, prevent continuous input to avoid issues
                        bool should_process_input = false;

                        if (app->player.player_mode == PlayerModeBall) {
                            // For ball mode, always register input press for immediate response
                            // The cooldown logic is handled inside player_handle_input
                            should_process_input = true;
                        } else {
                            // For other modes, only process if button not already pressed
                            should_process_input = !app->input_pressed;
                        }

                        if (should_process_input) {
                            // Use the centralized input handling function
                            player_handle_input(&app->player, true);
                            app->input_pressed = true; // Mark that button is pressed
                        }
                    } else if(event.key == InputKeyBack) {
                         // Возврат в меню по кнопке Back во время игры
                         app->state = GameStateMenu;
                         scan_level_files(app); // Пересканируем файлы на случай изменений
                    }
                } else if (app->state == GameStateGameOver || app->state == GameStateWin) {
                     // Логика в состояниях Game Over и Win
                     if(event.key == InputKeyOk) {
                         // Перезапуск текущего уровня
                         char** selected_filename_ptr = LevelFileArray_get(app->level_files, app->selected_menu_item);
                         // Проверка указателя перед использованием
                         if(selected_filename_ptr && *selected_filename_ptr) {
                             char* selected_filename = *selected_filename_ptr;
                             // Пытаемся снова загрузить тот же уровень
                             if(load_level_from_file(app, selected_filename)) {
                                reset_game(app); // Сбрасываем игру
                             } else {
                                FURI_LOG_E(TAG, "Failed to reload level: %s", selected_filename);
                             }
                         }
                     } else if(event.key == InputKeyBack) {
                         // Возврат в меню
                         app->state = GameStateMenu;
                         scan_level_files(app); // Пересканируем файлы
                     }
                }
            } else if (event.type == InputTypeRelease && event.key == InputKeyOk) {
                 // Отпускаем флаг нажатия кнопки OK
                 player_handle_input(&app->player, false); // Pass false for release
                 app->input_pressed = false;
            }
        }

        // Обновляем логику игры
        uint32_t current_tick = furi_get_tick(); // Получаем текущее время
        uint32_t dt_ms = current_tick - app->last_tick; // Вычисляем дельту времени
        // Обновляем игру только если дельта времени положительна и разумна
        if (dt_ms > 0 && dt_ms < 100) {
            process_game_logic(app, dt_ms); // Обрабатываем игровую логику
            app->last_tick = current_tick; // Обновляем время последнего обновления
        }
        // Обновляем отображение на экране
        view_port_update(view_port);
    }

    // --- Завершение работы приложения ---
    // Удаляем ViewPort из GUI
    gui_remove_view_port(gui, view_port);
    // Освобождаем ViewPort
    view_port_free(view_port);
    // Освобождаем очередь сообщений
    furi_message_queue_free(event_queue);
    // Освобождаем память, выделенную под имена файлов
    free_level_files(&app->level_files);
    // Очищаем массив файлов
    LevelFileArray_clear(app->level_files);
    // Закрываем запись в GUI
    furi_record_close(RECORD_GUI);
    // Освобождаем память, выделенную под структуру приложения
    free(app);
    // Возвращаем код завершения (0 - успех)
    return 0;
}