// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT
//
// VaultFlipper – offline password vault for the Flipper Zero.
//
// Home -> account -> folder -> entry card. The card types the login over USB.
// Home -> 2FA codes lists every login with a TOTP secret and its current code.
// Only the directory and the open page live in RAM, so vaults can be large.
// Lock: hold Back, menu item "Lock", or 2 minutes without input.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define VF_VERSION "0.1.0"

#include "vaultflipper_icons.h"
#include "vf_core.h"
#include "vf_device.h"
#include "vf_hid.h"
#include "vf_i18n.h"
#include "vf_totp.h"
#include "vf_ui.h"

#define TAG "VaultFlipper"

#define VF_DIR EXT_PATH("apps_data/vaultflipper")
#ifdef VF_TEST_NO_HID /* test build: own vault, never touches the real one */
#define VF_VAULT_PATH VF_DIR "/test.vfv"
#define VF_FAILS_PATH VF_DIR "/test.fails"
#else
#define VF_VAULT_PATH VF_DIR "/vault.vfv"
#define VF_FAILS_PATH VF_DIR "/fails"
#endif
#define VF_PUB_PATH         VF_DIR "/device.pub"
#define VF_LAYOUT_PATH      VF_DIR "/layout"
#define VF_LANG_PATH        VF_DIR "/lang"
#define VF_CLOCK_PATH       VF_DIR "/clock" /* seconds the Flipper clock is ahead of UTC */
#define VF_CLOCK_UNSET      1577836800u /* 2020-01-01: the clock was never set */
#define VF_TICK_MS          250
#define VF_TZ_ITEMS         27 /* UTC-12 .. UTC+14 */
#define VF_CLOCK_MAX_OFFSET (18L * 3600) /* ignore a damaged clock file */
#define VF_MAX_FAILS        10
#define VF_IDLE_MS          (120u * 1000u)
#define VF_MAX_LAYOUTS      48
#define VF_MAX_HITS         50
#define VF_QUERY_MAX        32
#define VF_ARGON_SIZE       (32u * 1024u) /* default Argon2id memory, reserved at start */
#define VF_MEM_MARGIN       2048u

typedef enum {
    ViewPin,
    ViewHome,
    ViewFolders,
    ViewEntries,
    ViewCard,
    ViewMore,
    ViewSettings,
    ViewLayouts,
    ViewResults,
    ViewSearch,
    ViewText,
    ViewMacSetup,
    ViewCode,
    ViewTimezone,
    ViewBusy, /* keep last */
} VfViewId;

typedef enum {
    EvPinSubmit = 100,
    EvIdle,
    EvMacButton,
    EvSearch,
    EvTick,
    EvCardBase = 200, /* + VfCardAction */
    EvCodeBase = 300, /* + VfCodeAction */
} VfEvent;

typedef enum {
    HomeCodes, /* only when the vault has 2FA codes; keep first */
    HomeSearch,
    HomeSettings,
    HomeLock,
    HomeExtraCount,
} VfHomeExtra;

typedef enum {
    SetKeyboard,
    SetMacSetup,
    SetTypingTest,
    SetTimeZone,
    SetLanguage,
    SetHelp,
    SetAbout,
    SetCount,
} VfSetting;

typedef enum {
    MoreLoginEnter,
    MoreLogin,
    MoreUser,
    MorePass,
    MoreCode, /* only for logins with a 2FA secret */
    MoreDetails,
    MoreCount,
} VfMore;

typedef struct {
    char seq[VF_MAX_PIN + 1];
    uint8_t len;
    char msg[40];
    VfLang lang;
} PinModel;

typedef struct {
    uint16_t page;
    uint16_t idx;
    char name[24];
    char user[24];
} VfHit;

/* Current code of each entry on an open 2FA page, computed in the app thread
 * (the GUI thread has too little stack for SHA-512). */
typedef struct {
    char code[VF_TOTP_CODE_MAX];
    bool ok;
} VfCodeSlot;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Storage* storage;
    NotificationApp* notif;
    View* pin_view;
    VfList* home;
    VfList* folders;
    VfList* entries;
    VfList* more;
    VfList* settings;
    VfList* layouts;
    VfList* results;
    VfList* timezones;
    VfCard* card;
    VfCode* code;
    VfBusy* busy;
    Widget* text;
    Widget* mac_setup;
    TextInput* search;

    FuriPubSub* input_events;
    FuriPubSubSubscription* input_sub;
    FuriTimer* idle_timer;
    FuriTimer* tick_timer;
    volatile bool tick_pending;

    uint8_t dev_sk[32];
    uint8_t dev_pk[32];
    char fingerprint[10];
    bool device_ok;

    void* argon_work;
    File* vault_file;
    VfFile vf;
    VfDir dir;
    VfPage page; /* the open page, if page.buf != NULL */
    uint32_t acc_logins[VF_MAX_ACCOUNTS];
    uint16_t acc_folders[VF_MAX_ACCOUNTS];
    volatile bool unlocked;
    volatile bool back_long; /* set by the input hook, consumed in navigation */

    uint8_t cur_account;
    uint16_t* folder_pages; /* page index per folder list row */
    uint16_t folder_count;
    uint16_t cur_entry;
    char card_note[12];
    VfViewId entries_return;
    VfViewId card_return;

    VfCodeSlot* codes; /* for the open page, if it is a 2FA page */
    uint16_t codes_count;
    uint16_t codes_page;
    uint32_t codes_second; /* clock second the codes were computed for */
    uint16_t code_entry; /* entry shown in the code view */
    VfViewId code_return;
    char code_note[12];
    int32_t clock_offset;
    int32_t tz_frac; /* seconds beyond whole hours, kept when picking a zone */

    char query[VF_QUERY_MAX + 1];
    VfHit hits[VF_MAX_HITS];
    uint8_t hit_count;

    VfHid hid;
    char layout_names[VF_MAX_LAYOUTS][VF_LAYOUT_NAME_MAX + 1];
    uint8_t layout_count;
    uint8_t mac_step;

    VfLang lang;
    VfViewId current;
    VfViewId text_return;
    uint8_t fails;
    bool exit_after_text;
    uint32_t unlock_ms;
    uint32_t heap_max_block;
} VfApp;

static inline const char* T(VfApp* app, VfStr s) {
    return vf_strings[app->lang][s];
}

/* ---------- small helpers ---------- */

static void vf_switch(VfApp* app, VfViewId id) {
    app->current = id;
    view_dispatcher_switch_to_view(app->vd, id);
}

static void vf_show_text(VfApp* app, const char* text, VfViewId back_to) {
    widget_reset(app->text);
    widget_add_text_scroll_element(app->text, 0, 0, 128, 64, text);
    app->text_return = back_to;
    vf_switch(app, ViewText);
}

static void vf_busy(VfApp* app, VfStr text, const Icon* icon) {
    vf_busy_set(app->busy, T(app, text), icon);
    vf_switch(app, ViewBusy);
}

static size_t vf_file_read_small(VfApp* app, const char* path, void* out, size_t size) {
    File* f = storage_file_alloc(app->storage);
    size_t n = 0;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        n = storage_file_read(f, out, size);
    }
    storage_file_close(f);
    storage_file_free(f);
    return n;
}

static void vf_file_write_small(VfApp* app, const char* path, const void* data, size_t size) {
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, data, size);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void vf_read_setting(VfApp* app, const char* path, char* out, size_t size) {
    size_t n = vf_file_read_small(app, path, out, size - 1);
    out[n] = 0;
    char* nl = strchr(out, '\n');
    if(nl) *nl = 0;
}

static void vf_write_pubkey(VfApp* app) {
    static const char hex[] = "0123456789abcdef";
    char line[65];
    for(int i = 0; i < 32; i++) {
        line[2 * i] = hex[app->dev_pk[i] >> 4];
        line[2 * i + 1] = hex[app->dev_pk[i] & 0x0F];
    }
    line[64] = '\n';
    vf_file_write_small(app, VF_PUB_PATH, line, sizeof(line));
}

