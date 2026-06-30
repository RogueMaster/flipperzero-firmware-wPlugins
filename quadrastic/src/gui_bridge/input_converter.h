/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stddef.h>

#include <input/input.h>

#include "src/engine/game_engine.h"

typedef struct InputConverter InputConverter;

InputConverter*
input_converter_alloc(void);

void
input_converter_free(InputConverter* input_converter);

void
input_converter_reset(InputConverter* input_converter);

void
input_converter_process_state(InputConverter* input_converter,
                              InputState* input_state);

FuriStatus
input_converter_get_event(InputConverter* input_converter, InputEvent* event);
