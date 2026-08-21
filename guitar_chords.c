#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "chord_db.h"
#include "chord_view.h"
#include "song_db.h"
#include "practice_view.h"

#define TAG "GuitarChords"

#define MAX_ROOTS 20
#define MAX_NAMES 64
#define MAX_VOICINGS 16

/* Submenu index for the "Practice" entry; kept clear of any root index. */
#define MENU_INDEX_PRACTICE 0xFFFFUL

typedef enum {
    ViewIdSplash,
    ViewIdRootMenu,
    ViewIdChordMenu,
    ViewIdDiagram,
    ViewIdSongMenu,
    ViewIdPractice,
} ViewId;

typedef enum {
    EventBeat = 0, // metronome tick, posted from the timer thread
} AppCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* root_menu;
    Submenu* chord_menu;
    Submenu* song_menu;
    ChordView* chord_view;
    PracticeView* practice_view;
    View* splash_view;

    ChordDb* db;
    SongDb* songs;

    FuriTimer* beat_timer;
    size_t song_index;
    uint8_t practice_step;
    uint8_t practice_beat; // 0..SONG_BEATS_PER_CHORD-1
    uint16_t practice_bpm;
    bool practice_playing;

    const char* roots[MAX_ROOTS];
    size_t root_count;

    const char* names[MAX_NAMES];
    size_t name_count;
    size_t name_index;

    size_t voicings[MAX_VOICINGS];
    size_t voicing_count;
    size_t voicing_index;

    ViewId current_view;
} GuitarChordsApp;

/* ------------------------------------------------------------------ */
/* Splash screen                                                       */
/* ------------------------------------------------------------------ */

static void splash_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignCenter, "Guitar Chords");
    canvas_draw_line(canvas, 12, 24, 115, 24);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "by KingBoa");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "NoodleNugget.com");
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "Press any key");
}

/** Leave the splash for the root menu. Only ever driven by a key press. */
static void app_splash_finish(GuitarChordsApp* app) {
    if(app->current_view != ViewIdSplash) return;
    app->current_view = ViewIdRootMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdRootMenu);
}

static bool splash_input_callback(InputEvent* event, void* context) {
    GuitarChordsApp* app = context;
    if(event->type != InputTypeShort) return false;
    // Back falls through to the navigation callback, which exits the app.
    if(event->key == InputKeyBack) return false;
    app_splash_finish(app);
    return true;
}

/* ------------------------------------------------------------------ */
/* Practice mode                                                       */
/* ------------------------------------------------------------------ */

static const NotificationMessage practice_tick_sound = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 1000.0f, .volume = 1.0f},
};
static const NotificationMessage practice_accent_sound = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 1600.0f, .volume = 1.0f},
};

/* Plain beat: a short click, no vibro (it would blur into a buzz at tempo). */
static const NotificationSequence sequence_beat = {
    &practice_tick_sound,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

/* Chord change: higher click plus a bump you can feel without looking. */
static const NotificationSequence sequence_change = {
    &practice_accent_sound,
    &message_vibro_on,
    &message_delay_25,
    &message_sound_off,
    &message_vibro_off,
    NULL,
};

static const Song* app_current_song(GuitarChordsApp* app) {
    if(app->songs->count == 0) return NULL;
    return &app->songs->items[app->song_index];
}

/** Resolve the current step's chord name to its first voicing and redraw. */
static void app_practice_refresh(GuitarChordsApp* app) {
    const Song* song = app_current_song(app);
    if(!song) return;

    const char* name = song->chords[app->practice_step];
    size_t idx[1];
    const Chord* chord = NULL;
    if(chord_db_voicings(app->db, name, idx, 1) > 0) {
        chord = &app->db->items[idx[0]];
    }

    practice_view_update(
        app->practice_view,
        chord,
        name,
        app->practice_step,
        song->count,
        app->practice_bpm,
        app->practice_playing);
}

/** One beat at the current tempo. Restarting is safe while running. */
static void app_practice_restart_timer(GuitarChordsApp* app) {
    furi_timer_stop(app->beat_timer);
    if(!app->practice_playing) return;
    uint32_t period_ms = 60000UL / app->practice_bpm;
    furi_timer_start(app->beat_timer, furi_ms_to_ticks(period_ms));
}

static void app_practice_set_playing(GuitarChordsApp* app, bool playing) {
    app->practice_playing = playing;
    app->practice_beat = 0;
    app_practice_restart_timer(app);
    app_practice_refresh(app);
}

static void app_practice_step_by(GuitarChordsApp* app, int delta) {
    const Song* song = app_current_song(app);
    if(!song || song->count == 0) return;

    if(delta > 0) {
        app->practice_step = (uint8_t)((app->practice_step + 1) % song->count);
    } else {
        app->practice_step =
            (uint8_t)((app->practice_step + song->count - 1) % song->count);
    }
    app->practice_beat = 0;
}

/* Runs on the furi timer thread: hand the beat to the dispatcher queue rather
 * than touching views or the notification service from here. */
static void beat_timer_callback(void* context) {
    GuitarChordsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EventBeat);
}

