#include "ui_frame.h"

#include <string.h>

#include "nfc_classify.h"
#include "tag_facts.h"
#include "ui_text.h"

_Static_assert(APP_MENU_MAX <= UI_LIST_MAX, "every Menu row needs a slot");
_Static_assert(AppSettingCount <= UI_LIST_MAX, "every Settings row needs a slot");
_Static_assert(APP_RECENT_MAX <= UI_LIST_MAX, "every Recent row needs a slot");
_Static_assert(APP_DETAILS_SHOWN <= UI_LINES, "every visible Tag details row needs a line");

/* Append-only text into a fixed buffer; stops quietly when full. */
typedef struct {
    char* out;
    size_t cap;
    size_t len;
} Text;

#define TEXT(buf) text_start((buf), sizeof(buf))

static Text text_start(char* out, size_t cap) {
    out[0] = '\0';
    return (Text){.out = out, .cap = cap, .len = 0};
}

static void put(Text* t, const char* s) {
    while(*s && t->len + 1 < t->cap) {
        t->out[t->len++] = *s++;
        t->out[t->len] = '\0';
    }
}

static void put_uint(Text* t, uint32_t n) {
    char digits[11];
    size_t i = sizeof(digits) - 1;
    digits[i] = '\0';
    do {
        digits[--i] = (char)('0' + n % 10);
        n /= 10;
    } while(n > 0);
    put(t, digits + i);
}

static void put_hex(Text* t, uint32_t n, unsigned digits) {
    static const char hex[] = "0123456789ABCDEF";
    char out[9];
    for(unsigned i = 0; i < digits; i++)
        out[i] = hex[(n >> (4 * (digits - 1 - i))) & 0x0F];
    out[digits] = '\0';
    put(t, out);
}

static void set(char* out, size_t cap, const char* s) {
    Text t = text_start(out, cap);
    put(&t, s);
}

#define SET(buf, s) set((buf), sizeof(buf), (s))

/* ---- facts about the tag and spool on screen ---- */

/* Result · no host (no app, or it went away) shows only what the tag
 * itself says; every other result only what the host sent. */
static bool own_facts(const AppState* s) {
    return s->result_kind == AppResultNoHost;
}

static bool shows_spool(const AppState* s) {
    return s->has_spool && !own_facts(s);
}

/* What is known about a tag that is not a Bambuddy spool: the tag's own
 * facts (tag_facts.h, read into `own`) without the host, else the host's
 * `tag_info`. NULL when it says nothing. */
static const ProtoTagInfo* tag_info(const AppState* s, TagFacts* own) {
    if(own_facts(s)) return tag_facts_read(&s->scan.tag, own) ? &own->info : NULL;
    if(s->has_spool || !s->lookup.has_tag_info) return NULL;
    return &s->lookup.tag_info;
}

static bool rgba_of(const AppState* s, const ProtoTagInfo* info, uint32_t* rgba) {
    if(shows_spool(s)) {
        *rgba = s->spool.rgba;
        return s->spool.has_rgba;
    }
    *rgba = info ? info->rgba : 0;
    return info && info->has_rgba;
}

/* Line `i` the host sent for a found spool it gave no `spool` for (the
 * protocol's fallback), else NULL. The lines are shown verbatim in order,
 * with no meaning given to a position: the host drops the empty ones. */
static const char* host_line(const AppState* s, uint8_t i) {
    if(s->has_spool || s->lookup.status != ProtoLookupFound) return NULL;
    return i < s->lookup.lines.count && i < PROTO_LINES_MAX ? s->lookup.lines.line[i] : NULL;
}

/* What the tag itself says: the detailed type, else the material. */
static bool local_name(const AppState* s, char out[TAG_BLOCK_TEXT_MAX]) {
    return tag_block_text(&s->scan.tag, TAG_BLOCK_DETAILED_TYPE, out) ||
           tag_block_text(&s->scan.tag, TAG_BLOCK_MATERIAL, out);
}

/* The spool's name from Bambuddy, the host's reading of the tag or the tag
 * itself; "Spool" when none says. The host's `lines` are never a name: the
 * host drops empty ones, so what comes first is not always the name. */
