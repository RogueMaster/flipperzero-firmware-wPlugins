// Pod — Flipper-to-Flipper encounter game over sub-GHz.
//
// Encounters vs battles:
//   - Catching a beacon = an ENCOUNTER (logged). A walk-by.
//   - A BATTLE is a live 10-second tap race that only happens when two people
//     are in range AND both opt in (challenge -> accept). Most taps wins.
//
// Modes:
//   - Walk Mode: broadcast + listen. OK shows who's IN RANGE to challenge.
//   - Practice: solo tap race vs a simulated opponent.
//
// The two-device challenge/accept/tap-exchange runs over radio and needs a
// second Flipper to verify end to end. The wire format is host-tested.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "profile.h"
#include "radio.h"
#include "encounters.h"

#define POD_BEACON_PERIOD_MS 3000
#define POD_ANIM_PERIOD_MS   120
#define POD_TICK_MS          100

#define POD_COUNTDOWN_MS      3000
#define POD_ACTIVE_MS         10000
#define POD_CHAL_TIMEOUT_MS   8000
#define POD_CHALD_TIMEOUT_MS  15000
#define POD_RESULT_TIMEOUT_MS 6000
#define POD_CHAL_RESEND_MS    500
#define POD_TAPS_RESEND_MS    400

#define POD_INRANGE_MAX    8
#define POD_INRANGE_TTL_MS 12000

// Countdown beep + go tone.
static const NotificationSequence pod_seq_beep = {
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence pod_seq_go = {
    &message_note_c7,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

typedef enum {
    PodViewMainMenu,
    PodViewWalkMode,
    PodViewInRange,
    PodViewEncounters,
    PodViewEncounterDetail,
    PodViewClearConfirm,
    PodViewBattle,
    PodViewProfile,
    PodViewTextInput,
    PodViewAbout,
} PodView;

typedef enum {
    PodMenuWalkMode,
    PodMenuEncounters,
    PodMenuPractice,
    PodMenuProfile,
    PodMenuAbout,
} PodMenuItem;

typedef enum {
    PodEventMsg = 100,
} PodCustomEvent;

typedef struct {
    char name[POD_NAME_MAX + 1];
    uint8_t dolphin_level;
    uint32_t beacons_sent;
    uint16_t frame;
    uint8_t in_range_count;
    bool radio_ok;
} PodWalkModel;

typedef enum {
    BPhaseChallenging,
    BPhaseChallenged,
    BPhaseCountdown,
    BPhaseActive,
    BPhaseWaitResult,
    BPhaseResult,
    BPhaseCancelled,
} BPhase;

typedef struct {
    BPhase phase;
    bool practice;
    uint32_t elapsed_ms;
    uint32_t timeout_ms;
    uint32_t resend_ms;
    uint32_t my_taps;
    uint32_t opp_taps;
    uint32_t opp_target;
    bool opp_taps_known;
    uint32_t peer_id;
    uint16_t battle_id;
    uint8_t my_level;
    uint8_t opp_level;
    char opp_name[POD_NAME_MAX + 1];
    int8_t result;
    uint16_t xp_gained;
    uint16_t anim; // result-screen animation frame
    char reason[24];
} PodBattleModel;

typedef struct {
    uint32_t id;
    char name[POD_NAME_MAX + 1];
    uint8_t level;
    uint32_t last_tick;
} PodInRange;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    View* walk_view;
    Submenu* inrange_menu;
    Submenu* encounters_menu;
    Widget* detail_widget;
    Widget* clear_widget;
    View* battle_view;
    Widget* profile_widget;
    TextInput* text_input;
    Widget* about_widget;
    FuriTimer* beacon_timer;
    FuriTimer* anim_timer;
    FuriTimer* battle_timer;
    FuriMessageQueue* msg_queue;
    PodProfile profile;
    PodEncounters encounters;
    PodRadio* radio;
    PodInRange in_range[POD_INRANGE_MAX];
    char name_edit[POD_NAME_MAX + 1];
    uint16_t selected_encounter;
    bool in_battle;
    uint32_t battle_return_view;
} Pod;

static void pod_profile_widget_populate(Pod* app);
static void pod_name_input_done(void* context);
static void pod_encounters_menu_populate(Pod* app);
static void pod_encounter_detail_populate(Pod* app);
static void pod_battle_finish(Pod* app);
static void pod_inrange_menu_cb(void* context, uint32_t index);

// --- helpers ---

static const char* pod_rank(uint32_t xp) {
    if(xp >= 1000) return "Kraken";
    if(xp >= 500) return "Orca";
    if(xp >= 250) return "Dolphin";
    if(xp >= 100) return "Tuna";
    if(xp >= 40) return "Guppy";
    return "Minnow";
}

static void pod_reltime(char* out, size_t n, uint32_t then, uint32_t now) {
    uint32_t d = (now > then) ? now - then : 0;
    if(d < 60)
        snprintf(out, n, "%lus ago", (unsigned long)d);
    else if(d < 3600)
        snprintf(out, n, "%lum ago", (unsigned long)(d / 60));
    else if(d < 86400)
        snprintf(out, n, "%luh ago", (unsigned long)(d / 3600));
    else
        snprintf(out, n, "%lud ago", (unsigned long)(d / 86400));
}

// --- navigation ---

static uint32_t pod_prev_main_menu(void* context) {
    UNUSED(context);
    return PodViewMainMenu;
}
static uint32_t pod_prev_profile(void* context) {
    UNUSED(context);
    return PodViewProfile;
}
static uint32_t pod_prev_encounters(void* context) {
    UNUSED(context);
    return PodViewEncounters;
}
static uint32_t pod_prev_walk(void* context) {
    UNUSED(context);
    return PodViewWalkMode;
}

static bool pod_navigation_callback(void* context) {
    furi_assert(context);
    Pod* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

// --- in-range tracking ---

static void pod_inrange_update(Pod* app, uint32_t id, const char* name, uint8_t level) {
    uint32_t now = furi_get_tick();
    uint32_t ttl = furi_ms_to_ticks(POD_INRANGE_TTL_MS);
    int freeslot = -1, stale = -1;
    for(int k = 0; k < POD_INRANGE_MAX; k++) {
        PodInRange* pr = &app->in_range[k];
        if(pr->id == id) {
            strncpy(pr->name, name, POD_NAME_MAX);
            pr->name[POD_NAME_MAX] = '\0';
            pr->level = level;
            pr->last_tick = now;
            return;
        }
        if(pr->id == 0 && freeslot < 0) freeslot = k;
        if(pr->id != 0 && (now - pr->last_tick) > ttl && stale < 0) stale = k;
    }
    int slot = (freeslot >= 0) ? freeslot : (stale >= 0) ? stale : 0;
    PodInRange* pr = &app->in_range[slot];
    pr->id = id;
    strncpy(pr->name, name, POD_NAME_MAX);
    pr->name[POD_NAME_MAX] = '\0';
    pr->level = level;
    pr->last_tick = now;
}

// --- incoming radio messages (GUI thread) ---

static void pod_handle_beacon(Pod* app, const PodMsg* msg) {
    if(msg->src_id == app->profile.profile_id) return;
    pod_inrange_update(app, msg->src_id, msg->name, msg->level);

    PodBeacon b;
    b.profile_id = msg->src_id;
    strncpy(b.name, msg->name, POD_NAME_MAX);
    b.name[POD_NAME_MAX] = '\0';
    b.icon = msg->icon;
    b.dolphin_level = msg->level;
    b.nonce = msg->nonce;

    uint32_t now = furi_hal_rtc_get_timestamp();
    bool is_new = false;
    int idx = pod_encounters_update(&app->encounters, &b, now, &is_new);
    if(idx < 0) return;
    if(is_new) notification_message(app->notifications, &sequence_single_vibro);
    pod_encounters_save(&app->encounters);
    pod_encounters_menu_populate(app);
}

static void pod_handle_challenge(Pod* app, const PodMsg* msg) {
    if(app->in_battle) {
        pod_radio_send_decline(app->radio, app->profile.profile_id, msg->src_id, msg->battle_id);
        return;
    }
    app->in_battle = true;
    app->battle_return_view = PodViewWalkMode;
    uint8_t my_level = pod_profile_dolphin_level();
    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            m->phase = BPhaseChallenged;
            m->practice = false;
            m->elapsed_ms = 0;
            m->timeout_ms = 0;
            m->resend_ms = 0;
            m->my_taps = 0;
            m->opp_taps = 0;
            m->opp_taps_known = false;
            m->xp_gained = 0;
            m->anim = 0;
            m->peer_id = msg->src_id;
            m->battle_id = msg->battle_id;
            m->my_level = my_level;
            m->opp_level = msg->level;
            m->result = 0;
            strncpy(m->opp_name, msg->name, sizeof(m->opp_name) - 1);
            m->opp_name[sizeof(m->opp_name) - 1] = '\0';
        },
        true);
    furi_timer_start(app->battle_timer, furi_ms_to_ticks(POD_TICK_MS));
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewBattle);
    notification_message(app->notifications, &sequence_single_vibro);
}

