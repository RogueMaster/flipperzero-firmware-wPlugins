#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/icon.h>
#include "openshock_app_icons.h"
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <input/input.h>

#include "protocols.h"
#include "transmit.h"
#include "receiver.h"
#include "storage.h"
#include "settings.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TX_ICON_DRAW_ROTATION IconRotation270
#define TX_SOUND_ICON_ROTATION IconRotation180

typedef enum {
    ScreenList,
    ScreenTransmit,
    ScreenEdit,
    ScreenReceive,
    ScreenSettings,
} Screen;

typedef enum {
    EditName,
    EditModel,
    EditID,
    EditChannel,
    EditSync,
    EditFieldCount,
} EditField;

typedef struct {
    char name[OPENSHOCK_NAME_MAX_LEN];
    char saved_path[OPENSHOCK_PATH_MAX_LEN];
    ShockerModel model;
    uint16_t shocker_id;
    uint8_t channel;
    uint8_t sync_group;
    ShockerCommand command;
    uint8_t intensity;
} ShockerState;

typedef struct {
    Screen screen;

    SavedShocker saved[OPENSHOCK_MAX_SAVED];
    size_t saved_count;
    int list_sel;

    ShockerState current;
    bool current_is_saved;
    int edit_saved_idx;

    bool transmitting;
    bool transmit_bar_focus;
    uint8_t transmit_sync_cycle;
    uint8_t saved_intensity;
    uint8_t saved_light;

    EditField edit_field;

    bool rx_found;
    DecodedShocker rx_result;

    uint32_t flash_tick;
    const char* flash_msg;
    char flash_msg_buf[40];
    bool flash_draw_vertical;

    bool transmit_vertical_ui;
} AppState;

typedef struct OpenshockCtx OpenshockCtx;

typedef struct {
    OpenshockCtx* app;
} OpenshockMainModel;

typedef enum {
    OpenshockViewMain = 0,
    OpenshockViewTextInput = 1,
} OpenshockViewId;

struct OpenshockCtx {
    AppState state;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    TextInput* text_input;
    char text_input_buffer[OPENSHOCK_NAME_MAX_LEN];
    OpenshockViewId shown_view;
    FuriMutex* mutex;
    OpenshockTx* tx;
    OpenshockRx* rx;
    OpenshockTx* tx_active;
};

static bool handle_list(OpenshockCtx* app, InputEvent* e);
static bool handle_transmit(OpenshockCtx* app, InputEvent* e);
static bool handle_transmit_horizontal(OpenshockCtx* app, InputEvent* e);
static void draw_transmit_horizontal(Canvas* canvas, AppState* s);
static bool handle_settings(OpenshockCtx* app, InputEvent* e);
static void draw_settings(Canvas* canvas, AppState* s);
static bool handle_edit(OpenshockCtx* app, InputEvent* e);
static bool handle_receive(OpenshockCtx* app, InputEvent* e);

static uint8_t max_channel(ShockerModel model) {
    switch(model) {
    case ShockerModelCaiXianlin:
        return 15;
    case ShockerModelPetrainer:
        return 0;
    case ShockerModelPetrainer998DR:
        return 1;
    case ShockerModelT330:
        return 1;
    case ShockerModelD80:
        return 2;
    default:
        return 0;
    }
}

static void ensure_valid(ShockerState* s) {
    uint8_t ch_max = max_channel(s->model);
    if(s->channel > ch_max) s->channel = ch_max;
    if(s->sync_group > 9) s->sync_group = 9;
    if(!openshock_command_supported(s->model, s->command)) {
        for(int i = 0; i < ShockerCmdCount; i++) {
            if(openshock_command_supported(s->model, (ShockerCommand)i)) {
                s->command = (ShockerCommand)i;
                break;
            }
        }
    }
}

#define OPENSHOCK_INTER_PACKET_GAP_HIGH_US 50
#define OPENSHOCK_INTER_PACKET_GAP_LOW_US 10000

typedef struct {
    ShockerModel model;
    uint16_t id;
    uint8_t ch;
} TxSeg;

static void stem_trim(char* s) {
    size_t n = strlen(s);
    while(n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
    char* p = s;
    while(*p && isspace((unsigned char)*p)) p++;
    if(p != s) memmove(s, p, strlen(p) + 1);
}

static size_t tx_group_member_indices(AppState* s, size_t* out_idx, size_t max_idx) {
    ShockerState* c = &s->current;
    if(c->sync_group == 0) return 0;
    size_t n = 0;
    for(size_t i = 0; i < s->saved_count && n < max_idx; i++) {
        if(s->saved[i].sync_group == c->sync_group) {
            out_idx[n++] = i;
        }
    }
    return n;
}

static size_t tx_collect_segments(AppState* s, TxSeg* seg, size_t max_seg) {
    ShockerState* c = &s->current;

    if(c->sync_group == 0 || !s->current_is_saved || c->saved_path[0] == '\0') {
        seg[0] = (TxSeg){c->model, c->shocker_id, c->channel};
        return 1;
    }

    size_t group_idx[OPENSHOCK_MAX_SAVED];
    size_t gn = tx_group_member_indices(s, group_idx, OPENSHOCK_MAX_SAVED);
    if(gn == 0) {
        seg[0] = (TxSeg){c->model, c->shocker_id, c->channel};
        return 1;
    }

    if(s->transmit_sync_cycle > gn) {
        s->transmit_sync_cycle = 0;
    }

    if(s->transmit_sync_cycle == 0) {
        size_t n = 0;
        seg[n++] = (TxSeg){c->model, c->shocker_id, c->channel};
        for(size_t gi = 0; gi < gn && n < max_seg; gi++) {
            SavedShocker* sh = &s->saved[group_idx[gi]];
            if(strcmp(sh->filename, c->saved_path) == 0) continue;
            seg[n++] = (TxSeg){sh->model, sh->shocker_id, sh->channel};
        }
        return n;
    }

    size_t slot = (size_t)(s->transmit_sync_cycle - 1);
    if(slot >= gn) slot = 0;
    SavedShocker* sh = &s->saved[group_idx[slot]];
    seg[0] = (TxSeg){sh->model, sh->shocker_id, sh->channel};
    return 1;
}

static bool tx_fill_pulses(AppState* s, OokPulse* pulses, size_t* out_count) {
    ShockerState* c = &s->current;
    TxSeg seg[OPENSHOCK_MAX_SAVED + 1];
    size_t ns = tx_collect_segments(s, seg, sizeof(seg) / sizeof(seg[0]));

    size_t offset = 0;
    for(size_t si = 0; si < ns; si++) {
        OokPulse chunk[OPENSHOCK_MAX_PULSES];
        size_t n = openshock_encode(
            seg[si].model,
            seg[si].id,
            c->command,
            c->intensity,
            seg[si].ch,
            chunk,
            OPENSHOCK_MAX_PULSES);
        if(n == 0) return false;
        if(offset + n > OPENSHOCK_MAX_PULSES) return false;
        memcpy(pulses + offset, chunk, n * sizeof(OokPulse));
        offset += n;
        if(si + 1 < ns) {
            if(offset + 1 > OPENSHOCK_MAX_PULSES) return false;
            pulses[offset].high_us = OPENSHOCK_INTER_PACKET_GAP_HIGH_US;
            pulses[offset].low_us = OPENSHOCK_INTER_PACKET_GAP_LOW_US;
            offset++;
        }
    }
    *out_count = offset;
    return true;
}

static int list_total(AppState* s) {
    return (int)s->saved_count + 2;
}

static bool list_row_is_collar(AppState* s, int row) {
    return row >= 2 && row < 2 + (int)s->saved_count;
}

static int list_collar_index(int row) {
    return row - 2;
}

static void reload_list(AppState* s) {
    s->saved_count = openshock_shocker_list(s->saved, OPENSHOCK_MAX_SAVED);
    if(s->list_sel >= list_total(s)) s->list_sel = list_total(s) - 1;
    if(s->list_sel < 0) s->list_sel = 0;
}

static void show_flash(AppState* s, const char* msg) {
    s->flash_msg = msg;
    s->flash_tick = furi_get_tick() + 1000;
    s->flash_draw_vertical = false;
}

static void show_flash_tx(AppState* s, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->flash_msg_buf, sizeof(s->flash_msg_buf), fmt, ap);
    va_end(ap);
    s->flash_msg = s->flash_msg_buf;
    s->flash_tick = furi_get_tick() + 1200;
    s->flash_draw_vertical = true;
}

