#pragma once

#include <gui/canvas.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t focal_mm;
    size_t fstop_idx;
    float coc_mm;
    float h_meters;
    char result_text[32];
} CalcSceneModel;

void calc_scene_draw(Canvas *canvas, const CalcSceneModel *model);