/* VfReadFn for the open vault file */
static bool vf_vault_read(void* ctx, uint32_t offset, void* buf, size_t len) {
    VfApp* app = ctx;
    if(!storage_file_seek(app->vault_file, offset, true)) return false;
    return storage_file_read(app->vault_file, buf, len) == len;
}

/* Keyboard layout priority: chosen on the device > given at sync > en-US. */
static void vf_apply_layout(VfApp* app) {
    char chosen[VF_LAYOUT_NAME_MAX + 2];
    vf_read_setting(app, VF_LAYOUT_PATH, chosen, sizeof(chosen));
    vf_hid_init(&app->hid);
    if(chosen[0] && vf_hid_load_layout(&app->hid, app->storage, chosen)) return;
    vf_hid_load_layout(&app->hid, app->storage, app->dir.meta.f[VfFieldLayout]);
}

static void vf_empty_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    UNUSED(ctx);
    UNUSED(index);
    UNUSED(item);
    UNUSED(sub);
    UNUSED(size);
}

/* Detach every view from vault data before it is freed. */
static void vf_views_clear(VfApp* app) {
    VfList* lists[] = {
        app->home, app->folders, app->entries, app->more, app->settings, app->results};
    for(size_t i = 0; i < COUNT_OF(lists); i++) {
        vf_list_set(lists[i], "", NULL, 0, VfListPlain, vf_empty_item, NULL, app, 0);
    }
    vf_card_hide_password(app->card);
    vf_code_clear(app->code);
    widget_reset(app->text);
}

static void vf_codes_free(VfApp* app) {
    if(app->codes) {
        vf_wipe(app->codes, sizeof(VfCodeSlot) * app->codes_count);
        free(app->codes);
        app->codes = NULL;
        app->codes_count = 0;
    }
}

static void vf_close_vault(VfApp* app) {
    vf_codes_free(app);
    vf_page_free(&app->page);
    vf_dir_free(&app->dir);
    vf_file_close(&app->vf);
    if(app->vault_file) {
        storage_file_close(app->vault_file);
        storage_file_free(app->vault_file);
        app->vault_file = NULL;
    }
    if(app->folder_pages) {
        free(app->folder_pages);
        app->folder_pages = NULL;
    }
    app->folder_count = 0;
    vf_wipe(app->query, sizeof(app->query));
    vf_wipe(app->hits, sizeof(app->hits));
    app->hit_count = 0;
}

static void vf_pin_reset(VfApp* app, const char* msg) {
    with_view_model(
        app->pin_view,
        PinModel * m,
        {
            vf_wipe(m->seq, sizeof(m->seq));
            m->len = 0;
            m->lang = app->lang;
            strlcpy(m->msg, msg, sizeof(m->msg));
        },
        true);
}

static void vf_lock(VfApp* app, VfStr msg) {
    app->unlocked = false;
    furi_timer_stop(app->idle_timer);
    furi_timer_stop(app->tick_timer);
    vf_pin_reset(app, T(app, msg));
    vf_switch(app, ViewPin);
    vf_views_clear(app);
    vf_hid_end(&app->hid);
    vf_close_vault(app);
}

/* The Flipper's malloc never returns NULL - it reboots. So check first. */
static bool vf_can_alloc(size_t size) {
    return memmgr_heap_get_max_free_block() >= size + VF_MEM_MARGIN;
}

static VfResult vf_open_page(VfApp* app, uint16_t index) {
    if(app->page.buf && app->page.index == index) return VfOk;
    vf_codes_free(app);
    vf_page_free(&app->page);
    if(!vf_can_alloc(vf_page_mem(&app->dir, index))) return VfErrMemory;
    return vf_page_load(&app->vf, &app->dir, index, &app->page);
}

static void vf_show_error(VfApp* app, VfResult r, VfViewId back_to) {
    vf_show_text(app, T(app, r == VfErrMemory ? S_NoMemory : S_IoError), back_to);
}

static bool vf_page_split(VfApp* app, uint16_t i) {
    const VfPageInfo* pi = &app->dir.pages[i];
    const VfPageInfo* pages = app->dir.pages;
    return (i > 0 && pages[i - 1].account == pi->account &&
            strcmp(pages[i - 1].group, pi->group) == 0) ||
           (i + 1 < app->dir.page_count && pages[i + 1].account == pi->account &&
            strcmp(pages[i + 1].group, pi->group) == 0);
}

static bool vf_is_codes(VfApp* app, uint16_t page) {
    return app->dir.pages[page].account == VF_CODES_ACCOUNT;
}

static const char* vf_group_name(VfApp* app, uint16_t page) {
    if(vf_is_codes(app, page)) return T(app, S_Codes);
    const char* g = app->dir.pages[page].group;
    return g[0] ? g : T(app, S_GeneralGroup);
}

/* "https://mail.example.com/login" -> "mail.example.com" */
static void vf_uri_host(const char* uri, char* out, size_t size) {
    const char* p = strstr(uri, "://");
    p = p ? p + 3 : uri;
    size_t i = 0;
    while(p[i] && p[i] != '/' && p[i] != '?' && p[i] != ':' && i + 1 < size) {
        out[i] = p[i];
        i++;
    }
    out[i] = 0;
}

/* "1 login" / "5 logins" */
static int
    vf_count_str(VfApp* app, char* out, size_t size, unsigned long n, VfStr one, VfStr many) {
    return n == 1 ? snprintf(out, size, "%s", T(app, one)) : snprintf(out, size, T(app, many), n);
}

static void vf_feedback(VfApp* app, bool ok) {
    notification_message(app->notif, ok ? &sequence_blink_green_100 : &sequence_error);
}

/* ---------- clock and 2FA codes ----------
 * The Flipper keeps local time (set by qFlipper or the mobile app). The
 * companion measures how far it is ahead of UTC at every sync. */

static void vf_format_zone(int32_t seconds, char* out, size_t size) {
    int32_t q = (seconds >= 0 ? seconds + 450 : seconds - 450) / 900 * 900;
    char sign = q < 0 ? '-' : '+';
    if(q < 0) q = -q;
    snprintf(out, size, "UTC%c%02ld:%02ld", sign, (long)(q / 3600), (long)(q % 3600 / 60));
}

/* false if the clock was never set */
static bool vf_utc_now(VfApp* app, uint64_t* out) {
    uint32_t rtc = furi_hal_rtc_get_timestamp();
    if(rtc < VF_CLOCK_UNSET) return false;
    *out = (uint64_t)((int64_t)rtc - app->clock_offset);
    return true;
}

/* "123456" -> "123 456" */
static void vf_group_code(const char* code, char* out, size_t size) {
    size_t n = strlen(code);
    size_t half = (n + 1) / 2;
    snprintf(out, size, "%.*s %s", (int)half, code, code + half);
}

/* Code for an entry, or false with a reason. */
static bool vf_entry_code(
    const VfRecord* e,
    uint64_t now,
    char code[VF_TOTP_CODE_MAX],
    uint32_t* left,
    uint32_t* period,
    VfStr* why) {
    VfTotp t;
    bool ok = vf_totp_parse(e->f[VfFieldTotp], &t);
    if(!ok) {
        *why = S_SecretBad;
    } else {
        vf_totp_code(&t, now, code);
        *left = vf_totp_remaining(&t, now);
        *period = t.period;
    }
    vf_totp_wipe(&t);
    return ok;
}

