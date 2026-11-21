#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>
#include <storage/storage.h>

enum {
    WifiSpamEventStop,
};

/* "attack -t beacon -r/funny" - fake-AP beacon spam types that need no target
   selection (unlike "attack -t beacon -a", which spams using the currently selected APs, or
   "-l", which needs an SSID list loaded first via the "ssid" command - both skipped for now).
   See ATTACK_TYPE_BEACON/_RR/_FUNNY handling in the ESP32 Marauder source's CommandLine.cpp.
   Exposed via accessor functions so marauder_gui_scene_wifi_spam_menu.c can list them without
   duplicating this table. */
static const struct {
    const char* label_tr;
    const char* label_en;
    const char* command;
    const char* description_tr;
    const char* description_en;
    bool uses_builtin_list; /* true = routed through the ssid -a -n/select/beacon -l pipeline
                                (same as Random SSID, gets the lock icon) instead of a single
                                passthrough command to the ESP32 */
} marauder_wifi_spam_types[] = {
    {"Rastgele SSID",
     "Random SSID",
     "attack -t beacon -r",
     "ssids.txt dosyasindan SSID'leri batch halinde yayinlar.",
     "Broadcasts SSIDs from ssids.txt in batches.",
     false},
    {"Komik Isimler",
     "Funny Names",
     "attack -t funny",
     "Yerlesik esprili SSID isimleriyle sahte agler yayinlar (kilit simgesiyle).",
     "Broadcasts fake networks with built-in joke SSID names (shows lock icon).",
     true},
};
#define MARAUDER_WIFI_SPAM_TYPE_COUNT \
    (sizeof(marauder_wifi_spam_types) / sizeof(marauder_wifi_spam_types[0]))

/* Built into the FAP itself (not read from any file), same idea as Random SSID's ssids.txt list,
   so Funny Names goes through the exact same ssid -a -n + select -s all + attack -t beacon -l
   pipeline and gets the same lock icon on scanning devices instead of the open/shield icon that
   the ESP32-side "attack -t funny" command produces. */
static const char* const marauder_funny_names[] = {
    "Abraham Linksys",
    "Benjamin FrankLAN",
    "Dora the Internet Explorer",
    "FBI Surveillance Van 4",
    "Get Off My Lawn",
    "Hide Yo Kids Hide Yo WiFi",
    "IT HURTS WHEN IP",
    "LAN Solo",
    "Loading...",
    "Mom Use This One",
    "Pretty Fly for a WiFi",
    "Virus Distribution Center",
};
#define MARAUDER_FUNNY_NAME_COUNT (sizeof(marauder_funny_names) / sizeof(marauder_funny_names[0]))

size_t marauder_gui_wifi_spam_type_count(void) {
    return MARAUDER_WIFI_SPAM_TYPE_COUNT;
}

const char* marauder_gui_wifi_spam_type_label(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ? marauder_wifi_spam_types[index].label_tr :
                                                     "?";
}

const char* marauder_gui_wifi_spam_type_label_en(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ? marauder_wifi_spam_types[index].label_en :
                                                     "?";
}

const char* marauder_gui_wifi_spam_type_command(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ? marauder_wifi_spam_types[index].command :
                                                     "attack -t beacon -r";
}

const char* marauder_gui_wifi_spam_type_description(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ?
               marauder_wifi_spam_types[index].description_tr :
               "?";
}

const char* marauder_gui_wifi_spam_type_description_en(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ?
               marauder_wifi_spam_types[index].description_en :
               "?";
}

bool marauder_gui_wifi_spam_type_uses_builtin_list(size_t index) {
    return (index < MARAUDER_WIFI_SPAM_TYPE_COUNT) ?
               marauder_wifi_spam_types[index].uses_builtin_list :
               false;
}

/* ESP32 Marauder's free-heap guard (MemoryGuard.h) starts rejecting new SSID adds somewhere
   around ~1000-1100 entries loaded at once (observed on hardware as the board going silent after
   roughly 1024 names). Keep each loaded window safely below that regardless of what the user
   picks in the batch-size prompt, which already caps at 500. */
