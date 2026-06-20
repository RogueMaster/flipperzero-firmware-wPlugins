#include "cell_lab_config.h"

#include <stdio.h>

// RU: Модуль конфигурации хранит все настраиваемые законы мира.
// EN: The configuration module stores every editable law of the world.

// RU: Базовые значения специально суровые: жизнь должна бороться, а не заливать весь экран.
// EN: Defaults are intentionally harsh: life should struggle, not fill the entire screen.
void cell_lab_config_default(CellLabWorldConfig* config) {
    config->cell_size = 1;
    config->initial_life = 180;
    config->initial_food = 360;
    config->start_energy_min = 8;
    config->start_energy_spread = 10;
    config->move_cost = 2;
    config->food_energy = 6;
    config->reproduce_energy = 32;
    config->max_energy = 42;
    config->food_growth = 5;
    config->food_low_watermark = 60;
    config->food_rescue_burst = 16;
    config->heat_swing = 9;
    config->radiation_level = 7;
    config->toxin_level = 6;
}

// RU: Размер мира считается от размера клетки: 1x1 дает 128x64, 2x2 дает 64x32 и так далее.
// EN: World size is derived from cell size: 1x1 gives 128x64, 2x2 gives 64x32, and so on.
uint16_t cell_lab_config_world_width(const CellLabWorldConfig* config) {
    return CELL_LAB_SCREEN_WIDTH / config->cell_size;
}

uint16_t cell_lab_config_world_height(const CellLabWorldConfig* config) {
    return CELL_LAB_SCREEN_HEIGHT / config->cell_size;
}

uint16_t cell_lab_config_world_capacity(const CellLabWorldConfig* config) {
    return cell_lab_config_world_width(config) * cell_lab_config_world_height(config);
}

static uint16_t cell_lab_config_clamp_u16(uint16_t value, uint16_t min, uint16_t max) {
    if(value < min) {
        return min;
    } else if(value > max) {
        return max;
    }

    return value;
}

// RU: После изменения размера клетки пересчитываем стартовую жизнь и еду под новый объем мира.
// EN: After cell size changes, clamp starting life and food to the new world capacity.
static void cell_lab_config_clamp_to_world(CellLabWorldConfig* config) {
    const uint16_t capacity = cell_lab_config_world_capacity(config);
    config->initial_life = cell_lab_config_clamp_u16(config->initial_life, 1, capacity / 2U);
    config->initial_food = cell_lab_config_clamp_u16(config->initial_food, 1, capacity / 2U);
    config->food_low_watermark =
        (uint8_t)cell_lab_config_clamp_u16(config->food_low_watermark, 5, capacity / 3U);
}

// RU: Меню не имеет отдельного режима редактирования: OK циклически меняет выбранный параметр.
// EN: The menu has no separate edit mode: OK cycles the selected setting.
void cell_lab_config_change(CellLabWorldConfig* config, CellLabWorldSetting setting) {
    if(setting == CellLabWorldSettingCellSize) {
        config->cell_size++;
        if(config->cell_size > 6U) {
            config->cell_size = 1U;
        }
    } else if(setting == CellLabWorldSettingInitialLife) {
        config->initial_life += 40U;
    } else if(setting == CellLabWorldSettingInitialFood) {
        config->initial_food += 60U;
    } else if(setting == CellLabWorldSettingFoodGrowth) {
        config->food_growth++;
        if(config->food_growth > 12U) {
            config->food_growth = 1U;
        }
    } else if(setting == CellLabWorldSettingMoveCost) {
        config->move_cost++;
        if(config->move_cost > 5U) {
            config->move_cost = 1U;
        }
    } else if(setting == CellLabWorldSettingFoodEnergy) {
        config->food_energy++;
        if(config->food_energy > 12U) {
            config->food_energy = 3U;
        }
    } else if(setting == CellLabWorldSettingReproduceEnergy) {
        config->reproduce_energy += 4U;
        if(config->reproduce_energy > 48U) {
            config->reproduce_energy = 20U;
        }
    } else if(setting == CellLabWorldSettingMaxEnergy) {
        config->max_energy += 6U;
        if(config->max_energy > 72U) {
            config->max_energy = 30U;
        }
    } else if(setting == CellLabWorldSettingHeatSwing) {
        config->heat_swing += 2U;
        if(config->heat_swing > 15U) {
            config->heat_swing = 3U;
        }
    } else if(setting == CellLabWorldSettingRadiation) {
        config->radiation_level += 2U;
        if(config->radiation_level > 15U) {
            config->radiation_level = 0U;
        }
    } else if(setting == CellLabWorldSettingToxin) {
        config->toxin_level += 2U;
        if(config->toxin_level > 15U) {
            config->toxin_level = 0U;
        }
    }

    cell_lab_config_clamp_to_world(config);
}

const char* cell_lab_config_setting_name(CellLabWorldSetting setting) {
    if(setting == CellLabWorldSettingCellSize) {
        return "Cell";
    } else if(setting == CellLabWorldSettingInitialLife) {
        return "Start life";
    } else if(setting == CellLabWorldSettingInitialFood) {
        return "Start food";
    } else if(setting == CellLabWorldSettingFoodGrowth) {
        return "Food grow";
    } else if(setting == CellLabWorldSettingMoveCost) {
        return "Move cost";
    } else if(setting == CellLabWorldSettingFoodEnergy) {
        return "Food energy";
    } else if(setting == CellLabWorldSettingReproduceEnergy) {
        return "Split energy";
    } else if(setting == CellLabWorldSettingMaxEnergy) {
        return "Max energy";
    } else if(setting == CellLabWorldSettingHeatSwing) {
        return "Heat swing";
    } else if(setting == CellLabWorldSettingRadiation) {
        return "Radiation";
    }

    return "Toxin";
}

void cell_lab_config_setting_value(
    const CellLabWorldConfig* config,
    CellLabWorldSetting setting,
    char* buffer,
    size_t buffer_size) {
    if(setting == CellLabWorldSettingCellSize) {
        snprintf(
            buffer,
            buffer_size,
            "%ux%u %ux%u",
            (unsigned)config->cell_size,
            (unsigned)config->cell_size,
            (unsigned)cell_lab_config_world_width(config),
            (unsigned)cell_lab_config_world_height(config));
    } else if(setting == CellLabWorldSettingInitialLife) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->initial_life);
    } else if(setting == CellLabWorldSettingInitialFood) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->initial_food);
    } else if(setting == CellLabWorldSettingFoodGrowth) {
        snprintf(buffer, buffer_size, "%u/tick", (unsigned)config->food_growth);
    } else if(setting == CellLabWorldSettingMoveCost) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->move_cost);
    } else if(setting == CellLabWorldSettingFoodEnergy) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->food_energy);
    } else if(setting == CellLabWorldSettingReproduceEnergy) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->reproduce_energy);
    } else if(setting == CellLabWorldSettingMaxEnergy) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->max_energy);
    } else if(setting == CellLabWorldSettingHeatSwing) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->heat_swing);
    } else if(setting == CellLabWorldSettingRadiation) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->radiation_level);
    } else if(setting == CellLabWorldSettingToxin) {
        snprintf(buffer, buffer_size, "%u", (unsigned)config->toxin_level);
    } else {
        snprintf(buffer, buffer_size, "-");
    }
}