static void show_flash_tx_horizontal(AppState* s, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->flash_msg_buf, sizeof(s->flash_msg_buf), fmt, ap);
    va_end(ap);
    s->flash_msg = s->flash_msg_buf;
    s->flash_tick = furi_get_tick() + 1200;
    s->flash_draw_vertical = false;
}

static bool flash_active(const AppState* s) {
    return s->flash_tick && furi_get_tick() < s->flash_tick;
}

static bool transmit_sync_popup_active(const AppState* s) {
    if(!flash_active(s) || s->flash_msg == NULL) return false;
    return strncmp(s->flash_msg, "TX ", 3) == 0;
}

static void openshock_sync_edit_idx_to_saved_path(AppState* s) {
    s->edit_saved_idx = -1;
    if(s->current.saved_path[0] == '\0') return;
    for(size_t i = 0; i < s->saved_count; i++) {
        if(strcmp(s->saved[i].filename, s->current.saved_path) == 0) {
            s->edit_saved_idx = (int)i;
            return;
        }
    }
}

static void openshock_after_shocker_write_ok(OpenshockCtx* app) {
    AppState* s = &app->state;
    snprintf(
        s->current.saved_path,
        sizeof(s->current.saved_path),
        "%s/%s.shk",
        OPENSHOCK_SAVE_DIR,
        s->current.name);
    reload_list(s);
    openshock_sync_edit_idx_to_saved_path(s);
    show_flash(s, "Saved!");
}

static bool openshock_persist_new_from_current(OpenshockCtx* app) {
    AppState* s = &app->state;
    stem_trim(s->current.name);
    if(s->current.name[0] == '\0') {
        show_flash(s, "Need name");
        return false;
    }
    if(openshock_stem_exists(s->current.name)) {
        show_flash(s, "Name taken");
        return false;
    }
    if(!openshock_shocker_write(
           s->current.name,
           s->current.model,
           s->current.shocker_id,
           s->current.channel,
           s->current.sync_group,
           NULL)) {
        show_flash(s, "Save failed");
        return false;
    }
    s->current_is_saved = true;
    openshock_after_shocker_write_ok(app);
    return true;
}

static void openshock_save_current_shocker(OpenshockCtx* app) {
    AppState* s = &app->state;
    if(s->edit_saved_idx < 0) {
        (void)openshock_persist_new_from_current(app);
        return;
    }
    stem_trim(s->current.name);
    if(s->current.name[0] == '\0') {
        show_flash(s, "Need name");
        return;
    }
    const char* oldp = s->saved[s->edit_saved_idx].filename;
    if(openshock_shocker_write(
           s->current.name,
           s->current.model,
           s->current.shocker_id,
           s->current.channel,
           s->current.sync_group,
           oldp)) {
        openshock_after_shocker_write_ok(app);
    } else {
        show_flash(s, "Save failed");
    }
}

static bool openshock_save_name_keyboard(OpenshockCtx* app) {
    AppState* s = &app->state;
    if(s->edit_saved_idx < 0) {
        return openshock_persist_new_from_current(app);
    }

    stem_trim(s->current.name);
    if(s->current.name[0] == '\0') {
        show_flash(s, "Need name");
        return false;
    }

    if(s->edit_saved_idx >= (int)s->saved_count) {
        show_flash(s, "Save failed");
        return false;
    }

    SavedShocker disk = s->saved[s->edit_saved_idx];

    char new_path[OPENSHOCK_PATH_MAX_LEN];
    snprintf(new_path, sizeof(new_path), "%s/%s.shk", OPENSHOCK_SAVE_DIR, s->current.name);
    if(strcmp(disk.filename, new_path) != 0 && openshock_stem_exists(s->current.name)) {
        show_flash(s, "Name taken");
        return false;
    }

    if(openshock_shocker_write(
           s->current.name,
           disk.model,
           disk.shocker_id,
           disk.channel,
           disk.sync_group,
           disk.filename)) {
        snprintf(
            s->current.saved_path,
            sizeof(s->current.saved_path),
            "%s/%s.shk",
            OPENSHOCK_SAVE_DIR,
            s->current.name);
        s->current.model = disk.model;
        s->current.shocker_id = disk.shocker_id;
        s->current.channel = disk.channel;
        s->current.sync_group = disk.sync_group;
        ensure_valid(&s->current);
        openshock_after_shocker_write_ok(app);
        return true;
    }
    show_flash(s, "Save failed");
    return false;
}

static bool transmit_cmd_to_rc(ShockerCommand cmd, bool has_light, int* row, int* col) {
    switch(cmd) {
    case ShockerCmdVibrate:
        *row = 0;
        *col = 0;
        return true;
    case ShockerCmdLight:
        if(!has_light) return false;
        *row = 0;
        *col = 1;
        return true;
    case ShockerCmdShock:
        *row = 1;
        *col = 0;
        return true;
    case ShockerCmdSound:
        *row = 1;
        *col = 1;
        return true;
    default:
        return false;
    }
}

static bool transmit_rc_to_cmd(int row, int col, bool has_light, ShockerCommand* out) {
    if(row < 0 || row > 1 || col < 0 || col > 1) return false;
    if(row == 0 && col == 0) {
        *out = ShockerCmdVibrate;
        return true;
    }
    if(row == 0 && col == 1) {
        if(!has_light) return false;
        *out = ShockerCmdLight;
        return true;
    }
    if(row == 1 && col == 0) {
        *out = ShockerCmdShock;
        return true;
    }
    if(row == 1 && col == 1) {
        *out = ShockerCmdSound;
        return true;
    }
    return false;
}

static bool transmit_grid_try_move(
    const ShockerState* c,
    int dr,
    int dc,
    ShockerCommand* out_next) {
    bool hl = openshock_command_supported(c->model, ShockerCmdLight);
    int r, col;
    if(!transmit_cmd_to_rc(c->command, hl, &r, &col)) return false;
    int nr = r + dr;
    int nc = col + dc;
    ShockerCommand cand;
    if(transmit_rc_to_cmd(nr, nc, hl, &cand)) {
        if(openshock_command_supported(c->model, cand)) {
            *out_next = cand;
            return true;
        }
        return false;
    }
    if(!hl && dr == 0 && dc == 1 && r == 0 && col == 0) {
        *out_next = ShockerCmdSound;
        return openshock_command_supported(c->model, ShockerCmdSound);
    }
    if(!hl && r == 1 && col == 1 && dr == -1 && dc == 0) {
        *out_next = ShockerCmdVibrate;
        return openshock_command_supported(c->model, ShockerCmdVibrate);
    }
    return false;
}