/* Recompute the codes of the open 2FA page; true if anything changed. */
static bool vf_codes_refresh(VfApp* app) {
    if(!app->page.buf || !vf_is_codes(app, app->page.index)) return false;
    if(!app->codes || app->codes_page != app->page.index) {
        vf_codes_free(app);
        size_t size = sizeof(VfCodeSlot) * app->page.count;
        if(!vf_can_alloc(size)) return false;
        app->codes = malloc(size);
        memset(app->codes, 0, size);
        app->codes_count = app->page.count;
        app->codes_page = app->page.index;
        app->codes_second = 0;
    }
    uint32_t second = furi_hal_rtc_get_timestamp();
    if(second == app->codes_second) return false;
    app->codes_second = second;
    uint64_t now = 0;
    bool clock = vf_utc_now(app, &now);
    /* The countdown in the header fits every code on the page only if they
     * share one period (almost always 30 s); otherwise it is left out. */
    uint32_t shared_left = 0, shared_period = 0;
    bool mixed = false;
    for(uint16_t i = 0; i < app->page.count; i++) {
        uint32_t left = 0, period = 0;
        VfStr why;
        VfCodeSlot* s = &app->codes[i];
        s->ok = clock && vf_entry_code(&app->page.entries[i], now, s->code, &left, &period, &why);
        if(!s->ok) continue;
        if(shared_period && period != shared_period) mixed = true;
        shared_period = period;
        shared_left = left;
    }
    char note[16] = "";
    uint8_t bar = 0;
    if(clock) {
        if(shared_period && !mixed) {
            snprintf(note, sizeof(note), "%lus", (unsigned long)shared_left);
            bar = (uint8_t)(shared_left * 128u / shared_period);
        }
    } else {
        strlcpy(note, T(app, S_ClockUnset), sizeof(note));
    }
    vf_list_set_status(app->entries, note, bar);
    return true;
}

/* Push the current code of the shown entry into the code view. */
static void vf_code_refresh(VfApp* app) {
    if(!app->page.buf || app->code_entry >= app->page.count) return;
    uint64_t now = 0;
    char code[VF_TOTP_CODE_MAX] = "";
    uint32_t left = 0, period = 0;
    VfStr why = S_ClockUnset;
    bool ok = vf_utc_now(app, &now) &&
              vf_entry_code(&app->page.entries[app->code_entry], now, code, &left, &period, &why);
    vf_code_update(app->code, ok ? code : T(app, why), !ok, left, period);
    vf_wipe(code, sizeof(code));
}

static void vf_code_action(void* ctx, VfCodeAction action) {
    VfApp* app = ctx;
    /* runs in the input handler; defer to the event loop */
    view_dispatcher_send_custom_event(app->vd, EvCodeBase + action);
}

static void vf_open_code(VfApp* app, uint16_t entry, VfViewId back_to) {
    const VfRecord* e = &app->page.entries[entry];
    bool cycle = back_to == ViewEntries && app->page.count > 1;
    app->code_entry = entry;
    app->code_return = back_to;
    if(cycle)
        snprintf(app->code_note, sizeof(app->code_note), "%u/%u", entry + 1, app->page.count);
    else
        app->code_note[0] = 0;
    vf_code_set(
        app->code,
        e->f[VfFieldName][0] ? e->f[VfFieldName] : T(app, S_NoName),
        e->f[VfFieldUser],
        app->code_note,
        T(app, S_BtnType),
        cycle,
        vf_code_action,
        app);
    vf_code_refresh(app);
    if(app->current != ViewCode) vf_switch(app, ViewCode);
}

/* ---------- typing ----------
 * The Flipper stays a USB keyboard while unlocked, like a security key, so
 * typing starts at once and the host sees the keyboard only once. */

static bool vf_type_begin(VfApp* app, VfViewId back_to) {
#ifdef VF_TEST_NO_HID
    vf_show_text(app, T(app, S_NoUsbHost), back_to); /* test build: never type */
    return false;
#else
    app->hid.skipped = 0;
    if(vf_hid_connected(&app->hid)) return true;
    vf_busy(app, S_Connecting, &I_keyboard);
    if(vf_hid_ready(&app->hid)) return true;
    vf_feedback(app, false);
    vf_show_text(app, T(app, S_NoUsbHost), back_to);
    return false;
#endif
}

static void vf_type_end(VfApp* app, VfViewId back_to) {
    furi_hal_hid_kb_release_all();
    if(app->hid.skipped) {
        char msg[96];
        snprintf(msg, sizeof(msg), T(app, S_Skipped), app->hid.skipped);
        vf_feedback(app, false);
        vf_show_text(app, msg, back_to);
        return;
    }
    vf_feedback(app, true);
    if(app->current != back_to) vf_switch(app, back_to);
}

static void vf_code_event(VfApp* app, VfCodeAction action) {
    if(!app->page.buf || app->current != ViewCode) return;
    uint16_t n = app->page.count;
    if(action == VfCodePrev) {
        vf_open_code(app, (uint16_t)((app->code_entry + n - 1) % n), app->code_return);
        return;
    }
    if(action == VfCodeNext) {
        vf_open_code(app, (uint16_t)((app->code_entry + 1) % n), app->code_return);
        return;
    }
    uint64_t now;
    char code[VF_TOTP_CODE_MAX];
    uint32_t left, period;
    VfStr why;
    if(!vf_utc_now(app, &now) ||
       !vf_entry_code(&app->page.entries[app->code_entry], now, code, &left, &period, &why)) {
        vf_feedback(app, false);
        return;
    }
    if(vf_type_begin(app, ViewCode)) {
        vf_hid_type(&app->hid, code);
        vf_type_end(app, ViewCode);
    }
    vf_wipe(code, sizeof(code));
}

/* ---------- home ---------- */

static bool vf_has_codes(VfApp* app) {
    return app->dir.code_count > 0;
}

static uint32_t vf_home_count(VfApp* app) {
    return app->dir.account_count + HomeExtraCount - (vf_has_codes(app) ? 0 : 1);
}

static VfHomeExtra vf_home_extra(VfApp* app, uint32_t index) {
    uint32_t i = index - app->dir.account_count + (vf_has_codes(app) ? 0 : 1);
    return i < HomeExtraCount ? (VfHomeExtra)i : HomeLock;
}

static uint32_t vf_home_index(VfApp* app, VfHomeExtra extra) {
    return app->dir.account_count + extra - (vf_has_codes(app) ? 0 : 1);
}

static void vf_home_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    if(index < app->dir.account_count) {
        const char* acc = app->dir.accounts[index];
        item->title = acc[0] ? acc : T(app, S_NoAccount);
        item->icon = &I_account;
        int n = vf_count_str(app, sub, size, app->acc_logins[index], S_OneLogin, S_NLogins);
        if(n > 0 && (size_t)n + 2 < size) {
            sub[n] = ',';
            sub[n + 1] = ' ';
            vf_count_str(
                app, sub + n + 2, size - n - 2, app->acc_folders[index], S_OneFolder, S_NFolders);
        }
        item->subtitle = sub;
        return;
    }
    switch(vf_home_extra(app, index)) {
    case HomeCodes:
        item->title = T(app, S_Codes);
        vf_count_str(app, sub, size, app->dir.code_count, S_OneCode, S_NCodes);
        item->subtitle = sub;
        item->icon = &I_totp;
        break;
    case HomeSearch:
        item->title = T(app, S_Search);
        item->subtitle = T(app, S_SearchSub);
        item->icon = &I_search;
        break;
    case HomeSettings:
        item->title = T(app, S_Settings);
        snprintf(sub, size, "%s: %s", T(app, S_Keyboard), app->hid.name);
        item->subtitle = sub;
        item->icon = &I_settings;
        break;
    default:
        item->title = T(app, S_Lock);
        item->subtitle = T(app, S_LockSub);
        item->icon = &I_lock;
        break;
    }
}

static void vf_home_select(void* ctx, uint32_t index);

static void vf_show_home(VfApp* app, uint32_t selected) {
    vf_list_set(
        app->home,
        "VaultFlipper",
        &I_lock,
        vf_home_count(app),
        VfListOpen,
        vf_home_item,
        vf_home_select,
        app,
        selected);
    vf_switch(app, ViewHome);
}

/* ---------- folders and entries ---------- */

