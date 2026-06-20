#pragma once

#include "cell_lab_config.h"

#include <stddef.h>

// RU: Геном остается 16-битным, чтобы два буфера мира не съели всю память Flipper.
// EN: The genome stays 16-bit so double world buffers do not consume too much Flipper memory.
typedef uint16_t CellLabGenome;

// RU: Полное состояние симуляции, включая текущий мир, следующий мир и сводные метрики.
// EN: Full simulation state, including current world, next world, and summary metrics.
typedef struct {
    // RU: Текущие законы мира, редактируемые через меню World Settings.
    // EN: Current world rules edited through the World Settings menu.
    CellLabWorldConfig config;

    // RU: Текущий кадр мира: тип клетки, энергия живых клеток и их геном.
    // EN: Current world frame: cell type, life energy, and life genome.
    uint8_t cell[CELL_LAB_MAX_CELLS];
    uint8_t energy[CELL_LAB_MAX_CELLS];
    CellLabGenome genome[CELL_LAB_MAX_CELLS];

    // RU: Следующий кадр мира, куда записывается результат одного шага симуляции.
    // EN: Next world frame where one simulation step writes its result.
    uint8_t next_cell[CELL_LAB_MAX_CELLS];
    uint8_t next_energy[CELL_LAB_MAX_CELLS];
    CellLabGenome next_genome[CELL_LAB_MAX_CELLS];

    // RU: Временные маски шага: кто уже съеден и кто уже сделал ход.
    // EN: Temporary step masks: who has been eaten and who has already moved.
    uint8_t consumed[CELL_LAB_MAX_CELLS];
    uint8_t processed[CELL_LAB_MAX_CELLS];

    // RU: Счетчики времени, событий и псевдослучайное состояние мира.
    // EN: Time/event counters and the world's pseudo-random state.
    uint32_t rng;
    uint32_t generation;
    uint32_t epoch;
    uint32_t births;
    uint32_t deaths;
    uint32_t mutations;
    uint32_t cannibal_events;

    // RU: Текущее количество сущностей и распределение поведенческих программ.
    // EN: Current entity counts and distribution of behavior programs.
    uint16_t life_count;
    uint16_t food_count;
    uint16_t program_count[4];

    // RU: Средние значения для экрана World State.
    // EN: Average values shown on the World State screen.
    uint8_t avg_energy;
    uint8_t avg_temp_gene;
    uint8_t avg_light_gene;
    uint8_t avg_world_temp;
    uint8_t avg_radiation;
    uint8_t avg_toxin;
    uint8_t avg_crowding;
    uint8_t dominant_program;
    uint8_t food_phase;
    uint8_t climate_seed;
} CellLabWorld;

// RU: Инициализирует мир и сразу создает первую эпоху.
// EN: Initializes the world and immediately creates the first epoch.
void cell_lab_world_init(CellLabWorld* world, const CellLabWorldConfig* config);

// RU: Полностью пересоздает живой мир с текущими настройками.
// EN: Fully recreates the living world with current settings.
void cell_lab_world_start_new_generation(CellLabWorld* world);

// RU: Применяет новые настройки и начинает новую эпоху.
// EN: Applies new settings and starts a new epoch.
void cell_lab_world_apply_config(CellLabWorld* world, const CellLabWorldConfig* config);

// RU: Выполняет один тик симуляции.
// EN: Executes one simulation tick.
void cell_lab_world_step(CellLabWorld* world);

// RU: Размеры и вместимость активного мира в клетках.
// EN: Active world dimensions and capacity in cells.
uint16_t cell_lab_world_width(const CellLabWorld* world);
uint16_t cell_lab_world_height(const CellLabWorld* world);
uint16_t cell_lab_world_capacity(const CellLabWorld* world);

// RU: Значения раздражителей мира в конкретной точке.
// EN: Environmental irritant values at a specific point.
uint8_t cell_lab_world_light_at(const CellLabWorld* world, uint16_t y);
uint8_t cell_lab_world_temperature_at(const CellLabWorld* world, uint16_t y);
uint8_t cell_lab_world_radiation_at(const CellLabWorld* world, uint16_t x, uint16_t y);
uint8_t cell_lab_world_toxin_at(const CellLabWorld* world, uint16_t x, uint16_t y);

// RU: Перевод координат клетки в индекс плоского массива.
// EN: Converts cell coordinates into a flat array index.
static inline size_t cell_lab_world_index(const CellLabWorld* world, uint16_t x, uint16_t y) {
    return (y * cell_lab_world_width(world)) + x;
}