#define WIFI_SPAM_SAFE_WINDOW_MAX 500

/* How long each window stays on air before the loader clears it and loads the next slice of the
   file. Matches the old "periodically garbles then rebroadcasts" behavior the user described,
   and is what lets a file of any size be cycled through in safe-sized pieces instead of needing
   to fit entirely in the ESP32's list at once. Must be a multiple of the app's 100ms tick. */
#define WIFI_SPAM_ROTATE_MS    20000
#define WIFI_SPAM_ROTATE_TICKS (WIFI_SPAM_ROTATE_MS / 100)

/* How many ssids.txt lines the loader reads and sends per 100ms tick while filling a window.
   Keeps every tick short (a handful of quick UART writes) so the UI, the Stop button, and Back
   all stay responsive instead of the whole window being sent in one blocking burst. */
#define WIFI_SPAM_LINES_PER_TICK 8

typedef enum {
    WifiSpamLoadIdle = 0,
    WifiSpamLoadSkipping, /* re-reading from the top of the file past windows already sent this pass */
    WifiSpamLoadLoading, /* reading+sending the next window's worth of SSIDs */
    WifiSpamLoadSelecting, /* ssid -a leaves new entries unselected; this marks them for the beacon attack */
    WifiSpamLoadStarting, /* kicks off "attack -t beacon -l" for the window just loaded */
    WifiSpamLoadBroadcasting, /* window is live; waiting out WIFI_SPAM_ROTATE_MS */
    WifiSpamLoadStopping, /* stops the attack before rotating to the next window */
    WifiSpamLoadError, /* file missing, or nothing usable in it - loader stops here */
} WifiSpamLoadState;

/* Persistent loader state, stepped a little at a time from the app's 100ms tick handler rather
   than all at once in on_enter. File position is never stored as a byte offset list (which would
   grow with file size) - instead each rotation re-opens the pass at the top of the file and
   skips the lines already sent so far, so memory use stays flat no matter how large ssids.txt is. */
typedef struct {
    Storage* storage;
    File* file;
    WifiSpamLoadState state;
    bool use_array; /* true = pull lines from marauder_funny_names instead of a file */
    size_t array_index;

    uint8_t chunk[512];
    size_t chunk_len;
    size_t chunk_pos;
    char line_buf[256];
    size_t line_pos;
    bool file_eof;

    int lines_to_skip;
    int lines_skipped;
    int window_target;
    int window_sent;
    int total_sent;
    int rotation;
    int broadcast_ticks;
} WifiSpamLoader;

static WifiSpamLoader s_loader;

static void marauder_gui_scene_wifi_spam_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiSpamEventStop);
    }
}

static void marauder_gui_scene_wifi_spam_redraw(MarauderGuiApp* app) {
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "WiFi Spam");
    widget_add_string_element(
        app->widget, 64, 14, AlignCenter, AlignTop, FontSecondary, app->wifi_spam_status_label);
    widget_add_text_box_element(
        app->widget, 0, 26, 128, 22, AlignCenter, AlignTop, app->last_uart_line, true);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Durdur", "Stop"),
        marauder_gui_scene_wifi_spam_button_callback,
        app);
}

static void marauder_gui_scene_wifi_spam_uart_line(MarauderGuiApp* app, const char* line) {
    strncpy(app->last_uart_line, line, MARAUDER_LINE_MAX - 1);
    app->last_uart_line[MARAUDER_LINE_MAX - 1] = '\0';
    marauder_gui_scene_wifi_spam_redraw(app);
}

/* ESP32 Marauder's "ssid -a" accepts one name per command ("-n <name>"). Quotes keep names
   containing spaces together through the firmware's argument tokenizer. */
