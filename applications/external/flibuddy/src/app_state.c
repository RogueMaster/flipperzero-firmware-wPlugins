#include "app_state.h"

#include <string.h>

/* The row count of Tag details, which the frame builds from the state:
 * a pure function, so the reducer stops ▼ at the last row. */
#include "ui_frame.h"

void app_state_init(AppState* s, IntentStore* intents, uint32_t first_scan, uint32_t first_intent) {
    memset(s, 0, sizeof(*s));
    s->screen = AppScreenWaiting;
    s->ble = AppBleOff;
    s->mode = WeightModeGross;
    app_settings_defaults(&s->settings);
    s->connection = app_settings_connection(&s->settings);
    s->via = TransportKindUsb;
    s->intents = intents;
    id_seq_init(&s->scan_ids, 's', first_scan);
    id_seq_init(&s->intent_ids, 'i', first_intent);
}

/* ---- timers ---- */

static void timer_start(AppState* s, uint32_t now_ms, uint32_t ms) {
    s->timer_start_ms = now_ms;
    s->timer_ms = ms;
}

static void timer_stop(AppState* s) {
    s->timer_ms = 0;
}

static bool timer_due(const AppState* s, uint32_t now_ms) {
    return s->timer_ms > 0 && (uint32_t)(now_ms - s->timer_start_ms) >= s->timer_ms;
}

uint32_t app_timer_left_ms(const AppState* s, uint32_t now_ms) {
    if(s->timer_ms == 0) return 0;
    uint32_t elapsed = now_ms - s->timer_start_ms;
    return elapsed >= s->timer_ms ? 0 : s->timer_ms - elapsed;
}

/* ---- screen changes ---- */

static void go(AppState* s, AppScreen screen) {
    s->screen = screen;
    timer_stop(s);
}

static void go_home(AppState* s, AppOutcome last) {
    go(s, s->connected ? AppScreenReady : AppScreenWaiting);
    s->last = last;
}

/* Back to Ready / Waiting from a result nobody answered: the spool may
 * still lie on the Flipper, and Ready invites the next one. */
static void go_home_reread(AppState* s) {
    go_home(s, AppOutcomeNone);
    s->forget_tag = true;
}

static void go_result(AppState* s, AppResultKind kind) {
    go(s, AppScreenResult);
    s->result_kind = kind;
}

static void go_error(AppState* s, AppError error, bool retryable) {
    go(s, AppScreenError);
    s->error = error;
    s->retryable = retryable;
    s->cue = AppCueError;
}

static void go_toast(AppState* s, AppToast toast, uint32_t now_ms) {
    go(s, AppScreenToast);
    s->toast = toast;
    if(toast != AppToastAdded) timer_start(s, now_ms, APP_TOAST_MS);
}

static void list_open(AppState* s, AppScreen screen) {
    go(s, screen);
    s->sel = 0;
    s->top = 0;
}

/* ▲▼ on a list of n rows: wraps, keeps the selection among the visible rows. */
static void list_move(AppState* s, AppKey key, uint8_t n) {
    if(n == 0) return;
    s->sel = (uint8_t)(key == AppKeyUp ? (s->sel + n - 1) % n : (s->sel + 1) % n);
    if(s->sel < s->top) s->top = s->sel;
    if(s->sel >= s->top + APP_LIST_ROWS) s->top = (uint8_t)(s->sel - APP_LIST_ROWS + 1);
}

/* ---- the spool on screen, weight ---- */

static void take_spool(AppState* s, const ProtoSpool* spool) {
    s->spool = *spool;
    s->has_spool = true;
    s->result_kind = AppResultKnown;
}

static uint16_t cap_grams(unsigned grams) {
    return (uint16_t)(grams > PROTO_GRAMS_MAX ? PROTO_GRAMS_MAX : grams);
}

uint16_t app_weight_core(const AppState* s) {
    if(s->has_spool) return s->spool.core;
    return s->lookup.defaults.has_core ? s->lookup.defaults.core : 0;
}