static void vf_folder_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    uint16_t p = app->folder_pages[index];
    const VfPageInfo* pi = &app->dir.pages[p];
    if(vf_is_codes(app, p)) {
        /* the 2FA list in parts: "Amazon - GitHub" */
        snprintf(sub, size, "%.12s - %.12s", pi->first, pi->last);
        item->title = sub;
        item->icon = &I_totp;
        return;
    }
    item->title = vf_group_name(app, p);
    item->icon = &I_folder;
    int n = vf_count_str(app, sub, size, pi->count, S_OneLogin, S_NLogins);
    if(vf_page_split(app, p) && n > 0 && (size_t)n < size) {
        snprintf(sub + n, size - n, "  %.3s - %.3s", pi->first, pi->last);
    }
    item->subtitle = sub;
}

/* Every row shows name and username (or the host), so logins with the same
 * name are easy to tell apart. */
static void vf_entry_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    const VfRecord* e = &app->page.entries[index];
    item->title = e->f[VfFieldName][0] ? e->f[VfFieldName] : T(app, S_NoName);
    if(e->f[VfFieldUser][0]) {
        item->subtitle = e->f[VfFieldUser];
    } else {
        vf_uri_host(e->f[VfFieldUri], sub, size);
        item->subtitle = sub;
    }
}

static void vf_code_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    const VfRecord* e = &app->page.entries[index];
    item->title = e->f[VfFieldName][0] ? e->f[VfFieldName] : T(app, S_NoName);
    item->subtitle = e->f[VfFieldUser];
    if(app->codes && app->codes_page == app->page.index && app->codes[index].ok)
        vf_group_code(app->codes[index].code, sub, size);
    else
        strlcpy(sub, "--- ---", size);
    item->value = sub;
}

static void vf_folder_select(void* ctx, uint32_t index);
static void vf_entry_select(void* ctx, uint32_t index);
static void vf_code_select(void* ctx, uint32_t index);
static void vf_card_action(void* ctx, VfCardAction action);

static void vf_open_entries(VfApp* app, uint16_t page, VfViewId back_to, uint16_t selected) {
    VfResult r = vf_open_page(app, page);
    if(r != VfOk) {
        vf_show_error(app, r, back_to);
        return;
    }
    char header[40];
    const VfPageInfo* pi = &app->dir.pages[page];
    if(vf_page_split(app, page)) {
        snprintf(
            header, sizeof(header), "%s %.2s-%.2s", vf_group_name(app, page), pi->first, pi->last);
    } else {
        strlcpy(header, vf_group_name(app, page), sizeof(header));
    }
    bool codes = vf_is_codes(app, page);
    vf_list_set(
        app->entries,
        header,
        codes ? &I_totp : &I_folder,
        app->page.count,
        VfListDetail,
        codes ? vf_code_item : vf_entry_item,
        codes ? vf_code_select : vf_entry_select,
        app,
        selected);
    if(codes) {
        app->codes_second = 0;
        vf_codes_refresh(app);
    }
    app->entries_return = back_to;
    vf_switch(app, ViewEntries);
}

static void vf_open_account(VfApp* app, uint8_t account) {
    app->cur_account = account;
    free(app->folder_pages);
    app->folder_pages = NULL;
    app->folder_count = 0;
    if(!vf_can_alloc(sizeof(uint16_t) * app->dir.page_count)) {
        vf_show_text(app, T(app, S_NoMemory), ViewHome);
        return;
    }
    app->folder_pages = malloc(sizeof(uint16_t) * app->dir.page_count);
    for(uint16_t i = 0; i < app->dir.page_count; i++) {
        if(app->dir.pages[i].account == account) app->folder_pages[app->folder_count++] = i;
    }
    if(app->folder_count == 1) {
        vf_open_entries(app, app->folder_pages[0], ViewHome, 0); /* one folder: skip the list */
        return;
    }
    bool codes = account == VF_CODES_ACCOUNT;
    vf_list_set(
        app->folders,
        codes ? T(app, S_Codes) : app->dir.accounts[account],
        codes ? &I_totp : &I_account,
        app->folder_count,
        codes ? VfListPlain : VfListOpen,
        vf_folder_item,
        vf_folder_select,
        app,
        0);
    vf_switch(app, ViewFolders);
}

static void vf_open_card(VfApp* app, uint16_t entry, VfViewId back_to) {
    app->cur_entry = entry;
    app->card_return = back_to;
    const VfRecord* e = &app->page.entries[entry];
    snprintf(app->card_note, sizeof(app->card_note), "%u/%u", entry + 1, app->page.count);
    const char* labels[4] = {
        T(app, S_BtnUser), T(app, S_BtnPass), T(app, S_BtnLogin), T(app, S_ShowHint)};
    vf_card_set(
        app->card,
        e->f[VfFieldName][0] ? e->f[VfFieldName] : T(app, S_NoName),
        e->f[VfFieldUser],
        e->f[VfFieldPass],
        e->f[VfFieldUri],
        app->card_note,
        labels,
        vf_card_action,
        app);
    vf_switch(app, ViewCard);
}

static void vf_folder_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index < app->folder_count) vf_open_entries(app, app->folder_pages[index], ViewFolders, 0);
}

static void vf_entry_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index < app->page.count) vf_open_card(app, (uint16_t)index, ViewEntries);
}

static void vf_code_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index < app->page.count) vf_open_code(app, (uint16_t)index, ViewEntries);
}

/* ---------- entry card actions ---------- */

static void vf_type_entry(VfApp* app, VfMore what) {
    const VfRecord* e = &app->page.entries[app->cur_entry];
    VfViewId back = app->current;
    if(!vf_type_begin(app, back)) return;
    switch(what) {
    case MoreLoginEnter:
    case MoreLogin:
        vf_hid_type(&app->hid, e->f[VfFieldUser]);
        vf_hid_key(&app->hid, HID_KEYBOARD_TAB);
        vf_hid_type(&app->hid, e->f[VfFieldPass]);
        if(what == MoreLoginEnter) vf_hid_key(&app->hid, HID_KEYBOARD_RETURN);
        break;
    case MoreUser:
        vf_hid_type(&app->hid, e->f[VfFieldUser]);
        break;
    case MorePass:
        vf_hid_type(&app->hid, e->f[VfFieldPass]);
        break;
    default:
        break;
    }
    vf_type_end(app, back);
}

static void vf_show_details(VfApp* app) {
    const VfRecord* e = &app->page.entries[app->cur_entry];
    FuriString* s = furi_string_alloc_printf("\e#%s\n", e->f[VfFieldName]);
    if(e->f[VfFieldUser][0])
        furi_string_cat_printf(s, "%s: %s\n", T(app, S_User), e->f[VfFieldUser]);
    if(e->f[VfFieldUri][0]) furi_string_cat_printf(s, "%s: %s\n", T(app, S_Url), e->f[VfFieldUri]);
    furi_string_cat_printf(
        s,
        "%s: %s / %s\n",
        T(app, S_Group),
        vf_is_codes(app, app->page.index) ?
            T(app, S_Codes) :
            app->dir.accounts[app->dir.pages[app->page.index].account],
        vf_group_name(app, app->page.index));
    if(e->f[VfFieldTotp][0]) furi_string_cat_str(s, T(app, S_TotpStored));
    vf_show_text(app, furi_string_get_cstr(s), ViewMore);
    vf_wipe((void*)furi_string_get_cstr(s), furi_string_size(s));
    furi_string_free(s);
}

static bool vf_cur_has_code(VfApp* app) {
    return app->page.entries[app->cur_entry].f[VfFieldTotp][0] != 0;
}

static VfMore vf_more_at(VfApp* app, uint32_t index) {
    if(index >= MoreCode && !vf_cur_has_code(app)) index++;
    return index < MoreCount ? (VfMore)index : MoreDetails;
}

static void vf_more_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    UNUSED(sub);
    UNUSED(size);
    static const VfStr titles[MoreCount] = {
        S_TypeLoginEnter, S_TypeLogin, S_TypeUser, S_TypePass, S_ShowCode, S_Details};
    static const Icon* const icons[MoreCount] = {
        &I_key, &I_key, &I_user, &I_lock, &I_totp, &I_info};
    VfMore m = vf_more_at(app, index);
    item->title = T(app, titles[m]);
    item->icon = icons[m];
}

