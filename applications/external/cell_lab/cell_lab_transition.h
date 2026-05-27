#pragma once

#include "cell_lab_common.h"

// RU: Плавный переход живет отдельно: его можно отключить без касания логики меню.
// EN: Smooth page transition lives separately and can be disabled without touching menu logic.
typedef struct {
    bool enabled;
    bool active;
    uint16_t duration_ms;
    uint32_t started_at;
    CellLabScreen from_screen;
    CellLabScreen to_screen;
    int8_t direction;
} CellLabTransition;

// RU: Заполняет параметры перехода значениями по умолчанию.
// EN: Fills transition settings with default values.
void cell_lab_transition_init(CellLabTransition* transition);

// RU: Запускает анимацию перехода между двумя страницами.
// EN: Starts the animation between two pages.
void cell_lab_transition_start(
    CellLabTransition* transition,
    CellLabScreen from_screen,
    CellLabScreen to_screen,
    int8_t direction,
    uint32_t now);

// RU: Рассчитывает смещения старой и новой страниц для текущего кадра.
// EN: Calculates old/new page offsets for the current frame.
bool cell_lab_transition_offsets(
    CellLabTransition* transition,
    uint32_t now,
    int16_t* from_offset,
    int16_t* to_offset);

// RU: Включает или выключает плавную прокрутку.
// EN: Enables or disables smooth scrolling.
void cell_lab_transition_cycle_enabled(CellLabTransition* transition);

// RU: Циклически меняет длительность анимации.
// EN: Cycles the animation duration.
void cell_lab_transition_cycle_duration(CellLabTransition* transition);