static void pod_handle_accept(Pod* app, const PodMsg* msg) {
    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            if(app->in_battle && m->phase == BPhaseChallenging && m->battle_id == msg->battle_id &&
               m->peer_id == msg->src_id) {
                m->phase = BPhaseCountdown;
                m->elapsed_ms = 0;
            }
        },
        true);
}

static void pod_handle_decline(Pod* app, const PodMsg* msg) {
    bool cancelled = false;
    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            if(app->in_battle && m->phase == BPhaseChallenging && m->battle_id == msg->battle_id &&
               m->peer_id == msg->src_id) {
                m->phase = BPhaseCancelled;
                snprintf(m->reason, sizeof(m->reason), "Declined");
                cancelled = true;
            }
        },
        true);
    if(cancelled) notification_message(app->notifications, &sequence_single_vibro);
}

static void pod_handle_taps(Pod* app, const PodMsg* msg) {
    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            if(app->in_battle && m->battle_id == msg->battle_id && m->peer_id == msg->src_id) {
                m->opp_taps = msg->taps;
                m->opp_taps_known = true;
            }
        },
        true);
}

static void pod_on_msg(Pod* app, const PodMsg* m) {
    switch(m->type) {
    case PodMsgBeacon:
        pod_handle_beacon(app, m);
        break;
    case PodMsgChallenge:
        if(m->dst_id == app->profile.profile_id) pod_handle_challenge(app, m);
        break;
    case PodMsgAccept:
        if(m->dst_id == app->profile.profile_id) pod_handle_accept(app, m);
        break;
    case PodMsgDecline:
        if(m->dst_id == app->profile.profile_id) pod_handle_decline(app, m);
        break;
    case PodMsgTaps:
        if(m->dst_id == app->profile.profile_id) pod_handle_taps(app, m);
        break;
    }
}

