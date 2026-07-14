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

#include "view_module_descriptions.h"

#include "gui/modules/variable_item_list.h"

const ViewModuleDescription variable_item_list_description = {
    .alloc = (ViewModuleAllocCallback)variable_item_list_alloc,
    .free = (ViewModuleFreeCallback)variable_item_list_free,
    .get_view = (ViewModuleGetViewCallback)variable_item_list_get_view,
};