static void vf_more_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    VfMore m = vf_more_at(app, index);
    if(m == MoreDetails) {
        vf_show_details(app);
    } else if(m == MoreCode) {
        vf_open_code(app, app->cur_entry, ViewMore);
    } else {
        vf_type_entry(app, m);
    }
}

static void vf_card_action(void* ctx, VfCardAction action) {
    VfApp* app = ctx;
    /* runs in the input handler; defer to the event loop */
    view_dispatcher_send_custom_event(app->vd, EvCardBase + action);
}

static void vf_card_event(VfApp* app, VfCardAction action) {
    switch(action) {
    case VfCardTypeUser:
        vf_type_entry(app, MoreUser);
        break;
    case VfCardTypePass:
        vf_type_entry(app, MorePass);
        break;
    case VfCardTypeLogin:
        vf_type_entry(app, MoreLoginEnter);
        break;
    case VfCardMore:
        vf_list_set(
            app->more,
            T(app, S_More),
            &I_key,
            MoreCount - (vf_cur_has_code(app) ? 0 : 1),
            VfListPlain,
            vf_more_item,
            vf_more_select,
            app,
            0);
        vf_switch(app, ViewMore);
        break;
    }
}

/* ---------- search ---------- */

static void vf_result_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    const VfHit* h = &app->hits[index];
    item->title = h->name[0] ? h->name : T(app, S_NoName);
    if(h->user[0]) {
        item->subtitle = h->user;
    } else {
        snprintf(sub, size, "%s", vf_group_name(app, h->page));
        item->subtitle = sub;
    }
}

static void vf_result_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index >= app->hit_count) return;
    VfResult r = vf_open_page(app, app->hits[index].page);
    if(r != VfOk) {
        vf_show_error(app, r, ViewResults);
        return;
    }
    vf_open_card(app, app->hits[index].idx, ViewResults);
}

static void vf_search_done_cb(void* ctx) {
    VfApp* app = ctx;
    view_dispatcher_send_custom_event(app->vd, EvSearch);
}

static void vf_open_search(VfApp* app) {
    text_input_reset(app->search);
    text_input_set_header_text(app->search, T(app, S_SearchHeader));
    text_input_set_minimum_length(app->search, 1);
    text_input_set_result_callback(
        app->search, vf_search_done_cb, app, app->query, sizeof(app->query), true);
    vf_switch(app, ViewSearch);
}

static void vf_run_search(VfApp* app) {
    vf_busy(app, S_Searching, &I_search);
    app->hit_count = 0;
    for(uint16_t p = 0; p < app->dir.page_count && app->hit_count < VF_MAX_HITS; p++) {
        if(vf_is_codes(app, p) || vf_open_page(app, p) != VfOk) continue;
        for(uint16_t i = 0; i < app->page.count && app->hit_count < VF_MAX_HITS; i++) {
            const VfRecord* e = &app->page.entries[i];
            if(!vf_contains(e->f[VfFieldName], app->query) &&
               !vf_contains(e->f[VfFieldUser], app->query) &&
               !vf_contains(e->f[VfFieldUri], app->query))
                continue;
            VfHit* h = &app->hits[app->hit_count++];
            h->page = p;
            h->idx = i;
            strlcpy(h->name, e->f[VfFieldName], sizeof(h->name));
            if(e->f[VfFieldUser][0])
                strlcpy(h->user, e->f[VfFieldUser], sizeof(h->user));
            else
                vf_uri_host(e->f[VfFieldUri], h->user, sizeof(h->user));
        }
    }
    if(app->hit_count == 0) {
        vf_show_text(app, T(app, S_NoMatches), ViewHome);
        return;
    }
    char header[40];
    snprintf(header, sizeof(header), "%s: %s", T(app, S_Results), app->query);
    vf_list_set(
        app->results,
        header,
        &I_search,
        app->hit_count,
        VfListDetail,
        vf_result_item,
        vf_result_select,
        app,
        0);
    vf_switch(app, ViewResults);
}

/* ---------- settings ---------- */

static void vf_setting_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    switch(index) {
    case SetKeyboard:
        item->title = T(app, S_Keyboard);
        item->subtitle = app->hid.name;
        item->icon = &I_keyboard;
        break;
    case SetMacSetup:
        item->title = T(app, S_MacSetup);
        item->subtitle = "";
        item->icon = &I_laptop;
        break;
    case SetTypingTest:
        item->title = T(app, S_TypingTest);
        item->subtitle = T(app, S_TypingTestSub);
        item->icon = &I_check;
        break;
    case SetTimeZone:
        item->title = T(app, S_TimeZone);
        vf_format_zone(app->clock_offset, sub, size);
        item->subtitle = sub;
        item->icon = &I_totp;
        break;
    case SetLanguage:
        item->title = T(app, S_Language);
        item->subtitle = T(app, S_LangName);
        item->icon = &I_language;
        break;
    case SetHelp:
        item->title = T(app, S_Help);
        item->subtitle = "";
        item->icon = &I_help;
        break;
    default:
        item->title = T(app, S_About);
        item->subtitle = app->fingerprint;
        item->icon = &I_info;
        break;
    }
}

static void vf_setting_select(void* ctx, uint32_t index);

static void vf_show_settings(VfApp* app, uint32_t selected) {
    vf_list_set(
        app->settings,
        T(app, S_Settings),
        &I_settings,
        SetCount,
        VfListOpen,
        vf_setting_item,
        vf_setting_select,
        app,
        selected);
    vf_switch(app, ViewSettings);
}

static void vf_layout_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    UNUSED(sub);
    UNUSED(size);
    item->title = app->layout_names[index];
    item->icon = strcmp(app->layout_names[index], app->hid.name) == 0 ? &I_check : NULL;
}

static void vf_layout_found(const char* name, void* ctx) {
    VfApp* app = ctx;
    if(app->layout_count >= VF_MAX_LAYOUTS) return;
    strlcpy(app->layout_names[app->layout_count++], name, sizeof(app->layout_names[0]));
}

static void vf_layout_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index >= app->layout_count) return;
    const char* name = app->layout_names[index];
    if(vf_hid_load_layout(&app->hid, app->storage, name)) {
        vf_file_write_small(app, VF_LAYOUT_PATH, name, strlen(name));
    }
    vf_show_settings(app, SetKeyboard);
}

static void vf_open_layouts(VfApp* app) {
    app->layout_count = 0;
    vf_hid_list_layouts(app->storage, vf_layout_found, app);
    uint32_t sel = 0;
    for(uint8_t i = 0; i < app->layout_count; i++) {
        if(strcmp(app->layout_names[i], app->hid.name) == 0) sel = i;
    }
    vf_list_set(
        app->layouts,
        T(app, S_Keyboard),
        &I_keyboard,
        app->layout_count,
        VfListPlain,
        vf_layout_item,
        vf_layout_select,
        app,
        sel);
    vf_switch(app, ViewLayouts);
}

/* Time zone: whole hours around the offset measured at sync. The extra
 * seconds (half-hour zones, clock drift) are kept. */
static int32_t vf_tz_at(VfApp* app, uint32_t index) {
    return ((int32_t)index - 12) * 3600 + app->tz_frac;
}

static void vf_tz_item(void* ctx, uint32_t index, VfListItem* item, char* sub, size_t size) {
    VfApp* app = ctx;
    int32_t off = vf_tz_at(app, index);
    vf_format_zone(off, sub, size);
    item->title = sub;
    item->icon = off == app->clock_offset ? &I_check : NULL;
}

static void vf_tz_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index >= VF_TZ_ITEMS) return;
    app->clock_offset = vf_tz_at(app, index);
    char line[16];
    int n = snprintf(line, sizeof(line), "%ld\n", (long)app->clock_offset);
    vf_file_write_small(app, VF_CLOCK_PATH, line, (size_t)n);
    app->codes_second = 0;
    vf_show_settings(app, SetTimeZone);
}

