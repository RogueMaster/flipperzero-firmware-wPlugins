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

#include "src/engine/entity.h"

#include "view_module_descriptions.h"

/** Prototype for back event callback */
typedef bool (*ViewModuleBackCallback)(void* context);

Entity*
view_module_add_to_level(Level* level,
                         GameManager* manager,
                         const ViewModuleDescription* module_description);

void*
view_module_get_module(Entity* entity);

void
view_module_set_back_callback(Entity* entity,
                              ViewModuleBackCallback back_callback,
                              void* context);

extern const EntityDescription view_module_description;