/* See the header: the spool first, then the lookup defaults. */
static uint16_t weight_prefill(const AppState* s) {
    unsigned core = app_weight_core(s);
    if(s->has_spool) {
        unsigned net = s->spool.remaining;
        return cap_grams(s->mode == WeightModeNet ? net : net + core);
    }
    const ProtoDefaults* d = &s->lookup.defaults;
    bool has_label = d->has_label || s->lookup.tag_info.has_label;
    unsigned label = d->has_label ? d->label : s->lookup.tag_info.label;
    if(s->mode == WeightModeNet) {
        if(d->has_gross) return cap_grams(d->gross > core ? d->gross - core : 0);
        return has_label ? cap_grams(label) : 0;
    }
    if(d->has_gross) return cap_grams(d->gross);
    return has_label ? cap_grams(label + core) : 0;
}

static void weight_set(AppState* s, unsigned grams) {
    for(int i = APP_WEIGHT_DIGITS - 1; i >= 0; i--) {
        s->weight.digit[i] = (uint8_t)(grams % 10);
        grams /= 10;
    }
}

static void weight_fill(AppState* s) {
    weight_set(s, weight_prefill(s));
}

uint16_t app_weight_grams(const AppState* s) {
    unsigned grams = 0;
    for(int i = 0; i < APP_WEIGHT_DIGITS; i++)
        grams = grams * 10 + s->weight.digit[i];
    return (uint16_t)grams;
}

bool app_weight_below_core(const AppState* s) {
    return s->mode == WeightModeGross && app_weight_grams(s) < app_weight_core(s);
}

static void open_weight(AppState* s) {
    go(s, AppScreenWeight);
    s->mode = app_settings_weigh_as(&s->settings) == AppWeighAsNet ? WeightModeNet :
                                                                     WeightModeGross;
    s->weight.cursor = 2;
    s->weight.touched = false;
    weight_fill(s);
}

/* ---- Recent ---- */

static int recent_find(const AppState* s, const TagDump* tag) {
    for(uint8_t i = 0; i < s->recent_count; i++) {
        const TagDump* t = &s->recent[i].scan.tag;
        if(t->uid_len == tag->uid_len && memcmp(t->uid, tag->uid, tag->uid_len) == 0) return i;
    }
    return -1;
}

static void recent_remove(AppState* s, int i) {
    if(i < 0) return;
    s->recent_count--;
    memmove(
        &s->recent[i], &s->recent[i + 1], (size_t)(s->recent_count - i) * sizeof(s->recent[0]));
}

/* The spool on screen was weighed: it goes on top, once. */
static void recent_push(AppState* s) {
    if(!s->has_spool) return;
    int i = recent_find(s, &s->scan.tag);
    if(i >= 0) {
        recent_remove(s, i);
    } else if(s->recent_count == APP_RECENT_MAX) {
        s->recent_count--; /* the oldest goes */
    }
    memmove(&s->recent[1], &s->recent[0], s->recent_count * sizeof(s->recent[0]));
    s->recent[0].scan = s->scan;
    s->recent[0].spool = s->spool;
    s->recent_count++;
}

/* Newer facts about a listed tag replace the entry's, in place. */
static void recent_refresh(AppState* s) {
    int i = recent_find(s, &s->scan.tag);
    if(i < 0 || !s->has_spool) return;
    s->recent[i].scan = s->scan;
    s->recent[i].spool = s->spool;
}

static void recent_open(AppState* s, uint8_t i) {
    if(i >= s->recent_count) return;
    s->scan = s->recent[i].scan;
    memset(&s->lookup, 0, sizeof(s->lookup));
    memset(&s->result, 0, sizeof(s->result));
    take_spool(s, &s->recent[i].spool);
    s->last = AppOutcomeNone;
    go_result(s, AppResultKnown);
}

/* ---- intents ---- */

static AppEffect send_held(AppState* s, uint32_t now_ms) {
    if(!s->connected || !intent_store_put(s->intents, &s->held)) return AppEffectNone;
    memset(&s->result, 0, sizeof(s->result));
    bool on_host = s->held.action == IntentActionLinkOnHost ||
                   s->held.action == IntentActionCreateOnHost;
    go(s, on_host ? AppScreenLinkOnHost : AppScreenBusy);
    timer_start(s, now_ms, APP_RESULT_TIMEOUT_MS);
    return AppEffectSendIntent;
}