static void marauder_gui_scene_wifi_spam_send_ssid(MarauderGuiApp* app, const char* ssid) {
    static char cmd[256 + 16];
    snprintf(cmd, sizeof(cmd), "ssid -a -n \"%s\"", ssid);
    marauder_uart_send_line(app->uart, cmd);
}

static void marauder_gui_scene_wifi_spam_show_loading(MarauderGuiApp* app) {
    snprintf(
        app->wifi_spam_status_label,
        sizeof(app->wifi_spam_status_label),
        app->language == MarauderLanguageTurkish ? "Tur %d: %d/%d" : "Round %d: %d/%d",
        s_loader.rotation + 1,
        s_loader.window_sent,
        s_loader.window_target);
    marauder_gui_scene_wifi_spam_redraw(app);
}

static void marauder_gui_scene_wifi_spam_show_broadcasting(MarauderGuiApp* app) {
    int seconds_left = (WIFI_SPAM_ROTATE_TICKS - s_loader.broadcast_ticks) / 10;
    if(seconds_left < 0) seconds_left = 0;
    snprintf(
        app->wifi_spam_status_label,
        sizeof(app->wifi_spam_status_label),
        app->language == MarauderLanguageTurkish ? "Tur %d: %d SSID (%ds)" :
                                                   "Round %d: %d SSIDs (%ds)",
        s_loader.rotation + 1,
        s_loader.window_sent,
        seconds_left);
    marauder_gui_scene_wifi_spam_redraw(app);
}

/* Resets the chunk/line reader and seeks back to byte 0, ready to start a fresh pass over the
   file (used both for the very first window and to wrap around once the end is reached). */
static void marauder_gui_scene_wifi_spam_restart_pass(void) {
    if(s_loader.use_array) {
        s_loader.array_index = 0;
        s_loader.file_eof = false;
        return;
    }
    storage_file_seek(s_loader.file, 0, true);
    s_loader.chunk_len = 0;
    s_loader.chunk_pos = 0;
    s_loader.line_pos = 0;
    s_loader.file_eof = false;
}

/* Pulls the next non-empty line out of the file into s_loader.line_buf, reading in 512-byte
   chunks so memory use never depends on file size. Returns false once the current pass has run
   out of lines (s_loader.file_eof is then true). */
static bool marauder_gui_scene_wifi_spam_next_line(void) {
    if(s_loader.use_array) {
        if(s_loader.array_index >= MARAUDER_FUNNY_NAME_COUNT) {
            s_loader.file_eof = true;
            return false;
        }
        strncpy(
            s_loader.line_buf,
            marauder_funny_names[s_loader.array_index],
            sizeof(s_loader.line_buf) - 1);
        s_loader.line_buf[sizeof(s_loader.line_buf) - 1] = '\0';
        s_loader.array_index++;
        return true;
    }
    while(true) {
        if(s_loader.chunk_pos >= s_loader.chunk_len) {
            if(s_loader.file_eof) return false;
            s_loader.chunk_len =
                storage_file_read(s_loader.file, s_loader.chunk, sizeof(s_loader.chunk));
            s_loader.chunk_pos = 0;
            if(s_loader.chunk_len == 0) {
                s_loader.file_eof = true;
                /* Flush a final line that has no trailing newline */
                if(s_loader.line_pos > 0) {
                    s_loader.line_buf[s_loader.line_pos] = '\0';
                    s_loader.line_pos = 0;
                    return true;
                }
                return false;
            }
        }

        uint8_t byte = s_loader.chunk[s_loader.chunk_pos++];
        if(byte == '\n' || byte == '\r') {
            if(s_loader.line_pos > 0) {
                s_loader.line_buf[s_loader.line_pos] = '\0';
                s_loader.line_pos = 0;
                return true;
            }
            /* blank line - keep scanning for the next one */
        } else if(byte >= 32 && byte < 127) {
            if(s_loader.line_pos < sizeof(s_loader.line_buf) - 1) {
                s_loader.line_buf[s_loader.line_pos++] = (char)byte;
            }
        }
    }
}