static void vf_open_timezones(VfApp* app) {
    int32_t off = app->clock_offset;
    int32_t hours = (off >= 0 ? off + 1800 : off - 1800) / 3600;
    app->tz_frac = off - hours * 3600;
    int32_t sel = hours + 12;
    if(sel < 0) sel = 0;
    if(sel >= VF_TZ_ITEMS) sel = VF_TZ_ITEMS - 1;
    vf_list_set(
        app->timezones,
        T(app, S_TimeZone),
        &I_totp,
        VF_TZ_ITEMS,
        VfListPlain,
        vf_tz_item,
        vf_tz_select,
        app,
        (uint32_t)sel);
    vf_switch(app, ViewTimezone);
}

static void vf_typing_test(VfApp* app) {
    /* Types every printable ASCII character, then Enter.
     * Compare on the computer with: vaultflipper typetest */
    char all[95];
    for(int c = 33; c < 127; c++)
        all[c - 33] = (char)c;
    all[94] = 0;
    if(!vf_type_begin(app, ViewSettings)) return;
    vf_hid_type(&app->hid, all);
    vf_hid_key(&app->hid, HID_KEYBOARD_RETURN);
    vf_type_end(app, ViewSettings);
}

/* Mac keyboard setup: macOS asks new keyboards to press the key right of the
 * left Shift and the key left of the right Shift to tell ISO, ANSI and JIS
 * apart. The Flipper presses them on request, matching the selected layout. */

static void vf_mac_button_cb(GuiButtonType button, InputType type, void* ctx) {
    VfApp* app = ctx;
    if(button == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->vd, EvMacButton);
    }
}

static void vf_mac_render(VfApp* app) {
    static const VfStr texts[] = {S_MacStep1, S_MacStep2, S_MacDone};
    widget_reset(app->mac_setup);
    widget_add_icon_element(app->mac_setup, 116, 2, &I_laptop);
    widget_add_string_multiline_element(
        app->mac_setup, 0, 0, AlignLeft, AlignTop, FontSecondary, T(app, texts[app->mac_step]));
    widget_add_button_element(
        app->mac_setup,
        GuiButtonTypeCenter,
        T(app, app->mac_step < 2 ? S_SendKey : S_Ok),
        vf_mac_button_cb,
        app);
}

static void vf_mac_button(VfApp* app) {
    if(app->mac_step >= 2) {
        vf_show_settings(app, SetMacSetup);
        return;
    }
    if(!vf_type_begin(app, ViewMacSetup)) return;
    uint16_t key = app->mac_step == 0 ?
                       (vf_hid_is_iso(&app->hid) ? HID_KEYBOARD_NONUS_BACKSLASH : HID_KEYBOARD_Z) :
                       HID_KEYBOARD_SLASH;
    vf_hid_key(&app->hid, key);
    app->mac_step++;
    vf_mac_render(app);
    vf_type_end(app, ViewMacSetup);
}

static void vf_show_about(VfApp* app) {
    char zone[16];
    vf_format_zone(app->clock_offset, zone, sizeof(zone));
    FuriString* s = furi_string_alloc_printf(
        T(app, S_AboutBody),
        app->fingerprint,
        app->dir.meta.f[VfFieldSynced],
        (unsigned long)app->dir.entry_count,
        app->dir.page_count,
        (unsigned long)app->dir.code_count,
        zone,
        app->hid.name,
        T(app, vf_hid_connected(&app->hid) ? S_UsbOn : S_UsbOff),
        app->unlock_ms,
        app->heap_max_block);
    vf_show_text(app, furi_string_get_cstr(s), ViewSettings);
    furi_string_free(s);
}

static void vf_setting_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    switch(index) {
    case SetKeyboard:
        vf_open_layouts(app);
        break;
    case SetMacSetup:
        app->mac_step = 0;
        vf_mac_render(app);
        vf_switch(app, ViewMacSetup);
        break;
    case SetTypingTest:
        vf_typing_test(app);
        break;
    case SetTimeZone:
        vf_open_timezones(app);
        break;
    case SetLanguage:
        app->lang = (app->lang + 1) % VfLangCount;
        vf_file_write_small(app, VF_LANG_PATH, app->lang == VfLangDe ? "de" : "en", 2);
        vf_show_settings(app, SetLanguage);
        break;
    case SetHelp:
        vf_show_text(app, T(app, S_HelpBody), ViewSettings);
        break;
    default:
        vf_show_about(app);
        break;
    }
}

static void vf_home_select(void* ctx, uint32_t index) {
    VfApp* app = ctx;
    if(index < app->dir.account_count) {
        vf_open_account(app, (uint8_t)index);
        return;
    }
    switch(vf_home_extra(app, index)) {
    case HomeCodes:
        vf_open_account(app, VF_CODES_ACCOUNT);
        break;
    case HomeSearch:
        vf_open_search(app);
        break;
    case HomeSettings:
        vf_show_settings(app, 0);
        break;
    default:
        vf_lock(app, S_Locked);
        break;
    }
}

/* ---------- unlock ---------- */

