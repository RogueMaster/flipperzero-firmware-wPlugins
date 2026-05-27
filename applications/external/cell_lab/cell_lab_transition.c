#include "cell_lab_transition.h"

// RU: Модуль переходов знает только про анимацию страниц и ничего не знает о мире.
// EN: The transition module only knows page animation and knows nothing about the world.

// RU: Значения по умолчанию дают короткий переход, который ощущается плавно, но не тормозит меню.
// EN: Defaults provide a short transition that feels smooth without slowing down navigation.
void cell_lab_transition_init(CellLabTransition* transition) {
    transition->enabled = true;
    transition->active = false;
    transition->duration_ms = 140;
    transition->started_at = 0;
    transition->from_screen = CellLabScreenWorld;
    transition->to_screen = CellLabScreenWorld;
    transition->direction = 1;
}

void cell_lab_transition_start(
    CellLabTransition* transition,
    CellLabScreen from_screen,
    CellLabScreen to_screen,
    int8_t direction,
    uint32_t now) {
    // RU: Если анимация выключена, сразу считаем новую страницу активной.
    // EN: If animation is disabled, immediately treat the new page as active.
    if(!transition->enabled || (from_screen == to_screen)) {
        transition->active = false;
        transition->from_screen = to_screen;
        transition->to_screen = to_screen;
        return;
    }

    transition->active = true;
    transition->started_at = now;
    transition->from_screen = from_screen;
    transition->to_screen = to_screen;
    transition->direction = (direction >= 0) ? 1 : -1;
}

// RU: Возвращает offsets двух страниц. Когда время вышло, переход сам выключается.
// EN: Returns offsets for two pages. When time is over, the transition disables itself.
bool cell_lab_transition_offsets(
    CellLabTransition* transition,
    uint32_t now,
    int16_t* from_offset,
    int16_t* to_offset) {
    if(!transition->active) {
        *from_offset = 0;
        *to_offset = 0;
        return false;
    }

    const uint32_t elapsed = now - transition->started_at;

    // RU: По окончании анимации обе страницы больше не смещаются.
    // EN: Once animation is complete, pages no longer need offsets.
    if(elapsed >= transition->duration_ms) {
        transition->active = false;
        *from_offset = 0;
        *to_offset = 0;
        return false;
    }

    const int16_t slide = (int16_t)((elapsed * CELL_LAB_SCREEN_WIDTH) / transition->duration_ms);

    // RU: Направление определяет, откуда въезжает новая страница.
    // EN: Direction controls which side the new page slides in from.
    if(transition->direction > 0) {
        *from_offset = -slide;
        *to_offset = CELL_LAB_SCREEN_WIDTH - slide;
    } else {
        *from_offset = slide;
        *to_offset = (int16_t)(-CELL_LAB_SCREEN_WIDTH + slide);
    }

    return true;
}

void cell_lab_transition_cycle_enabled(CellLabTransition* transition) {
    transition->enabled = !transition->enabled;
    transition->active = false;
}

void cell_lab_transition_cycle_duration(CellLabTransition* transition) {
    if(transition->duration_ms < 120U) {
        transition->duration_ms = 140U;
    } else if(transition->duration_ms < 220U) {
        transition->duration_ms = 240U;
    } else {
        transition->duration_ms = 80U;
    }
}