static void transmit_apply_command(AppState* s, ShockerCommand new_cmd) {
    ShockerState* c = &s->current;
    if(new_cmd == c->command) return;
    bool was_light = (c->command == ShockerCmdLight);
    bool is_light = (new_cmd == ShockerCmdLight);
    c->command = new_cmd;
    if(!was_light && is_light) {
        s->saved_intensity = c->intensity;
        c->intensity = s->saved_light;
    } else if(was_light && !is_light) {
        s->saved_light = c->intensity;
        c->intensity = s->saved_intensity;
    }
}

static ShockerCommand cycle_cmd_fwd_cmd(const ShockerState* c) {
    for(int i = 1; i < ShockerCmdCount; i++) {
        ShockerCommand n = (ShockerCommand)(((int)c->command + i) % ShockerCmdCount);
        if(openshock_command_supported(c->model, n)) return n;
    }
    return c->command;
}

static ShockerCommand cycle_cmd_back_cmd(const ShockerState* c) {
    for(int i = ShockerCmdCount - 1; i >= 1; i--) {
        ShockerCommand n = (ShockerCommand)(((int)c->command + i) % ShockerCmdCount);
        if(openshock_command_supported(c->model, n)) return n;
    }
    return c->command;
}

static void str_trunc_fit(char* dst, size_t dst_sz, const char* src, size_t max_chars) {
    size_t len = strlen(src);
    if(len <= max_chars) {
        snprintf(dst, dst_sz, "%s", src);
        return;
    }
    if(max_chars <= 3) {
        snprintf(dst, dst_sz, "%.*s", (int)dst_sz - 1, src);
        return;
    }
    snprintf(dst, dst_sz, "%.*s..", (int)(max_chars - 2), src);
}

static void str_trunc_fit_hard(char* dst, size_t dst_sz, const char* src, size_t max_chars) {
    size_t len = strlen(src);
    if(len <= max_chars) {
        snprintf(dst, dst_sz, "%s", src);
        return;
    }
    if(max_chars == 0) {
        if(dst_sz > 0) dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%.*s", (int)max_chars, src);
}

static void draw_tx_icon_cell(
    Canvas* canvas,
    int cell_x,
    int cell_y,
    int cell_w,
    int cell_h,
    const Icon* icon,
    IconRotation rotation,
    bool selected,
    int icon_dy) {
    if(selected) {
        const int inset_x = 2;
        canvas_draw_rframe(
            canvas,
            cell_x + inset_x,
            cell_y,
            (size_t)(cell_w - 2 * inset_x),
            (size_t)cell_h,
            2);
    }
    uint16_t iw = icon_get_width(icon);
    uint16_t ih = icon_get_height(icon);
    uint16_t cw = iw;
    uint16_t ch = ih;
    if(rotation == IconRotation90 || rotation == IconRotation270) {
        uint16_t t = cw;
        cw = ch;
        ch = t;
    }
    int ix = cell_x + (cell_w - (int)cw) / 2;
    int iy = cell_y + (cell_h - (int)ch) / 2 + icon_dy;
    if(iy + (int)ch > cell_y + cell_h) iy = cell_y + cell_h - (int)ch;
    canvas_draw_icon_ex(canvas, ix, iy, icon, rotation);
}

static void draw_tx_icon_for_command(
    Canvas* canvas,
    int cell_x,
    int cell_y,
    int cell_w,
    int cell_h,
    ShockerCommand cell_cmd,
    bool selected) {
    const Icon* icon;
    switch(cell_cmd) {
    case ShockerCmdShock:
        icon = &I_TxShock_16x16;
        break;
    case ShockerCmdVibrate:
        icon = &I_TxVibrate_16x16;
        break;
    case ShockerCmdSound:
        icon = &I_TxSound_16x16;
        break;
    case ShockerCmdLight:
        icon = &I_TxLight_16x16;
        break;
    default:
        return;
    }
    IconRotation rot =
        (cell_cmd == ShockerCmdSound) ? TX_SOUND_ICON_ROTATION : TX_ICON_DRAW_ROTATION;
    int icon_dy = (cell_cmd == ShockerCmdShock) ? 1 : 0;
    draw_tx_icon_cell(canvas, cell_x, cell_y, cell_w, cell_h, icon, rot, selected, icon_dy);
}

static void transmit_draw_collar_title(Canvas* canvas, const ShockerState* c, size_t max_fit) {
    char narrow[OPENSHOCK_NAME_MAX_LEN + 16];
    const char* base = c->name[0] ? c->name : openshock_model_name(c->model);
    str_trunc_fit_hard(narrow, sizeof(narrow), base, max_fit);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
    canvas_draw_str_aligned(canvas, 10, 58, AlignLeft, AlignCenter, narrow);
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
}

static void transmit_draw_horizontal_header(Canvas* canvas, const ShockerState* c) {
    char name_fit[OPENSHOCK_NAME_MAX_LEN];
    const char* nm = c->name[0] ? c->name : openshock_model_name(c->model);
    str_trunc_fit_hard(name_fit, sizeof(name_fit), nm, 20);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 0, 8, name_fit);
}

static void transmit_draw_vertical_in_bar(
    Canvas* canvas,
    int bar_x,
    int bar_y,
    int bar_w,
    int bar_h,
    const char* text,
    bool is_light_label) {
    if(text == NULL || text[0] == '\0') return;
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
    canvas_set_color(canvas, ColorXOR);
    int cy = bar_y + bar_h / 2 + 4;
    int cx = bar_x + bar_w / 2 + 7;
    if(is_light_label) {
        cx += 1;
        cy += 1;
    }
    if(!is_light_label) {
        cx += 4;
        size_t nt = strlen(text);
        if(nt == 2) {
            cx -= 3;
            cy -= 1;
        } else if(nt >= 4) {
            cx += 2;
            cy += 2;
        }
    }
    if(is_light_label) {
        if(strcmp(text, "OFF") == 0) {
            cx += 3;
        } else if(strcmp(text, "ON") == 0) {
            cy -= 2;
        }
    }
    canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, text);
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
}

static void draw_transmit_sync_strip(
    Canvas* canvas,
    AppState* app_s,
    int bar_x,
    int bar_y,
    int bar_w,
    int bar_h) {
    ShockerState* c = &app_s->current;
    if(c->sync_group == 0 || !app_s->current_is_saved) return;

    size_t group_idx[OPENSHOCK_MAX_SAVED];
    size_t gn = tx_group_member_indices(app_s, group_idx, OPENSHOCK_MAX_SAVED);
    if(gn == 0) return;

    char tag[20];
    if(app_s->transmit_sync_cycle == 0) {
        snprintf(tag, sizeof(tag), "Group %u", c->sync_group);
    } else {
        snprintf(tag, sizeof(tag), "Collar %u", (unsigned)app_s->transmit_sync_cycle);
    }

    const int gap_after_bar = 3;
    const int label_col_w = 16;
    const int gap_before_dots = 1;
    const int label_nudge_x = 7;
    const int dots_nudge_left = 6;
    const size_t r = 2;
    const int dot_edge_gap = 4;
    const int step = (int)(2 * r) + dot_edge_gap;

    int label_left = bar_x + bar_w + gap_after_bar + label_nudge_x;
    int tag_y = bar_y + bar_h - 10;
    if(app_s->transmit_sync_cycle == 0) {
        tag_y += 2;
    } else {
        tag_y += 1;
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, label_left, tag_y, AlignLeft, AlignBottom, tag);
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);

    int dots_cx = bar_x + bar_w + gap_after_bar + label_col_w + gap_before_dots + (int)r -
                    dots_nudge_left;

    size_t span_h = (gn - 1) * (size_t)step + 2 * r;
    int y_top_center = bar_y + (int)((bar_h - (int)span_h) / 2) + (int)r;

    for(size_t i = 0; i < gn; i++) {
        int cy = y_top_center + (int)(i * step);
        if(cy + (int)r >= bar_y + bar_h) break;
        bool on = (app_s->transmit_sync_cycle == 0) ||
                    (app_s->transmit_sync_cycle == (uint8_t)(gn - i));
        if(on) {
            canvas_draw_disc(canvas, dots_cx, cy, r);
        } else {
            canvas_draw_circle(canvas, dots_cx, cy, r);
        }
    }
}