static VfResult vf_unlock_vault(VfApp* app, const char* pin, size_t len) {
    app->vault_file = storage_file_alloc(app->storage);
    if(!storage_file_open(app->vault_file, VF_VAULT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        return VfErrIo;
    }
    VfResult r = vf_file_open(&app->vf, vf_vault_read, app);
    if(r != VfOk) return r;
    size_t argon = (size_t)app->vf.blocks * 1024u;
    size_t reserved = app->argon_work ? VF_ARGON_SIZE : 0;
    size_t dir_mem = app->vf.dir_len + (app->vf.dir_len / 12u + 1u) * sizeof(VfPageInfo);
    if((argon > reserved && !vf_can_alloc(argon)) || !vf_can_alloc(dir_mem)) {
        return VfErrMemory;
    }
    return vf_file_unlock(&app->vf, app->dev_sk, pin, len, app->argon_work, reserved, &app->dir);
}

static void vf_count_accounts(VfApp* app) {
    memset(app->acc_logins, 0, sizeof(app->acc_logins));
    memset(app->acc_folders, 0, sizeof(app->acc_folders));
    for(uint16_t i = 0; i < app->dir.page_count; i++) {
        const VfPageInfo* pi = &app->dir.pages[i];
        if(pi->account == VF_CODES_ACCOUNT) continue;
        app->acc_logins[pi->account] += pi->count;
        bool continues = i > 0 && app->dir.pages[i - 1].account == pi->account &&
                         strcmp(app->dir.pages[i - 1].group, pi->group) == 0;
        if(!continues) app->acc_folders[pi->account]++;
    }
}

static void vf_try_unlock(VfApp* app) {
    char pin[VF_MAX_PIN + 1];
    uint8_t len = 0;
    with_view_model(
        app->pin_view,
        PinModel * m,
        {
            memcpy(pin, m->seq, sizeof(pin));
            len = m->len;
            vf_wipe(m->seq, sizeof(m->seq));
            m->len = 0;
        },
        false);
    pin[len] = 0;

    vf_busy(app, S_Unlocking, &I_lock);
    if(app->fails >= 3) furi_delay_ms(1000u * app->fails); /* slow down repeated guessing */

    /* Count the try before it runs, so a reset during Argon2 cannot skip it. */
    uint8_t counted = app->fails < 255 ? app->fails + 1 : 255;
    vf_file_write_small(app, VF_FAILS_PATH, &counted, 1);

    uint32_t t0 = furi_get_tick();
    VfResult r = vf_unlock_vault(app, pin, len);
    vf_wipe(pin, sizeof(pin));
    app->unlock_ms = furi_get_tick() - t0;
    app->heap_max_block = memmgr_heap_get_max_free_block();
    FURI_LOG_I(
        TAG,
        "unlock r=%d in %lu ms, %lu entries, %u pages",
        r,
        app->unlock_ms,
        (unsigned long)app->dir.entry_count,
        app->dir.page_count);

    if(r == VfOk) {
        app->fails = 0;
        vf_file_write_small(app, VF_FAILS_PATH, &app->fails, 1);
        app->unlocked = true;
        vf_count_accounts(app);
        vf_apply_layout(app);
#ifndef VF_TEST_NO_HID
        vf_hid_start(&app->hid); /* become a USB keyboard while unlocked */
#endif
        furi_timer_start(app->idle_timer, VF_IDLE_MS);
        app->tick_pending = false;
        furi_timer_start(app->tick_timer, VF_TICK_MS);
        vf_feedback(app, true);
        vf_show_home(app, 0);
        return;
    }

    vf_close_vault(app);
    vf_feedback(app, false);
    if(r != VfErrAuth) vf_file_write_small(app, VF_FAILS_PATH, &app->fails, 1); /* not a guess */
    if(r == VfErrAuth) {
        app->fails = counted;
        if(app->fails >= VF_MAX_FAILS) {
            storage_simply_remove(app->storage, VF_VAULT_PATH);
            app->fails = 0;
            vf_file_write_small(app, VF_FAILS_PATH, &app->fails, 1);
            app->exit_after_text = true;
            vf_show_text(app, T(app, S_VaultWiped), ViewPin);
            return;
        }
        char msg[40];
        snprintf(msg, sizeof(msg), T(app, S_WrongPin), app->fails, VF_MAX_FAILS);
        vf_pin_reset(app, msg);
    } else if(r == VfErrMemory) {
        vf_pin_reset(app, T(app, S_NoMemory));
    } else if(r == VfErrParams) {
        vf_pin_reset(app, T(app, S_BadParams));
    } else if(r == VfErrIo) {
        vf_pin_reset(app, T(app, S_IoError));
    } else {
        vf_pin_reset(app, T(app, S_BadFile));
    }
    vf_switch(app, ViewPin);
}

/* ---------- PIN view ---------- */

static void vf_draw_dpad(Canvas* canvas, int cx, int cy) {
    canvas_draw_triangle(canvas, cx, cy - 4, 5, 3, CanvasDirectionBottomToTop);
    canvas_draw_triangle(canvas, cx, cy + 4, 5, 3, CanvasDirectionTopToBottom);
    canvas_draw_triangle(canvas, cx - 4, cy, 5, 3, CanvasDirectionRightToLeft);
    canvas_draw_triangle(canvas, cx + 4, cy, 5, 3, CanvasDirectionLeftToRight);
}

static void vf_pin_draw(Canvas* canvas, void* model) {
    PinModel* m = model;
    canvas_clear(canvas);
    vf_draw_header(canvas, "VaultFlipper", NULL, &I_lock);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, m->msg);

    /* PIN slots: 8 shown at least, filled as you press */
    uint8_t slots = m->len > 8 ? m->len : 8;
    if(slots > 14) slots = 14;
    int w = slots * 8 - 2;
    int x0 = 64 - w / 2;
    for(uint8_t i = 0; i < slots; i++) {
        int x = x0 + i * 8;
        if(i < m->len)
            canvas_draw_rbox(canvas, x, 28, 6, 8, 1);
        else
            canvas_draw_rframe(canvas, x, 28, 6, 8, 1);
    }
    if(m->len > 14) {
        char more[6];
        snprintf(more, sizeof(more), "%u", m->len);
        canvas_draw_str_aligned(canvas, 127, 32, AlignRight, AlignCenter, more);
    }

    vf_draw_dpad(canvas, 8, 57);
    canvas_draw_str(canvas, 16, 61, "PIN");
    if(m->len > 0) elements_button_center(canvas, vf_strings[m->lang][S_PinOk]);
    /* Back key glyph + "Del" */
    const char* del = vf_strings[m->lang][S_PinDel];
    int dw = canvas_string_width(canvas, del);
    int bx = 126 - dw - 10, by = 55;
    canvas_draw_line(canvas, bx + 1, by, bx + 5, by);
    canvas_draw_line(canvas, bx + 6, by + 1, bx + 6, by + 3);
    canvas_draw_line(canvas, bx + 5, by + 4, bx + 2, by + 4);
    canvas_draw_dot(canvas, bx + 2, by - 1);
    canvas_draw_dot(canvas, bx + 2, by + 1);
    canvas_draw_str_aligned(canvas, 126, 61, AlignRight, AlignBottom, del);
}

static bool vf_pin_input(InputEvent* event, void* ctx) {
    VfApp* app = ctx;
    if(event->type != InputTypeShort) return true; /* a long Back must not quit by accident */
    char sym = 0;
    switch(event->key) {
    case InputKeyUp:
        sym = 'U';
        break;
    case InputKeyDown:
        sym = 'D';
        break;
    case InputKeyLeft:
        sym = 'L';
        break;
    case InputKeyRight:
        sym = 'R';
        break;
    case InputKeyOk: {
        bool has = false;
        with_view_model(app->pin_view, PinModel * m, { has = m->len > 0; }, false);
        if(has) view_dispatcher_send_custom_event(app->vd, EvPinSubmit);
        return true;
    }
    case InputKeyBack: {
        bool consumed = false;
        with_view_model(
            app->pin_view,
            PinModel * m,
            {
                if(m->len > 0) {
                    m->seq[--m->len] = 0;
                    consumed = true;
                }
            },
            true);
        return consumed; /* empty PIN: Back quits the app */
    }
    default:
        return false;
    }
    with_view_model(
        app->pin_view,
        PinModel * m,
        {
            if(m->len < VF_MAX_PIN) m->seq[m->len++] = sym;
            m->msg[0] = 0;
        },
        true);
    return true;
}

/* ---------- events ---------- */

static void vf_input_hook(const void* message, void* ctx) {
    /* Runs in the input thread, synchronously on publish - before the
     * ViewDispatcher handles the same event. The ViewDispatcher treats a long
     * Back as navigation, so navigation checks this flag and locks instead. */
    VfApp* app = ctx;
    const InputEvent* ev = message;
    if(ev->key == InputKeyBack) {
        if(ev->type == InputTypePress) app->back_long = false;
        if(ev->type == InputTypeLong) app->back_long = app->unlocked;
    }
    if(app->unlocked) furi_timer_restart(app->idle_timer, VF_IDLE_MS);
}

static void vf_idle_cb(void* ctx) {
    VfApp* app = ctx;
    view_dispatcher_send_custom_event(app->vd, EvIdle);
}

static void vf_tick_cb(void* ctx) {
    VfApp* app = ctx;
    /* at most one tick in the queue, so a busy event loop never blocks the timer thread */
    if(app->tick_pending) return;
    app->tick_pending = true;
    view_dispatcher_send_custom_event(app->vd, EvTick);
}

static void vf_tick(VfApp* app) {
    app->tick_pending = false;
    if(!app->unlocked) return;
    if(app->current == ViewEntries) {
        vf_codes_refresh(app); /* redraws via the status line when a second passed */
    } else if(app->current == ViewCode) {
        uint32_t second = furi_hal_rtc_get_timestamp();
        if(second != app->codes_second) {
            app->codes_second = second;
            vf_code_refresh(app);
        }
    }
}

static bool vf_custom_event(void* ctx, uint32_t event) {
    VfApp* app = ctx;
    if(event >= EvCardBase && event <= EvCardBase + VfCardMore) {
        if(app->unlocked) vf_card_event(app, (VfCardAction)(event - EvCardBase));
        return true;
    }
    if(event >= EvCodeBase && event <= EvCodeBase + VfCodeNext) {
        if(app->unlocked) vf_code_event(app, (VfCodeAction)(event - EvCodeBase));
        return true;
    }
    switch(event) {
    case EvTick:
        vf_tick(app);
        return true;
    case EvPinSubmit:
        vf_try_unlock(app);
        return true;
    case EvMacButton:
        if(app->unlocked) vf_mac_button(app);
        return true;
    case EvSearch:
        if(app->unlocked) vf_run_search(app);
        return true;
    case EvIdle:
        if(app->unlocked) vf_lock(app, S_AutoLocked);
        return true;
    default:
        return false;
    }
}