static void put_name(Text* t, const AppState* s) {
    char name[UI_TEXT_MAX];
    name[0] = '\0';
    if(s->has_spool) {
        ui_spool_name(s->spool.material, s->spool.subtype, name, sizeof(name));
    } else if(s->lookup.has_tag_info) {
        ui_spool_name(s->lookup.tag_info.material, s->lookup.tag_info.subtype, name, sizeof(name));
    }
    if(name[0] == '\0') local_name(s, name);
    put(t, name[0] ? name : "Spool");
}

/* "PLA Basic Jade White" */
static void put_name_and_colour(Text* t, const AppState* s) {
    put_name(t, s);
    if(s->has_spool && s->spool.color_name[0]) {
        put(t, " ");
        put(t, s->spool.color_name);
    }
}

static void put_colour(Text* t, const AppState* s, const ProtoTagInfo* info) {
    uint32_t rgba;
    if(shows_spool(s) && s->spool.color_name[0]) {
        put(t, s->spool.color_name);
    } else if(info && info->color_name[0]) {
        put(t, info->color_name);
    } else if(rgba_of(s, info, &rgba)) {
        put(t, "Color #");
        put_hex(t, rgba >> 8, 6);
    } else {
        put(t, ui_tag_type_name(&s->scan.tag));
    }
}

static void result_title(const AppState* s, char* out, size_t cap) {
    Text t = text_start(out, cap);
    switch(s->result_kind) {
    case AppResultKnown:
        if(host_line(s, 0)) {
            put(&t, host_line(s, 0));
            break;
        }
        put_name(&t, s);
        break;
    case AppResultUnknownPrefill:
        put_name(&t, s);
        break;
    case AppResultUnknownBare:
        put(&t, "Spool tag");
        break;
    case AppResultBlank:
        put(&t, "Blank tag");
        break;
    case AppResultNoHost: {
        /* the tag's own reading (tag_facts.h), as the rest of the screen */
        TagFacts own;
        char name[UI_TEXT_MAX] = "";
        if(tag_facts_read(&s->scan.tag, &own)) {
            ui_spool_name(own.info.material, own.info.subtype, name, sizeof(name));
        }
        put(&t, name[0] ? name : ui_tag_type_name(&s->scan.tag));
        break;
    }
    }
}

static bool runs(const AppState* s, TransportKind kind) {
    return app_connection_uses(s->connection, kind) && !s->transport_failed[kind];
}

/* Bluetooth blinks while it advertises without a host. */
static UiIcon bt_icon(const AppState* s, const UiEnv* env) {
    bool blinking = s->ble == AppBleAdvertising && !s->connected &&
                    runs(s, TransportKindBluetooth);
    return blinking && (env->anim & 1) ? UiIconNone : UiIconBt;
}

/* The connected host's transport; without one Bluetooth when it runs. */
static UiIcon transport_icon(const AppState* s, const UiEnv* env) {
    if(s->connected) return s->via == TransportKindBluetooth ? UiIconBt : UiIconUsb;
    if(s->connection == AppConnectionUsb) return UiIconUsb;
    if(s->connection == AppConnectionAuto && !runs(s, TransportKindBluetooth)) return UiIconUsb;
    return bt_icon(s, env);
}

/* ---- screens ---- */

/* "App protocol 12": a host app of another protocol said hello there. */
static bool put_other_version(Text* t, const AppState* s, TransportKind kind) {
    if(s->other_version[kind] == 0) return false;
    put(t, "App protocol ");
    put_uint(t, s->other_version[kind]);
    return true;
}

