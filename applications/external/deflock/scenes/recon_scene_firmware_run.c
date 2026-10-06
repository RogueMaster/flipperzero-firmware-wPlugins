// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/plugin_host.h"
#include "../plugins/flasher_plugin_api.h"

#include <string.h>

// The flasher is an on-demand .fal, not app code. Mapped in on entry to this
// screen and dropped on the way out -- ~27.6 KB that every other screen, and the
// launch itself, no longer has to find room for.
//
// File-scope because the WORKER THREAD needs the API and only ever gets `app` as
// its context, and because only one flash or backup can be in flight at a time
// (this scene owns the thread). Same shape as g_qr_plugin in the handoff scene.
static PluginHost* g_fw_plugin = NULL;
static const FlasherPluginApi* g_fw_api = NULL;

static void fw_log_cb(void* ctx, const char* line) {
    ReconApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    furi_string_cat_printf(app->fw_log, "%s\n", line);
    app->fw_log_dirty = true;
    furi_mutex_release(app->mutex);
}

static void fw_progress_cb(void* ctx, int pct) {
    ReconApp* app = ctx;
    if(pct < 0) pct = 0;
    if(pct > 100) pct = 100;
    app->fw_pct = pct; // volatile int, single writer -- no lock needed
    app->fw_log_dirty = true;
}

static int32_t fw_worker(void* context) {
    ReconApp* app = context;
    const FlasherPluginApi* api = g_fw_api;
    // on_enter refuses to start this thread without a loaded plugin, so this is
    // belt-and-braces rather than an expected path -- but it is called from a
    // thread, and a null deref here would take the whole app down mid-flash.
    if(!api) {
        fw_log_cb(app, "Flasher unavailable.");
        fw_log_cb(app, "== FAILED ==");
        app->fw_ok = false;
        app->fw_running = false;
        return 0;
    }
    EspFlasher* fl =
        api->alloc((FuriHalSerialId)app->settings.esp_uart, fw_log_cb, fw_progress_cb, app);
    bool ok = false;
    if(!fl) {
        fw_log_cb(app, "UART busy.");
    } else {
        // Both flash (write) and backup (read) talk to the raw ROM loader -- no
        // stub is ever uploaded (the 0xchocolate approach), so the "loader
        // resident / overlapping address" error can't occur. Backup forces Safe
        // baud (ROM reads are slow + integrity matters); flash allows the user's
        // Fast (230400) and verifies the write afterwards.
        uint32_t fast = (app->fw_op == 0 || !app->settings.flash_fast) ? 0 : 230400;
        if(api->connect(fl, fast)) {
            if(app->fw_op == 0) {
                ok = api->backup(fl, app->storage, app->fw_path);
            } else {
                app->fw_writing = true;
                ok = api->flash_file(fl, app->storage, app->fw_path, 0);
                app->fw_writing = false;
            }
        }
        api->free(fl);
    }
    // BACK TO THE LOG VIEW BEFORE THE LAST LINES GO OUT.
    //
    // While pct >= 0 the renderer draws the progress layout, which has room for
    // one status line and a bar -- so anything logged after the transfer would
    // be invisible behind "100%". Dropping to -1 hands the screen to the
    // one-page log, where the closing lines below are actually readable.
    app->fw_pct = -1;

    // The "Tap RESET on ESP" hint belongs to the flasher, which is what knows it
    // left the board in the download loader -- both operations emit their own.
    // Do NOT add one here: doing that once produced three lines all saying the
    // same thing, because the plugin's had merely been INVISIBLE behind the
    // progress layout rather than missing.
    fw_log_cb(app, ok ? "== DONE ==" : "== FAILED ==");
    app->fw_ok = ok;
    app->fw_running = false;
    return 0;
}

