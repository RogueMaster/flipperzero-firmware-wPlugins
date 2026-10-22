#include "vendor/internet_bridge/sdk/flipper/fib_bridge_setup.h"
#include "yt_fonts.h"
#include "yt_parse.h"
#include "yt_strings.h"
#include "yt_wifi.h"

#include <furi.h>
#include <math.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YT_CONFIG_PATH      APP_DATA_PATH("config.txt")
#define YT_CONFIG_HINT      "SD: apps_data/sub_counter"
#define YT_BODY_SIZE        2048U
#define YT_URL_SIZE         384U
#define YT_REQUEST_TIMEOUT  20000U
#define YT_RETRY_SECONDS    30U
#define YT_MIN_INTERVAL     60U
#define YT_DEFAULT_INTERVAL 300U
#define YT_PAGES            3U

/* Motion timings in ms, tuned to feel like UIKit's default curves. */
#define YT_SLIDE_MS  280U
#define YT_BOUNCE_MS 360U
#define YT_SHEET_MS  300U
#define YT_COUNT_MS  900U
#define YT_SHEET_TOP 14
#define YT_TOGGLE_MS 160U

typedef enum {
    YtRowSound,
    YtRowVibro,
    YtRowLed,
    YtRowBacklight,
    YtRowInterval,
    YtRowConnection,
    YtRowLanguage,
    YtRowFont,
    YtRowTest,
    YtRowCount,
} YtSettingsRow;

#define YT_SETTINGS_VISIBLE 4U
static const uint16_t yt_intervals[] = {60, 120, 300, 600, 900, 1800, 3600};

typedef struct {
    char api_key[64];
    char channel[64];
    uint32_t interval_s;
    bool backlight;
    bool sound; /* New-subscriber alert: chime, haptic tick, green LED. */
    bool vibro;
    bool led;
    uint8_t language; /* YtLang */
    uint8_t font; /* YtFontStyle */
    uint8_t transport; /* YtTransport */
    char wifi_ssid[33]; /* Optional: the board's saved network is used otherwise */
    char wifi_password[65];
} YtConfig;

typedef enum {
    YtTransportBridge, /* Flipper Internet Bridge desktop helper, USB or Bluetooth */
    YtTransportWifi, /* FlipperHTTP board on the GPIO header */
} YtTransport;

typedef enum {
    YtPhaseIdle,
    YtPhaseFetching,
    YtPhaseOffline,
} YtPhase;

typedef struct {
    FibBridgeClient* bridge;
    FuriMutex* mutex;
    FuriMessageQueue* input;
    NotificationApp* notification;
    YtConfig config;
    char url[YT_URL_SIZE];

    /* Written by the transport worker, guarded by mutex. */
    char body[YT_BODY_SIZE];
    size_t body_length;
    bool body_overflow;

    /* Shared with the draw callback, guarded by mutex. */
    char config_error[96];
    YtStats stats;
    bool have_stats;
    uint64_t start_subscribers;
    YtPhase phase;
    char status_line[160]; /* Last error; empty when the data is current. */
    uint32_t next_fetch_tick;
    uint32_t fetch_tick;
    uint32_t last_success_tick;
    bool bluetooth;
    YtWifi* wifi; /* Set while the Wi-Fi transport is open */
    bool wifi_pending; /* A refresh waits for the board to join Wi-Fi */

    /* UI and motion state, guarded by mutex. */
    uint8_t page;
    uint8_t from_page;
    uint32_t slide_tick;
    int8_t bounce_dir;
    uint32_t bounce_tick;
    bool sheet_open;
    uint32_t sheet_tick;
    uint64_t count_from[YT_PAGES];
    uint32_t count_tick;
    bool settings_open;
    uint32_t settings_tick;
    uint8_t settings_row;
    uint8_t settings_scroll;
    uint8_t toggle_row;
    uint32_t toggle_tick;
    bool settings_dirty;

    /* Rotating buffers so a queued alert is never rewritten while it plays. */
    const NotificationMessage* alert[4][16];
    uint8_t alert_index;
} YtApp;

#define YT_TEXT(app)  (&yt_strings[(app)->config.language])
#define YT_STYLE(app) (&yt_styles[(app)->config.font])

static bool yt_body(void* context, const uint8_t* data, size_t length) {
    YtApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool keep = true;
    for(size_t i = 0; i < length; ++i) {
        if(app->body_length >= YT_BODY_SIZE - 1U) {
            app->body_overflow = true;
            keep = false;
            break;
        }
        app->body[app->body_length++] = (char)data[i];
    }
    app->body[app->body_length] = '\0';
    furi_mutex_release(app->mutex);
    return keep;
}

static void yt_trim(char* text) {
    char* start = text;
    while(*start == ' ' || *start == '\t')
        ++start;
    if(start != text) memmove(text, start, strlen(start) + 1U);
    size_t length = strlen(text);
    while(length &&
          (text[length - 1U] == ' ' || text[length - 1U] == '\t' || text[length - 1U] == '\r'))
        text[--length] = '\0';
}

