/*
 * Purpose: Populate and navigate the in-app help pages.
 * Owns: help text arrays, selected chapter/page state, and canvas refresh.
 * Depends on: morse_flipper_app_i.h and cw markdown rendering.
 * Tests: firmware build; UI text flow is hardware-only.
 */

#include "morse_flipper_app_i.h"

static const uint8_t morse_help_arrow_right_xbm[] = {
    0x10,
    0x30,
    0x7f,
    0x30,
    0x10,
};

static const uint8_t morse_help_micro_xbm[] = {
    0x09,
    0x09,
    0x09,
    0x09,
    0x07,
    0x01,
    0x01,
};

static const uint8_t morse_help_back_triangle_xbm[] = {
    0x08,
    0x0c,
    0x0e,
    0x0f,
    0x0e,
    0x0c,
    0x08,
};

static const uint8_t morse_help_dit_xbm[] = {
    0x06,
    0x0f,
    0x0f,
    0x06,
};

static const uint8_t morse_help_dah_xbm[] = {
    0x7e,
    0xff,
    0xff,
    0x7e,
};

static const uint8_t morse_help_slash_xbm[] = {
    0x04,
    0x04,
    0x02,
    0x02,
    0x02,
    0x01,
    0x01,
};

static const uint8_t morse_help_ground_xbm[] = {
    0x08,
    0x08,
    0x7f,
    0x00,
    0x3e,
    0x00,
    0x1c,
};

static const CwmdIcon morse_help_icons[] = {
    {
        .id = 1U,
        .width = 7U,
        .height = 5U,
        .y_offset = 0,
        .left_bearing = 0U,
        .right_bearing = 0U,
        .xbm = morse_help_arrow_right_xbm,
    },
    {
        .id = 2U,
        .width = 4U,
        .height = 7U,
        .y_offset = 2,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_micro_xbm,
    },
    {
        .id = 3U,
        .width = 4U,
        .height = 7U,
        .y_offset = 0,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_back_triangle_xbm,
    },
    {
        .id = 4U,
        .width = 4U,
        .height = 4U,
        .y_offset = 1,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_dit_xbm,
    },
    {
        .id = 5U,
        .width = 8U,
        .height = 4U,
        .y_offset = 1,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_dah_xbm,
    },
    {
        .id = 6U,
        .width = 3U,
        .height = 7U,
        .y_offset = 0,
        .left_bearing = 1U,
        .right_bearing = 0U,
        .xbm = morse_help_slash_xbm,
    },
    {
        .id = 7U,
        .width = 7U,
        .height = 7U,
        .y_offset = 0,
        .left_bearing = 0U,
        .right_bearing = 0U,
        .xbm = morse_help_ground_xbm,
    },
};

#define MORSE_FLIPPER_HELP_ASSET_MAX_BYTES 12288U
#define MORSE_FLIPPER_HELP_ASSET_SIGNATURE APP_ASSETS_PATH(".assets.signature")
#define MORSE_FLIPPER_HELP_ASSET_MISSING   "Help assets missing.\nExit and open Morse Flipper again."
#define MORSE_FLIPPER_ONBOARDING_ASSET     APP_ASSETS_PATH("help/00-onboarding")
#define MORSE_FLIPPER_ONBOARDING_MISSING   "Onboarding assets missing.\nPress OK to start."

typedef struct {
    const char* path;
    const char* title;
} MorseFlipperHelpAsset;

static const MorseFlipperHelpAsset morse_help_assets[MorseFlipperHelpCount] = {
    [MorseFlipperHelpFirstSteps] = {APP_ASSETS_PATH("help/01-first-steps"), "First steps"},
    [MorseFlipperHelpInputKeys] = {APP_ASSETS_PATH("help/02-input-and-keys"), "Input & keys"},
    [MorseFlipperHelpConnectingPaddle] =
        {APP_ASSETS_PATH("help/03-connecting-the-paddle"), "Connecting the paddle"},
    [MorseFlipperHelpPractice] = {APP_ASSETS_PATH("help/04-how-to-practice"), "How to practice"},
    [MorseFlipperHelpPrepping] = {APP_ASSETS_PATH("help/05-prepping"), "Prepping"},
    [MorseFlipperHelpContact] =
        {APP_ASSETS_PATH("help/06-a-complete-morse-contact"), "A complete Morse contact"},
    [MorseFlipperHelpContesting] = {APP_ASSETS_PATH("help/07-contesting"), "Contesting"},
    [MorseFlipperHelpUsbLive] =
        {APP_ASSETS_PATH("help/08-usb-and-live-practice"), "USB & live practice"},
    [MorseFlipperHelpHamUsage] = {APP_ASSETS_PATH("help/09-ham-usage"), "Ham usage"},
    [MorseFlipperHelpTroubleshooting] =
        {APP_ASSETS_PATH("help/10-troubleshooting"), "Troubleshooting"},
    [MorseFlipperHelpMovingForward] = {APP_ASSETS_PATH("help/11-moving-forward"), "Moving forward"},
};

