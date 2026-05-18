#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <i2c_tools_cli_icons.h>

#define INFOS_TEXT        "INFOS"
#define INFOS_PAGES_COUNT 6

typedef struct {
    uint8_t page;
} i2cInfos;

i2cInfos* i2c_infos_alloc(void);
void i2c_infos_free(i2cInfos* infos);
void draw_infos_view(Canvas* canvas, i2cInfos* infos);