/* Writes all settings back to config.txt, keeping key and channel. */
static bool yt_write_config(const YtConfig* config) {
    char* text = malloc(1536);
    if(!text) return false;
    int length = snprintf(
        text,
        1536,
        "# Subscriber Counter - configuration\n"
        "# API key from the Google Cloud Console (YouTube Data API v3)\n"
        "api_key=%s\n"
        "# Channel ID (UC...) or @handle\n"
        "channel=%s\n"
        "# Update interval in seconds (min. 60)\n"
        "interval=%lu\n"
        "# Keep the backlight on (1/0)\n"
        "backlight=%d\n"
        "# New subscriber alert: sound, vibration, LED (1/0)\n"
        "sound=%d\n"
        "vibro=%d\n"
        "led=%d\n"
        "# en or de\n"
        "language=%s\n"
        "# inter, flipper or mix\n"
        "font=%s\n"
        "# bridge (desktop helper) or wifi (FlipperHTTP board)\n"
        "transport=%s\n"
        "# Optional Wi-Fi network for the board\n"
        "wifi_ssid=%s\n"
        "wifi_password=%s\n"
        "# Everything except key, channel and Wi-Fi can be changed in the app (press Down)\n",
        config->api_key,
        config->channel,
        (unsigned long)config->interval_s,
        config->backlight,
        config->sound,
        config->vibro,
        config->led,
        config->language == YtLangGerman ? "de" : "en",
        yt_styles[config->font].config_value,
        config->transport == YtTransportWifi ? "wifi" : "bridge",
        config->wifi_ssid,
        config->wifi_password);
    bool ok = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    if(length > 0 && storage_file_open(file, YT_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, text, (size_t)length) == (size_t)length;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(text);
    return ok;
}

/* True if text contains a quote or backslash, which would break the JSON the
 * Wi-Fi board receives. */
static bool yt_json_unsafe(const char* text) {
    for(; *text; ++text) {
        if(*text == '"' || *text == '\\') return true;
    }
    return false;
}

/* Reads /ext/apps_data/sub_counter/config.txt; writes a template on first start. */
static bool yt_load_config(YtConfig* config, char* error, size_t error_size) {
    memset(config, 0, sizeof(*config));
    config->interval_s = YT_DEFAULT_INTERVAL;
    config->backlight = true;
    config->sound = true;
    config->vibro = true;
    config->led = true;
    /* Built-in fonts read cleanly on every display; Inter is opt-in. */
    config->font = YtStyleFlipper;

    char* buffer = malloc(1536);
    if(!buffer) {
        snprintf(error, error_size, "Out of memory\nRestart the app");
        return false;
    }
    size_t length = 0;
    bool exists = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, YT_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        exists = true;
        length = storage_file_read(file, buffer, 1535);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    buffer[length] = '\0';

    char* line = buffer;
    while(line && *line) {
        char* next = strchr(line, '\n');
        if(next) *next++ = '\0';
        char* equals = strchr(line, '=');
        if(line[0] != '#' && equals) {
            *equals = '\0';
            char* key = line;
            char* value = equals + 1;
            yt_trim(key);
            yt_trim(value);
            if(strcmp(key, "api_key") == 0) {
                strlcpy(config->api_key, value, sizeof(config->api_key));
            } else if(strcmp(key, "channel") == 0) {
                strlcpy(config->channel, value, sizeof(config->channel));
            } else if(strcmp(key, "interval") == 0) {
                long seconds = strtol(value, NULL, 10);
                config->interval_s = seconds < (long)YT_MIN_INTERVAL ? YT_MIN_INTERVAL :
                                                                       (uint32_t)seconds;
            } else if(strcmp(key, "backlight") == 0) {
                config->backlight = value[0] != '0';
            } else if(strcmp(key, "font") == 0) {
                for(uint8_t i = 0; i < YtStyleCount; ++i) {
                    if(strcmp(value, yt_styles[i].config_value) == 0) config->font = i;
                }
            } else if(strcmp(key, "transport") == 0) {
                config->transport = strcmp(value, "wifi") == 0 ? YtTransportWifi :
                                                                 YtTransportBridge;
            } else if(strcmp(key, "wifi_ssid") == 0) {
                strlcpy(config->wifi_ssid, value, sizeof(config->wifi_ssid));
            } else if(strcmp(key, "wifi_password") == 0) {
                strlcpy(config->wifi_password, value, sizeof(config->wifi_password));
            } else if(strcmp(key, "language") == 0) {
                config->language = strcmp(value, "de") == 0 ? YtLangGerman : YtLangEnglish;
            } else if(strcmp(key, "sound") == 0) {
                config->sound = value[0] != '0';
            } else if(strcmp(key, "vibro") == 0) {
                config->vibro = value[0] != '0';
            } else if(strcmp(key, "led") == 0) {
                config->led = value[0] != '0';
            }
        }
        line = next;
    }
    free(buffer);

    if(!exists) {
        strlcpy(config->channel, "@yourchannel", sizeof(config->channel));
        yt_write_config(config);
        snprintf(
            error,
            error_size,
            "%s\n%s",
            yt_strings[config->language].config_created,
            YT_CONFIG_HINT);
        return false;
    }
    if(!yt_api_key_valid(config->api_key)) {
        snprintf(
            error, error_size, "%s\n%s", yt_strings[config->language].key_missing, YT_CONFIG_HINT);
        return false;
    }
    /* The board receives these inside a JSON string. */
    if(yt_json_unsafe(config->wifi_ssid) || yt_json_unsafe(config->wifi_password)) {
        snprintf(
            error,
            error_size,
            "%s\n%s",
            yt_strings[config->language].wifi_config_invalid,
            YT_CONFIG_HINT);
        return false;
    }
    char query[80];
    if(strcmp(config->channel, "@yourchannel") == 0 ||
       !yt_channel_query(config->channel, query, sizeof(query))) {
        snprintf(
            error,
            error_size,
            "%s\n%s",
            yt_strings[config->language].channel_missing,
            YT_CONFIG_HINT);
        return false;
    }
    return true;
}

static void yt_build_url(YtApp* app) {
    char query[80];
    yt_channel_query(app->config.channel, query, sizeof(query));
    snprintf(
        app->url,
        sizeof(app->url),
        "https://www.googleapis.com/youtube/v3/channels?part=snippet,statistics"
        "&fields=items(snippet/title,statistics)&%s&key=%s",
        query,
        app->config.api_key);
}

/* ---------------------------------------------------------------------------
 * Motion
 * ------------------------------------------------------------------------- */

static uint32_t yt_elapsed_ms(uint32_t since) {
    return (uint32_t)((uint64_t)(furi_get_tick() - since) * 1000U /
                      furi_kernel_get_tick_frequency());
}

/* 0...1000, ease-out cubic: fast start, soft landing. */
static int32_t yt_ease_out(uint32_t elapsed, uint32_t duration) {
    if(elapsed >= duration) return 1000;
    float t = 1.0f - (float)elapsed / (float)duration;
    return (int32_t)((1.0f - t * t * t) * 1000.0f);
}

static bool yt_is_animating(YtApp* app) {
    return yt_elapsed_ms(app->slide_tick) < YT_SLIDE_MS ||
           yt_elapsed_ms(app->bounce_tick) < YT_BOUNCE_MS ||
           yt_elapsed_ms(app->sheet_tick) < YT_SHEET_MS ||
           yt_elapsed_ms(app->count_tick) < YT_COUNT_MS ||
           yt_elapsed_ms(app->settings_tick) < YT_SHEET_MS ||
           yt_elapsed_ms(app->toggle_tick) < YT_TOGGLE_MS;
}

static uint64_t yt_card_value(const YtApp* app, uint8_t page) {
    switch(page) {
    case 0:
        return app->stats.subscribers;
    case 1:
        return app->stats.views;
    default:
        return app->stats.videos;
    }
}

/* Value currently on screen, including the count-up animation. */
static uint64_t yt_shown_value(const YtApp* app, uint8_t page) {
    uint64_t to = yt_card_value(app, page);
    uint64_t from = app->count_from[page];
    int32_t e = yt_ease_out(yt_elapsed_ms(app->count_tick), YT_COUNT_MS);
    if(e >= 1000 || from == to) return to;
    if(to > from) return from + (to - from) * (uint64_t)e / 1000U;
    return from - (from - to) * (uint64_t)e / 1000U;
}

/* ---------------------------------------------------------------------------
 * Drawing primitives
 * ------------------------------------------------------------------------- */

static const char* yt_card_label(const YtApp* app, uint8_t page) {
    const YtStrings* text = YT_TEXT(app);
    return page == 0U ? text->subscribers : page == 1U ? text->views : text->videos;
}

/* Draws text centered at cx with the current font, shortened with ".." to max_width. */
static void yt_draw_fit(Canvas* canvas, const char* text, int cx, int baseline, int max_width) {
    char line[48];
    strlcpy(line, text, sizeof(line));
    size_t length = strlen(line);
    while(length > 3 && canvas_string_width(canvas, line) > max_width) {
        line[--length] = '\0';
        line[length - 1U] = '.';
        line[length - 2U] = '.';
    }
    canvas_draw_str_aligned(canvas, cx, baseline, AlignCenter, AlignBottom, line);
}

static void yt_set_font(Canvas* canvas, const YtStyle* style, YtFontRole role) {
    if(style->custom[role]) {
        canvas_set_custom_u8g2_font(canvas, style->custom[role]);
    } else {
        canvas_set_font(canvas, style->builtin[role]);
    }
}

static void yt_draw_text(
    Canvas* canvas,
    const YtStyle* style,
    YtFontRole role,
    const char* text,
    int cx,
    int baseline,
    int max_width) {
    yt_set_font(canvas, style, role);
    yt_draw_fit(canvas, text, cx, baseline, max_width);
}

/* Filled capsule with white text sitting on baseline: button or badge. */
static int yt_draw_pill(
    Canvas* canvas,
    const YtStyle* style,
    const char* text,
    int x,
    int baseline,
    bool center) {
    char line[24];
    strlcpy(line, text, sizeof(line));
    yt_set_font(canvas, style, YtFontLabel);
    int width = canvas_string_width(canvas, line) + 7;
    if(center) x -= width / 2;
    canvas_draw_rbox(canvas, x, baseline - style->label_cap - 2, width, style->label_cap + 4, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x + 3, baseline, line);
    canvas_set_color(canvas, ColorBlack);
    return width;
}

/* iOS-style activity indicator: eight spokes, a bright head with a fading tail. */
static void yt_draw_spinner(Canvas* canvas, int cx, int cy) {
    static const int8_t spokes[8][4] = {
        {0, -3, 0, -7},
        {2, -2, 5, -5},
        {3, 0, 7, 0},
        {2, 2, 5, 5},
        {0, 3, 0, 7},
        {-2, 2, -5, 5},
        {-3, 0, -7, 0},
        {-2, -2, -5, -5},
    };
    uint32_t head = (furi_get_tick() / furi_ms_to_ticks(90U)) % 8U;
    for(uint32_t k = 0; k < 8U; ++k) {
        uint32_t age = (head + 8U - k) % 8U;
        const int8_t* s = spokes[k];
        if(age < 3U) {
            canvas_draw_line(canvas, cx + s[0], cy + s[1], cx + s[2], cy + s[3]);
        } else {
            canvas_draw_dot(canvas, cx + s[2], cy + s[3]);
        }
    }
}

static void yt_draw_alert_icon(Canvas* canvas, int cx, int cy) {
    canvas_draw_circle(canvas, cx, cy, 7);
    canvas_draw_line(canvas, cx, cy - 4, cx, cy + 1);
    canvas_draw_dot(canvas, cx, cy + 3);
}

/* Person with a plus: "new subscriber". */
static void yt_draw_subscriber_icon(Canvas* canvas, int cx, int cy) {
    canvas_draw_disc(canvas, cx - 4, cy - 4, 4);
    canvas_draw_rbox(canvas, cx - 12, cy + 2, 17, 8, 3);
    canvas_draw_box(canvas, cx + 8, cy - 5, 2, 8);
    canvas_draw_box(canvas, cx + 5, cy - 2, 8, 2);
}

/* Hero number, stepping down a size when it would not fit. */
static void
    yt_draw_number(Canvas* canvas, const YtStyle* style, uint64_t value, char separator, int cx) {
    char text[24];
    yt_format_grouped(value, separator, text, sizeof(text));
    int baseline = style->big_baseline;
    yt_set_font(canvas, style, YtFontBig);
    if(canvas_string_width(canvas, text) > 124) {
        yt_set_font(canvas, style, YtFontMid);
        baseline = style->mid_baseline;
    }
    if(canvas_string_width(canvas, text) > 124) {
        canvas_set_font(canvas, FontPrimary);
        baseline = 34;
    }
    canvas_draw_str_aligned(canvas, cx, baseline, AlignCenter, AlignBottom, text);
}

/* Header title, card body (number + label + badge) offset by x for sliding. */
static void yt_draw_card(Canvas* canvas, YtApp* app, uint8_t page, int x) {
    const int cx = 64 + x;
    const YtStyle* style = YT_STYLE(app);
    if(page == 0U && app->stats.hidden) {
        yt_draw_text(canvas, style, YtFontLabel, YT_TEXT(app)->hidden, cx, 33, 124);
    } else {
        yt_draw_number(
            canvas, style, yt_shown_value(app, page), YT_TEXT(app)->thousands_separator, cx);
    }

    char label[24];
    strlcpy(label, yt_card_label(app, page), sizeof(label));
    yt_set_font(canvas, style, YtFontLabel);
    int label_width = canvas_string_width(canvas, label);
    char badge[16] = "";
    int badge_width = 0;
    int64_t delta = (int64_t)app->stats.subscribers - (int64_t)app->start_subscribers;
    if(page == 0U && !app->stats.hidden && delta != 0) {
        snprintf(badge, sizeof(badge), "%+lld", (long long)delta);
        badge_width = canvas_string_width(canvas, badge) + 7 + 4;
    }
    int left = cx - (label_width + badge_width) / 2;
    canvas_draw_str(canvas, left, style->label_baseline, label);
    if(badge_width) {
        yt_draw_pill(canvas, style, badge, left + label_width + 4, style->label_baseline, false);
    }
}

/* Page control: dots with a capsule that glides to the active page. */
static void yt_draw_page_dots(Canvas* canvas, YtApp* app) {
    const int spacing = 9;
    const int first = 64 - spacing;
    for(uint8_t i = 0; i < YT_PAGES; ++i) {
        canvas_draw_rbox(canvas, first + i * spacing - 1, 55, 3, 3, 1);
    }
    int32_t e = yt_ease_out(yt_elapsed_ms(app->slide_tick), YT_SLIDE_MS);
    int from = first + app->from_page * spacing;
    int to = first + app->page * spacing;
    int center = from + (to - from) * e / 1000;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, center - 5, 54, 11, 5);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, center - 4, 55, 9, 3, 1);
}