// Render the flasher status. FIXED LAYOUT IN BOTH STATES -- this screen never
// scrolls.
//
// The percentage used to be log lines, so a flash pushed eleven of them into a
// scrolling text box and the operator had to scroll to see where it was. A
// transfer in progress gets a fixed three-line layout with the bar pinned to the
// bottom. Everything else -- connecting, retrying, failed -- gets the newest few
// lines at fixed positions, because the one line that matters there ("hold BOOT,
// tap RESET") is only actionable while the attempt counter is running, and a
// prompt you have to scroll to find is a prompt you miss.
static void fw_render(ReconApp* app) {
    widget_reset(app->widget);
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    const char* full = furi_string_get_cstr(app->fw_log);
    size_t len = strlen(full);
    int pct = app->fw_pct;

    if(pct >= 0) {
        // Last non-empty log line = what the flasher is currently doing.
        const char* last = full;
        for(int i = (int)len - 2; i >= 0; i--) {
            if(full[i] == '\n') {
                last = full + i + 1;
                break;
            }
        }
        snprintf(app->fw_status, sizeof(app->fw_status), "%s", last);
        char* nl = strchr(app->fw_status, '\n');
        if(nl) *nl = '\0';

        // 12 cells at ~6 px each keeps "[############] 100%" inside 128 px with
        // the default font. Integer maths only -- no floats on this path.
        char bar[40];
        int filled = (pct * 12) / 100;
        int o = 0;
        bar[o++] = '[';
        for(int i = 0; i < 12; i++)
            bar[o++] = (i < filled) ? '#' : '-';
        bar[o++] = ']';
        snprintf(bar + o, sizeof(bar) - (size_t)o, " %d%%", pct);

        widget_add_string_element(
            app->widget,
            0,
            2,
            AlignLeft,
            AlignTop,
            FontPrimary,
            app->fw_op == 0 ? "Backing up..." : "Flashing...");
        widget_add_string_element(
            app->widget, 0, 22, AlignLeft, AlignTop, FontSecondary, app->fw_status);
        widget_add_string_element(app->widget, 64, 44, AlignCenter, AlignTop, FontPrimary, bar);
        furi_mutex_release(app->mutex);
        return;
    }

    // Not transferring (connecting, retrying, failed): ONE PAGE, NEVER A SCROLL.
    //
    // This used to be a text-scroll element holding the last eight lines, so the
    // operator had to scroll to read a prompt that is only actionable in the
    // moment -- "hold BOOT, tap RESET" is useless if it is off-screen while the
    // attempt counter is running. Nothing here is long enough to need paging;
    // the newest four lines are the whole story and they fit.
    //
    // Newest LAST, reading top to bottom like the log it replaces.
#define FW_VIEW_LINES 4
// Copy budget only, NOT a display width. widget_add_string_element draws onto a
// canvas that clips at 128 px, so a long line is cut at the true pixel edge for
// free. Truncating to an estimated character count instead threw away text that
// would have fitted: "Power-cycle" rendered as "Power-cyc" at a 25-char guess,
// with the screen visibly not full. Copy generously and let the canvas decide.
#define FW_VIEW_COLS  47
    char lines[FW_VIEW_LINES][FW_VIEW_COLS + 1];
    int got = 0;

    // Walk backwards, newest first, collecting non-empty lines.
    int end = (int)len;
    while(end > 0 && got < FW_VIEW_LINES) {
        while(end > 0 && (full[end - 1] == '\n' || full[end - 1] == '\r'))
            end--;
        if(end <= 0) break;
        int start_i = end;
        while(start_i > 0 && full[start_i - 1] != '\n')
            start_i--;
        int n = end - start_i;
        if(n > FW_VIEW_COLS) n = FW_VIEW_COLS;
        memcpy(lines[got], full + start_i, (size_t)n);
        lines[got][n] = '\0';
        got++;
        end = start_i;
    }

    widget_add_string_element(
        app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, app->fw_op == 0 ? "Backup" : "Flash");

    // Reverse into display order: oldest of the four at the top.
    for(int i = 0; i < got; i++) {
        widget_add_string_element(
            app->widget, 0, 15 + (got - 1 - i) * 12, AlignLeft, AlignTop, FontSecondary, lines[i]);
    }
    furi_mutex_release(app->mutex);
}

