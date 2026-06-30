#include "level_loader.h"
#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <stdlib.h>

// Исправленная функция load_level_from_file с правильной проверкой символа новой строки
bool load_level_from_file(GeometryDashApp* app, const char* filename) {
    if (!app || !filename) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Invalid arguments to load_level_from_file");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if (!storage) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to open storage record");
        return false;
    }

    Stream* stream = file_stream_alloc(storage);
    if (!stream) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to allocate file stream");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    FURI_LOG_I(TAG, "Loading level: %s", filename);
    char full_path[256];
    // Используем sizeof(full_path) - 1 для snprintf для предотвращения переполнения
    int written = snprintf(full_path, sizeof(full_path), "%s/%s", GD_APP_DATA_PATH, filename);
    if (written < 0 || written >= (int)sizeof(full_path)) {
        FURI_LOG_E(TAG, "Path too long for level file: %s", filename);
        file_stream_close(stream);
        stream_free(stream);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = false;

    if(file_stream_open(stream, full_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        if (!line) { // Добавлена проверка на NULL
             FURI_LOG_E(TAG, "Failed to allocate FuriString");
             file_stream_close(stream);
             stream_free(stream);
             furi_record_close(RECORD_STORAGE);
             return false;
        }

        int index = 0;
        int max_x = 0; // Инициализация max_x

        while(stream_read_line(stream, line) && index < MAX_LEVEL_OBSTACLES) {
            const char* line_str = furi_string_get_cstr(line);
            if (!line_str) continue; // Добавлена проверка на NULL
            size_t line_len = strlen(line_str);

            // Исправлено: Правильная проверка на символы новой строки и комментарии
            // Ошибка была здесь: пропущен символ ' перед \n
            // Также добавлена проверка на \r для файлов Windows
            if(line_len > 0 && line_str[0] != '#' && line_str[0] != '\n' && line_str[0] != '\r') {
                char command_char = line_str[0];
                char* commas[4] = {0};
                int comma_count = 0;
                char* temp = strchr(line_str, ',');
                while(temp && comma_count < 3) {
                    commas[comma_count++] = temp;
                    temp = strchr(temp + 1, ',');
                }

                if (comma_count >= 2) {
                    *commas[0] = '\0';
                    *commas[1] = '\0';

                    int x = safe_atoi(commas[0] + 1);
                    int y = safe_atoi(commas[1] + 1);

                    int length_or_height = 1;
                    if (comma_count >= 3) {
                        *commas[2] = '\0';
                        length_or_height = safe_atoi(commas[2] + 1);
                    }

                    if (length_or_height < 1) length_or_height = 1;
                    if (length_or_height > 100) length_or_height = 100;

                    if (command_char == 'b') {
                        if (index < MAX_LEVEL_OBSTACLES) {
                            Obstacle* obs = &app->current_level.obstacles[index];
                            obs->type = ObstacleTypeBlock;
                            obs->grid_x = x;
                            obs->grid_y = y;
                            obs->length = length_or_height;
                            index++;
                            // Обновляем максимальную координату x для этой платформы
                            int last_x = x + length_or_height - 1;
                            if (last_x > max_x) max_x = last_x;
                        }
                    } else if (command_char == 'w') {
                         // Предположим, 'w' тоже создает блок, но, возможно, с другим поведением в будущем
                         // Пока обрабатываем как обычный блок
                        if (index < MAX_LEVEL_OBSTACLES) {
                            Obstacle* obs = &app->current_level.obstacles[index];
                            obs->type = ObstacleTypeBlock;
                            obs->grid_x = x;
                            obs->grid_y = y;
                            obs->length = length_or_height; // И для 'w' используем как длину
                            index++;
                            // Обновляем максимальную координату x
                            int last_x = x + length_or_height - 1; // Используем length_or_height
                            if (last_x > max_x) max_x = last_x;
                        }
                    } else {
                        ObstacleType type = char_to_obstacle_type(command_char);
                        if (type != ObstacleTypeNone) {
                            if (index < MAX_LEVEL_OBSTACLES) {
                                Obstacle* obs = &app->current_level.obstacles[index];
                                obs->type = type;
                                obs->grid_x = x;
                                obs->grid_y = y;
                                obs->length = 1; // Для не-блоков длина всегда 1
                                index++;
                                // Обновляем максимальную координату x
                                if (x > max_x) max_x = x;
                            }
                        }
                    }
                }
                // Если параметров меньше 2, строка игнорируется
            }
            // Пустые строки и комментарии игнорируются
        }

        // --- Исправление: Корректное завершение ---
        // Устанавливаем длину уровня
        app->current_level.length = index;
        // Устанавливаем максимальную координату x + запас
        app->current_level.max_grid_x = max_x + 10;
        // Копируем имя файла в структуру уровня
        if (filename) { // Дополнительная проверка
            strncpy(app->current_level.name, filename, sizeof(app->current_level.name) - 1);
            app->current_level.name[sizeof(app->current_level.name) - 1] = '\0'; // Обеспечиваем завершение строки
            // Убираем расширение .gflvl из имени уровня
            char* dot = strrchr(app->current_level.name, '.'); // Находим последнюю точку
            if(dot) *dot = '\0'; // Если точка найдена, заменяем её на завершающий ноль
        }

        furi_string_free(line); // Освобождаем строку
        success = true; // Успешно загружено
        FURI_LOG_I(TAG, "Loaded level: %s (%d obstacles, max_x=%d)", filename, index, max_x);
    } else {
        FURI_LOG_E(TAG, "Failed to open level file: %s", full_path);
        // success остаётся false
    }

    // Закрываем поток и освобождаем ресурсы в любом случае
    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    // Возвращаем результат операции
    return success;
}

void free_level_files(LevelFileArray_t* level_files) {
    if (!level_files) return;
    for(size_t i = 0; i < LevelFileArray_size(*level_files); i++) {
        char** filename_ptr = LevelFileArray_get(*level_files, i);
        if(filename_ptr) {
            free(*filename_ptr);
        }
    }
    LevelFileArray_clear(*level_files);
}

void scan_level_files(GeometryDashApp* app) {
    if (!app) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Invalid app pointer in scan_level_files");
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if (!storage) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to open storage record in scan_level_files");
        return;
    }

    FS_Error mkdir_result = storage_common_mkdir(storage, GD_APP_DATA_PATH);
    if(mkdir_result != FSE_OK && mkdir_result != FSE_EXIST) {
        FURI_LOG_E(TAG, "Failed to create directory %s: %d", GD_APP_DATA_PATH, mkdir_result);
        // Продолжаем, даже если не удалось создать директорию
    }

    File* dir = storage_file_alloc(storage);
    if (!dir) { // Добавлена проверка на NULL
        FURI_LOG_E(TAG, "Failed to allocate directory file handle");
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if(storage_dir_open(dir, GD_APP_DATA_PATH)) {
        FileInfo fileinfo;
        char name[256]; // Буфер для имени файла
        free_level_files(&app->level_files);
        LevelFileArray_init(app->level_files); // Инициализируем массив заново

        while(storage_dir_read(dir, &fileinfo, name, sizeof(name))) {
            if(!(fileinfo.flags & FSF_DIRECTORY)) {
                // Ищем точку в имени файла (для определения расширения)
                const char *dot = strrchr(name, '.');
                // Если точка найдена и расширение ".gflvl"
                if(dot && strcmp(dot, ".gflvl") == 0) {
                    char* name_copy = strdup(name);
                    // Проверка на успешное выделение памяти
                    if (name_copy) {
                        LevelFileArray_push_back(app->level_files, name_copy);
                    } else {
                        FURI_LOG_E(TAG, "Failed to allocate memory for filename: %s", name);
                    }
                }
            }
        }
        FURI_LOG_I(TAG, "Found %zu level files", LevelFileArray_size(app->level_files));
    } else {
        FURI_LOG_E(TAG, "Failed to open directory %s", GD_APP_DATA_PATH);
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

void reset_game(GeometryDashApp* app) {
    if (!app) return; // Добавлена проверка на NULL
    app->player.player_mode = PlayerModeCube;
    app->player.gravity_dir = GravityDirectionDown;
    app->player.cube_y = GROUND_Y - CUBE_SIZE;
    app->player.cube_velocity_y = 0.0f;
    app->scroll_offset = 0.0f;
    app->player.is_on_ground = true;
    app->input_pressed = false;
    app->player.ground_lost_time = furi_get_tick();
    app->last_tick = furi_get_tick();
    app->player.ball_flip_timer = 0;
    app->state = GameStatePlaying;
    FURI_LOG_I(TAG, "Game reset complete");
}