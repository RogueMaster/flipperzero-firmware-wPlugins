/* Copyright (c) 2025 Eero Prittinen */

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include "types.h"

void drawEnemy(Canvas *canvas, double dir, fighter_jet_t enemy);
void drawCompass(Canvas *canvas, double direction);
void drawMinimap(Canvas *canvas, double dir, fighter_jet_t *enemies, size_t enemiesCount);
void drawJet(Canvas *canvas, turning_t turning, bool shooting);
void drawExplosion(Canvas *canvas);