/* Hairline at the bottom edge: dotted track, solid fill = time until next update.
 * While loading, a short segment sweeps across like an indeterminate progress bar. */
static void yt_draw_progress(Canvas* canvas, YtApp* app) {
    if(app->phase == YtPhaseFetching) {
        uint32_t t = yt_elapsed_ms(app->fetch_tick) % 1200U;
        int32_t e = yt_ease_out(t, 1200U);
        int x = -32 + (128 + 32) * e / 1000;
        canvas_draw_line(canvas, x < 0 ? 0 : x, 63, x + 31 > 127 ? 127 : x + 31, 63);
        return;
    }
    if(app->status_line[0] || app->phase == YtPhaseOffline) return;
    for(int x = 0; x < 128; x += 4)
        canvas_draw_dot(canvas, x, 63);
    uint32_t interval_ms = app->config.interval_s * 1000U;
    int32_t remaining = (int32_t)(app->next_fetch_tick - furi_get_tick());
    uint32_t remaining_ms =
        remaining <= 0 ?
            0U :
            (uint32_t)((uint64_t)remaining * 1000U / furi_kernel_get_tick_frequency());
    if(remaining_ms > interval_ms) remaining_ms = interval_ms;
    int width = (int)((uint64_t)(interval_ms - remaining_ms) * 128U / interval_ms);
    if(width > 0) canvas_draw_line(canvas, 0, 63, width - 1, 63);
}

