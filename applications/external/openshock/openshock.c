#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>

#include "protocols.h"
#include "transmit.h"
#include "receiver.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

// --- Types ---

typedef enum {
    ScreenList,
    ScreenTransmit,
    ScreenEdit,
    ScreenReceive,
} Screen;

typedef enum {
    EditModel,
    EditID,
    EditChannel,
    EditFieldCount,
} EditField;

typedef struct {
    char name[OPENSHOCK_NAME_MAX_LEN];
    ShockerModel model;
    uint16_t shocker_id;
    uint8_t channel;
    ShockerCommand command;
    uint8_t intensity;
} ShockerState;

typedef struct {
    Screen screen;

    // List
    SavedShocker saved[OPENSHOCK_MAX_SAVED];
    size_t saved_count;
    int list_sel; // 0..saved_count-1 = shockers, saved_count = "Add New", saved_count+1 = "Receive"

    // Current shocker being used/edited
    ShockerState current;
    bool current_is_saved; // true if loaded from a saved entry

    // Transmit
    bool transmitting;
    uint8_t saved_intensity; // non-light intensity, preserved when in Light mode
    uint8_t saved_light; // light state (0=ON, 100=OFF), preserved when not in Light mode

    // Edit
    EditField edit_field;

    // Receive
    bool rx_found;
    DecodedShocker rx_result;

    // Flash
    uint32_t flash_tick;
    const char* flash_msg;

    FuriMutex* mutex;
    OpenshockTx* tx;
    OpenshockRx* rx;
} AppState;

// --- Helpers ---

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
    if(!openshock_command_supported(s->model, s->command)) {
        for(int i = 0; i < ShockerCmdCount; i++) {
            if(openshock_command_supported(s->model, (ShockerCommand)i)) {
                s->command = (ShockerCommand)i;
                break;
            }
        }
    }
}

static int list_total(AppState* s) {
    return (int)s->saved_count + 2; // shockers + "Add New" + "Receive"
}

static void reload_list(AppState* s) {
    s->saved_count = openshock_shocker_list(s->saved, OPENSHOCK_MAX_SAVED);
    if(s->list_sel >= list_total(s)) s->list_sel = list_total(s) - 1;
    if(s->list_sel < 0) s->list_sel = 0;
}

static void show_flash(AppState* s, const char* msg) {
    s->flash_msg = msg;
    s->flash_tick = furi_get_tick() + 1000;
}

static bool flash_active(AppState* s) {
    return s->flash_tick && furi_get_tick() < s->flash_tick;
}

static void cycle_cmd_fwd(ShockerState* s) {
    for(int i = 1; i < ShockerCmdCount; i++) {
        ShockerCommand n = (s->command + i) % ShockerCmdCount;
        if(openshock_command_supported(s->model, n)) {
            s->command = n;
            return;
        }
    }
}

static void cycle_cmd_back(ShockerState* s) {
    for(int i = ShockerCmdCount - 1; i >= 1; i--) {
        ShockerCommand n = (s->command + i) % ShockerCmdCount;
        if(openshock_command_supported(s->model, n)) {
            s->command = n;
            return;
        }
    }
}

// --- Draw ---

static void draw_flash(Canvas* canvas, AppState* s) {
    if(flash_active(s)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, s->flash_msg);
    }
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

    int scroll = 0;
    if(s->list_sel >= max_visible) scroll = s->list_sel - max_visible + 1;

    for(int vi = 0; vi < max_visible; vi++) {
        int i = scroll + vi;
        if(i >= total) break;
        int y = cy + vi * rh;
        char line[48];

        if(i < (int)s->saved_count) {
            snprintf(
                line,
                sizeof(line),
                "%s %s #%u",
                (i == s->list_sel) ? ">" : " ",
                openshock_model_name(s->saved[i].model),
                s->saved[i].shocker_id);
        } else if(i == (int)s->saved_count) {
            snprintf(line, sizeof(line), "%s + Add New", (i == s->list_sel) ? ">" : " ");
        } else {
            snprintf(line, sizeof(line), "%s ~ Receive", (i == s->list_sel) ? ">" : " ");
        }
        canvas_draw_str(canvas, 0, y, line);
    }

    if(scroll > 0) canvas_draw_str(canvas, 122, cy, "^");
    if(scroll + max_visible < total)
        canvas_draw_str(canvas, 122, cy + (max_visible - 1) * rh, "v");

    if(s->list_sel < (int)s->saved_count) {
        elements_button_left(canvas, "Del");
        elements_button_center(canvas, "Use");
        elements_button_right(canvas, "Edit");
    } else {
        elements_button_center(canvas, "Select");
    }
}