static bool vf_navigation(void* ctx) {
    VfApp* app = ctx;
    if(app->back_long) {
        app->back_long = false;
        if(app->unlocked) vf_lock(app, S_Locked);
        return true;
    }
    switch(app->current) {
    case ViewFolders:
    case ViewSearch:
    case ViewResults:
        vf_switch(app, ViewHome);
        return true;
    case ViewSettings:
        vf_show_home(app, vf_home_index(app, HomeSettings));
        return true;
    case ViewLayouts:
        vf_show_settings(app, SetKeyboard);
        return true;
    case ViewTimezone:
        vf_show_settings(app, SetTimeZone);
        return true;
    case ViewCode:
        if(app->code_return == ViewEntries && app->page.buf) {
            vf_open_entries(app, app->page.index, app->entries_return, app->code_entry);
        } else {
            vf_switch(app, app->code_return);
        }
        return true;
    case ViewMacSetup:
        vf_show_settings(app, SetMacSetup);
        return true;
    case ViewEntries:
        vf_switch(app, app->entries_return);
        return true;
    case ViewCard:
        vf_card_hide_password(app->card);
        vf_switch(app, app->card_return);
        return true;
    case ViewMore:
        vf_open_card(app, app->cur_entry, app->card_return);
        return true;
    case ViewText:
        if(app->exit_after_text) return false;
        vf_switch(app, app->text_return);
        return true;
    case ViewBusy:
        return true;
    default:
        return false; /* PIN or home: quit (secrets are wiped on exit) */
    }
}

/* ---------- lifecycle ---------- */

static VfApp* vf_app_alloc(void) {
    VfApp* app = malloc(sizeof(VfApp));
    memset(app, 0, sizeof(*app));
    /* Reserve the Argon2id work area first, while the heap is still in one piece.
     * malloc never fails on the Flipper - it reboots - so check first; without
     * the reserve, unlocking tries again and reports "Out of memory". */
    if(vf_can_alloc(VF_ARGON_SIZE)) app->argon_work = malloc(VF_ARGON_SIZE);
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    storage_simply_mkdir(app->storage, VF_DIR);

    char lang[4];
    vf_read_setting(app, VF_LANG_PATH, lang, sizeof(lang));
    app->lang = strcmp(lang, "de") == 0 ? VfLangDe : VfLangEn;

    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, vf_custom_event);
    view_dispatcher_set_navigation_event_callback(app->vd, vf_navigation);
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    app->pin_view = view_alloc();
    view_allocate_model(app->pin_view, ViewModelTypeLocking, sizeof(PinModel));
    view_set_context(app->pin_view, app);
    view_set_draw_callback(app->pin_view, vf_pin_draw);
    view_set_input_callback(app->pin_view, vf_pin_input);
    view_dispatcher_add_view(app->vd, ViewPin, app->pin_view);

    app->home = vf_list_alloc();
    app->folders = vf_list_alloc();
    app->entries = vf_list_alloc();
    app->more = vf_list_alloc();
    app->settings = vf_list_alloc();
    app->layouts = vf_list_alloc();
    app->results = vf_list_alloc();
    app->timezones = vf_list_alloc();
    view_dispatcher_add_view(app->vd, ViewHome, vf_list_get_view(app->home));
    view_dispatcher_add_view(app->vd, ViewFolders, vf_list_get_view(app->folders));
    view_dispatcher_add_view(app->vd, ViewEntries, vf_list_get_view(app->entries));
    view_dispatcher_add_view(app->vd, ViewMore, vf_list_get_view(app->more));
    view_dispatcher_add_view(app->vd, ViewSettings, vf_list_get_view(app->settings));
    view_dispatcher_add_view(app->vd, ViewLayouts, vf_list_get_view(app->layouts));
    view_dispatcher_add_view(app->vd, ViewResults, vf_list_get_view(app->results));
    view_dispatcher_add_view(app->vd, ViewTimezone, vf_list_get_view(app->timezones));
    app->code = vf_code_alloc();
    view_dispatcher_add_view(app->vd, ViewCode, vf_code_get_view(app->code));
    app->card = vf_card_alloc();
    view_dispatcher_add_view(app->vd, ViewCard, vf_card_get_view(app->card));
    app->busy = vf_busy_alloc();
    view_dispatcher_add_view(app->vd, ViewBusy, vf_busy_get_view(app->busy));
    app->text = widget_alloc();
    view_dispatcher_add_view(app->vd, ViewText, widget_get_view(app->text));
    app->mac_setup = widget_alloc();
    view_dispatcher_add_view(app->vd, ViewMacSetup, widget_get_view(app->mac_setup));
    app->search = text_input_alloc();
    view_dispatcher_add_view(app->vd, ViewSearch, text_input_get_view(app->search));
    vf_views_clear(app);

    app->idle_timer = furi_timer_alloc(vf_idle_cb, FuriTimerTypeOnce, app);
    app->tick_timer = furi_timer_alloc(vf_tick_cb, FuriTimerTypePeriodic, app);
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    app->input_sub = furi_pubsub_subscribe(app->input_events, vf_input_hook, app);

    vf_hid_init(&app->hid);
    vf_file_read_small(app, VF_FAILS_PATH, &app->fails, 1);
    char clock[16];
    vf_read_setting(app, VF_CLOCK_PATH, clock, sizeof(clock));
    long offset = strtol(clock, NULL, 10);
    if(offset < -VF_CLOCK_MAX_OFFSET || offset > VF_CLOCK_MAX_OFFSET) offset = 0;
    app->clock_offset = (int32_t)offset;

    uint8_t secret[32];
    app->device_ok = vf_device_secret(secret);
    if(app->device_ok) {
        vf_device_keypair(secret, app->dev_sk, app->dev_pk);
        vf_fingerprint(app->dev_pk, app->fingerprint);
        vf_write_pubkey(app);
    } else {
        strlcpy(app->fingerprint, "ERROR", sizeof(app->fingerprint));
    }
    vf_wipe(secret, sizeof(secret));
    return app;
}

static void vf_app_free(VfApp* app) {
    furi_pubsub_unsubscribe(app->input_events, app->input_sub);
    furi_record_close(RECORD_INPUT_EVENTS);
    furi_timer_stop(app->idle_timer);
    furi_timer_free(app->idle_timer);
    furi_timer_stop(app->tick_timer);
    furi_timer_free(app->tick_timer);

    app->unlocked = false;
    vf_views_clear(app);
    vf_hid_end(&app->hid);
    vf_close_vault(app);
    vf_wipe(app->dev_sk, sizeof(app->dev_sk));
    free(app->argon_work);

    for(uint32_t v = ViewPin; v <= ViewBusy; v++)
        view_dispatcher_remove_view(app->vd, v);
    view_free(app->pin_view);
    vf_list_free(app->home);
    vf_list_free(app->folders);
    vf_list_free(app->entries);
    vf_list_free(app->more);
    vf_list_free(app->settings);
    vf_list_free(app->layouts);
    vf_list_free(app->results);
    vf_list_free(app->timezones);
    vf_code_free(app->code);
    vf_card_free(app->card);
    vf_busy_free(app->busy);
    widget_free(app->text);
    widget_free(app->mac_setup);
    text_input_free(app->search);
    view_dispatcher_free(app->vd);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t vaultflipper_app(void* p) {
    UNUSED(p);
    VfApp* app = vf_app_alloc();

    if(!app->device_ok) {
        app->exit_after_text = true;
        vf_show_text(app, T(app, S_EnclaveError), ViewPin);
    } else if(!storage_file_exists(app->storage, VF_VAULT_PATH)) {
        FuriString* s = furi_string_alloc_printf(T(app, S_NoVault), app->fingerprint);
        app->exit_after_text = true;
        vf_show_text(app, furi_string_get_cstr(s), ViewPin);
        furi_string_free(s);
    } else {
        vf_pin_reset(app, T(app, app->fails ? S_PrevFails : S_EnterPin));
        vf_switch(app, ViewPin);
    }

    view_dispatcher_run(app->vd);
    vf_app_free(app);
    return 0;
}