static void build_waiting(UiFrame* f, const AppState* s, const UiEnv* env) {
    SET(f->title, "FliBuddy");
    Text t = TEXT(f->note);
    put(&t, "v");
    put(&t, env->version);
    f->line_icon[0] = UiIconUsb;
    t = TEXT(f->line[0]);
    if(!app_connection_uses(s->connection, TransportKindUsb)) {
        put(&t, "USB off");
    } else if(s->transport_failed[TransportKindUsb]) {
        put(&t, "USB unavailable");
    } else if(!put_other_version(&t, s, TransportKindUsb)) {
        put(&t, "USB: no app");
    }
    f->line_icon[1] = bt_icon(s, env);
    t = TEXT(f->line[1]);
    if(!runs(s, TransportKindBluetooth) || s->ble == AppBleOff) {
        put(&t, "Bluetooth off");
    } else if(!put_other_version(&t, s, TransportKindBluetooth)) {
        if(s->ble == AppBleAdvertising) {
            put(&t, "Visible as ");
            put(&t, env->device_name);
        } else {
            put(&t, "Bluetooth: no app");
        }
    }
    t = TEXT(f->line[2]);
    put(&t, "Waiting for app");
    for(uint32_t i = 0; i < env->anim % 4; i++)
        put(&t, ".");
    SET(f->center, "Get app");
    SET(f->right, "Settings");
}

/* The QR code beside these lines is drawn by ui_draw.c. */
static void build_get_app(UiFrame* f) {
    SET(f->line[0], "No app yet?");
    SET(f->line[1], "Scan for the");
    SET(f->line[2], "iPhone app");
    SET(f->line[3], "App Store:");
    SET(f->line[4], "FliBuddy");
    SET(f->left, "Back");
}

static void put_outcome(Text* t, const AppState* s) {
    switch(s->last) {
    case AppOutcomeSaved:
        put(t, "Saved ");
        put_name(t, s);
        if(s->has_spool) {
            put(t, " ");
            put_uint(t, s->spool.remaining);
            put(t, " g");
        }
        break;
    case AppOutcomeAdded:
        put(t, "Added ");
        put_name(t, s);
        break;
    case AppOutcomeMarkedNew:
        put(t, "New ");
        put_name(t, s);
        if(s->has_spool) {
            put(t, " ");
            put_uint(t, s->spool.label);
            put(t, " g");
        }
        break;
    case AppOutcomeMarkedEmpty:
        put(t, "Empty ");
        put_name(t, s);
        break;
    case AppOutcomeDeleted:
        put(t, "Deleted ");
        put_name(t, s);
        break;
    case AppOutcomeNone:
        break;
    }
}

static void build_ready(UiFrame* f, const AppState* s) {
    SET(f->title, "FliBuddy");
    Text t = TEXT(f->bar);
    put_outcome(&t, s);
    if(f->bar[0]) {
        SET(f->line[0], "Next spool");
        SET(f->line[1], "Hold tag to");
        SET(f->line[2], "the back");
    } else {
        SET(f->line[0], "Ready");
        SET(f->line[1], "Hold spool tag");
        SET(f->line[2], "to the back");
    }
    SET(f->left, "Recent");
    SET(f->right, "Settings");
}

/* A saved spool per row: its name, and what was left right-aligned so a
 * long name is cut, not the grams. */
static void build_recent(UiFrame* f, const AppState* s) {
    SET(f->title, "Recent");
    if(s->recent_count == 0) {
        SET(f->line[0], "No scans yet");
        return;
    }
    f->count = s->recent_count;
    f->checks = true;
    for(uint8_t i = 0; i < s->recent_count; i++) {
        const ProtoSpool* spool = &s->recent[i].spool;
        ui_spool_name(spool->material, spool->subtype, f->item[i], sizeof(f->item[i]));
        Text t = TEXT(f->value[i]);
        put_uint(&t, spool->remaining);
        put(&t, " g");
        f->fixed[i] = true;
    }
    f->sel = s->sel;
    f->top = s->top;
}

/* An unknown tag's own facts in place of its UID: `11803 - 1.75 mm -
 * 1000 g` (Bambu's colour code, diameter, label weight). False when it
 * has none. */
static bool put_tag_specs(Text* t, const ProtoTagInfo* i) {
    if(i == NULL) return false;
    bool any = false;
    if(i->code[0]) {
        put(t, i->code);
        any = true;
    }
    if(i->diameter[0]) {
        if(any) put(t, " - ");
        put(t, i->diameter);
        put(t, " mm");
        any = true;
    }
    if(i->has_label) {
        if(any) put(t, " - ");
        put_uint(t, i->label);
        put(t, " g");
        any = true;
    }
    return any;
}