static bool pod_custom_event_callback(void* context, uint32_t event) {
    Pod* app = context;
    if(event == PodEventMsg) {
        PodMsg m;
        while(furi_message_queue_get(app->msg_queue, &m, 0) == FuriStatusOk) {
            pod_on_msg(app, &m);
        }
        return true;
    }
    return false;
}

static void pod_radio_on_msg(void* context, const PodMsg* msg) {
    Pod* app = context;
    furi_message_queue_put(app->msg_queue, msg, 0);
    view_dispatcher_send_custom_event(app->view_dispatcher, PodEventMsg);
}

// --- main menu ---

static void pod_start_practice(Pod* app) {
    app->in_battle = true;
    app->battle_return_view = PodViewMainMenu;
    uint8_t level = (uint8_t)(5 + (furi_hal_random_get() % 20));
    uint32_t target = 45 + (uint32_t)level * 2 + (furi_hal_random_get() % 15);
    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            m->phase = BPhaseCountdown;
            m->practice = true;
            m->elapsed_ms = 0;
            m->my_taps = 0;
            m->opp_taps = 0;
            m->opp_target = target;
            m->opp_level = level;
            m->result = 0;
            m->xp_gained = 0;
            m->anim = 0;
            strncpy(m->opp_name, "PRACTICE", sizeof(m->opp_name) - 1);
            m->opp_name[sizeof(m->opp_name) - 1] = '\0';
        },
        true);
    furi_timer_start(app->battle_timer, furi_ms_to_ticks(POD_TICK_MS));
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewBattle);
}

static void pod_submenu_callback(void* context, uint32_t index) {
    Pod* app = context;
    switch(index) {
    case PodMenuWalkMode:
        view_dispatcher_switch_to_view(app->view_dispatcher, PodViewWalkMode);
        break;
    case PodMenuEncounters:
        pod_encounters_menu_populate(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, PodViewEncounters);
        break;
    case PodMenuPractice:
        pod_start_practice(app);
        break;
    case PodMenuProfile:
        view_dispatcher_switch_to_view(app->view_dispatcher, PodViewProfile);
        break;
    case PodMenuAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, PodViewAbout);
        break;
    }
}

// --- Walk Mode ---

static void pod_walk_draw(Canvas* canvas, void* model) {
    PodWalkModel* m = model;
    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "WALK MODE");
    canvas_set_color(canvas, ColorBlack);

    if(!m->radio_ok) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Radio failed to start");
        return;
    }

    // Sonar rings emanating from a small source point.
    const int cx = 100, cy = 36;
    canvas_draw_disc(canvas, cx, cy, 2);
    for(int k = 0; k < 3; k++) {
        int r = ((m->frame + k * 6) % 18) + 2;
        canvas_draw_circle(canvas, cx, cy, r);
    }

    canvas_set_font(canvas, FontSecondary);
    char line[32];
    canvas_draw_str(canvas, 4, 24, m->name);
    snprintf(line, sizeof(line), "Dolphin Lv %u", m->dolphin_level);
    canvas_draw_str(canvas, 4, 35, line);
    if(m->in_range_count > 0) {
        snprintf(line, sizeof(line), "In range: %u", m->in_range_count);
    } else {
        snprintf(line, sizeof(line), "Beacons %lu", (unsigned long)m->beacons_sent);
    }
    canvas_draw_str(canvas, 4, 46, line);
    canvas_draw_str(canvas, 4, 62, "OK: who's in range");
}

static bool pod_walk_input(InputEvent* event, void* context) {
    Pod* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        submenu_reset(app->inrange_menu);
        submenu_set_header(app->inrange_menu, "In Range");
        uint32_t now = furi_get_tick();
        uint32_t ttl = furi_ms_to_ticks(POD_INRANGE_TTL_MS);
        int shown = 0;
        for(int k = 0; k < POD_INRANGE_MAX; k++) {
            PodInRange* pr = &app->in_range[k];
            if(pr->id == 0 || (now - pr->last_tick) > ttl) continue;
            char label[40];
            snprintf(label, sizeof(label), "%s Lv%u", pr->name, pr->level);
            submenu_add_item(app->inrange_menu, label, k, pod_inrange_menu_cb, app);
            shown++;
        }
        if(shown == 0)
            submenu_add_item(app->inrange_menu, "Nobody nearby", 0xFF, pod_inrange_menu_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, PodViewInRange);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        pod_radio_stop(app->radio);
        return false;
    }
    return false;
}

static void pod_walk_enter(void* context) {
    Pod* app = context;
    bool ok = pod_radio_is_running(app->radio) ? true : pod_radio_start(app->radio);
    uint8_t level = pod_profile_dolphin_level();
    with_view_model(
        app->walk_view,
        PodWalkModel * m,
        {
            strncpy(m->name, app->profile.name, sizeof(m->name) - 1);
            m->name[sizeof(m->name) - 1] = '\0';
            m->dolphin_level = level;
            m->beacons_sent = 0;
            m->frame = 0;
            m->in_range_count = 0;
            m->radio_ok = ok;
        },
        true);
    if(ok) {
        furi_timer_start(app->beacon_timer, furi_ms_to_ticks(POD_BEACON_PERIOD_MS));
        furi_timer_start(app->anim_timer, furi_ms_to_ticks(POD_ANIM_PERIOD_MS));
    }
}