/* Group / collar: horizontal dot row (left) + tag (horizontal transmit). */
static void draw_transmit_sync_strip_horizontal(Canvas* canvas, AppState* app_s) {
    ShockerState* c = &app_s->current;
    if(c->sync_group == 0 || !app_s->current_is_saved) return;

    size_t group_idx[OPENSHOCK_MAX_SAVED];
    size_t gn = tx_group_member_indices(app_s, group_idx, OPENSHOCK_MAX_SAVED);
    if(gn == 0) return;

    char tag[22];
    if(app_s->transmit_sync_cycle == 0) {
        snprintf(tag, sizeof(tag), "Group %u", c->sync_group);
    } else {
        snprintf(tag, sizeof(tag), "Collar %u", (unsigned)app_s->transmit_sync_cycle);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
    canvas_set_color(canvas, ColorBlack);

    const int tag_x = 50;
    const int tag_y = 8;
    canvas_draw_str(canvas, tag_x, tag_y, tag);

    const size_t r = 2;
    const int step = (int)(2 * (int)r) + 4;
    int dots_span = (int)((gn > 0 ? gn - 1 : 0) * (size_t)step) + (int)(2 * (int)r);
    /* Fixed anchor (wider "Collar" prefix) so dots do not shift when tag toggles Group/Collar. */
    const int prefix_w = 6 * 6;
    int anchor_x = tag_x + prefix_w / 2 - 4;
    int x_first = anchor_x - dots_span / 2 + 2;
    if(x_first < (int)r + 1) x_first = (int)r + 1;

    const int dot_y = 13;

    for(size_t i = 0; i < gn; i++) {
        int cx = x_first + (int)(i * step);
        bool on = (app_s->transmit_sync_cycle == 0) ||
                    (app_s->transmit_sync_cycle == (uint8_t)(i + 1));
        if(on) {
            canvas_draw_disc(canvas, cx, dot_y, r);
        } else {
            canvas_draw_circle(canvas, cx, dot_y, r);
        }
    }
}

static void draw_transmit_mode_grid_and_bar(Canvas* canvas, AppState* app_s, int gy0) {
    ShockerState* c = &app_s->current;
    const bool bar_focus = app_s->transmit_bar_focus;
    const bool has_light = openshock_command_supported(c->model, ShockerCmdLight);

    const int gap_x = 3;
    const int gap_y = 2;
    const int cell_w = 34;
    const int cell_h = 19;
    const int gx0 = 11;
    const int gx1 = gx0 + cell_w + gap_x;

    const bool cg = !bar_focus;

    draw_tx_icon_for_command(canvas, gx0, gy0, cell_w, cell_h, ShockerCmdVibrate, cg && c->command == ShockerCmdVibrate);

    if(has_light) {
        draw_tx_icon_for_command(canvas, gx1, gy0, cell_w, cell_h, ShockerCmdLight, cg && c->command == ShockerCmdLight);
    }

    const int gy1 = gy0 + cell_h + gap_y;
    draw_tx_icon_for_command(canvas, gx0, gy1, cell_w, cell_h, ShockerCmdShock, cg && c->command == ShockerCmdShock);

    draw_tx_icon_for_command(canvas, gx1, gy1, cell_w, cell_h, ShockerCmdSound, cg && c->command == ShockerCmdSound);

    const int bar_pad = 3;
    const int bar_y = gy0 - bar_pad;
    const int bar_x = gx1 + cell_w + 3;
    const int bar_w = 15;
    const int bar_h = (gy1 + cell_h + bar_pad) - gy0;

    if(bar_focus) {
        elements_bold_rounded_frame(canvas, bar_x - 1, bar_y - 1, (size_t)(bar_w + 2), (size_t)(bar_h + 2));
    }
    canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    if(c->command == ShockerCmdLight) {
        int inner = bar_h - 2;
        int fill = ((c->intensity == 0) ? inner : 0);
        if(fill > 0) canvas_draw_box(canvas, bar_x + 1, bar_y + bar_h - 1 - fill, bar_w - 2, fill);
    } else {
        int inner = bar_h - 2;
        int fill = ((int)c->intensity * inner) / 100;
        if(fill > 0) canvas_draw_box(canvas, bar_x + 1, bar_y + bar_h - 1 - fill, bar_w - 2, fill);
    }

    char lvl[8];
    if(c->command == ShockerCmdLight) {
        snprintf(lvl, sizeof(lvl), "%s", (c->intensity == 0) ? "ON" : "OFF");
    } else {
        snprintf(lvl, sizeof(lvl), "%u%%", (unsigned)c->intensity);
    }
    transmit_draw_vertical_in_bar(
        canvas,
        bar_x,
        bar_y,
        bar_w,
        bar_h,
        lvl,
        c->command == ShockerCmdLight);

    draw_transmit_sync_strip(canvas, app_s, bar_x, bar_y, bar_w, bar_h);
}

/* Classic horizontal transmit UI (ported from OpenShock/FlipperZero), + header + group strip. */
static void draw_transmit_horizontal(Canvas* canvas, AppState* s) {
    ShockerState* c = &s->current;

    transmit_draw_horizontal_header(canvas, c);

    draw_transmit_sync_strip_horizontal(canvas, s);

    if(s->transmitting && c->command != ShockerCmdLight) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Transmitting...");
        canvas_set_font(canvas, FontSecondary);
        char info[32];
        snprintf(
            info, sizeof(info), "%s @ %u%%", openshock_command_name(c->command), c->intensity);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, info);
        return;
    }

    if(!flash_active(s)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, 26, AlignCenter, AlignCenter, openshock_command_name(c->command));
    }

    if(c->command == ShockerCmdLight) {
        if(!flash_active(s)) {
            canvas_set_font(canvas, FontSecondary);
            const char* light_state = (c->intensity == 0) ? "ON" : "OFF";
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, light_state);
        }
    } else if(!flash_active(s)) {
        canvas_set_font(canvas, FontSecondary);
        char int_str[16];
        snprintf(int_str, sizeof(int_str), "%u%%", c->intensity);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, int_str);

        const int bar_x = 14;
        const int bar_y = 44;
        const int bar_w = 100;
        const int bar_h = 6;
        canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
        int fill = ((int)c->intensity * (bar_w - 2)) / 100;
        if(fill > 0) canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill, bar_h - 2);
    }

    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Mode");
    elements_button_center(canvas, "TX");
    elements_button_right(canvas, "Mode");
}

