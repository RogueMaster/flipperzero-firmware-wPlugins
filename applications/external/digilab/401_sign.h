/**
*  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
*  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
*  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
*    + Tixlegeek
*/
#ifndef L401_SIGN_H_
#define L401_SIGN_H_

#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
//#include <assets_icons.h>
#include <401_digilab_icons.h>
#include <401_err.h>

void l401_sign_input_callback(InputEvent* input, void* ctx);
void l401_sign_render_callback(Canvas* canvas, void* ctx);
int32_t l401_sign_app(l401_err err);

#endif /* end of include guard: L401_SIGN_H_ */