static void pod_walk_exit(void* context) {
    Pod* app = context;
    furi_timer_stop(app->beacon_timer);
    furi_timer_stop(app->anim_timer);
}

static void pod_beacon_timer_cb(void* context) {
    Pod* app = context;
    uint8_t level = 0;
    with_view_model(app->walk_view, PodWalkModel * m, { level = m->dolphin_level; }, false);
    bool sent = pod_radio_beacon(app->radio, &app->profile, level);
    if(sent) {
        with_view_model(app->walk_view, PodWalkModel * m, { m->beacons_sent++; }, false);
    }
}

static void pod_anim_timer_cb(void* context) {
    Pod* app = context;
    uint32_t now = furi_get_tick();
    uint32_t ttl = furi_ms_to_ticks(POD_INRANGE_TTL_MS);
    uint8_t cnt = 0;
    for(int k = 0; k < POD_INRANGE_MAX; k++)
        if(app->in_range[k].id != 0 && (now - app->in_range[k].last_tick) <= ttl) cnt++;
    with_view_model(
        app->walk_view,
        PodWalkModel * m,
        {
            m->frame++;
            m->in_range_count = cnt;
        },
        true);
}

// --- In-range list ---

static void pod_inrange_menu_cb(void* context, uint32_t index) {
    Pod* app = context;
    if(index >= POD_INRANGE_MAX) return;
    PodInRange* pr = &app->in_range[index];
    if(pr->id == 0) return;

    app->in_battle = true;
    app->battle_return_view = PodViewWalkMode;
    uint16_t bid = (uint16_t)(furi_hal_random_get() & 0xFFFF);
    uint8_t my_level = pod_profile_dolphin_level();
    uint32_t peer = pr->id;
    uint8_t plevel = pr->level;
    char pname[POD_NAME_MAX + 1];
    strncpy(pname, pr->name, sizeof(pname) - 1);
    pname[sizeof(pname) - 1] = '\0';

    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            m->phase = BPhaseChallenging;
            m->practice = false;
            m->elapsed_ms = 0;
            m->timeout_ms = 0;
            m->resend_ms = 0;
            m->my_taps = 0;
            m->opp_taps = 0;
            m->opp_taps_known = false;
            m->xp_gained = 0;
            m->anim = 0;
            m->peer_id = peer;
            m->battle_id = bid;
            m->my_level = my_level;
            m->opp_level = plevel;
            m->result = 0;
            strncpy(m->opp_name, pname, sizeof(m->opp_name) - 1);
            m->opp_name[sizeof(m->opp_name) - 1] = '\0';
        },
        true);
    pod_radio_send_challenge(
        app->radio, app->profile.profile_id, app->profile.name, my_level, peer, bid);
    furi_timer_start(app->battle_timer, furi_ms_to_ticks(POD_TICK_MS));
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewBattle);
}

// --- Battle view ---

static void pod_battle_draw(Canvas* canvas, void* model) {
    PodBattleModel* m = model;
    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    char head[28];
    snprintf(head, sizeof(head), "VS %s", m->opp_name);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, head);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char buf[24];
    switch(m->phase) {
    case BPhaseChallenging:
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Challenging...");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "waiting for accept");
        break;
    case BPhaseChallenged:
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "wants to BATTLE!");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "OK = accept");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back = decline");
        break;
    case BPhaseCountdown: {
        int c = 3 - (int)(m->elapsed_ms / 1000);
        if(c < 1) c = 1;
        snprintf(buf, sizeof(buf), "%d", c);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, buf);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Get ready - MASH OK!");
        break;
    }
    case BPhaseActive: {
        int secs = (int)((POD_ACTIVE_MS - m->elapsed_ms + 999) / 1000);
        bool show_opp = m->practice;
        bool you_lead = show_opp ? (m->my_taps >= m->opp_taps) : true;
        // highlight the leader's panel
        if(you_lead)
            canvas_draw_rframe(canvas, 6, 15, 56, 25, 3);
        else
            canvas_draw_rframe(canvas, 66, 15, 56, 25, 3);
        canvas_set_font(canvas, FontBigNumbers);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->my_taps);
        canvas_draw_str_aligned(canvas, 34, 28, AlignCenter, AlignCenter, buf);
        if(show_opp) {
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->opp_taps);
            canvas_draw_str_aligned(canvas, 94, 28, AlignCenter, AlignCenter, buf);
        } else {
            // FontBigNumbers carries digits only: a '?' drawn in it renders as
            // nothing, leaving the rival's panel looking broken rather than
            // unknown. Their count genuinely is unknown until the race ends,
            // since tap counts are exchanged once, afterwards.
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 94, 26, AlignCenter, AlignCenter, "?");
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 34, 46, AlignCenter, AlignCenter, "YOU");
        canvas_draw_str_aligned(canvas, 94, 46, AlignCenter, AlignCenter, "RIVAL");
        snprintf(buf, sizeof(buf), "%ds left", secs);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, buf);
        break;
    }
    case BPhaseWaitResult:
        snprintf(buf, sizeof(buf), "Your taps: %lu", (unsigned long)m->my_taps);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, buf);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "waiting for rival...");
        break;
    case BPhaseResult: {
        const char* r = (m->result > 0) ? "YOU WIN!" : (m->result < 0) ? "YOU LOSE" : "DRAW";
        if(m->result > 0) {
            // celebratory expanding rings (behind the text)
            int rr = (m->anim % 14);
            canvas_draw_circle(canvas, 64, 23, rr + 2);
            canvas_draw_circle(canvas, 64, 23, ((rr + 7) % 14) + 2);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignCenter, r);
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            buf, sizeof(buf), "%lu - %lu", (unsigned long)m->my_taps, (unsigned long)m->opp_taps);
        canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignCenter, buf);
        if(m->xp_gained > 0) {
            snprintf(buf, sizeof(buf), "+%u XP", m->xp_gained);
            canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignCenter, buf);
        }
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back to continue");
        break;
    }
    case BPhaseCancelled:
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, m->reason);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Back to continue");
        break;
    }
}