static void draw_flash(Canvas* canvas, AppState* s) {
    if(!flash_active(s)) return;
    canvas_set_font(canvas, FontPrimary);
    if(s->flash_draw_vertical) {
        canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
        canvas_draw_str_aligned(canvas, 55, 55, AlignLeft, AlignCenter, s->flash_msg);
        canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
    } else {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, s->flash_msg);
    }
}

static void draw_settings(Canvas* canvas, AppState* s) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "Settings");
    char line[48];
    snprintf(
        line,
        sizeof(line),
        "> Transmit UI: %s",
        s->transmit_vertical_ui ? "Vertical" : "Horizontal");
    canvas_draw_str(canvas, 0, 22, line);
    elements_button_left(canvas, "Horiz");
    elements_button_right(canvas, "Vert");
    elements_button_center(canvas, "Done");
}

static void draw_list(Canvas* canvas, AppState* s) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 11, "OpenShock");

    if(flash_active(s)) {
        draw_flash(canvas, s);
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    int total = list_total(s);
    const int max_visible = 4;
    const int rh = 10;
    const int cy = 20;

    int scroll = s->list_sel - max_visible + 1;
    if(scroll < 0) scroll = 0;
    int max_scroll = total - max_visible;
    if(max_scroll < 0) max_scroll = 0;
    if(scroll > max_scroll) scroll = max_scroll;

    for(int vi = 0; vi < max_visible; vi++) {
        int i = scroll + vi;
        if(i >= total) break;
        int y = cy + vi * rh;
        char line[48];

        if(i == 0) {
            snprintf(line, sizeof(line), "%s + Add New", (i == s->list_sel) ? ">" : " ");
        } else if(i == 1) {
            snprintf(line, sizeof(line), "%s ~ Receive", (i == s->list_sel) ? ">" : " ");
        } else {
            char nm[44];
            str_trunc_fit(nm, sizeof(nm), s->saved[list_collar_index(i)].name, 22);
            snprintf(
                line,
                sizeof(line),
                "%s %s",
                (i == s->list_sel) ? ">" : " ",
                nm);
        }
        canvas_draw_str(canvas, 0, y, line);
    }

    elements_scrollbar_pos(
        canvas,
        128,
        cy - 4,
        (size_t)(max_visible * rh),
        (size_t)s->list_sel,
        (size_t)total);

    if(list_row_is_collar(s, s->list_sel)) {
        elements_button_left(canvas, "Del");
        elements_button_center(canvas, "Use");
        elements_button_right(canvas, "Edit");
    } else {
        elements_button_left(canvas, "Set");
        elements_button_center(canvas, "Select");
    }
}

static void draw_transmit(Canvas* canvas, AppState* s) {
    if(!s->transmit_vertical_ui) {
        draw_transmit_horizontal(canvas, s);
        draw_flash(canvas, s);
        return;
    }

    ShockerState* c = &s->current;

    const int transmit_grid_y0 = 17;

    transmit_draw_collar_title(canvas, c, 14);

    if(s->transmitting) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Transmitting...");
        canvas_set_font(canvas, FontSecondary);
        char info[32];
        snprintf(
            info, sizeof(info), "%s @ %u%%", openshock_command_name(c->command), c->intensity);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, info);
        draw_flash(canvas, s);
        return;
    }

    draw_transmit_mode_grid_and_bar(canvas, s, transmit_grid_y0);
    draw_flash(canvas, s);
}

static void draw_edit(Canvas* canvas, AppState* s) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "Edit");

    if(flash_active(s)) {
        draw_flash(canvas, s);
        return;
    }

    const int max_visible = 4;
    const int rh = 8;
    const int cy = 18;
    const int vx = 44;

    int scroll = (int)s->edit_field - max_visible + 1;
    if(scroll < 0) scroll = 0;
    int max_scroll = EditFieldCount - max_visible;
    if(max_scroll < 0) max_scroll = 0;
    if(scroll > max_scroll) scroll = max_scroll;

    char buf[40];
    char val[OPENSHOCK_NAME_MAX_LEN + 8];

    const char* labels[] = {"Name:", "Model:", "ID:", "Ch:", "Sync:"};

    for(int vi = 0; vi < max_visible; vi++) {
        int row = scroll + vi;
        if(row >= EditFieldCount) break;

        EditField fi = (EditField)row;
        int y = cy + vi * rh;
        const char* prefix = (fi == s->edit_field) ? ">" : " ";
        snprintf(buf, sizeof(buf), "%s%s", prefix, labels[row]);
        canvas_draw_str(canvas, 0, y, buf);

        switch(fi) {
        case EditName:
            str_trunc_fit(val, sizeof(val), s->current.name, 18);
            canvas_draw_str(canvas, vx, y, val);
            break;
        case EditModel:
            str_trunc_fit(val, sizeof(val), openshock_model_name(s->current.model), 18);
            canvas_draw_str(canvas, vx, y, val);
            break;
        case EditID:
            snprintf(val, sizeof(val), "%u", s->current.shocker_id);
            canvas_draw_str(canvas, vx, y, val);
            break;
        case EditChannel:
            snprintf(val, sizeof(val), "%u", s->current.channel);
            canvas_draw_str(canvas, vx, y, val);
            break;
        case EditSync:
            if(s->current.sync_group == 0) {
                canvas_draw_str(canvas, vx, y, "off");
            } else {
                snprintf(val, sizeof(val), "Group %u", s->current.sync_group);
                canvas_draw_str(canvas, vx, y, val);
            }
            break;
        default:
            break;
        }
    }

    elements_scrollbar_pos(
        canvas,
        128,
        cy - 4,
        (size_t)(max_visible * rh),
        (size_t)s->edit_field,
        (size_t)EditFieldCount);

    if(s->edit_field == EditName) {
        elements_button_center(canvas, "Set");
        elements_button_left(canvas, "-");
        elements_button_right(canvas, "+");
    } else {
        elements_button_center(canvas, "Save");
        elements_button_left(canvas, "-");
        elements_button_right(canvas, "+");
    }
}

static void draw_receive(Canvas* canvas, AppState* s) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 11, "Receive");

    if(flash_active(s)) {
        draw_flash(canvas, s);
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    if(!s->rx_found) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Listening...");
    } else {
        char buf[48];
        int y = 24;
        snprintf(buf, sizeof(buf), "Model: %s", openshock_model_name(s->rx_result.model));
        canvas_draw_str(canvas, 4, y, buf);
        y += 10;
        snprintf(
            buf, sizeof(buf), "ID: %u  CH: %u", s->rx_result.shocker_id, s->rx_result.channel);
        canvas_draw_str(canvas, 4, y, buf);
        y += 10;
        snprintf(
            buf,
            sizeof(buf),
            "Cmd: %s  Int: %u",
            openshock_command_name(s->rx_result.command),
            s->rx_result.intensity);
        canvas_draw_str(canvas, 4, y, buf);

        elements_button_left(canvas, "Save");
        elements_button_center(canvas, "Use");
    }
}

static void openshock_name_input_cb(void* ctx) {
    OpenshockCtx* app = ctx;
    stem_trim(app->text_input_buffer);

    char prev[OPENSHOCK_NAME_MAX_LEN];
    snprintf(prev, sizeof(prev), "%s", app->state.current.name);

    snprintf(
        app->state.current.name,
        sizeof(app->state.current.name),
        "%s",
        app->text_input_buffer);
    stem_trim(app->state.current.name);
    if(app->state.current.name[0] == '\0') {
        snprintf(app->state.current.name, sizeof(app->state.current.name), "Collar");
    }

    if(!openshock_save_name_keyboard(app)) {
        snprintf(app->state.current.name, sizeof(app->state.current.name), "%s", prev);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, OpenshockViewMain);
    app->shown_view = OpenshockViewMain;
}

