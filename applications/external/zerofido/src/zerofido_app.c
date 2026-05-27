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

#include "zerofido_app.h"

#include "app/lifecycle.h"
#include "zerofido_telemetry.h"
#include "zerofido_ui.h"
#include "zerofido_ui_i.h"

/*
 * Application composition root. Lifecycle setup opens storage/UI/runtime state,
 * then the view dispatcher owns the foreground thread until exit triggers
 * transport shutdown and secret scrubbing.
 */
int32_t zerofido_main(void *p) {
    UNUSED(p);

    zf_telemetry_log("main entry");
    ZerofidoApp *app = zf_app_lifecycle_alloc();
    if (!app) {
        zf_telemetry_log_oom("app alloc", sizeof(ZerofidoApp));
        return -1;
    }

    if (!zf_app_lifecycle_open(app)) {
        zf_telemetry_log("lifecycle open failed");
        zf_app_lifecycle_free(app);
        return -1;
    }

    zerofido_ui_switch_to_view(app, ZfViewStatus);
    zf_telemetry_log("startup async before");
    zf_app_lifecycle_startup_async(app);
    zf_telemetry_log("startup async after");
    view_dispatcher_run(app->view_dispatcher);
    zf_app_lifecycle_shutdown(app);
    zf_app_lifecycle_free(app);
    return 0;
}
