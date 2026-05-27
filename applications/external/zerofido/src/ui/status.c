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

#include "../zerofido_ui.h"

#include <furi/core/string.h>
#include <stdio.h>
#include <string.h>

#include <gui/elements.h>

#include "../zerofido_app_i.h"
#include "../zerofido_store.h"
#include "../zerofido_ui_i.h"
#include "status.h"

#define ZF_HOME_VISIBLE_ITEMS 2U
#define ZF_HOME_TEXT_SCROLL_TICK_DIVISOR 500U

typedef struct {
    bool resident_key;
    char title[72];
    char subtitle[96];
} ZfHomeCredentialItem;

typedef enum {
    ZfHomeStatusReady = 0,
    ZfHomeStatusConnected,
    ZfHomeStatusWaiting,
    ZfHomeStatusError,
} ZfHomeStatus;

typedef struct {
    ZfHomeStatus status;
    ZfTransportMode transport_mode;
    size_t item_count;
    size_t selected_item;
    size_t visible_start;
    size_t visible_count;
    ZfHomeCredentialItem items[ZF_HOME_VISIBLE_ITEMS];
} ZfStatusModel;

typedef struct {
    ZfCredentialRecord record;
    uint8_t store_io[ZF_STORE_RECORD_IO_SIZE];
} ZfStatusCredentialScratch;

_Static_assert(sizeof(ZfStatusCredentialScratch) <= ZF_UI_SCRATCH_SIZE,
               "status credential scratch exceeds UI arena");

static bool zf_status_text_is_error(const char *text) {
    return text && (strstr(text, "failed") || strstr(text, "Failed") || strstr(text, "error") ||
                    strstr(text, "denied") || strstr(text, "timed out") ||
                    strstr(text, "Could not") || strstr(text, "not found"));
}

static const char *zf_home_status_label(ZfHomeStatus status) {
    switch (status) {
    case ZfHomeStatusConnected:
        return "Connected";
    case ZfHomeStatusWaiting:
        return "Waiting";
    case ZfHomeStatusError:
        return "Error";
    case ZfHomeStatusReady:
    default:
        return "Ready";
    }
}

static const char *zf_home_transport_title_label(ZfTransportMode mode) {
    return mode == ZfTransportModeNfc ? "NFC" : "USB";
}

static bool zf_status_format_record_item(ZfCredentialRecord *record, char *title, size_t title_size,
                                         char *subtitle, size_t subtitle_size) {
    const char *user = NULL;
    const char *website = NULL;

    if (!record || !title || !subtitle) {
        return false;
    }

    website = record->rp_id[0] ? record->rp_id : "Unknown website";
    if (record->user_display_name[0]) {
        user = record->user_display_name;
    } else if (record->user_name[0]) {
        user = record->user_name;
    } else {
        user = "No account name";
    }

    snprintf(title, title_size, "%.70s", website);
    snprintf(subtitle, subtitle_size, "%.94s", user);
    return true;
}

