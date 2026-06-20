#pragma once

#include "cell_lab_common.h"

#include <stddef.h>

// RU: Все изменяемые законы мира собраны здесь; World Settings меняет именно эту структуру.
// EN: All editable world rules live here; World Settings modifies this structure.
typedef struct {
    // RU: Размер одной симуляционной клетки в физических пикселях экрана.
    // EN: Size of one simulation cell in physical screen pixels.
    uint8_t cell_size;

    // RU: Стартовое количество живых клеток и еды при создании новой эпохи.
    // EN: Initial amount of life and food when a new epoch is created.
    uint16_t initial_life;
    uint16_t initial_food;

    // RU: Энергетика клетки: стартовый запас, цена хода, питание, размножение и максимум.
    // EN: Cell energy rules: start range, move cost, food value, reproduction cost, and cap.
    uint8_t start_energy_min;
    uint8_t start_energy_spread;
    uint8_t move_cost;
    uint8_t food_energy;
    uint8_t reproduce_energy;
    uint8_t max_energy;

    // RU: Правила появления еды и спасательный минимум для сурового мира.
    // EN: Food spawning rules and a rescue minimum for a harsh world.
    uint8_t food_growth;
    uint8_t food_low_watermark;
    uint8_t food_rescue_burst;

    // RU: Явные раздражители мира, влияющие на поведение и отбор.
    // EN: Explicit world irritants that affect behavior and selection.
    uint8_t heat_swing;
    uint8_t radiation_level;
    uint8_t toxin_level;
} CellLabWorldConfig;

// RU: Заполняет конфигурацию суровыми, но играбельными значениями по умолчанию.
// EN: Fills the config with harsh but playable default values.
void cell_lab_config_default(CellLabWorldConfig* config);

// RU: Возвращает ширину мира в клетках с учетом выбранного размера клетки.
// EN: Returns world width in cells for the selected cell size.
uint16_t cell_lab_config_world_width(const CellLabWorldConfig* config);

// RU: Возвращает высоту мира в клетках с учетом выбранного размера клетки.
// EN: Returns world height in cells for the selected cell size.
uint16_t cell_lab_config_world_height(const CellLabWorldConfig* config);

// RU: Возвращает общее количество активных клеток мира.
// EN: Returns the total number of active world cells.
uint16_t cell_lab_config_world_capacity(const CellLabWorldConfig* config);

// RU: Циклически меняет один выбранный параметр мира.
// EN: Cycles one selected world setting.
void cell_lab_config_change(CellLabWorldConfig* config, CellLabWorldSetting setting);

// RU: Возвращает короткое имя пункта меню настроек мира.
// EN: Returns a short display name for a world settings item.
const char* cell_lab_config_setting_name(CellLabWorldSetting setting);

// RU: Форматирует текущее значение пункта настроек мира для экрана.
// EN: Formats the current value of a world setting for the screen.
void cell_lab_config_setting_value(
    const CellLabWorldConfig* config,
    CellLabWorldSetting setting,
    char* buffer,
    size_t buffer_size);