/* Builds `held` for the scan on screen; false (nothing changed) when the
 * intent cannot be built. */
static bool hold(AppState* s, IntentAction action) {
    if(!s->connected) return false;
    s->weigh_after_full = false;
    return intent_new_action(&s->held, &s->intent_ids, &s->scan, action);
}

static AppEffect act(AppState* s, IntentAction action, uint32_t now_ms) {
    return hold(s, action) ? send_held(s, now_ms) : AppEffectNone;
}

static void mark(AppState* s, IntentAction action, uint32_t now_ms) {
    if(!hold(s, action)) return;
    go_toast(s, action == IntentActionFull ? AppToastMarkedNew : AppToastMarkedEmpty, now_ms);
    s->cue = AppCueSuccess;
}

static AppEffect commit(AppState* s, bool weigh_after, uint32_t now_ms) {
    AppEffect effect = send_held(s, now_ms);
    s->weigh_after_full = weigh_after && effect == AppEffectSendIntent;
    return effect;
}

static AppEffect save_weight(AppState* s, uint32_t now_ms) {
    if(!s->connected) return AppEffectNone;
    if(app_weight_below_core(s)) {
        s->cue = AppCueError;
        return AppEffectNone;
    }
    s->weigh_after_full = false;
    if(!intent_new_weigh(&s->held, &s->intent_ids, &s->scan, app_weight_grams(s), s->mode)) {
        return AppEffectNone;
    }
    return send_held(s, now_ms);
}

size_t app_encode_scan(const AppState* s, char* buf, size_t cap) {
    return proto_encode_scan(
        &s->scan.tag, s->scan.id, app_settings_autosave(&s->settings), buf, cap);
}

size_t app_encode_intent(const AppState* s, char* buf, size_t cap) {
    const Intent* intent = intent_store_current(s->intents);
    return intent ? intent_encode(intent, buf, cap) : 0;
}

/* ---- inbound ---- */

/* A failed read shows for a moment, then the reader is back on Ready. */
static void read_failed(AppState* s, uint32_t now_ms) {
    go_error(s, AppErrorReadFailed, false);
    timer_start(s, now_ms, APP_READ_RETRY_MS);
}

static void tag_unsupported(AppState* s, uint32_t now_ms) {
    read_failed(s, now_ms);
    s->error = AppErrorUnsupportedTag;
    s->cue = AppCueNone;
}

static AppEffect on_tag_read(AppState* s, const TagDump* tag, uint32_t now_ms) {
    memset(&s->lookup, 0, sizeof(s->lookup));
    memset(&s->result, 0, sizeof(s->result));
    s->has_spool = false;
    s->last = AppOutcomeNone;
    s->scan.tag = *tag;
    if(!id_seq_next(&s->scan_ids, s->scan.id)) {
        read_failed(s, now_ms);
        return AppEffectNone;
    }
    s->cue = AppCueRead;
    if(!s->connected) {
        go_result(s, AppResultNoHost);
        return AppEffectNone;
    }
    go(s, AppScreenLookingUp);
    return AppEffectSendScan;
}

static AppResultKind unknown_kind(const ProtoLookup* lookup) {
    if(lookup->prefill) return AppResultUnknownPrefill;
    return lookup->has_tag_info ? AppResultUnknownBare : AppResultBlank;
}

static void on_lookup(AppState* s, const ProtoLookup* lookup, uint32_t now_ms) {
    if(strcmp(lookup->scan_id, s->scan.id) != 0) return;
    bool fresh = s->screen != AppScreenLookingUp; /* replaces a result */
    if(fresh && lookup->status == ProtoLookupError) return;
    s->lookup = *lookup;
    s->has_spool = false;
    switch(lookup->status) {
    case ProtoLookupFound:
        if(lookup->has_spool) take_spool(s, &lookup->spool);
        recent_refresh(s);
        if(lookup->created) {
            s->result_kind = AppResultKnown;
            go_toast(s, AppToastAdded, now_ms);
            s->cue = AppCueSuccess;
            return;
        }
        if(!fresh && app_settings_after_scan(&s->settings) == AppAfterScanWeigh) {
            s->result_kind = AppResultKnown; /* what Back shows */
            open_weight(s);
            return;
        }
        go_result(s, AppResultKnown);
        break;
    case ProtoLookupUnknown:
        go_result(s, unknown_kind(lookup));
        break;
    case ProtoLookupError:
        go_error(s, AppErrorLookup, false);
        return;
    }
    if(fresh) s->cue = AppCueSuccess;
}

