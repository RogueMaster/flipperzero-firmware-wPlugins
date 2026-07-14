/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _401GUI_H_
#define _401GUI_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <furi.h>
#include <gui/gui.h>
#include <app_params.h>

uint8_t l401Gui_draw_btn(Canvas* canvas, uint8_t x, uint8_t y, uint8_t w, bool state, char* text);
#endif /* end of include guard: _401GUI_H_ */
