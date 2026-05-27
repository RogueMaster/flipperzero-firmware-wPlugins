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

#include "../zerofido_app_i.h"

/*
 * App lifecycle order:
 *   alloc -> open -> startup_async/startup_pending -> shutdown -> free
 *
 * startup_pending is polled from the UI loop. It joins a completed startup
 * thread and may start the selected transport worker once backend init succeeds.
 */
ZerofidoApp *zf_app_lifecycle_alloc(void);
bool zf_app_lifecycle_open(ZerofidoApp *app);
bool zf_app_lifecycle_startup(ZerofidoApp *app);
bool zf_app_lifecycle_startup_async(ZerofidoApp *app);
void zf_app_lifecycle_wait_startup(ZerofidoApp *app);
bool zf_app_lifecycle_startup_pending(ZerofidoApp *app);
bool zf_app_lifecycle_restart_transport(ZerofidoApp *app);
bool zf_app_lifecycle_set_transport_mode(ZerofidoApp *app, Storage *storage, ZfTransportMode mode);
void zf_app_lifecycle_shutdown(ZerofidoApp *app);
void zf_app_lifecycle_free(ZerofidoApp *app);