static bool accepts_lookup(AppScreen screen) {
    switch(screen) {
    case AppScreenLookingUp:
    case AppScreenResult:
    case AppScreenMenu:
    case AppScreenDetails:
    case AppScreenLinkOnHost:
        return true;
    default:
        return false;
    }
}

static void on_result_ok(AppState* s, const ProtoResult* result, uint32_t now_ms) {
    if(result->has_spool) take_spool(s, &result->spool);
    if(s->held.action == IntentActionWeigh) {
        recent_push(s);
    } else if(s->held.action == IntentActionDelete) {
        recent_remove(s, recent_find(s, &s->scan.tag));
    } else {
        recent_refresh(s);
    }
    switch(s->held.action) {
    case IntentActionWeigh:
        go(s, AppScreenSaved);
        timer_start(s, now_ms, APP_SAVED_MS);
        s->cue = AppCueSuccess;
        break;
    case IntentActionNew:
        s->result_kind = AppResultKnown;
        go_toast(s, AppToastAdded, now_ms);
        s->cue = AppCueSuccess;
        break;
    case IntentActionFull:
        if(s->weigh_after_full) {
            s->weigh_after_full = false;
            open_weight(s);
        } else {
            go_home(s, AppOutcomeMarkedNew);
        }
        break;
    case IntentActionEmpty:
        go_home(s, AppOutcomeMarkedEmpty);
        break;
    case IntentActionDelete:
        go_toast(s, AppToastDeleted, now_ms);
        s->cue = AppCueSuccess;
        break;
    case IntentActionLinkOnHost:
    case IntentActionCreateOnHost:
        timer_stop(s); /* the host took it: wait for its fresh lookup */
        break;
    }
}

static void on_result(AppState* s, const ProtoResult* result, uint32_t now_ms) {
    if(!intent_store_resolve(s->intents, result->id)) return;
    s->result = *result;
    if(result->ok) {
        on_result_ok(s, result, now_ms);
    } else {
        go_error(s, AppErrorHost, true);
    }
}

static bool waits_for_result(const AppState* s) {
    return (s->screen == AppScreenBusy || s->screen == AppScreenLinkOnHost) &&
           intent_store_current(s->intents) != NULL;
}

static void on_host_lost(AppState* s) {
    s->connected = false;
    switch(s->screen) {
    case AppScreenReady:
    case AppScreenRecent:
        go_home(s, s->last);
        break;
    case AppScreenLookingUp:
    case AppScreenResult:
    case AppScreenMenu:
    case AppScreenDetails:
    case AppScreenConfirmDelete:
    case AppScreenWeight:
    case AppScreenLinkOnHost:
        go_result(s, AppResultNoHost);
        break;
    case AppScreenToast:
        /* an uncommitted mark cannot be sent: nothing leaves the Flipper */
        if(s->toast == AppToastMarkedNew || s->toast == AppToastMarkedEmpty) {
            go_result(s, AppResultNoHost);
        }
        break;
    case AppScreenBusy:
        go_error(s, AppErrorHostLost, true);
        break;
    default:
        break;
    }
}

static void on_tick(AppState* s, uint32_t now_ms, AppEffect* effect) {
    if(!timer_due(s, now_ms)) return;
    switch(s->screen) {
    case AppScreenBusy:
    case AppScreenLinkOnHost:
        go_error(s, AppErrorTimeout, true);
        break;
    case AppScreenSaved:
        go_home(s, AppOutcomeSaved);
        break;
    case AppScreenError: /* only a failed read runs a timer here */
        go_home(s, AppOutcomeNone);
        break;
    case AppScreenToast:
        if(s->toast == AppToastDeleted) {
            go_home(s, AppOutcomeDeleted);
        } else {
            *effect = commit(s, false, now_ms);
        }
        break;
    default:
        timer_stop(s);
        break;
    }
}