static bool zf_status_refresh_model(ZerofidoApp *app, bool redraw, bool reload_credentials) {
    ZfStatusModel snapshot = {0};
    ZfCredentialIndexEntry entries[ZF_HOME_VISIBLE_ITEMS];
    ZfStatusCredentialScratch *scratch = NULL;
    uint32_t selected_record_index = 0;

    if (!app || !app->status_view) {
        return false;
    }

    memset(entries, 0, sizeof(entries));
    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    snapshot.transport_mode = app->runtime_config.transport_mode;
    if (zf_status_text_is_error(app->status_text)) {
        snapshot.status = ZfHomeStatusError;
    } else if (app->approval.state == ZfApprovalPending || app->status_text[0] != '\0' ||
               !app->startup_complete || !app->worker_thread) {
        snapshot.status = ZfHomeStatusWaiting;
    } else if (app->transport_connected) {
        snapshot.status = ZfHomeStatusConnected;
    } else {
        snapshot.status = ZfHomeStatusReady;
    }
    if (!reload_credentials) {
        furi_mutex_release(app->ui_mutex);
        with_view_model(
            app->status_view, ZfStatusModel * model,
            {
                model->status = snapshot.status;
                model->transport_mode = snapshot.transport_mode;
            },
            redraw);
        return true;
    }
    selected_record_index = app->credentials_selected_index;
    if (app->startup_complete && app->store.records) {
        uint32_t first_record_index = UINT32_MAX;
        bool selected_found = false;

        for (size_t i = 0; i < app->store.count; ++i) {
            if (!app->store.records[i].in_use) {
                continue;
            }

            if (first_record_index == UINT32_MAX) {
                first_record_index = (uint32_t)i;
            }
            if ((uint32_t)i == selected_record_index) {
                snapshot.selected_item = snapshot.item_count;
                selected_found = true;
            }
            ++snapshot.item_count;
        }

        if (snapshot.item_count > 0 && !selected_found) {
            selected_record_index = first_record_index;
            app->credentials_selected_index = selected_record_index;
            snapshot.selected_item = 0;
        }

        if (snapshot.selected_item >= ZF_HOME_VISIBLE_ITEMS) {
            snapshot.visible_start = snapshot.selected_item - ZF_HOME_VISIBLE_ITEMS + 1;
        }
        if (snapshot.visible_start + ZF_HOME_VISIBLE_ITEMS > snapshot.item_count &&
            snapshot.item_count > ZF_HOME_VISIBLE_ITEMS) {
            snapshot.visible_start = snapshot.item_count - ZF_HOME_VISIBLE_ITEMS;
        }

        size_t visible_end = snapshot.visible_start + ZF_HOME_VISIBLE_ITEMS;
        size_t position = 0;
        for (size_t i = 0; i < app->store.count && position < visible_end; ++i) {
            if (!app->store.records[i].in_use) {
                continue;
            }
            if (position >= snapshot.visible_start &&
                snapshot.visible_count < ZF_HOME_VISIBLE_ITEMS) {
                entries[snapshot.visible_count] = app->store.records[i];
                snapshot.items[snapshot.visible_count].resident_key =
                    app->store.records[i].resident_key;
                ++snapshot.visible_count;
            }
            ++position;
        }
    }
    furi_mutex_release(app->ui_mutex);

    scratch = zf_app_ui_scratch_acquire(app, sizeof(*scratch));
    for (size_t i = 0; i < snapshot.visible_count; ++i) {
        bool formatted = false;

        if (scratch && zf_store_load_record_for_display_with_buffer(
                           app->storage, &entries[i], &scratch->record, scratch->store_io,
                           sizeof(scratch->store_io))) {
            formatted = zf_status_format_record_item(
                &scratch->record, snapshot.items[i].title, sizeof(snapshot.items[i].title),
                snapshot.items[i].subtitle, sizeof(snapshot.items[i].subtitle));
            memset(&scratch->record, 0, sizeof(scratch->record));
        }

        if (!formatted) {
            zf_ui_format_passkey_index_title(&entries[i], snapshot.items[i].title,
                                             sizeof(snapshot.items[i].title));
            zf_ui_format_passkey_index_subtitle(&entries[i], snapshot.items[i].subtitle,
                                                sizeof(snapshot.items[i].subtitle));
        }

    }
    zf_app_ui_scratch_release(app);

    with_view_model(app->status_view, ZfStatusModel * model, { *model = snapshot; }, redraw);
    return true;
}

static void zf_status_draw_scrollable_line(Canvas *canvas, FuriString *line, int32_t x, int32_t y,
                                           size_t width, const char *text, bool scroll) {
    if (!line) return;

    furi_string_set(line, text ? text : "");
    elements_scrollable_text_line(canvas, x, y, width, line,
                                  scroll ? (furi_get_tick() / ZF_HOME_TEXT_SCROLL_TICK_DIVISOR) :
                                           0U,
                                  !scroll);
}

static void zf_status_draw_callback(Canvas *canvas, void *model) {
    ZfStatusModel *status = model;
    const char *transport_title = NULL;
    FuriString *scroll_line = NULL;

    furi_assert(status);
    if (!status) {
        return;
    }
    transport_title = zf_home_transport_title_label(status->transport_mode);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 10, "ZeroFIDO");
    canvas_draw_str(canvas, 5, 10, "ZeroFIDO");
    canvas_draw_str(canvas, 48, 10, transport_title);
    canvas_draw_str(canvas, 49, 10, transport_title);
    canvas_draw_str(canvas, 74, 10, zf_home_status_label(status->status));
    canvas_draw_line(canvas, 0, 13, 127, 13);

    if (status->item_count == 0) {
        canvas_draw_str(canvas, 14, 30, "No passkeys saved");
        canvas_draw_str(canvas, 14, 42, "Create one in a browser");
    } else {
        scroll_line = furi_string_alloc();
        for (size_t row = 0; row < status->visible_count; ++row) {
            size_t item_index = status->visible_start + row;
            int32_t y = 16 + (int32_t)(row * 18U);

            if (item_index == status->selected_item) {
                elements_slightly_rounded_box(canvas, 1, y - 1, 123, 17);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_draw_line(canvas, 2, y + 1, 2, y + 14);
            }
            canvas_set_font(canvas, FontPrimary);
            zf_status_draw_scrollable_line(
                canvas, scroll_line, 5, y + 7, status->items[row].resident_key ? 108U : 118U,
                status->items[row].title, item_index == status->selected_item);
            if (status->items[row].resident_key) {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, 112, y + 8, "RK");
            }
            canvas_set_font(canvas, FontSecondary);
            zf_status_draw_scrollable_line(canvas, scroll_line, 5, y + 16, 118U,
                                           status->items[row].subtitle,
                                           item_index == status->selected_item);
            canvas_set_color(canvas, ColorBlack);
        }
        furi_string_free(scroll_line);
        if (status->item_count > ZF_HOME_VISIBLE_ITEMS) {
            elements_scrollbar_pos(canvas, 127, 15, 37, status->selected_item, status->item_count);
        }
    }
    elements_button_left(canvas, "Exit");
    elements_button_right(canvas, "Settings");
}