static void draw_transmit(Canvas* canvas, AppState* s) {
    ShockerState* c = &s->current;

    canvas_set_font(canvas, FontSecondary);
    // Header: model + ID
    char header[40];
    snprintf(
        header,
        sizeof(header),
        "%s #%u  CH:%u",
        openshock_model_name(c->model),
        c->shocker_id,
        c->channel);
    canvas_draw_str(canvas, 0, 8, header);

    if(s->transmitting && c->command != ShockerCmdLight) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Transmitting...");
        canvas_set_font(canvas, FontSecondary);
        char info[32];
        snprintf(
            info, sizeof(info), "%s @ %u%%", openshock_command_name(c->command), c->intensity);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, info);
        return;
    }

    // Command mode (center, large)
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 26, AlignCenter, AlignCenter, openshock_command_name(c->command));

    if(c->command == ShockerCmdLight) {
        // Light: binary ON/OFF (0% = ON, 100% = OFF)
        canvas_set_font(canvas, FontPrimary);
        const char* light_state = (c->intensity == 0) ? "ON" : "OFF";
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, light_state);
    } else {
        // Intensity bar
        canvas_set_font(canvas, FontSecondary);
        char int_str[16];
        snprintf(int_str, sizeof(int_str), "%u%%", c->intensity);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, int_str);

        const int bar_x = 14;
        const int bar_y = 42;
        const int bar_w = 100;
        const int bar_h = 6;
        canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
        int fill = (c->intensity * (bar_w - 2)) / 100;
        if(fill > 0) canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill, bar_h - 2);
    }

    // Hints
    elements_button_left(canvas, "Mode");
    elements_button_center(canvas, "TX");
    elements_button_right(canvas, "Mode");
}

static void draw_edit(Canvas* canvas, AppState* s) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 11, "Edit Shocker");

    if(flash_active(s)) {
        draw_flash(canvas, s);
        return;
    }

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    const int rh = 12;
    const int cy = 24;
    const int vx = 56;

    const char* labels[] = {"Model:", "ID:", "Ch:"};
    for(int i = 0; i < EditFieldCount; i++) {
        int y = cy + i * rh;
        const char* prefix = (i == (int)s->edit_field) ? ">" : " ";
        snprintf(buf, sizeof(buf), "%s%s", prefix, labels[i]);
        canvas_draw_str(canvas, 0, y, buf);

        switch(i) {
        case EditModel:
            canvas_draw_str(canvas, vx, y, openshock_model_name(s->current.model));
            break;
        case EditID:
            snprintf(buf, sizeof(buf), "%u", s->current.shocker_id);
            canvas_draw_str(canvas, vx, y, buf);
            break;
        case EditChannel:
            snprintf(buf, sizeof(buf), "%u", s->current.channel);
            canvas_draw_str(canvas, vx, y, buf);
            break;
        }
    }

    elements_button_left(canvas, "Save");
    elements_button_right(canvas, "Change");
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

static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* s = ctx;
    furi_mutex_acquire(s->mutex, FuriWaitForever);
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
    }

    furi_mutex_release(s->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

// --- Input ---

// Returns true if app should exit
static bool handle_list(AppState* s, InputEvent* e) {
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
        if(s->list_sel < (int)s->saved_count) {
            // Load saved shocker into current and go to transmit
            SavedShocker* sel = &s->saved[s->list_sel];
            snprintf(s->current.name, sizeof(s->current.name), "%s", sel->name);
            s->current.model = sel->model;
            s->current.shocker_id = sel->shocker_id;
            s->current.channel = sel->channel;
            s->current.command = ShockerCmdVibrate;
            s->current.intensity = 0;
            s->saved_intensity = 0;
            s->saved_light = 100; // OFF by default
            s->current_is_saved = true;
            ensure_valid(&s->current);
            s->screen = ScreenTransmit;
        } else if(s->list_sel == (int)s->saved_count) {
            // Add new → go to edit with defaults
            memset(&s->current, 0, sizeof(s->current));
            s->current.command = ShockerCmdVibrate;
            s->current_is_saved = false;
            s->edit_field = EditModel;
            s->screen = ScreenEdit;
        } else {
            // Receive
            s->rx_found = false;
            s->screen = ScreenReceive;
            openshock_rx_start(s->rx);
        }
        break;
    case InputKeyRight:
        // Edit selected shocker
        if(s->list_sel < (int)s->saved_count) {
            SavedShocker* sel = &s->saved[s->list_sel];
            snprintf(s->current.name, sizeof(s->current.name), "%s", sel->name);
            s->current.model = sel->model;
            s->current.shocker_id = sel->shocker_id;
            s->current.channel = sel->channel;
            s->current.command = ShockerCmdVibrate;
            s->current.intensity = 0;
            s->current_is_saved = true;
            s->edit_field = EditModel;
            s->screen = ScreenEdit;
        }
        break;
    case InputKeyLeft:
        // Delete selected shocker
        if(s->list_sel < (int)s->saved_count) {
            openshock_shocker_delete(s->saved[s->list_sel].filename);
            reload_list(s);
            show_flash(s, "Deleted");
        }
        break;
    case InputKeyBack:
        return true;
    default:
        break;
    }
    return false;
}