static void yt_format_ago(const YtStrings* text, uint32_t since_tick, char* out, size_t size) {
    uint32_t seconds = yt_elapsed_ms(since_tick) / 1000U;
    if(seconds < 60U) {
        strlcpy(out, text->just_now, size);
    } else if(seconds < 3600U) {
        snprintf(out, size, text->minutes_ago, (unsigned long)(seconds / 60U));
    } else {
        snprintf(out, size, text->hours_ago, (unsigned long)(seconds / 3600U));
    }
}

static void yt_draw_sheet_row(Canvas* canvas, int y, const char* label, const char* value) {
    canvas_draw_str(canvas, 7, y, label);
    canvas_draw_str_aligned(canvas, 121, y, AlignRight, AlignBottom, value);
}

/* Info sheet sliding up from the bottom over a dimmed background. */
static void yt_draw_sheet(Canvas* canvas, YtApp* app) {
    int32_t e = yt_ease_out(yt_elapsed_ms(app->sheet_tick), YT_SHEET_MS);
    int travel = 64 - YT_SHEET_TOP;
    int top = app->sheet_open ? 64 - travel * e / 1000 : YT_SHEET_TOP + travel * e / 1000;
    if(top >= 64) return;

    /* Dim: knock out every other pixel above the sheet. */
    canvas_set_color(canvas, ColorWhite);
    for(int y = 0; y < top; ++y) {
        for(int x = y & 1; x < 128; x += 2)
            canvas_draw_dot(canvas, x, y);
    }
    canvas_draw_rbox(canvas, 0, top, 128, 64 - top + 6, 5);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 0, top, 128, 64 - top + 6, 5);
    canvas_draw_rbox(canvas, 56, top + 3, 16, 2, 1); /* Grabber */

    canvas_set_font(canvas, FontSecondary);
    const YtStrings* text = YT_TEXT(app);
    char value[32];
    if(app->last_success_tick) {
        yt_format_ago(text, app->last_success_tick, value, sizeof(value));
    } else {
        strlcpy(value, text->never, sizeof(value));
    }
    yt_draw_sheet_row(canvas, top + 16, text->sheet_last, value);

    if(app->phase == YtPhaseFetching) {
        strlcpy(value, text->now, sizeof(value));
    } else if(app->phase == YtPhaseOffline) {
        strlcpy(value, text->paused, sizeof(value));
    } else {
        int32_t remaining = (int32_t)(app->next_fetch_tick - furi_get_tick());
        uint32_t seconds =
            remaining <= 0 ?
                0U :
                (uint32_t)((uint64_t)remaining / furi_kernel_get_tick_frequency()) + 1U;
        snprintf(
            value,
            sizeof(value),
            "in %lu:%02lu",
            (unsigned long)(seconds / 60U),
            (unsigned long)(seconds % 60U));
    }
    yt_draw_sheet_row(canvas, top + 27, text->sheet_next, value);

    yt_draw_sheet_row(
        canvas,
        top + 38,
        text->sheet_connection,
        app->phase == YtPhaseOffline ? text->connection_offline :
        app->wifi                    ? text->transport_wifi :
        app->bluetooth               ? "Bluetooth" :
                                       "USB");

    if(app->status_line[0]) {
        yt_draw_fit(canvas, app->status_line, 64, top + 48, 118);
    } else {
        canvas_draw_str(canvas, 7, top + 48, text->sheet_refresh);
        canvas_draw_str_aligned(canvas, 121, top + 48, AlignRight, AlignBottom, text->settings);
        int arrow = 121 - canvas_string_width(canvas, text->settings) - 8;
        for(int i = 0; i < 3; ++i) {
            canvas_draw_line(canvas, arrow + i, top + 43 + i, arrow + 4 - i, top + 43 + i);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Screens
 * ------------------------------------------------------------------------- */

static void yt_draw_setup(Canvas* canvas, YtApp* app) {
    yt_draw_subscriber_icon(canvas, 64, 13);
    char headline[48];
    strlcpy(headline, app->config_error, sizeof(headline));
    char* hint = strchr(headline, '\n');
    if(hint) *hint++ = '\0';
    yt_draw_text(canvas, YT_STYLE(app), YtFontLabel, headline, 64, 35, 124);
    canvas_set_font(canvas, FontSecondary);
    if(hint) yt_draw_fit(canvas, hint, 64, 47, 124);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, YT_TEXT(app)->edit_config);
}

static void yt_draw_header(Canvas* canvas, YtApp* app) {
    const char* text = app->have_stats && app->stats.title[0] ? app->stats.title :
                                                                app->config.channel;
    if(app->have_stats && app->phase == YtPhaseOffline) {
        text = YT_TEXT(app)->offline;
    } else if(app->have_stats && app->status_line[0] && app->phase != YtPhaseFetching) {
        text = YT_TEXT(app)->not_current;
    }
    const YtStyle* style = YT_STYLE(app);
    yt_draw_text(canvas, style, YtFontTitle, text, 64, style->title_baseline, 124);
}

static void yt_draw_empty(Canvas* canvas, YtApp* app) {
    yt_draw_header(canvas, app);
    if(app->phase == YtPhaseFetching || (!app->status_line[0] && app->phase != YtPhaseOffline)) {
        yt_draw_spinner(canvas, 64, 30);
        yt_draw_text(canvas, YT_STYLE(app), YtFontLabel, YT_TEXT(app)->loading, 64, 52, 124);
        return;
    }
    yt_draw_alert_icon(canvas, 64, 21);
    bool offline = app->phase == YtPhaseOffline;
    const YtStrings* text = YT_TEXT(app);
    const YtStyle* style = YT_STYLE(app);
    yt_draw_text(canvas, style, YtFontLabel, offline ? text->offline : text->error, 64, 39, 124);
    canvas_set_font(canvas, FontSecondary);
    yt_draw_fit(canvas, app->status_line[0] ? app->status_line : text->start_helper, 64, 49, 124);
    yt_draw_pill(canvas, style, offline ? text->button_connect : text->button_retry, 64, 62, true);
}

static void yt_draw_cards(Canvas* canvas, YtApp* app) {
    yt_draw_header(canvas, app);
    uint32_t slide = yt_elapsed_ms(app->slide_tick);
    if(slide < YT_SLIDE_MS && app->from_page != app->page) {
        int dir = app->page > app->from_page ? 1 : -1;
        int32_t e = yt_ease_out(slide, YT_SLIDE_MS);
        int incoming = dir * 128 * (1000 - e) / 1000;
        yt_draw_card(canvas, app, app->from_page, incoming - dir * 128);
        yt_draw_card(canvas, app, app->page, incoming);
    } else {
        int offset = 0;
        uint32_t bounce = yt_elapsed_ms(app->bounce_tick);
        if(bounce < YT_BOUNCE_MS) {
            float t = (float)bounce / (float)YT_BOUNCE_MS;
            offset = (int)(-app->bounce_dir * 12.0f * sinf(t * 3.14159f) * (1.0f - t));
        }
        yt_draw_card(canvas, app, app->page, offset);
    }
    yt_draw_page_dots(canvas, app);
    yt_draw_progress(canvas, app);
    yt_draw_sheet(canvas, app);
}

/* ---------------------------------------------------------------------------
 * Settings (iOS-style list with switches, slides down from the top)
 * ------------------------------------------------------------------------- */

static const char* yt_settings_label(const YtApp* app, uint8_t row) {
    const YtStrings* text = YT_TEXT(app);
    switch(row) {
    case YtRowSound:
        return text->sound;
    case YtRowVibro:
        return text->vibration;
    case YtRowLed:
        return text->led;
    case YtRowBacklight:
        return text->backlight;
    case YtRowInterval:
        return text->interval;
    case YtRowConnection:
        return text->sheet_connection;
    case YtRowLanguage:
        return text->language;
    case YtRowFont:
        return text->font;
    default:
        return text->test;
    }
}

static bool* yt_settings_flag(YtApp* app, uint8_t row) {
    switch(row) {
    case YtRowSound:
        return &app->config.sound;
    case YtRowVibro:
        return &app->config.vibro;
    case YtRowLed:
        return &app->config.led;
    case YtRowBacklight:
        return &app->config.backlight;
    default:
        return NULL;
    }
}

static void yt_format_interval(const YtStrings* text, uint32_t seconds, char* out, size_t size) {
    if(seconds >= 3600U && seconds % 3600U == 0U) {
        snprintf(out, size, text->hours, (unsigned long)(seconds / 3600U));
    } else {
        snprintf(out, size, text->minutes, (unsigned long)(seconds / 60U));
    }
}

/* Switch: filled track with white knob = on, outlined track = off. The knob
 * glides over YT_TOGGLE_MS after a change. */
static void yt_draw_switch(Canvas* canvas, YtApp* app, uint8_t row, bool on, int x, int y) {
    int32_t e = 1000;
    if(app->toggle_row == row) e = yt_ease_out(yt_elapsed_ms(app->toggle_tick), YT_TOGGLE_MS);
    int from = on ? 0 : 8;
    int to = on ? 8 : 0;
    int knob = from + (to - from) * e / 1000;
    bool filled = on ? e >= 500 : e < 500;
    if(filled) {
        canvas_draw_rbox(canvas, x, y, 17, 9, 4);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, x + 4 + knob, y + 4, 2);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, x, y, 17, 9, 4);
        canvas_draw_disc(canvas, x + 4 + knob, y + 4, 2);
    }
}