static void app_practice_on_beat(GuitarChordsApp* app) {
    if(!app->practice_playing) return;

    if(app->practice_beat == 0) {
        notification_message(app->notifications, &sequence_change);
        app_practice_refresh(app);
    } else {
        notification_message(app->notifications, &sequence_beat);
    }

    app->practice_beat++;
    if(app->practice_beat >= SONG_BEATS_PER_CHORD) {
        app->practice_beat = 0;
        app_practice_step_by(app, +1);
    }
}

static bool custom_event_callback(void* context, uint32_t event) {
    GuitarChordsApp* app = context;
    if(event == EventBeat) {
        app_practice_on_beat(app);
        return true;
    }
    return false;
}

static void practice_action_callback(void* context, PracticeAction action) {
    GuitarChordsApp* app = context;

    switch(action) {
    case PracticeActionNext:
    case PracticeActionPrev:
        // Manual scrubbing implies you want it to hold still.
        app->practice_playing = false;
        furi_timer_stop(app->beat_timer);
        app_practice_step_by(app, action == PracticeActionNext ? +1 : -1);
        app_practice_refresh(app);
        break;

    case PracticeActionFaster:
        if(app->practice_bpm + SONG_BPM_STEP <= SONG_BPM_MAX) {
            app->practice_bpm += SONG_BPM_STEP;
            app_practice_restart_timer(app);
            app_practice_refresh(app);
        }
        break;

    case PracticeActionSlower:
        if(app->practice_bpm >= SONG_BPM_MIN + SONG_BPM_STEP) {
            app->practice_bpm -= SONG_BPM_STEP;
            app_practice_restart_timer(app);
            app_practice_refresh(app);
        }
        break;

    case PracticeActionToggle:
        app_practice_set_playing(app, !app->practice_playing);
        break;
    }
}

static void song_menu_callback(void* context, uint32_t index) {
    GuitarChordsApp* app = context;
    if(index >= app->songs->count) return;

    app->song_index = index;
    app->practice_step = 0;
    app->practice_beat = 0;
    app->practice_bpm = app->songs->items[index].bpm;
    app->practice_playing = false; // start paused; OK begins the count

    app_practice_refresh(app);
    app->current_view = ViewIdPractice;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPractice);
}

static void build_song_menu(GuitarChordsApp* app) {
    submenu_reset(app->song_menu);
    submenu_set_header(app->song_menu, "Progression");
    for(size_t i = 0; i < app->songs->count; i++) {
        submenu_add_item(app->song_menu, app->songs->items[i].name, i, song_menu_callback, app);
    }
}

/* ------------------------------------------------------------------ */
/* Menu building                                                       */
/* ------------------------------------------------------------------ */

static void app_show_diagram(GuitarChordsApp* app) {
    if(app->voicing_count == 0) return;
    const Chord* c = &app->db->items[app->voicings[app->voicing_index]];
    chord_view_set_chord(
        app->chord_view, c, (uint8_t)app->voicing_index, (uint8_t)app->voicing_count);
    app->current_view = ViewIdDiagram;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdDiagram);
}

/** Load the chord at `app->name_index` and show it from its first voicing. */
static void app_select_name(GuitarChordsApp* app) {
    app->voicing_count = chord_db_voicings(
        app->db, app->names[app->name_index], app->voicings, MAX_VOICINGS);
    app->voicing_index = 0;
    app_show_diagram(app);
}

static void chord_menu_callback(void* context, uint32_t index) {
    GuitarChordsApp* app = context;
    if(index >= app->name_count) return;

    app->name_index = index;
    app_select_name(app);
}

static void root_menu_callback(void* context, uint32_t index) {
    GuitarChordsApp* app = context;

    if(index == MENU_INDEX_PRACTICE) {
        build_song_menu(app);
        app->current_view = ViewIdSongMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSongMenu);
        return;
    }
    if(index >= app->root_count) return;

    app->name_count =
        chord_db_names_for_root(app->db, app->roots[index], app->names, MAX_NAMES);
    app->name_index = 0;

    submenu_reset(app->chord_menu);
    submenu_set_header(app->chord_menu, app->roots[index]);
    for(size_t i = 0; i < app->name_count; i++) {
        submenu_add_item(app->chord_menu, app->names[i], i, chord_menu_callback, app);
    }

    app->current_view = ViewIdChordMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdChordMenu);
}

static void build_root_menu(GuitarChordsApp* app) {
    app->root_count = chord_db_roots(app->db, app->roots, MAX_ROOTS);

    submenu_reset(app->root_menu);
    submenu_set_header(app->root_menu, "Guitar Chords");
    if(app->songs->count > 0) {
        submenu_add_item(
            app->root_menu, "Practice", MENU_INDEX_PRACTICE, root_menu_callback, app);
    }
    for(size_t i = 0; i < app->root_count; i++) {
        submenu_add_item(app->root_menu, app->roots[i], i, root_menu_callback, app);
    }
}

/* ------------------------------------------------------------------ */
/* Navigation                                                          */
/* ------------------------------------------------------------------ */

