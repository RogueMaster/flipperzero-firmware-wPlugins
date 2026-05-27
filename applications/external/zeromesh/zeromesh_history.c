#include "zeromesh_history.h"
#include <stdio.h>
#include <stdarg.h>

#define TAG "zeromesh_serial"

void history_add(ZeroMeshApp* app, const char* text, uint32_t from, uint32_t to, bool is_tx) {
    furi_mutex_acquire(app->lock, FuriWaitForever);

    uint8_t idx = app->history.head;
    Message* msg = &app->history.msgs[idx];

    snprintf(msg->text, sizeof(msg->text), "%s", text);
    msg->from = from;
    msg->to = to;
    msg->is_tx = is_tx;
    msg->timestamp = furi_get_tick() / 1000;

    app->history.head = (app->history.head + 1) % MSG_HISTORY;
    if(app->history.count < MSG_HISTORY) app->history.count++;

    FURI_LOG_I(TAG, "history_add: count=%u head=%u is_tx=%d text=%s",
               app->history.count, app->history.head, is_tx, text);

    furi_mutex_release(app->lock);
    view_port_update(app->vp);
}

void log_line(ZeroMeshApp* app, const char* fmt, ...) {
    if(!app) return;

    char buf[LOG_COLS];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    furi_mutex_acquire(app->lock, FuriWaitForever);
    snprintf(app->lines[app->line_head], LOG_COLS, "%s", buf);
    app->line_head = (app->line_head + 1) % LOG_LINES;

    if(app->log_paused) {
        if(app->log_scroll_offset < LOG_LINES - 7) {
            app->log_scroll_offset++;
        }
    }

    furi_mutex_release(app->lock);

    FURI_LOG_I(TAG, "%s", buf);
}

void set_status(ZeroMeshApp* app, const char* fmt, ...) {
    if(!app) return;

    va_list args;
    va_start(args, fmt);
    furi_mutex_acquire(app->lock, FuriWaitForever);
    vsnprintf(app->status, sizeof(app->status), fmt, args);
    furi_mutex_release(app->lock);
    va_end(args);

    view_port_update(app->vp);
}