static void openshock_open_name_input(OpenshockCtx* app) {
    strncpy(app->text_input_buffer, app->state.current.name, OPENSHOCK_NAME_MAX_LEN - 1);
    app->text_input_buffer[OPENSHOCK_NAME_MAX_LEN - 1] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Collar name");
    text_input_set_result_callback(
        app->text_input,
        openshock_name_input_cb,
        app,
        app->text_input_buffer,
        OPENSHOCK_NAME_MAX_LEN,
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, OpenshockViewTextInput);
    app->shown_view = OpenshockViewTextInput;
}

static void main_draw(Canvas* canvas, void* model) {
    OpenshockMainModel* m = model;
    OpenshockCtx* app = m->app;
    AppState* s = &app->state;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    if(s->screen == ScreenReceive) {
        DecodedShocker d;
        if(openshock_rx_get_result(app->rx, &d)) {
            s->rx_result = d;
            s->rx_found = true;
        }
    }

    canvas_clear(canvas);

    switch(s->screen) {
    case ScreenList:
        draw_list(canvas, s);
        break;
    case ScreenTransmit:
        draw_transmit(canvas, s);
        break;
    case ScreenEdit:
        draw_edit(canvas, s);
        break;
    case ScreenReceive:
        draw_receive(canvas, s);
        break;
    case ScreenSettings:
        draw_settings(canvas, s);
        break;
    }

    furi_mutex_release(app->mutex);
}

static bool main_view_input(InputEvent* event, void* context) {
    OpenshockCtx* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    bool consumed = false;
    switch(app->state.screen) {
    case ScreenList:
        consumed = handle_list(app, event);
        break;
    case ScreenTransmit:
        consumed = handle_transmit(app, event);
        break;
    case ScreenEdit:
        consumed = handle_edit(app, event);
        break;
    case ScreenReceive:
        consumed = handle_receive(app, event);
        break;
    case ScreenSettings:
        consumed = handle_settings(app, event);
        break;
    default:
        break;
    }

    furi_mutex_release(app->mutex);
    return consumed;
}

static bool openshock_navigation_cb(void* context) {
    OpenshockCtx* app = context;
    if(app->shown_view == OpenshockViewTextInput) {
        view_dispatcher_switch_to_view(app->view_dispatcher, OpenshockViewMain);
        app->shown_view = OpenshockViewMain;
        return true;
    }
    return false;
}

static bool handle_list(OpenshockCtx* app, InputEvent* e) {
    AppState* s = &app->state;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypePress) {
            return true;
        }
        if(e->type == InputTypeShort) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
        return false;
    }

    if(e->type != InputTypePress && e->type != InputTypeRepeat) return false;

    int total = list_total(s);
    switch(e->key) {
    case InputKeyUp:
        s->list_sel = (s->list_sel > 0) ? (s->list_sel - 1) : (total - 1);
        break;
    case InputKeyDown:
        s->list_sel = (s->list_sel < total - 1) ? (s->list_sel + 1) : 0;
        break;
    case InputKeyOk:
        if(s->list_sel == 0) {
            memset(&s->current, 0, sizeof(s->current));
            s->current.command = ShockerCmdShock;
            s->current.saved_path[0] = '\0';
            s->current.sync_group = 0;
            if(!openshock_shocker_unique_stem(
                   s->current.model,
                   s->current.shocker_id,
                   s->current.name,
                   sizeof(s->current.name))) {
                snprintf(s->current.name, sizeof(s->current.name), "Collar");
            }
            s->current_is_saved = false;
            s->edit_saved_idx = -1;
            s->edit_field = EditName;
            s->screen = ScreenEdit;
        } else if(s->list_sel == 1) {
            s->rx_found = false;
            s->screen = ScreenReceive;
            openshock_rx_start(app->rx);
        } else if(list_row_is_collar(s, s->list_sel)) {
            SavedShocker* sel = &s->saved[list_collar_index(s->list_sel)];
            snprintf(s->current.name, sizeof(s->current.name), "%s", sel->name);
            snprintf(s->current.saved_path, sizeof(s->current.saved_path), "%s", sel->filename);
            s->current.model = sel->model;
            s->current.shocker_id = sel->shocker_id;
            s->current.channel = sel->channel;
            s->current.sync_group = sel->sync_group;
            s->current.command = ShockerCmdShock;
            s->current.intensity = 0;
            s->saved_intensity = 0;
            s->saved_light = 100;
            s->current_is_saved = true;
            ensure_valid(&s->current);
            s->transmit_bar_focus = false;
            s->transmit_sync_cycle = 0;
            s->screen = ScreenTransmit;
        }
        break;
    case InputKeyRight:
        if(list_row_is_collar(s, s->list_sel)) {
            SavedShocker* sel = &s->saved[list_collar_index(s->list_sel)];
            snprintf(s->current.name, sizeof(s->current.name), "%s", sel->name);
            snprintf(s->current.saved_path, sizeof(s->current.saved_path), "%s", sel->filename);
            s->current.model = sel->model;
            s->current.shocker_id = sel->shocker_id;
            s->current.channel = sel->channel;
            s->current.sync_group = sel->sync_group;
            s->current.command = ShockerCmdShock;
            s->current.intensity = 0;
            s->current_is_saved = true;
            s->edit_saved_idx = list_collar_index(s->list_sel);
            s->edit_field = EditName;
            s->screen = ScreenEdit;
        }
        break;
    case InputKeyLeft:
        if(s->list_sel == 0 || s->list_sel == 1) {
            s->screen = ScreenSettings;
            break;
        }
        if(list_row_is_collar(s, s->list_sel)) {
            openshock_shocker_delete(s->saved[list_collar_index(s->list_sel)].filename);
            reload_list(s);
            show_flash(s, "Deleted");
        }
        break;
    default:
        break;
    }
    return false;
}

static bool handle_settings(OpenshockCtx* app, InputEvent* e) {
    AppState* s = &app->state;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypePress) {
            return true;
        }
        if(e->type == InputTypeShort) {
            s->screen = ScreenList;
            return true;
        }
        if(e->type == InputTypeLong) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
        return false;
    }

    if(e->type != InputTypePress && e->type != InputTypeRepeat) return false;

    switch(e->key) {
    case InputKeyOk:
        if(e->type != InputTypePress) return false;
        s->screen = ScreenList;
        return true;
    case InputKeyLeft:
        s->transmit_vertical_ui = false;
        openshock_settings_save(false);
        return true;
    case InputKeyRight:
        s->transmit_vertical_ui = true;
        openshock_settings_save(true);
        return true;
    default:
        break;
    }
    return false;
}