static void diagram_step_callback(void* context, int delta) {
    GuitarChordsApp* app = context;
    if(app->voicing_count <= 1) return;

    if(delta > 0) {
        app->voicing_index = (app->voicing_index + 1) % app->voicing_count;
    } else {
        app->voicing_index = (app->voicing_index + app->voicing_count - 1) % app->voicing_count;
    }
    app_show_diagram(app);
}

/** Up/Down while viewing a diagram: step through the current root's chord list. */
static void diagram_chord_step_callback(void* context, int delta) {
    GuitarChordsApp* app = context;
    if(app->name_count <= 1) return;

    if(delta > 0) {
        app->name_index = (app->name_index + 1) % app->name_count;
    } else {
        app->name_index = (app->name_index + app->name_count - 1) % app->name_count;
    }

    // Keep the menu's highlight in sync, so Back lands on the chord on screen.
    submenu_set_selected_item(app->chord_menu, (uint32_t)app->name_index);
    app_select_name(app);
}

static bool navigation_callback(void* context) {
    GuitarChordsApp* app = context;

    switch(app->current_view) {
    case ViewIdPractice:
        // Leaving practice must silence the metronome.
        app->practice_playing = false;
        furi_timer_stop(app->beat_timer);
        app->current_view = ViewIdSongMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSongMenu);
        return true;
    case ViewIdSongMenu:
        app->current_view = ViewIdRootMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdRootMenu);
        return true;
    case ViewIdDiagram:
        app->current_view = ViewIdChordMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdChordMenu);
        return true;
    case ViewIdChordMenu:
        app->current_view = ViewIdRootMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdRootMenu);
        return true;
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

/* ------------------------------------------------------------------ */
/* Library loading                                                     */
/* ------------------------------------------------------------------ */

static void app_load_library(GuitarChordsApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, CHORD_DB_DIR);
    FileInfo info;
    bool chords_exist = storage_common_stat(storage, CHORD_DB_PATH, &info) == FSE_OK;
    bool songs_exist = storage_common_stat(storage, SONG_DB_PATH, &info) == FSE_OK;
    furi_record_close(RECORD_STORAGE);

    if(!chords_exist) {
        FURI_LOG_I(TAG, "seeding %s", CHORD_DB_PATH);
        chord_db_write_default_csv(CHORD_DB_PATH);
    }
    if(!songs_exist) {
        FURI_LOG_I(TAG, "seeding %s", SONG_DB_PATH);
        song_db_write_default_csv(SONG_DB_PATH);
    }

    if(!chord_db_load_csv(app->db, CHORD_DB_PATH)) {
        FURI_LOG_W(TAG, "chord CSV unusable, falling back to builtin set");
        chord_db_load_builtin(app->db);
    }
    if(!song_db_load_csv(app->songs, SONG_DB_PATH)) {
        FURI_LOG_W(TAG, "song CSV unusable, falling back to builtin set");
        song_db_load_builtin(app->songs);
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static GuitarChordsApp* app_alloc(void) {
    GuitarChordsApp* app = malloc(sizeof(GuitarChordsApp));
    memset(app, 0, sizeof(GuitarChordsApp));

    app->db = chord_db_alloc();
    app->songs = song_db_alloc();
    app_load_library(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->root_menu = submenu_alloc();
    app->chord_menu = submenu_alloc();
    app->song_menu = submenu_alloc();
    app->chord_view = chord_view_alloc();
    chord_view_set_step_callback(app->chord_view, diagram_step_callback, app);
    chord_view_set_chord_step_callback(app->chord_view, diagram_chord_step_callback, app);

    app->practice_view = practice_view_alloc();
    practice_view_set_action_callback(app->practice_view, practice_action_callback, app);
    app->beat_timer = furi_timer_alloc(beat_timer_callback, FuriTimerTypePeriodic, app);
    app->practice_bpm = 80;

    app->splash_view = view_alloc();
    view_set_context(app->splash_view, app);
    view_set_draw_callback(app->splash_view, splash_draw_callback);
    view_set_input_callback(app->splash_view, splash_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdSplash, app->splash_view);

    view_dispatcher_add_view(app->view_dispatcher, ViewIdRootMenu, submenu_get_view(app->root_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdChordMenu, submenu_get_view(app->chord_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdDiagram, chord_view_get_view(app->chord_view));
    view_dispatcher_add_view(app->view_dispatcher, ViewIdSongMenu, submenu_get_view(app->song_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdPractice, practice_view_get_view(app->practice_view));

    build_root_menu(app);
    app->current_view = ViewIdSplash;

    return app;
}

static void app_free(GuitarChordsApp* app) {
    furi_timer_stop(app->beat_timer);
    furi_timer_free(app->beat_timer);

    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSplash);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdRootMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdChordMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdDiagram);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSongMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdPractice);

    submenu_free(app->root_menu);
    submenu_free(app->chord_menu);
    submenu_free(app->song_menu);
    chord_view_free(app->chord_view);
    practice_view_free(app->practice_view);
    view_free(app->splash_view);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    chord_db_free(app->db);
    song_db_free(app->songs);
    free(app);
}

int32_t guitar_chords_app(void* p) {
    UNUSED(p);

    GuitarChordsApp* app = app_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSplash);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);

    return 0;
}