static const MorseFlipperHelpAsset* morse_flipper_help_asset(uint8_t t) {
    if(t >= MorseFlipperHelpCount) t = MorseFlipperHelpFirstSteps;
    return &morse_help_assets[t];
}

uint8_t morse_flipper_help_card_count(const MorseFlipperApp* app) {
    if(app == NULL || app->help_card_count == 0U) return 1U;
    return app->help_card_count;
}

bool morse_flipper_help_is_chapter_card(const MorseFlipperApp* app) {
    return app != NULL && app->help_chapter_card;
}

static bool morse_flipper_help_has_next_topic(const MorseFlipperApp* app) {
    return app != NULL && app->help_topic + 1U < MorseFlipperHelpCount;
}

static void morse_flipper_help_load_chapter_card(MorseFlipperApp* app) {
    const MorseFlipperHelpAsset* asset;
    char text[96];

    if(app == NULL || app->help_text == NULL) return;
    asset = morse_flipper_help_asset(app->help_topic);
    app->help_card_count = 1U;
    app->help_page = 0U;
    snprintf(
        text,
        sizeof(text),
        "\n\n\033c\033#Chapter %u\n\033c%s",
        (unsigned)(app->help_topic + 1U),
        asset->title);
    furi_string_set_str(app->help_text, text);
}

bool morse_flipper_help_show_next_chapter(MorseFlipperApp* app) {
    if(!morse_flipper_help_has_next_topic(app)) return false;

    if(app->help_text == NULL) app->help_text = furi_string_alloc();
    if(app->help_text == NULL) return false;

    app->help_topic++;
    app->help_page = 0U;
    app->help_md = (CwmdState){0};
    app->help_chapter_card = true;
    scene_manager_set_scene_state(app->scene_manager, MorseFlipperSceneMenuHelp, app->help_topic);
    morse_flipper_help_load_chapter_card(app);
    morse_flipper_view_dirty(app);
    return true;
}

void morse_flipper_help_enter_chapter(MorseFlipperApp* app) {
    if(app == NULL || !app->help_chapter_card) return;
    app->help_chapter_card = false;
    app->help_page = 0U;
    app->help_md = (CwmdState){0};
    morse_flipper_help_open(app);
}

static bool morse_flipper_help_delimiter_at(const char* text, const char* p) {
    const char* q;

    if(p == NULL || p[0] != '-' || p[1] != '-' || p[2] != '-') return false;
    if(p != text && p[-1] != '\n') return false;
    q = p + 3;
    if(*q == '\r') q++;
    return *q == '\n' || *q == '\0';
}

static const char* morse_flipper_help_next_delimiter(const char* start) {
    const char* p = start;

    while((p = strstr(p, "---")) != NULL) {
        if(morse_flipper_help_delimiter_at(start, p)) return p;
        p += 3;
    }

    return NULL;
}

static const char* morse_flipper_help_after_delimiter(const char* p) {
    p += 3;
    if(*p == '\r') p++;
    if(*p == '\n') p++;
    return p;
}

static uint8_t morse_flipper_help_count_cards_in(const char* text) {
    const char* p = text;
    uint8_t n = 1U;

    if(text == NULL || *text == '\0') return 0U;
    while((p = morse_flipper_help_next_delimiter(p)) != NULL) {
        if(n < 0xffU) n++;
        p = morse_flipper_help_after_delimiter(p);
    }
    return n;
}

static bool morse_flipper_help_extract_card(FuriString* out, const char* text, uint8_t page) {
    const char* start = text;
    const char* end;
    uint8_t i;

    if(out == NULL || text == NULL) return false;
    for(i = 0U; i < page; i++) {
        const char* delim = morse_flipper_help_next_delimiter(start);
        if(delim == NULL) return false;
        start = morse_flipper_help_after_delimiter(delim);
    }

    end = morse_flipper_help_next_delimiter(start);
    if(end == NULL) end = start + strlen(start);
    while(end > start && (end[-1] == '\n' || end[-1] == '\r'))
        end--;

    furi_string_set_strn(out, start, (size_t)(end - start));
    return !furi_string_empty(out);
}

