/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "game_settings.h"

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#define SETTINGS_FILE_NAME "settings.txt"
#define SETTING_FILE_TYPE "Quadrastic Game Setting File"
#define SETTING_FILE_VERSION 1

#define KEY_DIFFICULTY "Difficulty"
#define KEY_BEST_SCORE "Best score"
#define KEY_SOUND "Sound"
#define KEY_VIBRO "Vibro"
#define KEY_LED "LED"

static bool
game_read_bool_setting(FlipperFormat* flipper_format,
                       const char* key,
                       State* setting)
{
    if (!flipper_format_rewind(flipper_format)) {
        FURI_LOG_E(GAME_NAME, "Rewind error");
        return false;
    }
    bool temp_bool = *setting;
    if (!flipper_format_read_bool(flipper_format, key, &temp_bool, 1)) {
        FURI_LOG_E(GAME_NAME, "Can't read %s setting", key);
        return false;
    }
    *setting = temp_bool ? StateOn : StateOff;

    return true;
}

void
game_read_settings(GameContext* game_context)
{
    // Default settings
    game_context->score = 0;
    game_context->best_score = 0;
    game_context->difficulty = DifficultyNormal;
    game_context->sound = StateOn;
    game_context->vibro = StateOn;
    game_context->led = StateOn;

    // Read settings
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    FuriString* temp_string = furi_string_alloc();
    uint32_t temp_uint32;

    do {
        // Check settings file
        if (!flipper_format_file_open_existing(
              flipper_format, APP_DATA_PATH(SETTINGS_FILE_NAME))) {
            FURI_LOG_D(GAME_NAME, "Settings file is not used");
            break;
        }
        if (!flipper_format_read_header(
              flipper_format, temp_string, &temp_uint32)) {
            FURI_LOG_E(GAME_NAME, "Missing or incorrect header");
            break;
        }
        if ((!strcmp(furi_string_get_cstr(temp_string), SETTING_FILE_TYPE)) &&
            temp_uint32 == SETTING_FILE_VERSION) {
        } else {
            FURI_LOG_E(GAME_NAME, "Type or version mismatch");
            break;
        }

        // Read difficulty
        temp_uint32 = (uint32_t)game_context->difficulty;
        if (!flipper_format_read_uint32(
              flipper_format, KEY_DIFFICULTY, &temp_uint32, 1)) {
            FURI_LOG_E(GAME_NAME, "Can't read " KEY_DIFFICULTY " setting");
            break;
        }
        if (temp_uint32 >= DifficultyCount) {
            temp_uint32 = DifficultyCount - 1;
        }
        game_context->difficulty = temp_uint32;

        // Read best score
        temp_uint32 = game_context->best_score;
        if (!flipper_format_read_uint32(
              flipper_format, KEY_BEST_SCORE, &temp_uint32, 1)) {
            FURI_LOG_E(GAME_NAME, "Can't read " KEY_BEST_SCORE " setting");
            break;
        }
        game_context->best_score = temp_uint32;

        // Read sound
        if (!game_read_bool_setting(
              flipper_format, KEY_SOUND, &game_context->sound)) {
            break;
        }

        // Read vibro
        if (!game_read_bool_setting(
              flipper_format, KEY_VIBRO, &game_context->vibro)) {
            break;
        }

        // Read led
        if (!game_read_bool_setting(
              flipper_format, KEY_LED, &game_context->led)) {
            break;
        }

    } while (false);

    furi_string_free(temp_string);

    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(GAME_NAME, "Settings read");
}

void
game_save_settings(GameContext* game_context)
{
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);

    bool temp_bool;
    uint32_t temp_uint32;

    do {
        // Write settings meta
        if (!flipper_format_file_open_always(
              flipper_format, APP_DATA_PATH(SETTINGS_FILE_NAME))) {
            FURI_LOG_E(GAME_NAME, "Settings file can't be saved");
            break;
        }
        if (!flipper_format_write_header_cstr(
              flipper_format, SETTING_FILE_TYPE, SETTING_FILE_VERSION)) {
            FURI_LOG_E(GAME_NAME, "Settings header can't be saved");
            break;
        }

        // Write difficulty
        temp_uint32 = game_context->difficulty;
        if (!flipper_format_write_uint32(
              flipper_format, KEY_DIFFICULTY, &temp_uint32, 1)) {
            FURI_LOG_E(GAME_NAME, "Difficulty state can't be saved");
            break;
        }

        // Write best score
        temp_uint32 = game_context->best_score;
        if (!flipper_format_write_uint32(
              flipper_format, KEY_BEST_SCORE, &temp_uint32, 1)) {
            FURI_LOG_E(GAME_NAME, "Best score state can't be saved");
            break;
        }

        // Write sound
        temp_bool = game_context->sound == StateOn;
        if (!flipper_format_write_bool(
              flipper_format, "Sound", &temp_bool, 1)) {
            FURI_LOG_E(GAME_NAME, "Sound state can't be saved");
            break;
        }

        // Write vibro
        temp_bool = game_context->vibro == StateOn;
        if (!flipper_format_write_bool(
              flipper_format, "Vibro", &temp_bool, 1)) {
            FURI_LOG_E(GAME_NAME, "Vibro state can't be saved");
            break;
        }

        // Write led
        temp_bool = game_context->led == StateOn;
        if (!flipper_format_write_bool(flipper_format, "LED", &temp_bool, 1)) {
            FURI_LOG_E(GAME_NAME, "LED state can't be saved");
            break;
        }

    } while (false);

    flipper_format_free(flipper_format);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(GAME_NAME, "Settings saved");
}
