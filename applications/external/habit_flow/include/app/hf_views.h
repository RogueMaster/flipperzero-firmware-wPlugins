#pragma once

#include <gui/canvas.h>
#include <input/input.h>

void hf_views_main_draw(Canvas *canvas, void *model);
bool hf_views_main_input(InputEvent *event, void *context);

void hf_views_detail_draw(Canvas *canvas, void *model);
bool hf_views_detail_input(InputEvent *event, void *context);

void hf_views_manage_draw(Canvas *canvas, void *model);
bool hf_views_manage_input(InputEvent *event, void *context);

void hf_views_edit_draw(Canvas *canvas, void *model);
bool hf_views_edit_input(InputEvent *event, void *context);