static bool pod_battle_input(InputEvent* event, void* context) {
    Pod* app = context;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        bool decline = false;
        uint32_t dst = 0;
        uint16_t bid = 0;
        with_view_model(
            app->battle_view,
            PodBattleModel * m,
            {
                if(m->phase == BPhaseChallenged) {
                    decline = true;
                    dst = m->peer_id;
                    bid = m->battle_id;
                }
            },
            false);
        furi_timer_stop(app->battle_timer);
        if(decline) pod_radio_send_decline(app->radio, app->profile.profile_id, dst, bid);
        app->in_battle = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, app->battle_return_view);
        return true;
    }

    if(event->key == InputKeyOk) {
        bool accept = false;
        uint32_t dst = 0;
        uint16_t bid = 0;
        uint8_t mylvl = 0;
        with_view_model(
            app->battle_view,
            PodBattleModel * m,
            {
                if(m->phase == BPhaseChallenged && event->type == InputTypeShort) {
                    m->phase = BPhaseCountdown;
                    m->elapsed_ms = 0;
                    accept = true;
                    dst = m->peer_id;
                    bid = m->battle_id;
                    mylvl = m->my_level;
                } else if(m->phase == BPhaseActive && event->type == InputTypePress) {
                    m->my_taps++;
                }
            },
            true);
        if(accept)
            pod_radio_send_accept(
                app->radio, app->profile.profile_id, app->profile.name, mylvl, dst, bid);
        return true;
    }
    return false;
}

static void pod_battle_timer_cb(void* context) {
    Pod* app = context;
    enum {
        ActNone,
        ActChallenge,
        ActTaps,
        ActFinish
    } act = ActNone;
    uint32_t dst = 0;
    uint16_t bid = 0;
    uint16_t taps = 0;
    uint8_t mylvl = 0;
    bool do_decline = false;
    bool beep = false, go = false;

    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            switch(m->phase) {
            case BPhaseChallenging:
                m->timeout_ms += POD_TICK_MS;
                m->resend_ms += POD_TICK_MS;
                if(m->timeout_ms >= POD_CHAL_TIMEOUT_MS) {
                    m->phase = BPhaseCancelled;
                    snprintf(m->reason, sizeof(m->reason), "No answer");
                } else if(m->resend_ms >= POD_CHAL_RESEND_MS) {
                    m->resend_ms = 0;
                    act = ActChallenge;
                    dst = m->peer_id;
                    bid = m->battle_id;
                    mylvl = m->my_level;
                }
                break;
            case BPhaseChallenged:
                m->timeout_ms += POD_TICK_MS;
                if(m->timeout_ms >= POD_CHALD_TIMEOUT_MS) {
                    m->phase = BPhaseCancelled;
                    snprintf(m->reason, sizeof(m->reason), "Timed out");
                    do_decline = true;
                    dst = m->peer_id;
                    bid = m->battle_id;
                }
                break;
            case BPhaseCountdown:
                m->elapsed_ms += POD_TICK_MS;
                if(m->elapsed_ms == 1000 || m->elapsed_ms == 2000) beep = true;
                if(m->elapsed_ms >= POD_COUNTDOWN_MS) {
                    m->phase = BPhaseActive;
                    m->elapsed_ms = 0;
                    m->my_taps = 0;
                    m->opp_taps = 0;
                    go = true;
                }
                break;
            case BPhaseActive:
                m->elapsed_ms += POD_TICK_MS;
                if(m->practice) {
                    uint32_t e = m->elapsed_ms > POD_ACTIVE_MS ? POD_ACTIVE_MS : m->elapsed_ms;
                    m->opp_taps = m->opp_target * e / POD_ACTIVE_MS;
                }
                if(m->elapsed_ms >= POD_ACTIVE_MS) {
                    if(m->practice) {
                        m->opp_taps = m->opp_target;
                        m->result = (m->my_taps > m->opp_taps) ? 1 :
                                    (m->my_taps < m->opp_taps) ? -1 :
                                                                 0;
                        m->phase = BPhaseResult;
                        act = ActFinish;
                    } else {
                        m->phase = BPhaseWaitResult;
                        m->timeout_ms = 0;
                        m->resend_ms = 0;
                        act = ActTaps;
                        dst = m->peer_id;
                        bid = m->battle_id;
                        taps = (uint16_t)m->my_taps;
                    }
                }
                break;
            case BPhaseWaitResult:
                m->timeout_ms += POD_TICK_MS;
                m->resend_ms += POD_TICK_MS;
                if(m->opp_taps_known) {
                    m->result = (m->my_taps > m->opp_taps) ? 1 :
                                (m->my_taps < m->opp_taps) ? -1 :
                                                             0;
                    m->phase = BPhaseResult;
                    act = ActFinish;
                } else if(m->timeout_ms >= POD_RESULT_TIMEOUT_MS) {
                    m->phase = BPhaseCancelled;
                    snprintf(m->reason, sizeof(m->reason), "Opponent left");
                } else if(m->resend_ms >= POD_TAPS_RESEND_MS) {
                    m->resend_ms = 0;
                    act = ActTaps;
                    dst = m->peer_id;
                    bid = m->battle_id;
                    taps = (uint16_t)m->my_taps;
                }
                break;
            case BPhaseResult:
            case BPhaseCancelled:
                m->anim++; // drive result animation
                break;
            }
        },
        true);

    if(beep) notification_message(app->notifications, &pod_seq_beep);
    if(go) notification_message(app->notifications, &pod_seq_go);
    switch(act) {
    case ActChallenge:
        pod_radio_send_challenge(
            app->radio, app->profile.profile_id, app->profile.name, mylvl, dst, bid);
        break;
    case ActTaps:
        pod_radio_send_taps(app->radio, app->profile.profile_id, dst, bid, taps);
        break;
    case ActFinish:
        pod_battle_finish(app);
        break;
    default:
        if(do_decline) {
            pod_radio_send_decline(app->radio, app->profile.profile_id, dst, bid);
            notification_message(app->notifications, &sequence_single_vibro);
        }
        break;
    }
}

