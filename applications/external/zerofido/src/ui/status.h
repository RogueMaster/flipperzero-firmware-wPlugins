/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>

#include "../zerofido_app_i.h"

/* Status view helpers bind the home screen and mirror transport connection state. */
void zerofido_ui_status_bind_view(ZerofidoApp *app);
void zerofido_ui_status_redraw(ZerofidoApp *app);
void zerofido_ui_apply_transport_connected(ZerofidoApp *app, bool connected);