static void yt_draw_settings(Canvas* canvas, YtApp* app) {
    int32_t e = yt_ease_out(yt_elapsed_ms(app->settings_tick), YT_SHEET_MS);
    int y0 = app->settings_open ? -64 + 64 * e / 1000 : -64 * e / 1000;
    if(y0 <= -64) return;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, y0, 128, 64);
    canvas_set_color(canvas, ColorBlack);
    if(y0 < 0) canvas_draw_line(canvas, 0, y0 + 64, 127, y0 + 64);

    const YtStyle* style = YT_STYLE(app);
    yt_draw_text(
        canvas, style, YtFontTitle, YT_TEXT(app)->settings, 64, y0 + style->title_baseline, 124);

    for(uint8_t i = 0; i < YT_SETTINGS_VISIBLE; ++i) {
        uint8_t row = (uint8_t)(app->settings_scroll + i);
        if(row >= YtRowCount) break;
        int y = y0 + 14 + i * 12;
        bool selected = row == app->settings_row;
        yt_set_font(canvas, style, YtFontLabel);
        canvas_draw_str(canvas, 6, y + 10, yt_settings_label(app, row));

        bool* flag = yt_settings_flag(app, row);
        if(flag) {
            yt_draw_switch(canvas, app, row, *flag, 100, y + 2);
        } else if(
            row == YtRowInterval || row == YtRowConnection || row == YtRowLanguage ||
            row == YtRowFont) {
            char value[24];
            char text[16];
            if(row == YtRowInterval) {
                yt_format_interval(YT_TEXT(app), app->config.interval_s, text, sizeof(text));
            } else if(row == YtRowConnection) {
                strlcpy(
                    text,
                    app->config.transport == YtTransportWifi ? YT_TEXT(app)->transport_wifi :
                                                               YT_TEXT(app)->transport_bridge,
                    sizeof(text));
            } else if(row == YtRowLanguage) {
                strlcpy(text, YT_TEXT(app)->language_name, sizeof(text));
            } else {
                strlcpy(text, style->name, sizeof(text));
            }
            snprintf(value, sizeof(value), selected ? "< %s >" : "%s", text);
            canvas_draw_str_aligned(canvas, 118, y + 10, AlignRight, AlignBottom, value);
        } else if(row == YtRowTest) {
            for(int k = 0; k < 4; ++k) {
                canvas_draw_line(canvas, 110 + k, y + 3 + k, 110 + k, y + 9 - k);
            }
        }
        if(selected) {
            /* Focus highlight: invert the whole row like tvOS... */
            canvas_set_color(canvas, ColorXOR);
            canvas_draw_rbox(canvas, 1, y, 121, 12, 3);
            canvas_set_color(canvas, ColorBlack);
            if(flag) {
                /* ...but keep the switch on a white island so on/off stays unambiguous. */
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_rbox(canvas, 98, y + 1, 21, 11, 5);
                canvas_set_color(canvas, ColorBlack);
                yt_draw_switch(canvas, app, row, *flag, 100, y + 2);
            }
        }
    }

    /* Scroll indicator */
    const int track = (int)YT_SETTINGS_VISIBLE * 12;
    int thumb = track * (int)YT_SETTINGS_VISIBLE / (int)YtRowCount;
    int pos = (track - thumb) * app->settings_scroll / (int)(YtRowCount - YT_SETTINGS_VISIBLE);
    canvas_draw_rbox(canvas, 125, y0 + 14 + pos, 2, thumb, 1);
}