static bool handle_transmit_horizontal(OpenshockCtx* app, InputEvent* e) {
    AppState* s = &app->state;
    OpenshockTx* tx = app->tx;
    OpenshockTx** active = &app->tx_active;
    ShockerState* c = &s->current;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypePress) {
            return true;
        }
        if(e->type == InputTypeShort) {
            if(*active) {
                openshock_tx_stop(tx);
                *active = NULL;
                s->transmitting = false;
            }
            if(c->sync_group != 0 && s->current_is_saved) {
                size_t group_idx[OPENSHOCK_MAX_SAVED];
                size_t gn = tx_group_member_indices(s, group_idx, OPENSHOCK_MAX_SAVED);
                if(gn > 0) {
                    s->transmit_sync_cycle =
                        (uint8_t)((s->transmit_sync_cycle + 1) % (uint8_t)(gn + 1));
                    if(s->transmit_sync_cycle == 0) {
                        show_flash_tx_horizontal(s, "TX Group %u", c->sync_group);
                    } else {
                        show_flash_tx_horizontal(
                            s, "TX Collar %u", (unsigned)s->transmit_sync_cycle);
                    }
                    return true;
                }
            }
            s->transmit_bar_focus = false;
            s->transmit_sync_cycle = 0;
            s->screen = ScreenList;
            return true;
        }
        if(e->type == InputTypeLong) {
            if(*active) {
                openshock_tx_stop(tx);
                *active = NULL;
                s->transmitting = false;
            }
            s->transmit_bar_focus = false;
            s->transmit_sync_cycle = 0;
            s->screen = ScreenList;
            return true;
        }
        return false;
    }

    if(e->key == InputKeyOk && e->type == InputTypePress) {
        if(transmit_sync_popup_active(s)) return true;
        s->transmitting = true;
        OokPulse pulses[OPENSHOCK_MAX_PULSES];
        size_t count = 0;
        if(tx_fill_pulses(s, pulses, &count) && count > 0) {
            openshock_tx_start(tx, pulses, count);
            *active = tx;
        }
        return true;
    }

    if(e->key == InputKeyOk && e->type == InputTypeRelease && *active) {
        openshock_tx_stop(tx);
        *active = NULL;
        s->transmitting = false;
        return true;
    }

    if(e->type != InputTypePress && e->type != InputTypeRepeat) return false;

    if(*active) {
        switch(e->key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight:
            openshock_tx_stop(tx);
            *active = NULL;
            s->transmitting = false;
            break;
        default:
            break;
        }
    }

    switch(e->key) {
    case InputKeyUp:
        if(c->command == ShockerCmdLight) {
            if(transmit_sync_popup_active(s)) return true;
            c->intensity = 0;
        } else if(c->intensity < 100) {
            uint8_t step = (e->type == InputTypeRepeat) ? 5 : 1;
            uint16_t n = (uint16_t)c->intensity + step;
            c->intensity = (n <= 100) ? (uint8_t)n : 100;
        }
        return true;
    case InputKeyDown:
        if(c->command == ShockerCmdLight) {
            if(transmit_sync_popup_active(s)) return true;
            c->intensity = 100;
        } else if(c->intensity > 0) {
            uint8_t step = (e->type == InputTypeRepeat) ? 5 : 1;
            c->intensity = (c->intensity >= step) ? (c->intensity - step) : 0;
        }
        return true;
    case InputKeyLeft:
    case InputKeyRight:
        if(*active) {
            openshock_tx_stop(tx);
            *active = NULL;
            s->transmitting = false;
        }
        {
            ShockerCommand n = (e->key == InputKeyLeft) ? cycle_cmd_back_cmd(c) : cycle_cmd_fwd_cmd(c);
            transmit_apply_command(s, n);
        }
        return true;
    default:
        break;
    }
    return false;
}

static bool handle_transmit(OpenshockCtx* app, InputEvent* e) {
    AppState* s = &app->state;
    if(!s->transmit_vertical_ui) {
        return handle_transmit_horizontal(app, e);
    }
    OpenshockTx* tx = app->tx;
    OpenshockTx** active = &app->tx_active;
    ShockerState* c = &s->current;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypePress) {
            return true;
        }
        if(e->type == InputTypeLong) {
            if(*active) {
                openshock_tx_stop(tx);
                *active = NULL;
                s->transmitting = false;
            }
            s->transmit_bar_focus = false;
            s->transmit_sync_cycle = 0;
            s->screen = ScreenList;
            return true;
        }
        if(e->type == InputTypeShort) {
            if(c->sync_group == 0 || !s->current_is_saved) {
                return true;
            }
            size_t group_idx[OPENSHOCK_MAX_SAVED];
            size_t gn = tx_group_member_indices(s, group_idx, OPENSHOCK_MAX_SAVED);
            if(gn == 0) return true;
            s->transmit_sync_cycle =
                (uint8_t)((s->transmit_sync_cycle + 1) % (uint8_t)(gn + 1));
            if(s->transmit_sync_cycle == 0) {
                show_flash_tx(s, "TX Group %u", c->sync_group);
            } else {
                show_flash_tx(s, "TX Collar %u", (unsigned)s->transmit_sync_cycle);
            }
            return true;
        }
        return true;
    }

    if(e->key == InputKeyOk && e->type == InputTypePress) {
        if(transmit_sync_popup_active(s)) return true;
        s->transmitting = true;
        OokPulse pulses[OPENSHOCK_MAX_PULSES];
        size_t count = 0;
        if(tx_fill_pulses(s, pulses, &count) && count > 0) {
            openshock_tx_start(tx, pulses, count);
            *active = tx;
        }
        return true;
    }

    if(e->key == InputKeyOk && e->type == InputTypeRelease && *active) {
        openshock_tx_stop(tx);
        *active = NULL;
        s->transmitting = false;
        return true;
    }

    if(e->type != InputTypePress && e->type != InputTypeRepeat) return false;

    if(*active) {
        switch(e->key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight:
            openshock_tx_stop(tx);
            *active = NULL;
            s->transmitting = false;
            break;
        default:
            break;
        }
    }

    switch(e->key) {
    case InputKeyUp: {
        if(s->transmit_bar_focus) {
            if(c->command == ShockerCmdLight) {
                c->intensity = 0;
            } else if(c->intensity < 100) {
                uint8_t step = (e->type == InputTypeRepeat) ? 5 : 1;
                uint16_t ni = (uint16_t)c->intensity + step;
                c->intensity = (ni <= 100) ? (uint8_t)ni : 100;
            }
            return true;
        }
        ShockerCommand n;
        if(transmit_grid_try_move(c, -1, 0, &n)) {
            transmit_apply_command(s, n);
            return true;
        }
        bool hl = openshock_command_supported(c->model, ShockerCmdLight);
        if(c->command == ShockerCmdVibrate && openshock_command_supported(c->model, ShockerCmdShock)) {
            transmit_apply_command(s, ShockerCmdShock);
        } else if(hl && c->command == ShockerCmdLight &&
                  openshock_command_supported(c->model, ShockerCmdSound)) {
            transmit_apply_command(s, ShockerCmdSound);
        }
        return true;
    }
    case InputKeyDown: {
        if(s->transmit_bar_focus) {
            if(c->command == ShockerCmdLight) {
                c->intensity = 100;
            } else if(c->intensity > 0) {
                uint8_t step = (e->type == InputTypeRepeat) ? 5 : 1;
                c->intensity = (c->intensity >= step) ? (c->intensity - step) : 0;
            }
            return true;
        }
        ShockerCommand n;
        if(transmit_grid_try_move(c, 1, 0, &n)) {
            transmit_apply_command(s, n);
            return true;
        }
        bool hl = openshock_command_supported(c->model, ShockerCmdLight);
        if(c->command == ShockerCmdShock && openshock_command_supported(c->model, ShockerCmdVibrate)) {
            transmit_apply_command(s, ShockerCmdVibrate);
        } else if(hl && c->command == ShockerCmdSound &&
                  openshock_command_supported(c->model, ShockerCmdLight)) {
            transmit_apply_command(s, ShockerCmdLight);
        }
        return true;
    }
    case InputKeyLeft: {
        if(s->transmit_bar_focus) {
            s->transmit_bar_focus = false;
            return true;
        }
        ShockerCommand n;
        if(transmit_grid_try_move(c, 0, -1, &n)) transmit_apply_command(s, n);
        return true;
    }
    case InputKeyRight: {
        if(s->transmit_bar_focus) {
            return true;
        }
        if(c->command == ShockerCmdLight || c->command == ShockerCmdSound) {
            s->transmit_bar_focus = true;
            return true;
        }
        ShockerCommand n;
        if(transmit_grid_try_move(c, 0, 1, &n)) transmit_apply_command(s, n);
        return true;
    }
    default:
        break;
    }
    return false;
}