static void pod_battle_finish(Pod* app) {
    bool practice = false;
    int8_t result = 0;
    uint8_t opp_level = 0;
    uint32_t peer_id = 0;
    with_view_model(
        app->battle_view,
        PodBattleModel * m,
        {
            practice = m->practice;
            result = m->result;
            opp_level = m->opp_level;
            peer_id = m->peer_id;
        },
        false);

    uint16_t xp = 0;
    if(!practice) {
        xp = (result > 0) ? (uint16_t)(15 + opp_level * 3) : (result == 0) ? 8 : 3;
        app->profile.xp += xp;
        if(result > 0)
            app->profile.wins++;
        else if(result < 0)
            app->profile.losses++;
        int idx = pod_encounters_find(&app->encounters, peer_id);
        if(idx >= 0) {
            PodEncounter* e = &app->encounters.items[idx];
            e->battled = 1;
            e->last_result = result;
            e->total_xp = (uint16_t)(e->total_xp + xp);
        }
        pod_profile_save(&app->profile);
        pod_encounters_save(&app->encounters);
        pod_encounters_menu_populate(app);
    }
    with_view_model(app->battle_view, PodBattleModel * m, { m->xp_gained = xp; }, true);

    const NotificationSequence* seq = (result > 0) ? &sequence_success :
                                      (result < 0) ? &sequence_error :
                                                     &sequence_single_vibro;
    notification_message(app->notifications, seq);
}

// --- Encounters list + detail + clear ---

static void pod_encounter_detail_populate(Pod* app) {
    PodEncounter* e = &app->encounters.items[app->selected_encounter];
    widget_reset(app->detail_widget);
    widget_add_string_element(
        app->detail_widget, 64, 2, AlignCenter, AlignTop, FontPrimary, e->name);

    char line[40];
    snprintf(line, sizeof(line), "Dolphin Lv %u", e->dolphin_level);
    widget_add_string_element(app->detail_widget, 4, 20, AlignLeft, AlignTop, FontSecondary, line);
    if(e->battled) {
        const char* r = (e->last_result > 0) ? "Last: You WON" :
                        (e->last_result < 0) ? "Last: You lost" :
                                               "Last: Draw";
        widget_add_string_element(
            app->detail_widget, 4, 32, AlignLeft, AlignTop, FontSecondary, r);
        snprintf(line, sizeof(line), "XP %u  Seen %ux", e->total_xp, e->times_seen);
    } else {
        widget_add_string_element(
            app->detail_widget, 4, 32, AlignLeft, AlignTop, FontSecondary, "Not battled yet");
        snprintf(line, sizeof(line), "Seen %u time(s)", e->times_seen);
    }
    widget_add_string_element(app->detail_widget, 4, 44, AlignLeft, AlignTop, FontSecondary, line);
    char when[24];
    pod_reltime(when, sizeof(when), e->last_seen, furi_hal_rtc_get_timestamp());
    widget_add_string_element(app->detail_widget, 4, 54, AlignLeft, AlignTop, FontSecondary, when);
}

static void pod_clear_button_cb(GuiButtonType type, InputType input, void* context) {
    if(input != InputTypeShort) return;
    Pod* app = context;
    if(type == GuiButtonTypeRight) { // Yes
        app->encounters.count = 0;
        pod_encounters_save(&app->encounters);
        pod_encounters_menu_populate(app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewEncounters);
}

static void pod_encounters_menu_cb(void* context, uint32_t index) {
    Pod* app = context;
    if(index == 0xFFFE) { // "Clear log"
        view_dispatcher_switch_to_view(app->view_dispatcher, PodViewClearConfirm);
        return;
    }
    if(index >= app->encounters.count) return;
    app->selected_encounter = (uint16_t)index;
    pod_encounter_detail_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewEncounterDetail);
}