static void build_result(UiFrame* f, const AppState* s) {
    result_title(s, f->title, sizeof(f->title));
    TagFacts own;
    const ProtoTagInfo* info = tag_info(s, &own);
    uint32_t rgba;
    if(rgba_of(s, info, &rgba)) {
        f->mark = UiMarkSwatch;
        f->lum = ui_luminance(rgba);
    }
    Text t = TEXT(f->line[0]);
    if(s->result_kind != AppResultNoHost && host_line(s, 1)) {
        put(&t, host_line(s, 1));
    } else {
        put_colour(&t, s, info);
    }
    t = TEXT(f->line[1]);
    f->level = -1;
    if(s->result_kind == AppResultKnown && s->has_spool) {
        /* A known spool says so; its UID is in Tag details. */
        f->in_base = true;
        put(&t, "In Bambuddy #");
        put_uint(&t, (uint32_t)s->spool.id);
    } else if(!put_tag_specs(&t, info)) {
        put(&t, "Tag ");
        char uid[2 * TAG_DUMP_MAX_UID + 1];
        ui_uid_short(s->scan.tag.uid, s->scan.tag.uid_len, uid, sizeof(uid));
        put(&t, uid);
    }
    SET(f->left, "Menu");
    switch(s->result_kind) {
    case AppResultKnown:
        t = TEXT(f->line[2]);
        if(s->has_spool && s->spool.archived) {
            SET(f->bar, "Archived");
        } else if(s->has_spool && s->spool.label > 0 && s->spool.remaining >= s->spool.label) {
            Text b = TEXT(f->bar);
            put(&b, "Unused - ");
            put_uint(&b, s->spool.label);
            put(&b, " g");
        } else if(s->has_spool) {
            unsigned label = s->spool.label;
            f->level = (int16_t)(label ? s->spool.remaining * 1000u / label : 0);
            put_uint(&t, s->spool.remaining);
            if(label) {
                put(&t, " / ");
                put_uint(&t, label);
            }
            put(&t, " g");
        } else if(host_line(s, 2)) {
            put(&t, host_line(s, 2));
        }
        SET(f->center, "Weigh");
        SET(f->right, "New");
        break;
    case AppResultUnknownPrefill:
        SET(f->bar, "Not in Bambuddy");
        SET(f->center, "Add");
        SET(f->right, "Link");
        break;
    case AppResultUnknownBare:
        SET(f->bar, "Not in Bambuddy");
        SET(f->center, "Create");
        SET(f->right, "Link");
        break;
    case AppResultBlank:
        SET(f->bar, "No spool data");
        SET(f->center, "Link");
        break;
    case AppResultNoHost:
        SET(f->bar, "No app");
        break;
    }
}

static const char* menu_label(AppMenuItem item) {
    switch(item) {
    case AppMenuWeigh:
        return "Weigh";
    case AppMenuMarkNew:
        return "Mark as new";
    case AppMenuMarkEmpty:
        return "Mark as empty";
    case AppMenuRelink:
        return "Relink in app";
    case AppMenuDetails:
        return "Tag details";
    case AppMenuDelete:
        return "Delete spool";
    case AppMenuAddNew:
        return "Add new";
    case AppMenuAddAndWeigh:
        return "Add and weigh";
    case AppMenuCreateOnHost:
        return "Create in app";
    case AppMenuLinkOnHost:
        return "Link in app";
    }
    return "";
}

static void build_menu(UiFrame* f, const AppState* s) {
    result_title(s, f->title, sizeof(f->title));
    AppMenuItem items[APP_MENU_MAX];
    f->count = app_menu_items(s, items);
    for(uint8_t i = 0; i < f->count; i++)
        SET(f->item[i], menu_label(items[i]));
    f->sel = s->sel;
    f->top = s->top;
}

/* One Tag details row. */
typedef struct {
    char key[UI_KEY_MAX];
    char value[UI_TEXT_MAX];
} DetailRow;