static void handle_transmit(AppState* s, InputEvent* e, OpenshockTx* tx, OpenshockTx** active) {
    ShockerState* c = &s->current;

    // OK press: start TX
    if(e->key == InputKeyOk && e->type == InputTypePress) {
        s->transmitting = true;
        OokPulse pulses[OPENSHOCK_MAX_PULSES];
        size_t count = openshock_encode(
            c->model,
            c->shocker_id,
            c->command,
            c->intensity,
            c->channel,
            pulses,
            OPENSHOCK_MAX_PULSES);
        if(count > 0) {
            openshock_tx_start(tx, pulses, count);
            *active = tx;
        }
        return;
    }

    // OK release: stop TX
    if(e->key == InputKeyOk && e->type == InputTypeRelease && *active) {
        openshock_tx_stop(tx);
        *active = NULL;
        s->transmitting = false;
        return;
    }

    // Back: return to list
    if(e->key == InputKeyBack && (e->type == InputTypePress || e->type == InputTypeShort)) {
        if(*active) {
            openshock_tx_stop(tx);
            *active = NULL;
            s->transmitting = false;
        }
        s->screen = ScreenList;
        return;
    }

    if(e->type != InputTypePress && e->type != InputTypeRepeat) return;

    switch(e->key) {
    case InputKeyUp:
        if(c->command == ShockerCmdLight) {
            c->intensity = 0; // ON
            if(*active) openshock_tx_stop(tx);
            OokPulse pulses_on[OPENSHOCK_MAX_PULSES];
            size_t cnt_on = openshock_encode(
                c->model,
                c->shocker_id,
                c->command,
                c->intensity,
                c->channel,
                pulses_on,
                OPENSHOCK_MAX_PULSES);
            if(cnt_on > 0) {
                openshock_tx_start(tx, pulses_on, cnt_on);
                *active = tx;
                s->transmitting = true;
            }
        } else if(c->intensity < 100) {
            uint8_t step = (e->type == InputTypeRepeat) ? 5 : 1;
            uint16_t n = (uint16_t)c->intensity + step;
            c->intensity = (n <= 100) ? (uint8_t)n : 100;
        }
        break;
    case InputKeyDown:
        if(c->command == ShockerCmdLight) {
            c->intensity = 100; // OFF
            if(*active) openshock_tx_stop(tx);
            OokPulse pulses_off[OPENSHOCK_MAX_PULSES];
            size_t cnt_off = openshock_encode(
                c->model,
                c->shocker_id,
                c->command,
                c->intensity,
                c->channel,
                pulses_off,
                OPENSHOCK_MAX_PULSES);
            if(cnt_off > 0) {
                openshock_tx_start(tx, pulses_off, cnt_off);
                *active = tx;
                s->transmitting = true;
            }
        } else if(c->intensity > 0) {
            uint8_t step = (e->type == InputTypeRepeat) ? 5 : 1;
            c->intensity = (c->intensity >= step) ? (c->intensity - step) : 0;
        }
        break;
    case InputKeyLeft:
    case InputKeyRight:
        // Stop any active TX when switching modes
        if(*active) {
            openshock_tx_stop(tx);
            *active = NULL;
            s->transmitting = false;
        }
        {
            bool was_light = (c->command == ShockerCmdLight);
            if(e->key == InputKeyLeft)
                cycle_cmd_back(c);
            else
                cycle_cmd_fwd(c);
            bool is_light = (c->command == ShockerCmdLight);
            if(!was_light && is_light) {
                // Entering light: save intensity, restore light state
                s->saved_intensity = c->intensity;
                c->intensity = s->saved_light;
            } else if(was_light && !is_light) {
                // Leaving light: save light state, restore intensity
                s->saved_light = c->intensity;
                c->intensity = s->saved_intensity;
            }
        }
        break;
    default:
        break;
    }
}