/* ---- keys per screen ---- */

static AppEffect key_home(AppState* s, AppKey key) {
    if(key == AppKeyBack) return AppEffectExit;
    if(key == AppKeyLeft && s->screen == AppScreenReady) list_open(s, AppScreenRecent);
    if(key == AppKeyOk && s->screen == AppScreenWaiting) go(s, AppScreenGetApp);
    if(key == AppKeyRight) {
        s->settings_entered = s->settings;
        list_open(s, AppScreenSettings);
    }
    return AppEffectNone;
}

static AppEffect key_result(AppState* s, AppKey key, uint32_t now_ms) {
    if(key == AppKeyBack) {
        if(s->result_kind == AppResultNoHost) {
            go_home_reread(s);
        } else {
            go_home(s, AppOutcomeNone);
        }
        return AppEffectNone;
    }
    if(key == AppKeyLeft) {
        list_open(s, AppScreenMenu);
        return AppEffectNone;
    }
    switch(s->result_kind) {
    case AppResultKnown:
        if(key == AppKeyOk && s->connected) open_weight(s);
        if(key == AppKeyRight) mark(s, IntentActionFull, now_ms); /* New */
        break;
    case AppResultUnknownPrefill:
        if(key == AppKeyOk) return act(s, IntentActionNew, now_ms);
        if(key == AppKeyRight) return act(s, IntentActionLinkOnHost, now_ms);
        break;
    case AppResultUnknownBare:
        if(key == AppKeyOk) return act(s, IntentActionCreateOnHost, now_ms);
        if(key == AppKeyRight) return act(s, IntentActionLinkOnHost, now_ms);
        break;
    case AppResultBlank:
        if(key == AppKeyOk) return act(s, IntentActionLinkOnHost, now_ms);
        break;
    case AppResultNoHost:
        break;
    }
    return AppEffectNone;
}

uint8_t app_menu_items(const AppState* s, AppMenuItem out[APP_MENU_MAX]) {
    uint8_t n = 0;
    switch(s->result_kind) {
    case AppResultKnown:
        out[n++] = AppMenuWeigh;
        out[n++] = AppMenuMarkNew;
        out[n++] = AppMenuMarkEmpty;
        out[n++] = AppMenuRelink;
        out[n++] = AppMenuDetails;
        out[n++] = AppMenuDelete;
        break;
    case AppResultUnknownPrefill:
        out[n++] = AppMenuAddNew;
        out[n++] = AppMenuAddAndWeigh;
        out[n++] = AppMenuLinkOnHost;
        out[n++] = AppMenuDetails;
        break;
    case AppResultUnknownBare:
        out[n++] = AppMenuCreateOnHost;
        out[n++] = AppMenuLinkOnHost;
        out[n++] = AppMenuDetails;
        break;
    case AppResultBlank:
        out[n++] = AppMenuLinkOnHost;
        out[n++] = AppMenuDetails;
        break;
    case AppResultNoHost:
        out[n++] = AppMenuDetails;
        break;
    }
    return n;
}

static AppEffect menu_choose(AppState* s, AppMenuItem item, uint32_t now_ms) {
    switch(item) {
    case AppMenuWeigh:
    case AppMenuAddAndWeigh:
        if(s->connected) open_weight(s);
        break;
    case AppMenuMarkNew:
        mark(s, IntentActionFull, now_ms);
        break;
    case AppMenuMarkEmpty:
        mark(s, IntentActionEmpty, now_ms);
        break;
    case AppMenuRelink:
    case AppMenuLinkOnHost:
        return act(s, IntentActionLinkOnHost, now_ms);
    case AppMenuDetails:
        go(s, AppScreenDetails);
        s->details_top = 0;
        break;
    case AppMenuDelete:
        go(s, AppScreenConfirmDelete);
        break;
    case AppMenuAddNew:
        return act(s, IntentActionNew, now_ms);
    case AppMenuCreateOnHost:
        return act(s, IntentActionCreateOnHost, now_ms);
    }
    return AppEffectNone;
}

