#pragma once

#include <stdbool.h>
#include <stdint.h>

// RU: Физический экран Flipper Zero; мир масштабируется размером клетки, но экран остается 128x64.
// EN: Physical Flipper Zero screen; the world scales by cell size, while the screen stays 128x64.
#define CELL_LAB_SCREEN_WIDTH 128U
#define CELL_LAB_SCREEN_HEIGHT 64U
#define CELL_LAB_MAX_CELLS (CELL_LAB_SCREEN_WIDTH * CELL_LAB_SCREEN_HEIGHT)

// RU: Маленький HUD всплывает после действий и не перекрывает мир постоянно.
// EN: A small HUD appears after actions and does not permanently cover the world.
#define CELL_LAB_HUD_VISIBLE_MS 1800U

// RU: В Furi бесконечное ожидание обычно передают как максимальный uint32_t.
// EN: In Furi, infinite waiting is commonly represented by the maximum uint32_t value.
#define CELL_LAB_WAIT_FOREVER 0xFFFFFFFFU

// RU: Возможные состояния одной клетки мира: пустота, еда или жизнь.
// EN: Possible states of one world cell: empty space, food, or life.
typedef enum {
    CellLabCellEmpty = 0,
    CellLabCellFood = 1,
    CellLabCellLife = 2,
} CellLabCell;

// RU: Страницы приложения; стрелки влево/вправо ходят по этому кольцу.
// EN: Application pages; left/right buttons cycle through this ring.
typedef enum {
    CellLabScreenWorld = 0,
    CellLabScreenState = 1,
    CellLabScreenWorldSettings = 2,
    CellLabScreenAppSettings = 3,
    CellLabScreenAbout = 4,
    CellLabScreenCount = 5,
} CellLabScreen;

// RU: Поведенческие программы, которые геном использует как "характер" клетки.
// EN: Behavior programs used by the genome as a cell's "character".
typedef enum {
    CellLabProgramForager = 0,
    CellLabProgramPhoto = 1,
    CellLabProgramThermo = 2,
    CellLabProgramNomad = 3,
} CellLabProgram;

// RU: Пункты меню мира: меняют физику, размер и раздражители симуляции.
// EN: World menu items: change physics, size, and environmental irritants.
typedef enum {
    CellLabWorldSettingCellSize = 0,
    CellLabWorldSettingInitialLife,
    CellLabWorldSettingInitialFood,
    CellLabWorldSettingFoodGrowth,
    CellLabWorldSettingMoveCost,
    CellLabWorldSettingFoodEnergy,
    CellLabWorldSettingReproduceEnergy,
    CellLabWorldSettingMaxEnergy,
    CellLabWorldSettingHeatSwing,
    CellLabWorldSettingRadiation,
    CellLabWorldSettingToxin,
    CellLabWorldSettingCount,
} CellLabWorldSetting;

// RU: Пункты меню приложения: влияют на интерфейс и скорость, но не на законы мира.
// EN: App menu items: affect UI and speed, not the world's rules.
typedef enum {
    CellLabAppSettingSpeed = 0,
    CellLabAppSettingTransitionEnabled,
    CellLabAppSettingTransitionDuration,
    CellLabAppSettingCount,
} CellLabAppSetting;

// RU: Проверка времени с учетом переполнения системного счетчика тиков.
// EN: Time comparison that remains correct when the system tick counter wraps.
static inline bool cell_lab_time_reached(uint32_t now, uint32_t target) {
    return ((int32_t)(now - target) >= 0);
}