/* Advances the window-rotation state machine by a small, bounded amount of work. Registered as
   app->tick_handler (~100ms period) for the whole lifetime of the Random SSID scene, so no single
   call blocks the UI thread for more than a few quick UART writes or SD reads. */
static void marauder_gui_scene_wifi_spam_tick(MarauderGuiApp* app) {
    switch(s_loader.state) {
    case WifiSpamLoadSkipping: {
        int processed = 0;
        while(processed < WIFI_SPAM_LINES_PER_TICK &&
              s_loader.lines_skipped < s_loader.lines_to_skip) {
            if(!marauder_gui_scene_wifi_spam_next_line()) {
                /* Fewer lines left than we expected to skip - just wrap back to the top */
                marauder_gui_scene_wifi_spam_restart_pass();
                s_loader.lines_to_skip = 0;
                s_loader.lines_skipped = 0;
                break;
            }
            s_loader.lines_skipped++;
            processed++;
        }
        if(s_loader.lines_skipped >= s_loader.lines_to_skip) {
            s_loader.window_sent = 0;
            marauder_uart_send_line(app->uart, "clearlist -s");
            s_loader.state = WifiSpamLoadLoading;
        }
        break;
    }
    case WifiSpamLoadLoading: {
        int processed = 0;
        while(processed < WIFI_SPAM_LINES_PER_TICK &&
              s_loader.window_sent < s_loader.window_target) {
            if(!marauder_gui_scene_wifi_spam_next_line()) break;
            /* Skip names with '"', which can't be quoted in the "ssid -a -n" command */
            if(strlen(s_loader.line_buf) > 0 && strchr(s_loader.line_buf, '"') == NULL) {
                marauder_gui_scene_wifi_spam_send_ssid(app, s_loader.line_buf);
                s_loader.window_sent++;
                s_loader.total_sent++;
            }
            processed++;
        }
        marauder_gui_scene_wifi_spam_show_loading(app);
        if(s_loader.window_sent >= s_loader.window_target || s_loader.file_eof) {
            s_loader.state = WifiSpamLoadSelecting;
        }
        break;
    }
    case WifiSpamLoadSelecting:
        if(s_loader.window_sent == 0) {
            snprintf(
                app->wifi_spam_status_label,
                sizeof(app->wifi_spam_status_label),
                app->language == MarauderLanguageTurkish ? "Kullanilabilir SSID yok!" :
                                                           "No usable SSIDs found!");
            marauder_gui_scene_wifi_spam_redraw(app);
            s_loader.state = WifiSpamLoadError;
            break;
        }
        /* Newly added SSIDs default to unselected; "beacon -l" only broadcasts selected ones */
        marauder_uart_send_line(app->uart, "select -s all");
        s_loader.state = WifiSpamLoadStarting;
        break;
    case WifiSpamLoadStarting:
        marauder_uart_send_line(app->uart, "attack -t beacon -l");
        s_loader.broadcast_ticks = 0;
        marauder_gui_scene_wifi_spam_show_broadcasting(app);
        s_loader.state = WifiSpamLoadBroadcasting;
        break;
    case WifiSpamLoadBroadcasting:
        s_loader.broadcast_ticks++;
        if(s_loader.broadcast_ticks % 10 == 0) {
            marauder_gui_scene_wifi_spam_show_broadcasting(app);
        }
        if(s_loader.broadcast_ticks >= WIFI_SPAM_ROTATE_TICKS) {
            s_loader.state = WifiSpamLoadStopping;
        }
        break;
    case WifiSpamLoadStopping:
        marauder_uart_send_line(app->uart, "stopscan");
        s_loader.rotation++;
        if(s_loader.file_eof) {
            /* Reached the end of the file - wrap around for continuous rotation */
            marauder_gui_scene_wifi_spam_restart_pass();
            s_loader.lines_to_skip = 0;
        } else {
            s_loader.lines_to_skip += s_loader.window_sent;
        }
        s_loader.lines_skipped = 0;
        s_loader.state = WifiSpamLoadSkipping;
        break;
    case WifiSpamLoadError:
    case WifiSpamLoadIdle:
    default:
        break;
    }
}

