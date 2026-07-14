#include "../metroflip_i.h"
#include <nfc/protocols/mf_classic/mf_classic.h>
#include "../api/metroflip/metroflip_api.h"

/* Icon declarations (compiled from images/, not in API table) */
extern const Icon I_NfcScan_10x10;
extern const Icon I_NfcScan1_10x10;
extern const Icon I_NfcScan2_10x10;
extern const Icon I_NfcScan3_10x10;
extern const Icon I_Save_10x10;
extern const Icon I_Save1_10x10;
extern const Icon I_Save2_10x10;
extern const Icon I_Save3_10x10;
extern const Icon I_Ticket_10x10;
extern const Icon I_Ticket1_10x10;
extern const Icon I_Ticket2_10x10;
extern const Icon I_Ticket3_10x10;
extern const Icon I_Info_10x10;
extern const Icon I_Info1_10x10;
extern const Icon I_Info2_10x10;
extern const Icon I_Info3_10x10;
extern const Icon I_Check_10x10;
extern const Icon I_Check1_10x10;
extern const Icon I_Check2_10x10;
extern const Icon I_Check3_10x10;

/* ── Menu layout constants ── */
#define MENU_ITEM_HEIGHT   12
#define MENU_VISIBLE_ITEMS 4
#define MENU_HEADER_H      14
#define MENU_START_Y       15
#define MENU_ITEM_COUNT    5
#define MENU_ANIM_FRAMES   3
#define MENU_ANIM_PERIOD   200 /* ms between animation frames */

typedef struct {
    const char* label;
    const Icon* icon; /* static icon */
    const Icon* anim[MENU_ANIM_FRAMES]; /* animation frames */
    uint32_t scene_id;
} StartMenuItem;

static const StartMenuItem start_menu_items[MENU_ITEM_COUNT] = {
    {"Scan Card",
     &I_NfcScan_10x10,
     {&I_NfcScan1_10x10, &I_NfcScan2_10x10, &I_NfcScan3_10x10},
     MetroflipSceneAuto},
    {"Saved", &I_Save_10x10, {&I_Save1_10x10, &I_Save2_10x10, &I_Save3_10x10}, MetroflipSceneLoad},
    {"Supported Cards",
     &I_Ticket_10x10,
     {&I_Ticket1_10x10, &I_Ticket2_10x10, &I_Ticket3_10x10},
     MetroflipSceneSupported},
    {"About", &I_Info_10x10, {&I_Info1_10x10, &I_Info2_10x10, &I_Info3_10x10}, MetroflipSceneAbout},
    {"Credits",
     &I_Check_10x10,
     {&I_Check1_10x10, &I_Check2_10x10, &I_Check3_10x10},
     MetroflipSceneCredits},
};

typedef struct {
    uint8_t selected;
    uint8_t scroll_offset;
    uint8_t anim_frame;
} StartMenuModel;

/* ── Draw callback ── */
static void start_menu_draw(Canvas* canvas, void* model) {
    if(!model) return;
    StartMenuModel* m = (StartMenuModel*)model;
    canvas_set_bitmap_mode(canvas, true);

    /* Header bar (inverted) */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, MENU_HEADER_H);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_icon(canvas, 3, 2, &I_icon);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 16, 11, "Metroflip V2");

    /* Menu items */
    for(uint8_t i = 0; i < MENU_VISIBLE_ITEMS; i++) {
        uint8_t idx = m->scroll_offset + i;
        if(idx >= MENU_ITEM_COUNT) break;

        uint8_t y = MENU_START_Y + i * MENU_ITEM_HEIGHT;
        bool selected = (idx == m->selected);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 0, y, 123, MENU_ITEM_HEIGHT, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        /* Icon - animated when selected, static otherwise */
        const Icon* ic;
        if(selected) {
            ic = start_menu_items[idx].anim[m->anim_frame % MENU_ANIM_FRAMES];
        } else {
            ic = start_menu_items[idx].icon;
        }
        if(ic) {
            canvas_draw_icon(canvas, 4, y + 1, ic);
        }

        /* Label */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 18, y + 9, start_menu_items[idx].label);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    /* Scrollbar */
    if(MENU_ITEM_COUNT > MENU_VISIBLE_ITEMS) {
        uint8_t sb_h = MENU_VISIBLE_ITEMS * MENU_ITEM_HEIGHT;
        uint8_t ind_h = sb_h * MENU_VISIBLE_ITEMS / MENU_ITEM_COUNT;
        if(ind_h < 6) ind_h = 6;
        uint8_t max_scroll = MENU_ITEM_COUNT - MENU_VISIBLE_ITEMS;
        uint8_t ind_y = MENU_START_Y;
        if(max_scroll > 0) {
            ind_y += (uint8_t)((sb_h - ind_h) * m->scroll_offset / max_scroll);
        }
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 126, MENU_START_Y, 126, MENU_START_Y + sb_h - 1);
        canvas_draw_box(canvas, 125, ind_y, 3, ind_h);
    }
}