void zerofido_ui_status_bind_view(ZerofidoApp *app) {
    view_allocate_model(app->status_view, ViewModelTypeLocking, sizeof(ZfStatusModel));
    view_set_context(app->status_view, app);
    view_set_draw_callback(app->status_view, zf_status_draw_callback);
    zf_status_refresh_model(app, false, true);
}

void zerofido_ui_status_redraw(ZerofidoApp *app) {
    if (!app || !app->status_view) {
        return;
    }

    with_view_model(app->status_view, ZfStatusModel * model, { UNUSED(model); }, true);
}

static void zerofido_ui_refresh_status_internal(ZerofidoApp *app, bool credentials_dirty) {
    bool dispatch_event = false;
    bool reload_credentials = false;

    if (!app) {
        return;
    }

    if (app && app->ui_thread_id != NULL && app->ui_thread_id != furi_thread_get_current_id()) {
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        if (credentials_dirty) {
            app->status_credentials_dirty = true;
        }
        dispatch_event = !app->status_refresh_pending;
        app->status_refresh_pending = true;
        furi_mutex_release(app->ui_mutex);

        if (dispatch_event) {
            zerofido_ui_dispatch_custom_event(app, ZfEventActivity);
        }
        return;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (credentials_dirty) {
        app->status_credentials_dirty = true;
    }
    reload_credentials = app->status_credentials_dirty;
    app->status_refresh_pending = false;
    if (reload_credentials) {
        app->status_credentials_dirty = false;
    }
    furi_mutex_release(app->ui_mutex);

    if (!zf_status_refresh_model(app, true, reload_credentials) && reload_credentials) {
        furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
        app->status_credentials_dirty = true;
        furi_mutex_release(app->ui_mutex);
    }
}

void zerofido_ui_refresh_status(ZerofidoApp *app) {
    zerofido_ui_refresh_status_internal(app, true);
}

void zerofido_ui_refresh_status_line(ZerofidoApp *app) {
    zerofido_ui_refresh_status_internal(app, false);
}

void zerofido_ui_refresh_credentials_status(ZerofidoApp *app) {
    zerofido_ui_refresh_status_internal(app, true);
}

void zerofido_ui_set_status_locked(ZerofidoApp *app, const char *text) {
    if (!app) {
        return;
    }

    if (text) {
        if (strncmp(app->status_text, text, sizeof(app->status_text)) == 0) {
            return;
        }

        strncpy(app->status_text, text, sizeof(app->status_text) - 1U);
        app->status_text[sizeof(app->status_text) - 1U] = '\0';
    } else {
        if (app->status_text[0] == '\0') {
            return;
        }
        app->status_text[0] = '\0';
    }
}

void zerofido_ui_set_status(ZerofidoApp *app, const char *text) {
    bool changed = false;

    if (!app) {
        return;
    }

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    if (text) {
        changed = strncmp(app->status_text, text, sizeof(app->status_text)) != 0;
    } else {
        changed = app->status_text[0] != '\0';
    }
    zerofido_ui_set_status_locked(app, text);
    furi_mutex_release(app->ui_mutex);
    if (changed) {
        zerofido_ui_refresh_status_line(app);
    }
}

void zerofido_ui_apply_transport_connected(ZerofidoApp *app, bool connected) {
    bool changed = false;

    furi_mutex_acquire(app->ui_mutex, FuriWaitForever);
    changed = app->transport_connected != connected;
    app->transport_connected = connected;
    furi_mutex_release(app->ui_mutex);
    if (changed) {
        zerofido_ui_refresh_status_line(app);
    }
}

void zerofido_ui_set_transport_connected(ZerofidoApp *app, bool connected) {
    zerofido_ui_apply_transport_connected(app, connected);
}