static void yt_draw(Canvas* canvas, void* context) {
    YtApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(app->config_error[0]) {
        yt_draw_setup(canvas, app);
    } else if(!app->have_stats) {
        yt_draw_empty(canvas, app);
    } else {
        yt_draw_cards(canvas, app);
    }
    if(!app->config_error[0]) yt_draw_settings(canvas, app);
    furi_mutex_release(app->mutex);
}

static void yt_input(InputEvent* event, void* context) {
    YtApp* app = context;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        furi_message_queue_put(app->input, event, 0);
    }
}

/* ---------------------------------------------------------------------------
 * Data
 * ------------------------------------------------------------------------- */

/* Two-note chime (E6 -> B6), a 50 ms haptic tick and a green glow, each optional. */
static void yt_play_alert(YtApp* app, bool sound, bool vibro, bool led) {
    const NotificationMessage** seq = app->alert[app->alert_index];
    app->alert_index = (uint8_t)((app->alert_index + 1U) % 4U);
    size_t n = 0;
    if(led) seq[n++] = &message_green_255;
    if(vibro) seq[n++] = &message_vibro_on;
    if(sound) {
        seq[n++] = &message_note_e6;
        seq[n++] = &message_delay_50;
        seq[n++] = &message_delay_25;
        if(vibro) seq[n++] = &message_vibro_off;
        seq[n++] = &message_note_b6;
        seq[n++] = &message_delay_100;
        seq[n++] = &message_delay_50;
        seq[n++] = &message_sound_off;
    } else if(vibro) {
        seq[n++] = &message_delay_50;
        seq[n++] = &message_vibro_off;
    }
    if(led) {
        seq[n++] = &message_delay_250;
        seq[n++] = &message_green_0;
    }
    seq[n] = NULL;
    if(n) notification_message(app->notification, (const NotificationSequence*)seq);
}

static void yt_set_status(YtApp* app, YtPhase phase, const char* text) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->phase = phase;
    strlcpy(app->status_line, text, sizeof(app->status_line));
    if(phase == YtPhaseFetching) app->fetch_tick = furi_get_tick();
    furi_mutex_release(app->mutex);
}

static void yt_schedule(YtApp* app, uint32_t seconds) {
    app->next_fetch_tick = furi_get_tick() + furi_ms_to_ticks(seconds * 1000U);
}

static void yt_start_fetch(YtApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->body_length = 0;
    app->body[0] = '\0';
    app->body_overflow = false;
    char previous_error[sizeof(app->status_line)];
    strlcpy(previous_error, app->status_line, sizeof(previous_error));
    furi_mutex_release(app->mutex);
    if(app->wifi) {
        YtWifiState state = yt_wifi_state(app->wifi);
        if(state == YtWifiNoBoard || state == YtWifiFailed) {
            /* Board missing or not on Wi-Fi yet: shake hands again first. */
            yt_wifi_start(app->wifi, app->config.wifi_ssid, app->config.wifi_password);
            state = yt_wifi_state(app->wifi);
        }
        app->wifi_pending = state != YtWifiReady || !yt_wifi_get(app->wifi, app->url);
        /* Keep showing the last error until this request succeeds. */
        yt_set_status(app, YtPhaseFetching, previous_error);
        return;
    }
    if(!app->bridge) return;
    if(fib_bridge_client_get(app->bridge, app->url, YT_REQUEST_TIMEOUT)) {
        /* Keep showing the last error until this request succeeds. */
        yt_set_status(app, YtPhaseFetching, previous_error);
        return;
    }
    FibBridgeStatus status;
    fib_bridge_client_get_status(app->bridge, &status);
    if(!status.connected || !status.host_present) {
        yt_set_status(app, YtPhaseOffline, YT_TEXT(app)->helper_unreachable);
    } else {
        yt_set_status(
            app, YtPhaseIdle, status.detail[0] ? status.detail : YT_TEXT(app)->request_rejected);
        yt_schedule(app, YT_RETRY_SECONDS);
    }
}

/* complete: a full HTTP response arrived (any status); detail explains failures. */
static void yt_finish_fetch(YtApp* app, bool complete, uint16_t http_status, const char* detail) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool rose = false;
    if(!complete || http_status == 0U) {
        strlcpy(
            app->status_line,
            detail && detail[0] ? detail : YT_TEXT(app)->no_response,
            sizeof(app->status_line));
        app->next_fetch_tick = furi_get_tick() + furi_ms_to_ticks(YT_RETRY_SECONDS * 1000U);
    } else if(app->body_overflow) {
        strlcpy(app->status_line, YT_TEXT(app)->response_too_large, sizeof(app->status_line));
        app->next_fetch_tick = furi_get_tick() + furi_ms_to_ticks(app->config.interval_s * 1000U);
    } else {
        YtStats parsed;
        yt_parse_response(app->body, http_status, &parsed);
        if(parsed.ok) {
            /* Count up from what is on screen now (zero on first load). */
            for(uint8_t i = 0; i < YT_PAGES; ++i) {
                app->count_from[i] = app->have_stats ? yt_shown_value(app, i) : 0U;
            }
            if(!app->have_stats) {
                app->start_subscribers = parsed.subscribers;
            } else if(parsed.subscribers > app->stats.subscribers) {
                rose = true;
            }
            app->stats = parsed;
            app->have_stats = true;
            app->count_tick = furi_get_tick();
            app->last_success_tick = furi_get_tick();
            app->status_line[0] = '\0';
        } else {
            const YtStrings* text = YT_TEXT(app);
            strlcpy(
                app->status_line,
                parsed.result == YtParseNotFound ? text->channel_not_found :
                parsed.result == YtParseNoCount  ? text->no_subscriber_count :
                                                   parsed.error,
                sizeof(app->status_line));
        }
        app->next_fetch_tick = furi_get_tick() + furi_ms_to_ticks(app->config.interval_s * 1000U);
    }
    app->phase = YtPhaseIdle;
    furi_mutex_release(app->mutex);
    if(rose) yt_play_alert(app, app->config.sound, app->config.vibro, app->config.led);
}