static void handle_edit(AppState* s, InputEvent* e) {
    if(e->type != InputTypePress && e->type != InputTypeRepeat) return;

    switch(e->key) {
    case InputKeyUp:
        if(s->edit_field > 0) s->edit_field--;
        break;
    case InputKeyDown:
        if(s->edit_field < EditFieldCount - 1) s->edit_field++;
        break;
    case InputKeyRight:
        switch(s->edit_field) {
        case EditModel:
            s->current.model = (s->current.model + 1) % ShockerModelCount;
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
        default:
            break;
        }
        break;
    case InputKeyLeft:
        // Save
        if(e->type == InputTypePress) {
            char name[32];
            snprintf(
                name,
                sizeof(name),
                "%s_%u",
                openshock_model_name(s->current.model),
                s->current.shocker_id);
            openshock_shocker_save(
                name, s->current.model, s->current.shocker_id, s->current.channel);
            show_flash(s, "Saved!");
            reload_list(s);
        }
        break;
    case InputKeyBack:
        reload_list(s);
        s->screen = ScreenList;
        break;
    default:
        break;
    }
}

static void handle_receive(AppState* s, InputEvent* e) {
    if(e->type != InputTypePress && e->type != InputTypeShort) return;

    switch(e->key) {
    case InputKeyOk:
        if(s->rx_found) {
            // Load into current and go to transmit
            s->current.model = s->rx_result.model;
            s->current.shocker_id = s->rx_result.shocker_id;
            s->current.channel = s->rx_result.channel;
            s->current.command = s->rx_result.command;
            s->current.intensity = s->rx_result.intensity;
            s->current_is_saved = false;
            s->saved_intensity = 0;
            s->saved_light = 100;
            ensure_valid(&s->current);
            openshock_rx_stop(s->rx);
            s->screen = ScreenTransmit;
        }
        break;
    case InputKeyLeft:
        if(s->rx_found) {
            char name[32];
            snprintf(
                name,
                sizeof(name),
                "%s_%u",
                openshock_model_name(s->rx_result.model),
                s->rx_result.shocker_id);
            openshock_shocker_save(
                name, s->rx_result.model, s->rx_result.shocker_id, s->rx_result.channel);
            show_flash(s, "Saved!");
            reload_list(s);
        }
        break;
    case InputKeyBack:
        openshock_rx_stop(s->rx);
        reload_list(s);
        s->screen = ScreenList;
        break;
    default:
        break;
    }
}

// --- Main ---

int32_t openshock_app(void* p) {
    UNUSED(p);

    AppState state;
    memset(&state, 0, sizeof(state));
    state.screen = ScreenList;
    state.current.command = ShockerCmdVibrate;
    state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    state.tx = openshock_tx_alloc();
    state.rx = openshock_rx_alloc();
    reload_list(&state);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    OpenshockTx* tx_active = NULL;
    bool running = true;

    while(running) {
        // Poll RX results
        if(state.screen == ScreenReceive) {
            DecodedShocker decoded;
            if(openshock_rx_get_result(state.rx, &decoded)) {
                furi_mutex_acquire(state.mutex, FuriWaitForever);
                state.rx_result = decoded;
                state.rx_found = true;
                furi_mutex_release(state.mutex);
                view_port_update(view_port);
            }
        }

        FuriStatus status = furi_message_queue_get(event_queue, &event, 50);
        if(status != FuriStatusOk) {
            view_port_update(view_port);
            continue;
        }

        furi_mutex_acquire(state.mutex, FuriWaitForever);

        switch(state.screen) {
        case ScreenList:
            running = !handle_list(&state, &event);
            break;
        case ScreenTransmit:
            handle_transmit(&state, &event, state.tx, &tx_active);
            break;
        case ScreenEdit:
            handle_edit(&state, &event);
            break;
        case ScreenReceive:
            handle_receive(&state, &event);
            break;
        }

        furi_mutex_release(state.mutex);
        view_port_update(view_port);
    }

    if(tx_active) openshock_tx_stop(state.tx);
    openshock_tx_free(state.tx);
    openshock_rx_free(state.rx);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(state.mutex);
    furi_record_close(RECORD_GUI);

    return 0;
}