static AppEffect key_menu(AppState* s, AppKey key, uint32_t now_ms) {
    AppMenuItem items[APP_MENU_MAX];
    uint8_t n = app_menu_items(s, items);
    switch(key) {
    case AppKeyUp:
    case AppKeyDown:
        list_move(s, key, n);
        break;
    case AppKeyBack:
        go(s, AppScreenResult);
        break;
    case AppKeyOk:
        if(s->sel < n) return menu_choose(s, items[s->sel], now_ms);
        break;
    default:
        break;
    }
    return AppEffectNone;
}

static AppEffect key_confirm_delete(AppState* s, AppKey key, uint32_t now_ms) {
    if(key == AppKeyLeft || key == AppKeyBack) go(s, AppScreenMenu);
    if(key == AppKeyRight) return act(s, IntentActionDelete, now_ms);
    return AppEffectNone;
}

static void weight_turn(AppState* s, AppKey key) {
    AppWeight* w = &s->weight;
    if(w->cursor == 0) {
        /* The number on screen is converted, typed or not: gross - core
         * is the net of the same spool, net + core its gross. */
        unsigned grams = app_weight_grams(s), core = app_weight_core(s);
        if(s->mode == WeightModeGross) {
            s->mode = WeightModeNet;
            weight_set(s, grams > core ? grams - core : 0);
        } else {
            s->mode = WeightModeGross;
            weight_set(s, cap_grams(grams + core));
        }
        return;
    }
    uint8_t* d = &w->digit[w->cursor - 1];
    *d = (uint8_t)(key == AppKeyUp ? (*d + 1) % 10 : (*d + 9) % 10);
    w->touched = true;
}

static AppEffect key_weight(AppState* s, AppKey key, uint32_t now_ms) {
    AppWeight* w = &s->weight;
    switch(key) {
    case AppKeyLeft:
        w->cursor = (uint8_t)(w->cursor == 0 ? APP_WEIGHT_DIGITS : w->cursor - 1);
        break;
    case AppKeyRight:
        w->cursor = (uint8_t)(w->cursor == APP_WEIGHT_DIGITS ? 0 : w->cursor + 1);
        break;
    case AppKeyUp:
    case AppKeyDown:
        weight_turn(s, key);
        break;
    case AppKeyOk:
        return save_weight(s, now_ms);
    case AppKeyBack:
        go(s, AppScreenResult);
        break;
    }
    return AppEffectNone;
}

static AppEffect key_toast(AppState* s, AppKey key, uint32_t now_ms) {
    switch(s->toast) {
    case AppToastAdded:
        if(key == AppKeyOk && s->connected) open_weight(s);
        if(key == AppKeyRight) go_home(s, AppOutcomeAdded);
        if(key == AppKeyBack) go_home(s, AppOutcomeNone);
        break;
    case AppToastMarkedNew:
    case AppToastMarkedEmpty:
        if(key == AppKeyLeft) go(s, AppScreenResult); /* Undo: nothing was sent */
        if(key == AppKeyOk) return commit(s, s->toast == AppToastMarkedNew, now_ms);
        if(key == AppKeyBack) return commit(s, false, now_ms);
        break;
    case AppToastDeleted:
        if(key == AppKeyBack) go_home(s, AppOutcomeNone);
        break;
    }
    return AppEffectNone;
}

static AppEffect key_error(AppState* s, AppKey key, uint32_t now_ms) {
    bool intent_error = s->retryable && s->connected;
    if(key == AppKeyRight && intent_error) return send_held(s, now_ms);
    if(key != AppKeyLeft && key != AppKeyBack && key != AppKeyRight) return AppEffectNone;
    if(intent_error) {
        go(s, AppScreenResult);
    } else if(s->error == AppErrorLookup) {
        go_home_reread(s);
    } else {
        go_home(s, AppOutcomeNone);
    }
    return AppEffectNone;
}