void marauder_gui_scene_wifi_spam_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->last_uart_line[0] = '\0';
    app->uart_line_handler = marauder_gui_scene_wifi_spam_uart_line;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    if(app->wifi_spam_use_count) {
        memset(&s_loader, 0, sizeof(s_loader));

        snprintf(
            app->wifi_spam_status_label,
            sizeof(app->wifi_spam_status_label),
            app->language == MarauderLanguageTurkish ? "Dosya okunuyor..." : "Reading file...");
        marauder_gui_scene_wifi_spam_redraw(app);

        s_loader.storage = furi_record_open(RECORD_STORAGE);
        s_loader.file = storage_file_alloc(s_loader.storage);

        const char* ssid_file_path = EXT_PATH("apps_data/marauder_gui/ssids.txt");

        if(!storage_file_open(s_loader.file, ssid_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            snprintf(
                app->wifi_spam_status_label,
                sizeof(app->wifi_spam_status_label),
                app->language == MarauderLanguageTurkish ? "Dosya bulunamadi!" :
                                                           "File not found!");
            marauder_gui_scene_wifi_spam_redraw(app);
            storage_file_free(s_loader.file);
            furi_record_close(RECORD_STORAGE);
            s_loader.file = NULL;
            s_loader.storage = NULL;
            return;
        }

        int window_target = app->wifi_spam_count;
        if(window_target > WIFI_SPAM_SAFE_WINDOW_MAX) window_target = WIFI_SPAM_SAFE_WINDOW_MAX;
        if(window_target < 1) window_target = 1;
        s_loader.window_target = window_target;
        s_loader.state = WifiSpamLoadSkipping;

        app->tick_handler = marauder_gui_scene_wifi_spam_tick;

    } else if(marauder_gui_wifi_spam_type_uses_builtin_list(app->wifi_spam_type_index)) {
        /* Funny Names (and any future built-in-list type): no file, no window-size prompt -
           just run the whole small list through the same select/beacon -l pipeline Random SSID
           uses, so it broadcasts with the same lock icon. */
        memset(&s_loader, 0, sizeof(s_loader));
        s_loader.use_array = true;
        s_loader.window_target = MARAUDER_FUNNY_NAME_COUNT;
        s_loader.state = WifiSpamLoadSkipping;

        snprintf(
            app->wifi_spam_status_label,
            sizeof(app->wifi_spam_status_label),
            app->language == MarauderLanguageTurkish ? "Baslatiliyor..." : "Starting...");
        marauder_gui_scene_wifi_spam_redraw(app);

        app->tick_handler = marauder_gui_scene_wifi_spam_tick;

    } else {
        strncpy(
            app->wifi_spam_status_label,
            marauder_gui_text(
                app,
                marauder_gui_wifi_spam_type_label(app->wifi_spam_type_index),
                marauder_gui_wifi_spam_type_label_en(app->wifi_spam_type_index)),
            sizeof(app->wifi_spam_status_label) - 1);
        app->wifi_spam_status_label[sizeof(app->wifi_spam_status_label) - 1] = '\0';

        marauder_gui_scene_wifi_spam_redraw(app);
        marauder_uart_send_line(
            app->uart, marauder_gui_wifi_spam_type_command(app->wifi_spam_type_index));
    }
}

bool marauder_gui_scene_wifi_spam_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if((event.type == SceneManagerEventTypeCustom && event.event == WifiSpamEventStop) ||
       event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_spam_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;

    if(s_loader.file) {
        storage_file_close(s_loader.file);
        storage_file_free(s_loader.file);
        s_loader.file = NULL;
    }
    if(s_loader.storage) {
        furi_record_close(RECORD_STORAGE);
        s_loader.storage = NULL;
    }
    s_loader.state = WifiSpamLoadIdle;

    widget_reset(app->widget);
}