static bool morse_flipper_help_read_asset(FuriString* out, const char* path) {
    Storage* storage;
    File* file;
    bool ok = false;

    if(out == NULL || path == NULL) return false;
    furi_string_reset(out);

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(file);
        if(sz > 0U && sz <= MORSE_FLIPPER_HELP_ASSET_MAX_BYTES) {
            char buf[96];
            size_t total = 0U;
            size_t got;

            furi_string_reserve(out, (size_t)sz + 1U);
            do {
                got = storage_file_read(file, buf, sizeof(buf) - 1U);
                if(got > 0U) {
                    buf[got] = '\0';
                    furi_string_cat_str(out, buf);
                    total += got;
                }
            } while(got > 0U && total < (size_t)sz);
            ok = total == (size_t)sz && !furi_string_empty(out);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void morse_flipper_help_request_asset_unpack(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_remove(storage, MORSE_FLIPPER_HELP_ASSET_SIGNATURE);
    furi_record_close(RECORD_STORAGE);
}

static void morse_flipper_help_load_card(MorseFlipperApp* app) {
    const MorseFlipperHelpAsset* asset;
    FuriString* raw;
    const char* text;
    bool ok = false;
    uint8_t n = 0U;

    if(app == NULL || app->help_text == NULL) return;
    if(app->help_chapter_card) {
        morse_flipper_help_load_chapter_card(app);
        return;
    }
    asset = morse_flipper_help_asset(app->help_topic);
    app->help_card_count = 1U;

    raw = furi_string_alloc();
    if(raw != NULL) {
        if(morse_flipper_help_read_asset(raw, asset->path)) {
            text = furi_string_get_cstr(raw);
            n = morse_flipper_help_count_cards_in(text);
            if(n == 0U) n = 1U;
            app->help_card_count = n;
            if(app->help_page >= n) app->help_page = 0U;
            ok = morse_flipper_help_extract_card(app->help_text, text, app->help_page);
        }
        furi_string_free(raw);
    }

    if(!ok) {
        morse_flipper_help_request_asset_unpack();
        furi_string_set_str(app->help_text, MORSE_FLIPPER_HELP_ASSET_MISSING);
    }
}

static const char* morse_flipper_help_current_text(const MorseFlipperApp* app) {
    if(app == NULL || app->help_text == NULL) return "";
    return furi_string_get_cstr(app->help_text);
}

bool morse_flipper_onboarding_seen(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool seen =
        storage_file_open(file, MORSE_FLIPPER_ONBOARDING_PATH, FSAM_READ, FSOM_OPEN_EXISTING);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return seen;
}

static void morse_flipper_onboarding_mark_seen(MorseFlipperApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    const char marker[] = "seen\n";

    storage_common_mkdir(storage, MORSE_FLIPPER_APP_DATA_DIR);
    if(storage_file_open(file, MORSE_FLIPPER_ONBOARDING_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, marker, sizeof(marker) - 1U);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(app != NULL) app->onboarding_seen = true;
}

static uint8_t morse_flipper_onboarding_card_count(const MorseFlipperApp* app) {
    if(app == NULL || app->onboarding_card_count == 0U) return 1U;
    return app->onboarding_card_count;
}

static void morse_flipper_onboarding_load_card(MorseFlipperApp* app) {
    FuriString* raw;
    const char* text;
    bool ok = false;
    uint8_t n = 0U;

    if(app == NULL || app->help_text == NULL) return;
    app->onboarding_card_count = 1U;

    raw = furi_string_alloc();
    if(raw != NULL) {
        if(morse_flipper_help_read_asset(raw, MORSE_FLIPPER_ONBOARDING_ASSET)) {
            text = furi_string_get_cstr(raw);
            n = morse_flipper_help_count_cards_in(text);
            if(n == 0U) n = 1U;
            app->onboarding_card_count = n;
            if(app->onboarding_page >= n) app->onboarding_page = 0U;
            ok = morse_flipper_help_extract_card(app->help_text, text, app->onboarding_page);
        }
        furi_string_free(raw);
    }

    if(!ok) {
        morse_flipper_help_request_asset_unpack();
        furi_string_set_str(app->help_text, MORSE_FLIPPER_ONBOARDING_MISSING);
    }
}

void morse_flipper_onboarding_open(MorseFlipperApp* app) {
    if(app == NULL) return;
    if(app->help_text == NULL) app->help_text = furi_string_alloc();
    morse_flipper_onboarding_load_card(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewLive);
    morse_flipper_view_dirty(app);
}

void morse_flipper_onboarding_prev(MorseFlipperApp* app) {
    if(app == NULL || app->onboarding_page == 0U) return;
    app->onboarding_page--;
    app->onboarding_md = (CwmdState){0};
    morse_flipper_onboarding_open(app);
}

void morse_flipper_onboarding_next(MorseFlipperApp* app) {
    uint8_t n;

    if(app == NULL) return;
    n = morse_flipper_onboarding_card_count(app);
    if(app->onboarding_page + 1U >= n) {
        morse_flipper_onboarding_finish(app);
        return;
    }

    app->onboarding_page++;
    app->onboarding_md = (CwmdState){0};
    morse_flipper_onboarding_open(app);
}

void morse_flipper_onboarding_finish(MorseFlipperApp* app) {
    if(app == NULL) return;
    morse_flipper_onboarding_mark_seen(app);
    app->onboarding_md = (CwmdState){0};
    scene_manager_search_and_switch_to_another_scene(
        app->scene_manager, MorseFlipperSceneMenuMain);
}

void morse_flipper_help_open(MorseFlipperApp* app) {
    if(app == NULL) return;
    if(app->help_text == NULL) app->help_text = furi_string_alloc();
    morse_flipper_help_load_card(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewLive);
    morse_flipper_view_dirty(app);
}

void morse_flipper_about_open(MorseFlipperApp* app) {
    UNUSED(app);
}

static void morse_flipper_help_cfg(
    const MorseFlipperApp* app,
    CwmdConfig* cfg,
    char* page,
    size_t page_sz) {
    uint8_t n = morse_flipper_help_card_count(app);

    cwmd_config_default(cfg, true);
    cfg->height = 48U;
    cfg->scrollbar = true;
    cfg->icons = morse_help_icons;
    cfg->icon_count = COUNT_OF(morse_help_icons);

    if(app->help_chapter_card) {
        cfg->scrollbar = false;
        cfg->chrome = CwmdChromeRight;
        cfg->right_label = "Next";
        return;
    }

    cfg->chrome = CwmdChromeCenter;
    snprintf(page, page_sz, "%u/%u", (unsigned)(app->help_page + 1U), (unsigned)n);
    cfg->center_label = page;

    if(app->help_page > 0U) {
        static char left[8];
        snprintf(left, sizeof(left), "%u", (unsigned)app->help_page);
        cfg->left_label = left;
        cfg->chrome |= CwmdChromeLeft;
    }

    if(app->help_page + 1U < n) {
        static char right[8];
        if(app->help_page == 0U) {
            cfg->right_label = "Next";
        } else {
            snprintf(right, sizeof(right), "%u", (unsigned)(app->help_page + 2U));
            cfg->right_label = right;
        }
        cfg->chrome |= CwmdChromeRight;
    }
}

void morse_flipper_draw_onboarding(Canvas* canvas, MorseFlipperApp* app) {
    CwmdConfig cfg;
    uint8_t n;

    if(canvas == NULL || app == NULL) return;

    n = morse_flipper_onboarding_card_count(app);
    cwmd_config_default(&cfg, true);
    cfg.height = 48U;
    cfg.scrollbar = true;
    cfg.icons = morse_help_icons;
    cfg.icon_count = COUNT_OF(morse_help_icons);
    cfg.chrome = CwmdChromeCenter;
    cfg.center_label = "Start";

    if(app->onboarding_page > 0U) {
        cfg.chrome |= CwmdChromeLeft;
        cfg.left_label = "Prev";
    }

    if(app->onboarding_page + 1U < n) {
        cfg.chrome |= CwmdChromeRight;
        cfg.right_label = "Next";
    }

    cwmd_draw(canvas, &cfg, &app->onboarding_md, morse_flipper_help_current_text(app));
}

void morse_flipper_draw_help(Canvas* canvas, MorseFlipperApp* app) {
    CwmdConfig cfg;
    char page[12];

    if(canvas == NULL || app == NULL) return;
    morse_flipper_help_cfg(app, &cfg, page, sizeof(page));
    cwmd_draw(canvas, &cfg, &app->help_md, morse_flipper_help_current_text(app));
}

int16_t morse_flipper_help_max_scroll(Canvas* canvas, const MorseFlipperApp* app) {
    CwmdConfig cfg;
    char page[12];

    if(canvas == NULL || app == NULL) return 0;
    morse_flipper_help_cfg(app, &cfg, page, sizeof(page));
    return cwmd_max_scroll_px(canvas, &cfg, morse_flipper_help_current_text(app));
}
