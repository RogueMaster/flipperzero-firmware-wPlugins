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

typedef struct View View;

typedef void* (*ViewModuleAllocCallback)(void);
typedef void (*ViewModuleFreeCallback)(void* view_module);
typedef View* (*ViewModuleGetViewCallback)(void* view_module);

typedef struct
{
    ViewModuleAllocCallback alloc;
    ViewModuleFreeCallback free;
    ViewModuleGetViewCallback get_view;
} ViewModuleDescription;

extern const ViewModuleDescription variable_item_list_description;