/* The Tag details rows, in order; each shows when known. */
enum {
    DetailType,
    DetailUid,
    DetailData,
    DetailCode,
    DetailFila,
    DetailRgba,
    DetailLabel,
    DetailDiam,
    DetailNozzle,
    DetailDry,
    DetailBed,
    DetailKinds,
};
_Static_assert(DetailKinds == UI_DETAILS_ROWS, "one slot per Tag details row");

/* `rows` holds UI_DETAILS_ROWS + 1: should a row ever come past the last
 * slot, it is written into the extra one, which is never shown. */
static Text detail(DetailRow* rows, uint8_t* n, const char* key) {
    DetailRow* row = &rows[*n < UI_DETAILS_ROWS ? (*n)++ : UI_DETAILS_ROWS];
    SET(row->key, key);
    return TEXT(row->value);
}

/* Fills the Tag details rows of the tag on screen; returns their count. */
static uint8_t details_rows(const AppState* s, DetailRow rows[UI_DETAILS_ROWS + 1]) {
    const TagDump* tag = &s->scan.tag;
    TagFacts own;
    const ProtoTagInfo* info = tag_info(s, &own);
    memset(rows, 0, (UI_DETAILS_ROWS + 1) * sizeof(rows[0]));
    uint8_t n = 0;
    Text t = detail(rows, &n, "Type");
    put(&t, ui_tag_type_name(tag));
    t = detail(rows, &n, "UID");
    char uid[UI_TEXT_MAX];
    ui_uid_colons(tag->uid, tag->uid_len, uid, sizeof(uid));
    put(&t, uid);
    t = detail(rows, &n, "Data");
    if(tag->type == TagTypeMfc) {
        put_uint(&t, tag_sectors_read(tag));
        put(&t, "/16 sectors");
    } else {
        put_uint(&t, tag->page_count);
        put(&t, " pages");
    }
    if(info && info->code[0]) {
        t = detail(rows, &n, "Code");
        put(&t, info->code);
    }
    if(info && info->filament_id[0]) {
        t = detail(rows, &n, "Fila");
        put(&t, info->filament_id);
    }
    uint32_t rgba;
    if(rgba_of(s, info, &rgba)) {
        t = detail(rows, &n, "RGBA");
        put_hex(&t, rgba, 8);
    }
    bool has_label = shows_spool(s) || (info && info->has_label);
    if(has_label) {
        t = detail(rows, &n, "Label");
        put_uint(&t, shows_spool(s) ? s->spool.label : info->label);
        put(&t, " g");
    }
    if(info && info->diameter[0]) {
        t = detail(rows, &n, "Diam");
        put(&t, info->diameter);
        put(&t, " mm");
    }
    if(info && (info->has_nozzle_min || info->has_nozzle_max)) {
        t = detail(rows, &n, "Nozzle");
        if(info->has_nozzle_min) put_uint(&t, info->nozzle_min);
        if(info->has_nozzle_min && info->has_nozzle_max) put(&t, "-");
        if(info->has_nozzle_max) put_uint(&t, info->nozzle_max);
        put(&t, " C");
    }
    /* only the tag itself tells these; the host does not send them */
    if(info == &own.info && own.drying_temp) {
        t = detail(rows, &n, "Dry");
        put_uint(&t, own.drying_temp);
        put(&t, " C");
        if(own.drying_hours) {
            put(&t, " ");
            put_uint(&t, own.drying_hours);
            put(&t, " h");
        }
    }
    if(info == &own.info && own.bed_temp) {
        t = detail(rows, &n, "Bed");
        put_uint(&t, own.bed_temp);
        put(&t, " C");
    }
    return n;
}

uint8_t ui_details_count(const AppState* s) {
    DetailRow rows[UI_DETAILS_ROWS + 1];
    return details_rows(s, rows);
}

