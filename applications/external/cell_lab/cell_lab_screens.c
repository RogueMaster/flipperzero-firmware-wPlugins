#include "cell_lab_screens.h"

#include <furi.h>

#include <stdio.h>

// RU: Модуль экранов отвечает только за отрисовку; он не меняет состояние мира.
// EN: The screens module only renders; it does not mutate the world state.

// RU: Маленький helper, чтобы все страницы одинаково учитывали горизонтальное смещение.
// EN: Small helper so every page handles horizontal offset consistently.
static void cell_lab_draw_text(Canvas* canvas, int16_t x_offset, int32_t y, const char* text) {
    canvas_draw_str(canvas, x_offset, y, text);
}

static void cell_lab_draw_world_hud(
    Canvas* canvas,
    const CellLabWorld* world,
    const CellLabScreenContext* context,
    int16_t x_offset) {
    char line[48];

    snprintf(
        line,
        sizeof(line),
        "WORLD %c L:%u F:%u C:%u",
        context->running ? '>' : '=',
        (unsigned)world->life_count,
        (unsigned)world->food_count,
        (unsigned)world->config.cell_size);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x_offset, 0, CELL_LAB_SCREEN_WIDTH, 10);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    cell_lab_draw_text(canvas, x_offset + 1, 8, line);
}

// RU: Рисует одну симуляционную клетку как пиксель или блок пикселей.
// EN: Draws one simulation cell as either one pixel or a pixel block.
static void cell_lab_draw_cell_block(
    Canvas* canvas,
    int16_t x_offset,
    uint16_t x,
    uint16_t y,
    uint8_t size) {
    const int32_t px = x_offset + (x * size);
    const int32_t py = y * size;

    if((px >= (int32_t)CELL_LAB_SCREEN_WIDTH) || ((px + size) <= 0)) {
        return;
    }

    // RU: При размере 1x1 достаточно точки; большие клетки рисуются заполненным блоком.
    // EN: At 1x1 a dot is enough; larger cells are drawn as filled blocks.
    if(size == 1U) {
        canvas_draw_dot(canvas, px, py);
    } else {
        canvas_draw_box(canvas, px, py, size, size);
    }
}

static void cell_lab_draw_world(
    Canvas* canvas,
    const CellLabWorld* world,
    const CellLabScreenContext* context,
    int16_t x_offset) {
    const uint16_t width = cell_lab_world_width(world);
    const uint16_t height = cell_lab_world_height(world);
    const uint8_t cell_size = world->config.cell_size;

    // RU: Проходим только по активному миру, который зависит от размера клетки.
    // EN: Iterate only over the active world, which depends on cell size.
    for(uint16_t y = 0; y < height; y++) {
        for(uint16_t x = 0; x < width; x++) {
            const size_t index = cell_lab_world_index(world, x, y);
            const uint8_t cell = world->cell[index];

            if(cell == CellLabCellLife) {
                cell_lab_draw_cell_block(canvas, x_offset, x, y, cell_size);
            } else if(cell == CellLabCellFood) {
                // RU: Еда не мигает: если пиксель еды есть в мире, он всегда виден до съедения.
                // EN: Food does not blink: if a food pixel exists, it stays visible until eaten.
                cell_lab_draw_cell_block(canvas, x_offset, x, y, cell_size);
            }
        }
    }

    if(!context->running || !cell_lab_time_reached(furi_get_tick(), context->hud_until_tick)) {
        cell_lab_draw_world_hud(canvas, world, context, x_offset);
    }
}

static void cell_lab_draw_state(
    Canvas* canvas,
    const CellLabWorld* world,
    const CellLabScreenContext* context,
    int16_t x_offset) {
    char line[48];

    // RU: World State показывает только реальные текущие значения, без исторической статистики.
    // EN: World State shows only current real values, not historical statistics.
    canvas_set_font(canvas, FontSecondary);
    cell_lab_draw_text(canvas, x_offset, 7, "< World State >");

    snprintf(
        line,
        sizeof(line),
        "Mode:%s Tick:%lu",
        context->running ? "run" : "pause",
        (unsigned long)world->generation);
    cell_lab_draw_text(canvas, x_offset, 15, line);

    snprintf(
        line,
        sizeof(line),
        "World:%ux%u cell:%u",
        (unsigned)cell_lab_world_width(world),
        (unsigned)cell_lab_world_height(world),
        (unsigned)world->config.cell_size);
    cell_lab_draw_text(canvas, x_offset, 23, line);

    snprintf(
        line,
        sizeof(line),
        "Life:%u Food:%u",
        (unsigned)world->life_count,
        (unsigned)world->food_count);
    cell_lab_draw_text(canvas, x_offset, 31, line);

    snprintf(
        line,
        sizeof(line),
        "Temp:%u Rad:%u",
        (unsigned)world->avg_world_temp,
        (unsigned)world->avg_radiation);
    cell_lab_draw_text(canvas, x_offset, 39, line);

    snprintf(line, sizeof(line), "Toxin:%u Crowd:%u", (unsigned)world->avg_toxin, (unsigned)world->avg_crowding);
    cell_lab_draw_text(canvas, x_offset, 47, line);

    snprintf(line, sizeof(line), "Energy avg:%u", (unsigned)world->avg_energy);
    cell_lab_draw_text(canvas, x_offset, 55, line);

    snprintf(line, sizeof(line), "Epoch:%lu Speed:%ums", (unsigned long)world->epoch, (unsigned)context->tick_delay_ms);
    cell_lab_draw_text(canvas, x_offset, 63, line);
}

