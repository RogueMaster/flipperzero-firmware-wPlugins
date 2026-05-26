#pragma once

#include "ui_internal.h"

void load_score_store(AppContext* app);
void record_score_if_done(AppContext* app);
void save_sound_setting(AppContext* app);
void draw_scores(Canvas* canvas, AppContext* app);