/* Runs the SDK's modal setup UI; the caller's viewport must be disabled. */
static bool yt_connect(YtApp* app, ViewPort* viewport) {
    if(viewport) view_port_enabled_set(viewport, false);
    bool connected = fib_bridge_client_connect_ui(app->bridge);
    if(viewport) view_port_enabled_set(viewport, true);
    return connected;
}

/* Opens the configured transport. Returns false only when the user leaves the
 * bridge setup screen at launch (viewport == NULL), which exits the app. */
static bool yt_transport_open(YtApp* app, ViewPort* viewport) {
    const YtStrings* text = YT_TEXT(app);
    app->bluetooth = false;
    if(app->config.transport == YtTransportWifi) {
        app->wifi = yt_wifi_alloc();
        if(!app->wifi) {
            yt_set_status(app, YtPhaseIdle, text->wifi_busy);
            yt_schedule(app, YT_RETRY_SECONDS);
            return true;
        }
        yt_wifi_start(app->wifi, app->config.wifi_ssid, app->config.wifi_password);
        return true;
    }
    const FibBridgeClientConfig bridge_config = {.app_version = "sub-counter-1.2"};
    const FibBridgeClientCallbacks callbacks = {.on_body = yt_body, .context = app};
    app->bridge = fib_bridge_client_alloc(&bridge_config, &callbacks);
    if(!app->bridge) {
        yt_set_status(app, YtPhaseOffline, text->bridge_failed);
        return true;
    }
    if(!yt_connect(app, viewport)) {
        if(!viewport) return false;
        yt_set_status(app, YtPhaseOffline, text->helper_unreachable);
    }
    return true;
}

static void yt_transport_close(YtApp* app) {
    if(app->bridge) {
        fib_bridge_client_cancel(app->bridge);
        /* Joins transport callbacks before their context goes away. */
        fib_bridge_client_free(app->bridge);
        app->bridge = NULL;
    }
    if(app->wifi) {
        yt_wifi_free(app->wifi);
        app->wifi = NULL;
    }
    app->wifi_pending = false;
}

typedef enum {
    YtActionNone,
    YtActionRefresh,
    YtActionExit,
    YtActionSaveSettings,
    YtActionApplyBacklight,
    YtActionPreviewSound,
    YtActionPreviewVibro,
    YtActionPreviewLed,
    YtActionPreviewAll,
} YtAction;

static void yt_open_settings(YtApp* app, uint32_t now) {
    app->settings_open = true;
    app->settings_tick = now;
    app->settings_dirty = false;
}

/* Keys while the settings list is open. Caller holds the mutex. */
static YtAction yt_settings_key(YtApp* app, InputEvent* event, uint32_t now) {
    uint8_t row = app->settings_row;
    bool* flag = yt_settings_flag(app, row);
    switch(event->key) {
    case InputKeyBack:
        if(event->type != InputTypeShort) return YtActionNone;
        app->settings_open = false;
        app->settings_tick = now;
        return app->settings_dirty ? YtActionSaveSettings : YtActionNone;
    case InputKeyUp:
        if(row > 0) --row;
        break;
    case InputKeyDown:
        if(row + 1U < YtRowCount) ++row;
        break;
    case InputKeyLeft:
    case InputKeyRight:
    case InputKeyOk:
        if(event->key == InputKeyOk && event->type != InputTypeShort) return YtActionNone;
        if(flag) {
            bool target = event->key == InputKeyOk ? !*flag : event->key == InputKeyRight;
            if(target == *flag) return YtActionNone;
            *flag = target;
            app->toggle_row = row;
            app->toggle_tick = now;
            app->settings_dirty = true;
            if(row == YtRowBacklight) return YtActionApplyBacklight;
            if(!target) return YtActionNone;
            /* Preview what was just switched on, like iOS does for ringtones. */
            return row == YtRowSound ? YtActionPreviewSound :
                   row == YtRowVibro ? YtActionPreviewVibro :
                                       YtActionPreviewLed;
        }
        if(row == YtRowInterval) {
            const size_t count = sizeof(yt_intervals) / sizeof(yt_intervals[0]);
            size_t index = 0;
            while(index + 1U < count && yt_intervals[index] < app->config.interval_s)
                ++index;
            if(event->key == InputKeyLeft) {
                if(index > 0U) --index;
            } else if(event->key == InputKeyRight) {
                if(index + 1U < count) ++index;
            } else {
                index = (index + 1U) % count;
            }
            if(yt_intervals[index] != app->config.interval_s) {
                app->config.interval_s = yt_intervals[index];
                app->settings_dirty = true;
                /* Re-plan the next update from the last successful one. */
                if(app->last_success_tick && app->phase == YtPhaseIdle) {
                    app->next_fetch_tick =
                        app->last_success_tick + furi_ms_to_ticks(app->config.interval_s * 1000U);
                }
            }
            return YtActionNone;
        }
        if(row == YtRowConnection) {
            /* Applied when the settings close (the transport is reopened then). */
            app->config.transport = app->config.transport == YtTransportWifi ? YtTransportBridge :
                                                                               YtTransportWifi;
            app->settings_dirty = true;
            return YtActionNone;
        }
        if(row == YtRowFont) {
            /* Applies at once: the settings list itself redraws in the new style. */
            uint8_t font = app->config.font;
            font = event->key == InputKeyLeft ?
                       (uint8_t)((font + YtStyleCount - 1U) % YtStyleCount) :
                       (uint8_t)((font + 1U) % YtStyleCount);
            app->config.font = font;
            app->settings_dirty = true;
            return YtActionNone;
        }
        if(row == YtRowLanguage) {
            app->config.language = app->config.language == YtLangEnglish ? YtLangGerman :
                                                                           YtLangEnglish;
            app->settings_dirty = true;
            return YtActionNone;
        }
        if(row == YtRowTest && event->key == InputKeyOk) return YtActionPreviewAll;
        return YtActionNone;
    default:
        return YtActionNone;
    }
    app->settings_row = row;
    if(row < app->settings_scroll) app->settings_scroll = row;
    if(row >= app->settings_scroll + YT_SETTINGS_VISIBLE) {
        app->settings_scroll = (uint8_t)(row - YT_SETTINGS_VISIBLE + 1U);
    }
    return YtActionNone;
}