static void pod_encounters_menu_populate(Pod* app) {
    submenu_reset(app->encounters_menu);
    char header[24];
    snprintf(header, sizeof(header), "Encounters (%u)", app->encounters.count);
    submenu_set_header(app->encounters_menu, header);
    if(app->encounters.count == 0) {
        submenu_add_item(
            app->encounters_menu, "None yet - Walk Mode", 0xFFFF, pod_encounters_menu_cb, app);
        return;
    }
    // newest-first: order indices by last_seen descending (selection sort, N<=64)
    uint16_t order[POD_MAX_ENCOUNTERS];
    for(uint16_t i = 0; i < app->encounters.count; i++)
        order[i] = i;
    for(uint16_t i = 0; i < app->encounters.count; i++) {
        uint16_t best = i;
        for(uint16_t j = i + 1; j < app->encounters.count; j++) {
            if(app->encounters.items[order[j]].last_seen >
               app->encounters.items[order[best]].last_seen)
                best = j;
        }
        uint16_t t = order[i];
        order[i] = order[best];
        order[best] = t;
    }
    for(uint16_t k = 0; k < app->encounters.count; k++) {
        PodEncounter* e = &app->encounters.items[order[k]];
        char label[40];
        char mark = !e->battled          ? '.' :
                    (e->last_result > 0) ? 'W' :
                    (e->last_result < 0) ? 'L' :
                                           '-';
        snprintf(label, sizeof(label), "%c %s", mark, e->name);
        submenu_add_item(app->encounters_menu, label, order[k], pod_encounters_menu_cb, app);
    }
    submenu_add_item(app->encounters_menu, "Clear log", 0xFFFE, pod_encounters_menu_cb, app);
}

// --- Profile ---

static void pod_profile_edit_button_cb(GuiButtonType type, InputType input, void* context) {
    if(type != GuiButtonTypeCenter || input != InputTypeShort) return;
    Pod* app = context;
    strncpy(app->name_edit, app->profile.name, sizeof(app->name_edit) - 1);
    app->name_edit[sizeof(app->name_edit) - 1] = '\0';
    text_input_set_header_text(app->text_input, "Enter callsign");
    text_input_set_result_callback(
        app->text_input, pod_name_input_done, app, app->name_edit, sizeof(app->name_edit), false);
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewTextInput);
}

static void pod_name_input_done(void* context) {
    Pod* app = context;
    strncpy(app->profile.name, app->name_edit, POD_NAME_MAX);
    app->profile.name[POD_NAME_MAX] = '\0';
    pod_profile_save(&app->profile);
    pod_profile_widget_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewProfile);
}

static void pod_profile_widget_populate(Pod* app) {
    widget_reset(app->profile_widget);
    widget_add_string_element(
        app->profile_widget, 64, 3, AlignCenter, AlignTop, FontPrimary, app->profile.name);

    char line[40];
    uint32_t games = app->profile.wins + app->profile.losses;
    uint32_t rate = games ? (app->profile.wins * 100 / games) : 0;
    snprintf(
        line,
        sizeof(line),
        "Dolphin Lv %u   XP %lu",
        pod_profile_dolphin_level(),
        (unsigned long)app->profile.xp);
    widget_add_string_element(
        app->profile_widget, 4, 20, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(line, sizeof(line), "Rank: %s", pod_rank(app->profile.xp));
    widget_add_string_element(
        app->profile_widget, 4, 32, AlignLeft, AlignTop, FontSecondary, line);
    snprintf(
        line,
        sizeof(line),
        "W %lu  L %lu  %lu%%",
        (unsigned long)app->profile.wins,
        (unsigned long)app->profile.losses,
        (unsigned long)rate);
    widget_add_string_element(
        app->profile_widget, 4, 44, AlignLeft, AlignTop, FontSecondary, line);
    widget_add_button_element(
        app->profile_widget, GuiButtonTypeCenter, "Edit", pod_profile_edit_button_cb, app);
}

// --- alloc / free ---

static void pod_build_static_views(Pod* app) {
    app->profile_widget = widget_alloc();
    pod_profile_widget_populate(app);
    view_set_previous_callback(widget_get_view(app->profile_widget), pod_prev_main_menu);

    app->text_input = text_input_alloc();
    text_input_set_minimum_length(app->text_input, 1);
    view_set_previous_callback(text_input_get_view(app->text_input), pod_prev_profile);

    app->inrange_menu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->inrange_menu), pod_prev_walk);

    app->encounters_menu = submenu_alloc();
    pod_encounters_menu_populate(app);
    view_set_previous_callback(submenu_get_view(app->encounters_menu), pod_prev_main_menu);

    app->detail_widget = widget_alloc();
    view_set_previous_callback(widget_get_view(app->detail_widget), pod_prev_encounters);

    app->clear_widget = widget_alloc();
    widget_add_string_element(
        app->clear_widget, 64, 16, AlignCenter, AlignTop, FontPrimary, "Clear log?");
    widget_add_string_element(
        app->clear_widget, 64, 34, AlignCenter, AlignTop, FontSecondary, "Deletes all encounters");
    widget_add_button_element(
        app->clear_widget, GuiButtonTypeLeft, "No", pod_clear_button_cb, app);
    widget_add_button_element(
        app->clear_widget, GuiButtonTypeRight, "Yes", pod_clear_button_cb, app);
    view_set_previous_callback(widget_get_view(app->clear_widget), pod_prev_encounters);

    app->about_widget = widget_alloc();
    widget_add_text_scroll_element(
        app->about_widget,
        0,
        0,
        128,
        64,
        "Pod\n"
        "Pod finds other Flippers\n"
        "nearby using the radio.\n \n"
        "WALK MODE broadcasts your\n"
        "dolphin and listens. Press\n"
        "OK to see who is IN RANGE\n"
        "and challenge them.\n \n"
        "A quick pass-by is just an\n"
        "ENCOUNTER. If you both stay\n"
        "near and both agree, you\n"
        "BATTLE - a 10 second tap\n"
        "race. Most taps wins XP.\n \n"
        "PRACTICE tries it solo.\n \n"
        "Works only while open. You\n"
        "broadcast on 433.92 MHz.");
    view_set_previous_callback(widget_get_view(app->about_widget), pod_prev_main_menu);
}