static void build_details(UiFrame* f, const AppState* s) {
    SET(f->title, "Tag details");
    DetailRow rows[UI_DETAILS_ROWS + 1];
    uint8_t n = details_rows(s, rows);
    /* the state stops at the last row; this only guards the frame */
    uint8_t top = n > APP_DETAILS_SHOWN ? (uint8_t)(n - APP_DETAILS_SHOWN) : 0;
    if(s->details_top < top) top = s->details_top;
    uint8_t shown = 0;
    for(uint8_t i = top; i < n && shown < APP_DETAILS_SHOWN; i++, shown++) {
        SET(f->key[shown], rows[i].key);
        SET(f->line[shown], rows[i].value);
    }
    f->count = shown;
    f->more_up = top > 0;
    f->more_down = top + shown < n;
}

static void build_confirm_delete(UiFrame* f, const AppState* s) {
    SET(f->title, "Delete spool?");
    Text t = TEXT(f->line[0]);
    put_name_and_colour(&t, s);
    SET(f->line[1], "Removes it from");
    SET(f->line[2], "Bambuddy for good.");
    SET(f->left, "Cancel");
    SET(f->right, "Delete");
}

static void build_weight(UiFrame* f, const AppState* s) {
    SET(f->title, "Weight");
    memcpy(f->digit, s->weight.digit, sizeof(f->digit));
    f->cursor = s->weight.cursor;
    f->net = s->mode == WeightModeNet;
    f->refused = app_weight_below_core(s);
    unsigned grams = app_weight_grams(s);
    unsigned core = app_weight_core(s);
    Text t = TEXT(f->line[0]);
    if(f->refused) {
        SET(f->bar, "Below core weight");
    } else if(f->net) {
        put(&t, "Saves ");
        put_uint(&t, grams);
        put(&t, " g left");
    } else if(core > 0) {
        put(&t, "Net ");
        put_uint(&t, grams - core);
        put(&t, " g - core ");
        put_uint(&t, core);
    } else {
        put(&t, "Core weight unknown");
    }
    SET(f->center, "Save");
}

static void build_busy(UiFrame* f, const AppState* s) {
    const Intent* held = &s->held;
    switch(held->action) {
    case IntentActionWeigh:
        SET(f->title, "Saving");
        break;
    case IntentActionNew:
        SET(f->title, "Adding");
        break;
    case IntentActionDelete:
        SET(f->title, "Deleting");
        break;
    default:
        SET(f->title, "Updating");
        break;
    }
    Text t = TEXT(f->line[0]);
    put_name(&t, s);
    if(held->action == IntentActionWeigh) {
        unsigned core = app_weight_core(s);
        bool net = held->mode == WeightModeNet;
        if(net || (core > 0 && held->grams >= core)) {
            put(&t, " - ");
            put_uint(&t, net ? held->grams : held->grams - core);
            put(&t, " g");
        }
    }
    SET(f->line[1], "Waiting for Bambuddy");
}

static void build_saved(UiFrame* f, const AppState* s) {
    SET(f->title, "Saved");
    Text t = TEXT(f->line[0]);
    put_name(&t, s);
    if(s->has_spool) {
        put(&t, " - ");
        put_uint(&t, s->spool.remaining);
        put(&t, " g left");
    }
}

static void build_toast(UiFrame* f, const AppState* s, const UiEnv* env) {
    Text t = TEXT(f->line[0]);
    put_name_and_colour(&t, s);
    t = TEXT(f->line[1]);
    switch(s->toast) {
    case AppToastAdded:
        SET(f->title, "Added to Bambuddy");
        if(s->has_spool) {
            put(&t, "Spool #");
            put_uint(&t, (uint32_t)s->spool.id);
            put(&t, " - ");
            put_uint(&t, s->spool.remaining);
            put(&t, " g");
        }
        SET(f->center, "Weigh");
        SET(f->right, "Next");
        return; /* no deadline */
    case AppToastMarkedNew:
        SET(f->title, "Marked as new");
        if(s->has_spool) { /* without the spool's data there is no label to name */
            put_uint(&t, s->spool.label);
            put(&t, " g left - full");
        }
        SET(f->left, "Undo");
        SET(f->center, "Weigh");
        break;
    case AppToastMarkedEmpty:
        SET(f->title, "Marked as empty");
        put(&t, "0 g left - archived");
        SET(f->left, "Undo");
        break;
    case AppToastDeleted:
        SET(f->title, "Deleted");
        put(&t, "Tag is free to reuse");
        break;
    }
    uint32_t left = app_timer_left_ms(s, env->now_ms);
    f->progress = (uint16_t)(left * UI_TOAST_MAX / APP_TOAST_MS);
    t = TEXT(f->line[2]);
    put(&t, "Next spool in ");
    put_uint(&t, (left + 999) / 1000);
    put(&t, " s");
}

