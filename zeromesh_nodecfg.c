#include "zeromesh_nodecfg.h"
#include "zeromesh_gui.h"
#include "zeromesh_protocol.h"
#include "zeromesh_history.h"

#include <furi.h>
#include <gui/canvas.h>

#define NODECFG_REGION 0
#define NODECFG_PRESET 1
#define NODECFG_ROLE   2
#define NODECFG_REBOOT 3
#define NODECFG_COUNT  4

static const char* const region_names[] = {
    "UNSET", "US",     "EU_433", "EU_868", "CN",     "JP",  "ANZ",  "KR",
    "TW",    "RU",     "IN",     "NZ_865", "TH",     "LORA_24", "UA_433", "UA_868",
    "MY_433", "MY_919", "SG_923", "PH_433", "PH_868", "PH_915", "ANZ_433",
};
#define REGION_COUNT (sizeof(region_names) / sizeof(region_names[0]))

static const char* const preset_names[] = {
    "LongFast",   "LongSlow",  "VLongSlow", "MedSlow",   "MedFast",
    "ShortSlow",  "ShortFast", "LongMod",   "ShortTurbo", "LongTurbo",
};
#define PRESET_COUNT (sizeof(preset_names) / sizeof(preset_names[0]))

static const char* const role_names[] = {
    "Client", "ClientMute", "Router", "RouterClient", "Repeater",
    "Tracker", "Sensor", "TAK", "ClientHidden", "LostAndFound", "TAKTracker",
};
#define ROLE_COUNT (sizeof(role_names) / sizeof(role_names[0]))

static const char* name_or_num(const char* const* table, size_t count, uint8_t v, char* fallback, size_t fb_len) {
    if(v < count) return table[v];
    snprintf(fallback, fb_len, "%u", v);
    return fallback;
}

void render_nodecfg(Canvas* canvas, ZeroMeshApp* app) {
    draw_header(canvas, app, "Node Config");
    canvas_set_font(canvas, FontSecondary);

    if(!app->my_node_num) {
        canvas_draw_str(canvas, 4, 30, "Waiting for node...");
        canvas_draw_str(canvas, 4, 42, "Connect a radio first");
        return;
    }

    char fb[8];
    char buf[64];
    int y = 24;

    for(uint8_t i = 0; i < NODECFG_COUNT; i++) {
        const char* label = "";
        const char* value = "";

        switch(i) {
        case NODECFG_REGION:
            label = "Region";
            value = name_or_num(region_names, REGION_COUNT, app->cfg_region, fb, sizeof(fb));
            break;
        case NODECFG_PRESET:
            label = "Preset";
            value = name_or_num(preset_names, PRESET_COUNT, app->cfg_preset, fb, sizeof(fb));
            break;
        case NODECFG_ROLE:
            label = "Role";
            value = name_or_num(role_names, ROLE_COUNT, app->cfg_role, fb, sizeof(fb));
            break;
        case NODECFG_REBOOT:
            label = "Reboot node";
            value = "hold OK";
            break;
        default:
            break;
        }

        if(i == app->nodecfg_cursor) {
            canvas_draw_box(canvas, 0, y - 8, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }

        snprintf(buf, sizeof(buf), "%s", label);
        canvas_draw_str(canvas, 4, y, buf);
        int vw = canvas_string_width(canvas, value);
        canvas_draw_str(canvas, 124 - vw, y, value);

        if(i == app->nodecfg_cursor) canvas_set_color(canvas, ColorBlack);
        y += 11;
    }

    canvas_draw_str(canvas, 4, 63, app->nodecfg_editing ? "OK apply, Back cancel" : "OK edit");
}

bool nodecfg_wants_key(ZeroMeshApp* app, InputKey key) {
    if(!app) return false;
    if(key == InputKeyUp || key == InputKeyDown || key == InputKeyOk) return true;
    return app->nodecfg_editing && (key == InputKeyLeft || key == InputKeyRight);
}

static void cycle(uint8_t* v, uint8_t count, int dir) {
    int n = (int)*v + dir;
    if(n < 0) n = count - 1;
    if(n >= count) n = 0;
    *v = (uint8_t)n;
}

void input_nodecfg(InputEvent* e, ZeroMeshApp* app) {
    if(!app || !e || !app->my_node_num) return;

    if(e->key == InputKeyOk) {
        if(e->type == InputTypeLong && app->nodecfg_cursor == NODECFG_REBOOT) {
            reboot_node(app, 5);
            set_status(app, "Rebooting node");
            return;
        }
        if(e->type != InputTypeShort) return;

        if(app->nodecfg_cursor == NODECFG_REBOOT) return;

        if(app->nodecfg_editing) {
            switch(app->nodecfg_cursor) {
            case NODECFG_REGION:
            case NODECFG_PRESET:
                set_node_lora(app, app->cfg_region, app->cfg_preset);
                set_status(app, "LoRa config sent");
                break;
            case NODECFG_ROLE:
                set_node_role(app, app->cfg_role);
                set_status(app, "Role sent");
                break;
            default:
                break;
            }
            app->nodecfg_editing = false;
        } else {
            app->nodecfg_editing = true;
        }
        return;
    }

    if(e->type != InputTypeShort && e->type != InputTypeRepeat) return;

    if(!app->nodecfg_editing) {
        if(e->key == InputKeyUp) {
            app->nodecfg_cursor =
                (app->nodecfg_cursor == 0) ? NODECFG_COUNT - 1 : app->nodecfg_cursor - 1;
        } else if(e->key == InputKeyDown) {
            app->nodecfg_cursor = (app->nodecfg_cursor + 1) % NODECFG_COUNT;
        }
        return;
    }

    int dir = (e->key == InputKeyRight) ? 1 : (e->key == InputKeyLeft) ? -1 : 0;
    if(!dir) return;

    switch(app->nodecfg_cursor) {
    case NODECFG_REGION:
        cycle(&app->cfg_region, REGION_COUNT, dir);
        break;
    case NODECFG_PRESET:
        cycle(&app->cfg_preset, PRESET_COUNT, dir);
        break;
    case NODECFG_ROLE:
        cycle(&app->cfg_role, ROLE_COUNT, dir);
        break;
    default:
        break;
    }
}