/* ── Input callback ── */
static bool start_menu_input(InputEvent* event, void* context) {
    Metroflip* app = (Metroflip*)context;

    /* Handle Back: stop the view dispatcher to exit the app. */
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            view_dispatcher_stop(app->view_dispatcher);
        }
        return true;
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp: {
            with_view_model(
                app->main_menu,
                StartMenuModel * m,
                {
                    if(m->selected > 0) {
                        m->selected--;
                        m->anim_frame = 0;
                        if(m->selected < m->scroll_offset) {
                            m->scroll_offset = m->selected;
                        }
                    }
                },
                true);
            return true;
        }
        case InputKeyDown: {
            with_view_model(
                app->main_menu,
                StartMenuModel * m,
                {
                    if(m->selected < MENU_ITEM_COUNT - 1) {
                        m->selected++;
                        m->anim_frame = 0;
                        if(m->selected >= m->scroll_offset + MENU_VISIBLE_ITEMS) {
                            m->scroll_offset = m->selected - MENU_VISIBLE_ITEMS + 1;
                        }
                    }
                },
                true);
            return true;
        }
        case InputKeyOk: {
            uint8_t sel = 0;
            with_view_model(app->main_menu, StartMenuModel * m, { sel = m->selected; }, false);
            if(sel < MENU_ITEM_COUNT) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, start_menu_items[sel].scene_id);
            }
            return true;
        }
        default:
            break;
        }
    }
    return false;
}

static uint32_t start_menu_previous(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/* ── Scene callbacks ── */

void metroflip_scene_start_on_enter(void* context) {
    Metroflip* app = context;

    /* The main menu is the guaranteed return point for every "back" path, and
       it must never blink. Stopping here clears any scan LED that a scene left
       running (e.g. backing out of a scan before tapping a card). */
    metroflip_app_blink_stop(app);

    if(app->mfc_data) {
        mf_classic_free(app->mfc_data);
        app->mfc_data = NULL;
    }

    view_set_draw_callback(app->main_menu, start_menu_draw);
    view_set_input_callback(app->main_menu, start_menu_input);
    view_set_previous_callback(app->main_menu, start_menu_previous);

    if(!view_get_model(app->main_menu)) {
        view_allocate_model(app->main_menu, ViewModelTypeLockFree, sizeof(StartMenuModel));
    }

    /* Restore selection */
    uint32_t state = scene_manager_get_scene_state(app->scene_manager, MetroflipSceneStart);
    with_view_model(
        app->main_menu,
        StartMenuModel * m,
        {
            m->selected = 0;
            m->scroll_offset = 0;
            m->anim_frame = 0;
            for(uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
                if(start_menu_items[i].scene_id == state) {
                    m->selected = i;
                    if(i >= MENU_VISIBLE_ITEMS) {
                        m->scroll_offset = i - MENU_VISIBLE_ITEMS + 1;
                    }
                    break;
                }
            }
        },
        false);

    notification_message(app->notifications, &sequence_display_backlight_on);
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewMenu);
}

bool metroflip_scene_start_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventTick) {
            /* Animate the selected menu icon */
            with_view_model(
                app->main_menu,
                StartMenuModel * m,
                { m->anim_frame = (m->anim_frame + 1) % MENU_ANIM_FRAMES; },
                true);
            consumed = true;
        } else {
            scene_manager_set_scene_state(app->scene_manager, MetroflipSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, event.event);
            consumed = true;
        }
    }

    return consumed;
}

void metroflip_scene_start_on_exit(void* context) {
    UNUSED(context);
}