void recon_scene_firmware_run_on_enter(void* context) {
    ReconApp* app = context;

    // GIVE THE FLASHER THE MEMORY BEFORE IT ASKS FOR IT.
    //
    // The plugin is a ~23 KB .fal that has to map into ONE contiguous block, on
    // top of a 2 KB UART stream buffer and a 2 KB transfer chunk. With the app's
    // four detection tables resident the largest block left was ~25 KB, and a
    // firmware backup ran the device out of memory and crashed it outright --
    // reproduced on the bench, not theorised.
    //
    // Nothing is scanning here: this screen owns the UART and the ESP/GPS links
    // are already down, so no worker can touch the tables while they are gone.
    // Hits are persisted by release() first, and the tables come back on exit.
    recon_tables_release(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    furi_string_reset(app->fw_log);
    app->fw_pct = -1; // no transfer yet -- the log view, not the bar
    furi_mutex_release(app->mutex);

    fw_log_cb(app, app->fw_op == 0 ? "BACKUP firmware" : "FLASH firmware");
    fw_log_cb(app, "Put ESP in bootloader:");
    fw_log_cb(app, "hold BOOT, tap RESET.");
    fw_log_cb(app, "Working...");

    // The worker needs a chunk of heap (4 KB thread stack + UART buffer + the
    // esp-serial-flasher stub upload). A FAP shares the Flipper's ~256 KB RAM
    // with the firmware, so on a busy system this can come up short. Check up
    // front and fail with a message instead of letting an allocation abort the
    // whole app (the "out of memory" crash).
    if(memmgr_get_free_heap() < 10 * 1024) {
        fw_log_cb(app, "Not enough free RAM.");
        fw_log_cb(app, "Reboot the Flipper, open");
        fw_log_cb(app, "only FlipDeFlock, retry.");
        fw_log_cb(app, "== FAILED ==");
        app->fw_running = false;
        app->fw_ok = false;
        fw_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
        return;
    }

    // Map the flasher in for the lifetime of this screen. Deliberately AFTER the
    // heap check: the .fal needs room too, so asking for it on a device that is
    // already short would just turn a readable message into a failed load.
    //
    // A failure here is not a crash -- plugin_host_load() returns NULL for a
    // missing asset directory, a version mismatch or a corrupt .fal, and the
    // user gets told the feature is unavailable and can back out. The likeliest
    // real cause is a card the firmware has not extracted app assets onto yet.
    g_fw_api = NULL;
    g_fw_plugin = plugin_host_load(
        FLASHER_PLUGIN_APP_ID, FLASHER_PLUGIN_API_VERSION, (const void**)&g_fw_api);
    if(!g_fw_plugin || !g_fw_api) {
        plugin_host_free(g_fw_plugin);
        g_fw_plugin = NULL;
        g_fw_api = NULL;
        // Name the actual cause instead of guessing at one. The old text said
        // "reinstall the .fap so its assets are extracted", which is only ONE of
        // the reasons this fails and was simply wrong for issue #23 -- a
        // RogueMaster user whose assets were extracted under a different
        // directory name than the running app resolved to. Being told to
        // reinstall something already installed correctly wastes the reporter's
        // time and tells the maintainer nothing.
        //
        // The log carries every path that was tried (see plugin_host.c); this
        // points at it, because a screenshot of this screen is what actually
        // arrives on an issue.
        fw_log_cb(app, "Flasher plugin not found.");
        fw_log_cb(app, "Checked /ext/apps_assets/");
        fw_log_cb(app, "<app name>/plugins/.");
        fw_log_cb(app, "Try: reinstall the .fap,");
        fw_log_cb(app, "or send the CLI log -- it");
        fw_log_cb(app, "lists every path tried.");
        fw_log_cb(app, "== FAILED ==");
        app->fw_running = false;
        app->fw_ok = false;
        fw_render(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
        return;
    }

    app->fw_running = true;
    app->fw_ok = false;
    app->fw_back_warned = false;
    app->fw_writing = false;
    app->fw_log_dirty = false;
    app->fw_thread = furi_thread_alloc_ex("FlipDeFlockFlash", 4096, fw_worker, app);
    furi_thread_start(app->fw_thread);

    fw_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_firmware_run_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        // Only rebuild the widget when the log actually changed.
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool dirty = app->fw_log_dirty;
        app->fw_log_dirty = false;
        furi_mutex_release(app->mutex);
        if(dirty) fw_render(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && app->fw_running && app->fw_writing) {
        // Back is LOCKED while flash is actually being WRITTEN. Unconsumed, it
        // went straight to on_exit, which aborts the worker -- so one bump of the
        // key mid-write left a half-flashed ESP32. Recoverable (the ROM loader
        // always allows a reflash), but in the field that is a dead companion.
        // Say so once, then keep swallowing presses until the worker is done.
        //
        // ONLY THE FLASH OPERATION, and ALL of it. fw_writing brackets
        // flash_file() in the worker, which covers the erase as well as the
        // write: the erase runs before the first progress callback, so a lock
        // keyed on progress left a window in which Back aborted with the chip
        // already wiped. The connect phase stays cancellable -- up to twenty
        // sync attempts, over two minutes with no way out when the board is
        // simply not in its bootloader, and nothing has been touched yet. A
        // backup only reads, so aborting it costs a partial file and no more.
        if(!app->fw_back_warned) {
            app->fw_back_warned = true;
            fw_log_cb(app, "Back locked until done.");
        }
        return true;
    }
    return false;
}

void recon_scene_firmware_run_on_exit(void* context) {
    ReconApp* app = context;
    // ORDER IS LOAD-BEARING. The worker is executing code that lives inside the
    // mapped .fal, so the plugin must outlive the thread: abort, JOIN, and only
    // then unmap. Freeing first would pull the text out from under a running
    // thread mid-flash, which is both a crash and a half-written ESP32.
    if(app->fw_thread) {
        if(g_fw_api) g_fw_api->abort(); // stop a long flash/backup so join returns
        furi_thread_join(app->fw_thread);
        furi_thread_free(app->fw_thread);
        app->fw_thread = NULL;
    }
    g_fw_api = NULL; // drop the borrowed pointer before unmapping what it points into
    plugin_host_free(g_fw_plugin);
    g_fw_plugin = NULL;

    // Tables back, AFTER the plugin is unmapped so the allocation lands in the
    // block it just vacated rather than fragmenting around it. Restores the
    // saved hits release() flushed on the way in.
    recon_tables_acquire(app);
    recon_hits_load(app);
    recon_app_purge_excluded(app);
    widget_reset(app->widget);
}