static void cell_lab_world_setting_value(
    const CellLabWorld* world,
    CellLabWorldSetting setting,
    char* buffer,
    size_t buffer_size) {
    // RU: Значения мира форматирует модуль config, чтобы меню не дублировало правила.
    // EN: World values are formatted by config so the menu does not duplicate rules.
    cell_lab_config_setting_value(&world->config, setting, buffer, buffer_size);
}

// RU: Меню законов мира: изменение пункта пересоздает симуляцию с новыми правилами.
// EN: World rules menu: changing an item recreates the simulation with new rules.
static void cell_lab_draw_world_settings(
    Canvas* canvas,
    const CellLabWorld* world,
    const CellLabScreenContext* context,
    int16_t x_offset) {
    char line[56];
    uint8_t first = 0;

    if(context->selected_world_setting > 2U) {
        first = context->selected_world_setting - 2U;
    }

    if(first + 6U > CellLabWorldSettingCount) {
        first = CellLabWorldSettingCount - 6U;
    }

    canvas_set_font(canvas, FontSecondary);
    cell_lab_draw_text(canvas, x_offset, 7, "< World Settings >");

    for(uint8_t row = 0; row < 6U; row++) {
        const uint8_t setting = first + row;
        char value[24];

        cell_lab_world_setting_value(world, (CellLabWorldSetting)setting, value, sizeof(value));
        snprintf(
            line,
            sizeof(line),
            "%c %s: %s",
            (setting == context->selected_world_setting) ? '>' : ' ',
            cell_lab_config_setting_name((CellLabWorldSetting)setting),
            value);

        cell_lab_draw_text(canvas, x_offset, 16 + (row * 8), line);
    }

    cell_lab_draw_text(canvas, x_offset, 63, "OK change + reset");
}

static const char* cell_lab_app_setting_name(CellLabAppSetting setting) {
    if(setting == CellLabAppSettingSpeed) {
        return "Speed";
    } else if(setting == CellLabAppSettingTransitionEnabled) {
        return "Scroll";
    }

    return "Scroll ms";
}

static void cell_lab_app_setting_value(
    const CellLabScreenContext* context,
    CellLabAppSetting setting,
    char* buffer,
    size_t buffer_size) {
    // RU: Настройки приложения меняют интерфейс и скорость без пересоздания мира.
    // EN: App settings change UI and speed without recreating the world.
    if(setting == CellLabAppSettingSpeed) {
        snprintf(buffer, buffer_size, "%ums", (unsigned)context->tick_delay_ms);
    } else if(setting == CellLabAppSettingTransitionEnabled) {
        snprintf(buffer, buffer_size, "%s", context->transition->enabled ? "on" : "off");
    } else {
        snprintf(buffer, buffer_size, "%ums", (unsigned)context->transition->duration_ms);
    }
}

// RU: Меню приложения отделено от меню мира, чтобы не смешивать UI и физику симуляции.
// EN: App settings are separate from world settings so UI and simulation physics do not mix.
static void cell_lab_draw_app_settings(
    Canvas* canvas,
    const CellLabScreenContext* context,
    int16_t x_offset) {
    char line[56];

    canvas_set_font(canvas, FontSecondary);
    cell_lab_draw_text(canvas, x_offset, 7, "< App Settings >");

    for(uint8_t row = 0; row < CellLabAppSettingCount; row++) {
        char value[24];

        cell_lab_app_setting_value(context, (CellLabAppSetting)row, value, sizeof(value));
        snprintf(
            line,
            sizeof(line),
            "%c %s: %s",
            (row == context->selected_app_setting) ? '>' : ' ',
            cell_lab_app_setting_name((CellLabAppSetting)row),
            value);

        cell_lab_draw_text(canvas, x_offset, 16 + (row * 8), line);
    }

    cell_lab_draw_text(canvas, x_offset, 63, "OK change UI option");
}

static void cell_lab_draw_about(Canvas* canvas, int16_t x_offset) {
    // RU: About намеренно короткий: это описание приложения, а не инструкция по биологии мира.
    // EN: About is intentionally short: it describes the app, not the world's biology.
    canvas_set_font(canvas, FontSecondary);
    cell_lab_draw_text(canvas, x_offset, 7, "< About >");
    cell_lab_draw_text(canvas, x_offset, 15, "Cell Lab 0.4");
    cell_lab_draw_text(canvas, x_offset, 25, "Tiny digital terrarium");
    cell_lab_draw_text(canvas, x_offset, 35, "Pixels live, eat,");
    cell_lab_draw_text(canvas, x_offset, 45, "mutate and vanish.");
    cell_lab_draw_text(canvas, x_offset, 55, "A pocket world");
    cell_lab_draw_text(canvas, x_offset, 63, "for Flipper Zero.");
}

void cell_lab_screens_draw(
    Canvas* canvas,
    const CellLabWorld* world,
    const CellLabScreenContext* context,
    CellLabScreen screen,
    int16_t x_offset) {
    // RU: Единая точка входа отрисовки упрощает плавные переходы между страницами.
    // EN: A single rendering entry point makes smooth page transitions simpler.
    if(screen == CellLabScreenWorld) {
        cell_lab_draw_world(canvas, world, context, x_offset);
    } else if(screen == CellLabScreenState) {
        cell_lab_draw_state(canvas, world, context, x_offset);
    } else if(screen == CellLabScreenWorldSettings) {
        cell_lab_draw_world_settings(canvas, world, context, x_offset);
    } else if(screen == CellLabScreenAppSettings) {
        cell_lab_draw_app_settings(canvas, context, x_offset);
    } else {
        cell_lab_draw_about(canvas, x_offset);
    }
}