static bool handle_edit(OpenshockCtx* app, InputEvent* e) {
    AppState* s = &app->state;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypePress) {
            return true;
        }
        if(e->type == InputTypeShort) {
            reload_list(s);
            s->screen = ScreenList;
            return true;
        }
        return false;
    }

    if(e->type != InputTypePress && e->type != InputTypeRepeat) return false;

    switch(e->key) {
    case InputKeyUp:
        if(s->edit_field > 0) s->edit_field--;
        break;
    case InputKeyDown:
        if(s->edit_field < EditFieldCount - 1) s->edit_field++;
        break;
    case InputKeyLeft:
        switch(s->edit_field) {
        case EditName:
            return true;
        case EditModel:
            s->current.model =
                (ShockerModel)((s->current.model + ShockerModelCount - 1) % ShockerModelCount);
            ensure_valid(&s->current);
            break;
        case EditID: {
            uint16_t step = (e->type == InputTypeRepeat) ? 100 : 1;
            uint32_t v = (uint32_t)s->current.shocker_id + 65536u - (uint32_t)step;
            s->current.shocker_id = (uint16_t)(v % 65536u);
            break;
        }
        case EditChannel: {
            uint8_t mx = max_channel(s->current.model);
            s->current.channel = (s->current.channel == 0) ? mx : (uint8_t)(s->current.channel - 1);
            break;
        }
        case EditSync:
            s->current.sync_group = (s->current.sync_group + 9) % 10;
            break;
        default:
            break;
        }
        return true;
    case InputKeyRight:
        switch(s->edit_field) {
        case EditName:
            return true;
        case EditModel:
            s->current.model = (ShockerModel)((s->current.model + 1) % ShockerModelCount);
            ensure_valid(&s->current);
            break;
        case EditID: {
            uint16_t step = (e->type == InputTypeRepeat) ? 100 : 1;
            uint32_t n = (uint32_t)s->current.shocker_id + step;
            s->current.shocker_id = (n <= 65535) ? (uint16_t)n : (uint16_t)(n - 65536);
            break;
        }
        case EditChannel: {
            uint8_t mx = max_channel(s->current.model);
            s->current.channel = (s->current.channel >= mx) ? 0 : (s->current.channel + 1);
            break;
        }
        case EditSync:
            s->current.sync_group = (s->current.sync_group + 1) % 10;
            break;
        default:
            break;
        }
        return true;
    case InputKeyOk:
        if(e->type != InputTypePress) return false;
        if(s->edit_field == EditName) {
            openshock_open_name_input(app);
            return true;
        }
        openshock_save_current_shocker(app);
        return true;
    default:
        break;
    }
    return false;
}

static bool handle_receive(OpenshockCtx* app, InputEvent* e) {
    AppState* s = &app->state;

    if(e->key == InputKeyBack) {
        if(e->type == InputTypePress) {
            return true;
        }
        if(e->type == InputTypeShort) {
            openshock_rx_stop(app->rx);
            reload_list(s);
            s->screen = ScreenList;
            return true;
        }
        return false;
    }

    if(e->type != InputTypePress) return false;

    switch(e->key) {
    case InputKeyOk:
        if(s->rx_found) {
            s->current.model = s->rx_result.model;
            s->current.shocker_id = s->rx_result.shocker_id;
            s->current.channel = s->rx_result.channel;
            s->current.command = s->rx_result.command;
            s->current.intensity = s->rx_result.intensity;
            s->current.saved_path[0] = '\0';
            s->current.sync_group = 0;
            s->current_is_saved = false;
            s->saved_intensity = 0;
            s->saved_light = 100;
            ensure_valid(&s->current);
            snprintf(
                s->current.name,
                sizeof(s->current.name),
                "RX %s #%u",
                openshock_model_name(s->rx_result.model),
                s->rx_result.shocker_id);
            openshock_rx_stop(app->rx);
            s->transmit_bar_focus = false;
            s->transmit_sync_cycle = 0;
            s->screen = ScreenTransmit;
        }
        return true;
    case InputKeyLeft:
        if(s->rx_found) {
            char stem[OPENSHOCK_NAME_MAX_LEN];
            if(openshock_shocker_unique_stem(
                   s->rx_result.model,
                   s->rx_result.shocker_id,
                   stem,
                   sizeof(stem))) {
                if(openshock_shocker_write(
                       stem,
                       s->rx_result.model,
                       s->rx_result.shocker_id,
                       s->rx_result.channel,
                       0,
                       NULL)) {
                    show_flash(s, "Saved!");
                    reload_list(s);
                } else {
                    show_flash(s, "Save failed");
                }
            } else {
                show_flash(s, "No free name");
            }
        }
        return true;
    default:
        break;
    }
    return false;
}

int32_t openshock_app(void* p) {
    UNUSED(p);

    OpenshockCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.state.screen = ScreenList;
    ctx.state.current.command = ShockerCmdShock;
    ctx.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ctx.tx = openshock_tx_alloc();
    ctx.rx = openshock_rx_alloc();
    reload_list(&ctx.state);
    openshock_settings_load(&ctx.state.transmit_vertical_ui);

    Gui* gui = furi_record_open(RECORD_GUI);

    ctx.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(ctx.view_dispatcher, &ctx);
    view_dispatcher_set_navigation_event_callback(ctx.view_dispatcher, openshock_navigation_cb);

    ctx.main_view = view_alloc();
    view_allocate_model(ctx.main_view, ViewModelTypeLockFree, sizeof(OpenshockMainModel));
    OpenshockMainModel* mm = view_get_model(ctx.main_view);
    mm->app = &ctx;
    view_set_context(ctx.main_view, &ctx);
    view_set_draw_callback(ctx.main_view, main_draw);
    view_set_input_callback(ctx.main_view, main_view_input);

    ctx.text_input = text_input_alloc();

    view_dispatcher_add_view(ctx.view_dispatcher, OpenshockViewMain, ctx.main_view);
    view_dispatcher_add_view(
        ctx.view_dispatcher, OpenshockViewTextInput, text_input_get_view(ctx.text_input));

    view_dispatcher_attach_to_gui(ctx.view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    ctx.shown_view = OpenshockViewMain;
    view_dispatcher_switch_to_view(ctx.view_dispatcher, OpenshockViewMain);

    view_dispatcher_run(ctx.view_dispatcher);

    if(ctx.tx_active) openshock_tx_stop(ctx.tx);
    openshock_tx_free(ctx.tx);
    openshock_rx_free(ctx.rx);

    view_dispatcher_remove_view(ctx.view_dispatcher, OpenshockViewTextInput);
    view_dispatcher_remove_view(ctx.view_dispatcher, OpenshockViewMain);
    text_input_free(ctx.text_input);
    view_free(ctx.main_view);
    view_dispatcher_free(ctx.view_dispatcher);

    furi_mutex_free(ctx.mutex);
    furi_record_close(RECORD_GUI);

    return 0;
}