static void setting_change(AppState* s, AppKey key) {
    AppSettings* st = &s->settings;
    switch((AppSetting)s->sel) {
    case AppSettingConnection: {
        /* in the design's order: Auto, USB, Bluetooth */
        static const AppConnection order[] = {
            AppConnectionAuto, AppConnectionUsb, AppConnectionBluetooth};
        unsigned i = 0;
        while(i < 3 && order[i] != app_settings_connection(st))
            i++;
        i = key == AppKeyRight ? (i + 1) % 3 : (i + 2) % 3;
        app_settings_set_connection(st, order[i]);
        break;
    }
    case AppSettingAutosave:
        app_settings_set_autosave(st, !app_settings_autosave(st));
        break;
    case AppSettingAfterScan:
        app_settings_set_after_scan(
            st,
            app_settings_after_scan(st) == AppAfterScanShow ? AppAfterScanWeigh :
                                                              AppAfterScanShow);
        break;
    case AppSettingWeighAs:
        app_settings_set_weigh_as(
            st, app_settings_weigh_as(st) == AppWeighAsGross ? AppWeighAsNet : AppWeighAsGross);
        break;
    case AppSettingFeedback: {
        unsigned f = app_settings_feedback(st);
        f = key == AppKeyRight ? (f + 1) % 3 : (f + 2) % 3;
        app_settings_set_feedback(st, (AppFeedback)f);
        break;
    }
    case AppSettingSound:
        app_settings_set_sound(st, !app_settings_sound(st));
        break;
    case AppSettingVersion:
    case AppSettingCount:
        break;
    }
}

static AppEffect leave_settings(AppState* s) {
    go_home(s, s->last);
    AppConnection chosen = app_settings_connection(&s->settings);
    bool retry = false;
    for(unsigned k = 0; k < TRANSPORT_KINDS; k++)
        retry |= s->transport_failed[k] && app_connection_uses(chosen, (TransportKind)k);
    if(chosen != s->connection || retry) {
        bool ble_stays = app_connection_uses(s->connection, TransportKindBluetooth) &&
                         app_connection_uses(chosen, TransportKindBluetooth) &&
                         !s->transport_failed[TransportKindBluetooth];
        s->connection = chosen;
        memset(s->transport_failed, 0, sizeof(s->transport_failed));
        if(!ble_stays) s->ble = AppBleOff;
        return AppEffectSwitchTransport;
    }
    if(memcmp(&s->settings, &s->settings_entered, sizeof(s->settings)) != 0) {
        return AppEffectSaveSettings;
    }
    return AppEffectNone;
}

static AppEffect key_settings(AppState* s, AppKey key) {
    switch(key) {
    case AppKeyUp:
    case AppKeyDown:
        list_move(s, key, AppSettingCount);
        break;
    case AppKeyLeft:
    case AppKeyRight:
        setting_change(s, key);
        break;
    case AppKeyOk:
        if(s->sel == AppSettingVersion) go(s, AppScreenAbout);
        break;
    case AppKeyBack:
        return leave_settings(s);
    }
    return AppEffectNone;
}