static void build_link(UiFrame* f, const AppState* s) {
    bool create = s->held.action == IntentActionCreateOnHost;
    SET(f->title, create ? "Create in app" : "Link tag");
    SET(f->line[0], create ? "Finish the spool in" : "Pick the spool in");
    SET(f->line[1], "the FliBuddy app");
    SET(f->left, "Cancel");
}

static const char* host_error_title(IntentAction action) {
    switch(action) {
    case IntentActionWeigh:
        return "Not saved";
    case IntentActionNew:
        return "Not added";
    case IntentActionDelete:
        return "Not deleted";
    case IntentActionFull:
    case IntentActionEmpty:
        return "Not updated";
    case IntentActionLinkOnHost:
    case IntentActionCreateOnHost:
        break;
    }
    return "Not linked";
}

/* The host's lines and error, one per line, or `fallback`. */
static void put_host_text(Text* t, const ProtoLines* lines, const char* error, const char* fallback) {
    for(uint8_t i = 0; i < lines->count && i < PROTO_LINES_MAX; i++) {
        if(t->len > 0) put(t, "\n");
        put(t, lines->line[i]);
    }
    if(error[0]) {
        if(t->len > 0) put(t, "\n");
        put(t, error);
    }
    if(t->len == 0) put(t, fallback);
}

static void build_error(UiFrame* f, const AppState* s, const UiEnv* env) {
    Text t = TEXT(f->body);
    switch(s->error) {
    case AppErrorReadFailed:
        SET(f->title, "Tag not read");
        put(&t, "Hold the tag still against the back. Reading again...");
        f->progress =
            (uint16_t)(app_timer_left_ms(s, env->now_ms) * UI_TOAST_MAX / APP_READ_RETRY_MS);
        return; /* no keys needed */
    case AppErrorUnsupportedTag:
        SET(f->title, "Unsupported tag");
        put(&t, "Not a spool tag FliBuddy can read.");
        f->progress =
            (uint16_t)(app_timer_left_ms(s, env->now_ms) * UI_TOAST_MAX / APP_READ_RETRY_MS);
        return; /* no keys needed */
    case AppErrorLookup:
        SET(f->title, "Lookup failed");
        put_host_text(
            &t, &s->lookup.lines, s->lookup.error, "The app could not look the tag up.");
        break;
    case AppErrorHost:
        SET(f->title, host_error_title(s->held.action));
        put_host_text(&t, &s->result.lines, s->result.error, "Bambuddy did not take the change.");
        break;
    case AppErrorTimeout:
        SET(f->title, "No app reply");
        put(&t, "No reply in 10 s. Open the FliBuddy app.");
        break;
    case AppErrorHostLost:
        SET(f->title, "App lost");
        put(&t, "The app went away before it answered.");
        break;
    case AppErrorSendFailed:
        SET(f->title, "Not sent");
        put(&t, "Could not send to the app.");
        break;
    }
    SET(f->left, "Back");
    if(s->retryable && s->connected) SET(f->right, "Retry");
}

static const char* on_off(bool on) {
    return on ? "On" : "Off";
}

static const char* feedback_name(AppFeedback feedback) {
    switch(feedback) {
    case AppFeedbackVibroLed:
        return "Vibro+LED";
    case AppFeedbackLed:
        return "LED";
    case AppFeedbackOff:
        break;
    }
    return "Off";
}