static Pod* pod_alloc(void) {
    Pod* app = malloc(sizeof(Pod));
    memset(app->in_range, 0, sizeof(app->in_range));
    app->in_battle = false;
    app->battle_return_view = PodViewMainMenu;

    pod_profile_load(&app->profile);
    pod_encounters_load(&app->encounters);
    app->selected_encounter = 0;
    app->radio = pod_radio_alloc();
    pod_radio_set_rx_callback(app->radio, pod_radio_on_msg, app);
    app->msg_queue = furi_message_queue_alloc(12, sizeof(PodMsg));
    app->beacon_timer = furi_timer_alloc(pod_beacon_timer_cb, FuriTimerTypePeriodic, app);
    app->anim_timer = furi_timer_alloc(pod_anim_timer_cb, FuriTimerTypePeriodic, app);
    app->battle_timer = furi_timer_alloc(pod_battle_timer_cb, FuriTimerTypePeriodic, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pod_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pod_custom_event_callback);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Pod");
    submenu_add_item(app->submenu, "Walk Mode", PodMenuWalkMode, pod_submenu_callback, app);
    submenu_add_item(app->submenu, "Encounters", PodMenuEncounters, pod_submenu_callback, app);
    submenu_add_item(app->submenu, "Practice", PodMenuPractice, pod_submenu_callback, app);
    submenu_add_item(app->submenu, "Profile", PodMenuProfile, pod_submenu_callback, app);
    submenu_add_item(app->submenu, "About", PodMenuAbout, pod_submenu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewMainMenu, submenu_get_view(app->submenu));

    app->walk_view = view_alloc();
    view_allocate_model(app->walk_view, ViewModelTypeLocking, sizeof(PodWalkModel));
    view_set_context(app->walk_view, app);
    view_set_draw_callback(app->walk_view, pod_walk_draw);
    view_set_input_callback(app->walk_view, pod_walk_input);
    view_set_previous_callback(app->walk_view, pod_prev_main_menu);
    view_set_enter_callback(app->walk_view, pod_walk_enter);
    view_set_exit_callback(app->walk_view, pod_walk_exit);
    view_dispatcher_add_view(app->view_dispatcher, PodViewWalkMode, app->walk_view);

    app->battle_view = view_alloc();
    view_allocate_model(app->battle_view, ViewModelTypeLocking, sizeof(PodBattleModel));
    view_set_context(app->battle_view, app);
    view_set_draw_callback(app->battle_view, pod_battle_draw);
    view_set_input_callback(app->battle_view, pod_battle_input);

    pod_build_static_views(app);
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewInRange, submenu_get_view(app->inrange_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewEncounters, submenu_get_view(app->encounters_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewEncounterDetail, widget_get_view(app->detail_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewClearConfirm, widget_get_view(app->clear_widget));
    view_dispatcher_add_view(app->view_dispatcher, PodViewBattle, app->battle_view);
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewProfile, widget_get_view(app->profile_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, PodViewAbout, widget_get_view(app->about_widget));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void pod_free(Pod* app) {
    view_dispatcher_remove_view(app->view_dispatcher, PodViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewWalkMode);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewInRange);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewEncounters);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewEncounterDetail);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewClearConfirm);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewBattle);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewProfile);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, PodViewAbout);

    submenu_free(app->submenu);
    view_free(app->walk_view);
    submenu_free(app->inrange_menu);
    submenu_free(app->encounters_menu);
    widget_free(app->detail_widget);
    widget_free(app->clear_widget);
    view_free(app->battle_view);
    widget_free(app->profile_widget);
    text_input_free(app->text_input);
    widget_free(app->about_widget);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    furi_timer_stop(app->beacon_timer);
    furi_timer_stop(app->anim_timer);
    furi_timer_stop(app->battle_timer);
    if(pod_radio_is_running(app->radio)) pod_radio_stop(app->radio);
    furi_timer_free(app->beacon_timer);
    furi_timer_free(app->anim_timer);
    furi_timer_free(app->battle_timer);
    furi_message_queue_free(app->msg_queue);
    pod_radio_free(app->radio);
    free(app);
}

int32_t pod_app(void* p) {
    UNUSED(p);
    Pod* app = pod_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, PodViewMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    pod_free(app);
    return 0;
}