static AppEffect on_key(AppState* s, AppKey key, uint32_t now_ms) {
    switch(s->screen) {
    case AppScreenWaiting:
    case AppScreenReady:
        return key_home(s, key);
    case AppScreenReading:
    case AppScreenLookingUp:
        if(key == AppKeyBack) go_home(s, AppOutcomeNone);
        break;
    case AppScreenResult:
        return key_result(s, key, now_ms);
    case AppScreenMenu:
        return key_menu(s, key, now_ms);
    case AppScreenDetails:
        if(key == AppKeyOk || key == AppKeyBack) go(s, AppScreenMenu);
        if(key == AppKeyUp && s->details_top > 0) s->details_top--;
        if(key == AppKeyDown && s->details_top + APP_DETAILS_SHOWN < ui_details_count(s)) {
            s->details_top++;
        }
        break;
    case AppScreenConfirmDelete:
        return key_confirm_delete(s, key, now_ms);
    case AppScreenWeight:
        return key_weight(s, key, now_ms);
    case AppScreenBusy:
        break; /* the intent cannot be called back */
    case AppScreenSaved:
        if(key == AppKeyOk || key == AppKeyBack) go_home(s, AppOutcomeSaved);
        break;
    case AppScreenToast:
        return key_toast(s, key, now_ms);
    case AppScreenLinkOnHost:
        if(key == AppKeyLeft || key == AppKeyBack) go(s, AppScreenResult);
        break;
    case AppScreenError:
        return key_error(s, key, now_ms);
    case AppScreenSettings:
        return key_settings(s, key);
    case AppScreenAbout:
        if(key == AppKeyOk || key == AppKeyBack) go(s, AppScreenSettings);
        break;
    case AppScreenRecent:
        if(key == AppKeyUp || key == AppKeyDown) list_move(s, key, s->recent_count);
        if(key == AppKeyOk) recent_open(s, s->sel);
        if(key == AppKeyBack) go_home(s, s->last);
        break;
    case AppScreenGetApp:
        if(key == AppKeyLeft || key == AppKeyBack) go_home(s, s->last);
        break;
    }
    return AppEffectNone;
}

bool app_reader_wanted(const AppState* s) {
    switch(s->screen) {
    case AppScreenWaiting:
    case AppScreenGetApp:
    case AppScreenReady:
    case AppScreenReading:
    case AppScreenResult:
    case AppScreenSaved:
    case AppScreenToast:
        return true;
    default:
        return false;
    }
}

static AppEffect on_tag_detected(AppState* s, uint32_t now_ms) {
    if(!app_reader_wanted(s) || s->screen == AppScreenReading) return AppEffectNone;
    if(s->screen == AppScreenToast &&
       (s->toast == AppToastMarkedNew || s->toast == AppToastMarkedEmpty)) {
        return commit(s, false, now_ms);
    }
    go(s, AppScreenReading);
    return AppEffectNone;
}

AppEffect app_reduce(AppState* s, const AppEvent* e) {
    AppEffect effect = AppEffectNone;
    s->cue = AppCueNone;
    switch(e->type) {
    case AppEventHostConnected:
        s->connected = true;
        s->via = e->via;
        memset(s->other_version, 0, sizeof(s->other_version));
        s->cue = AppCueConnected;
        if(s->screen == AppScreenWaiting || s->screen == AppScreenGetApp) {
            go_home(s, AppOutcomeNone);
        }
        break;
    case AppEventHostLost:
        on_host_lost(s);
        break;
    case AppEventTagDetected:
        effect = on_tag_detected(s, e->now_ms);
        break;
    case AppEventTagRead:
        if(s->screen == AppScreenReading) effect = on_tag_read(s, e->tag, e->now_ms);
        break;
    case AppEventReadFailed:
        if(s->screen == AppScreenReading) read_failed(s, e->now_ms);
        break;
    case AppEventTagUnsupported:
        if(s->screen == AppScreenReading) tag_unsupported(s, e->now_ms);
        break;
    case AppEventLookupReceived:
        if(accepts_lookup(s->screen)) on_lookup(s, e->lookup, e->now_ms);
        break;
    case AppEventResultReceived:
        if(waits_for_result(s)) on_result(s, e->result, e->now_ms);
        break;
    case AppEventSendFailed:
        if(s->screen == AppScreenLookingUp) {
            go_result(s, AppResultNoHost);
        } else if(waits_for_result(s)) {
            go_error(s, AppErrorSendFailed, true);
        }
        break;
    case AppEventKey:
        effect = on_key(s, e->key, e->now_ms);
        break;
    case AppEventTick:
        on_tick(s, e->now_ms, &effect);
        break;
    case AppEventBleStatus:
        s->ble = e->ble;
        break;
    case AppEventTransportFailed:
        if(e->via < TRANSPORT_KINDS) s->transport_failed[e->via] = true;
        break;
    case AppEventOtherVersion:
        if(e->via < TRANSPORT_KINDS) s->other_version[e->via] = s->connected ? 0 : e->version;
        break;
    case AppEventStart:
        break; /* the caller starts the reader and draws after any event */
    }
    return effect;
}