static void build_settings(UiFrame* f, const AppState* s, const UiEnv* env) {
    static const char* const labels[AppSettingCount] = {
        [AppSettingConnection] = "Connection",
        [AppSettingAutosave] = "Auto-save new",
        [AppSettingAfterScan] = "After scan",
        [AppSettingWeighAs] = "Weigh as",
        [AppSettingFeedback] = "Feedback",
        [AppSettingSound] = "Sound",
        [AppSettingVersion] = "Version",
    };
    const AppSettings* st = &s->settings;
    SET(f->title, "Settings");
    f->count = AppSettingCount;
    for(uint8_t i = 0; i < AppSettingCount; i++)
        SET(f->item[i], labels[i]);
    switch(app_settings_connection(st)) {
    case AppConnectionUsb:
        SET(f->value[AppSettingConnection], "USB");
        break;
    case AppConnectionBluetooth:
        SET(f->value[AppSettingConnection], "Bluetooth");
        break;
    case AppConnectionAuto:
        SET(f->value[AppSettingConnection], "Auto");
        break;
    }
    SET(f->value[AppSettingAutosave], on_off(app_settings_autosave(st)));
    SET(f->value[AppSettingAfterScan],
        app_settings_after_scan(st) == AppAfterScanWeigh ? "Weigh" : "Show");
    SET(f->value[AppSettingWeighAs], app_settings_weigh_as(st) == AppWeighAsNet ? "Net" : "Gross");
    SET(f->value[AppSettingFeedback], feedback_name(app_settings_feedback(st)));
    SET(f->value[AppSettingSound], on_off(app_settings_sound(st)));
    SET(f->value[AppSettingVersion], env->version);
    f->fixed[AppSettingVersion] = true;
    f->sel = s->sel;
    f->top = s->top;
}

static void build_about(UiFrame* f, const AppState* s, const UiEnv* env) {
    SET(f->title, "FliBuddy");
    SET(f->line[0], "for Bambuddy");
    Text t = TEXT(f->line[1]);
    put(&t, "Version ");
    put(&t, env->version);
    t = TEXT(f->line[2]);
    put(&t, "Protocol ");
    put_uint(&t, PROTO_VERSION);
    put(&t, " - fw ");
    put(&t, env->firmware);
    t = TEXT(f->line[3]);
    if(s->connected) {
        /* the transport first: a long app name is cut, not the transport */
        put(&t, s->via == TransportKindBluetooth ? "BLE app: " : "USB app: ");
        put(&t, env->host_app[0] ? env->host_app : "connected");
    } else {
        put(&t, "App: not connected");
    }
}

void ui_frame_build(UiFrame* f, const AppState* s, const UiEnv* env) {
    memset(f, 0, sizeof(*f));
    f->screen = s->screen;
    f->kind = s->result_kind;
    f->toast = s->toast;
    f->anim = (uint8_t)env->anim;
    f->icon = transport_icon(s, env);
    char local[TAG_BLOCK_TEXT_MAX];
    switch(s->screen) {
    case AppScreenWaiting:
        build_waiting(f, s, env);
        break;
    case AppScreenReady:
        build_ready(f, s);
        break;
    case AppScreenReading:
        SET(f->title, "Reading tag");
        SET(f->line[0], "Keep still");
        break;
    case AppScreenLookingUp:
        SET(f->title, "Looking up");
        if(local_name(s, local)) SET(f->line[0], local);
        SET(f->line[1], "Asking the app");
        break;
    case AppScreenResult:
        build_result(f, s);
        break;
    case AppScreenMenu:
        build_menu(f, s);
        break;
    case AppScreenDetails:
        build_details(f, s);
        break;
    case AppScreenConfirmDelete:
        build_confirm_delete(f, s);
        break;
    case AppScreenWeight:
        build_weight(f, s);
        break;
    case AppScreenBusy:
        build_busy(f, s);
        break;
    case AppScreenSaved:
        build_saved(f, s);
        break;
    case AppScreenToast:
        build_toast(f, s, env);
        break;
    case AppScreenLinkOnHost:
        build_link(f, s);
        break;
    case AppScreenError:
        build_error(f, s, env);
        break;
    case AppScreenSettings:
        build_settings(f, s, env);
        break;
    case AppScreenAbout:
        build_about(f, s, env);
        break;
    case AppScreenRecent:
        build_recent(f, s);
        break;
    case AppScreenGetApp:
        build_get_app(f);
        break;
    }
}