static YtAction yt_handle_key(YtApp* app, InputEvent* event) {
    YtAction action = YtActionNone;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const uint32_t now = furi_get_tick();
    if(app->settings_open) {
        action = yt_settings_key(app, event, now);
        furi_mutex_release(app->mutex);
        return action;
    }
    switch(event->key) {
    case InputKeyBack:
        if(event->type != InputTypeShort) break;
        if(app->sheet_open) {
            app->sheet_open = false;
            app->sheet_tick = now;
        } else {
            action = YtActionExit;
        }
        break;
    case InputKeyUp:
        if(app->have_stats && !app->sheet_open) {
            app->sheet_open = true;
            app->sheet_tick = now;
        }
        break;
    case InputKeyDown:
        /* Down pulls the settings in from the top, also straight from the sheet. */
        if(app->sheet_open) {
            app->sheet_open = false;
            app->sheet_tick = now;
        }
        if(!app->config_error[0]) yt_open_settings(app, now);
        break;
    case InputKeyLeft:
    case InputKeyRight:
        if(app->have_stats && !app->sheet_open) {
            int dir = event->key == InputKeyRight ? 1 : -1;
            int target = (int)app->page + dir;
            if(target < 0 || target >= (int)YT_PAGES) {
                app->bounce_dir = (int8_t)dir;
                app->bounce_tick = now;
            } else {
                app->from_page = app->page;
                app->page = (uint8_t)target;
                app->slide_tick = now;
            }
        }
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort) action = YtActionRefresh;
        break;
    default:
        break;
    }
    furi_mutex_release(app->mutex);
    return action;
}

int32_t sub_counter_main(void* argument) {
    UNUSED(argument);
    YtApp* app = calloc(1, sizeof(YtApp));
    if(!app) return -1;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    /* Start every animation clock in the past so nothing plays on launch. */
    const uint32_t past = furi_get_tick() - furi_ms_to_ticks(10000U);
    app->slide_tick = app->bounce_tick = app->sheet_tick = app->count_tick = past;
    app->settings_tick = app->toggle_tick = past;

    bool config_ok = yt_load_config(&app->config, app->config_error, sizeof(app->config_error));
    if(config_ok) {
        yt_build_url(app);
        if(!yt_transport_open(app, NULL)) {
            /* Back pressed in the bridge setup screen: leave the app. */
            yt_transport_close(app);
            furi_record_close(RECORD_NOTIFICATION);
            furi_message_queue_free(app->input);
            furi_mutex_free(app->mutex);
            free(app);
            return 0;
        }
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, yt_draw, app);
    view_port_input_callback_set(viewport, yt_input, app);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);
    if(config_ok && app->config.backlight) {
        notification_message(app->notification, &sequence_display_backlight_enforce_on);
    }

    if(config_ok) yt_start_fetch(app);

    bool running = true;
    while(running) {
        if(app->bridge) {
            fib_bridge_client_tick(app->bridge);
            FibBridgeStatus status;
            fib_bridge_client_get_status(app->bridge, &status);
            app->bluetooth = status.bluetooth_mode;
            if(app->phase == YtPhaseFetching && !status.active_request) {
                yt_finish_fetch(
                    app, status.state == FibBridgeStateComplete, status.http_status, status.detail);
            }
        }
        if(app->wifi) {
            yt_wifi_tick(app->wifi);
            YtWifiState state = yt_wifi_state(app->wifi);
            if(app->phase == YtPhaseFetching) {
                if(state == YtWifiReady && app->wifi_pending) {
                    app->wifi_pending = false;
                    if(!yt_wifi_get(app->wifi, app->url)) {
                        yt_finish_fetch(app, false, 0, YT_TEXT(app)->request_rejected);
                    }
                } else if(state == YtWifiDone) {
                    uint16_t http_status = 0;
                    char error[96];
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->body_overflow = !yt_wifi_take_result(
                        app->wifi,
                        app->body,
                        sizeof(app->body),
                        &http_status,
                        error,
                        sizeof(error));
                    furi_mutex_release(app->mutex);
                    yt_finish_fetch(app, http_status != 0U, http_status, error);
                } else if(state == YtWifiNoBoard || state == YtWifiFailed) {
                    app->wifi_pending = false;
                    const char* error = yt_wifi_error(app->wifi);
                    yt_finish_fetch(
                        app,
                        false,
                        0,
                        state == YtWifiNoBoard ? YT_TEXT(app)->wifi_no_board :
                        error[0]               ? error :
                                                 YT_TEXT(app)->wifi_failed);
                }
            }
        }
        if((app->bridge || app->wifi) && app->phase == YtPhaseIdle &&
           (int32_t)(app->next_fetch_tick - furi_get_tick()) <= 0) {
            yt_start_fetch(app);
        }
        view_port_update(viewport);

        /* ~30 fps while something moves, otherwise a calm 10 fps. */
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool busy = yt_is_animating(app) || app->phase == YtPhaseFetching;
        furi_mutex_release(app->mutex);
        InputEvent event;
        if(furi_message_queue_get(app->input, &event, busy ? 33 : 100) != FuriStatusOk) continue;

        YtAction action = yt_handle_key(app, &event);
        switch(action) {
        case YtActionSaveSettings:
            yt_write_config(&app->config);
            if((app->config.transport == YtTransportWifi) != (app->wifi != NULL)) {
                /* Connection changed in Settings: switch over right away. */
                yt_transport_close(app);
                yt_transport_open(app, viewport);
                if(app->bridge || app->wifi) yt_start_fetch(app);
            }
            break;
        case YtActionApplyBacklight:
            notification_message(
                app->notification,
                app->config.backlight ? &sequence_display_backlight_enforce_on :
                                        &sequence_display_backlight_enforce_auto);
            break;
        case YtActionPreviewSound:
            yt_play_alert(app, true, false, false);
            break;
        case YtActionPreviewVibro:
            yt_play_alert(app, false, true, false);
            break;
        case YtActionPreviewLed:
            yt_play_alert(app, false, false, true);
            break;
        case YtActionPreviewAll:
            yt_play_alert(app, app->config.sound, app->config.vibro, app->config.led);
            break;
        default:
            break;
        }
        if(action == YtActionExit) {
            running = false;
        } else if(action == YtActionRefresh && app->phase != YtPhaseFetching) {
            if(!app->bridge && !app->wifi) {
                yt_transport_open(app, viewport);
                if(app->bridge || app->wifi) yt_start_fetch(app);
            } else if(app->wifi) {
                yt_start_fetch(app);
            } else if(app->phase == YtPhaseOffline) {
                if(yt_connect(app, viewport)) yt_start_fetch(app);
            } else {
                yt_start_fetch(app);
            }
        }
    }

    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
    /* Flush queued alerts: they point into app->alert, which is freed below. */
    notification_message_block(app->notification, &sequence_empty);
    view_port_enabled_set(viewport, false);
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    /* Join transport callbacks before freeing their context and mutex. */
    yt_transport_close(app);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->input);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
