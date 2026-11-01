// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "pcm_audio.h"
#include "taiko_gpio.h"
#include "taiko_hits.h"
#include "taiko_player_view.h"
#include "taiko_sfx.h"
#include "taiko_settings_storage.h"
#include "tja_parser.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <stdio.h>
#include <string.h>

#define PLAYER_PATH  EXT_PATH("apps_data/taiko_player")
#define SONGS_PATH   PLAYER_PATH "/songs"
#define HIT_FLASH_MS 70U

typedef struct {
    Storage* storage;
    DialogsApp* dialogs;
    Gui* gui;
    ViewPort* viewport;
    FuriString* draw_text;
    FuriMessageQueue* input;
    FuriMutex* mutex;
    TaikoGpio* gpio;
    TaikoMenu menu;
    TaikoSfx sfx;
    uint8_t volume;
    TaikoSettings saved_settings;
    bool settings_saved;
    bool settings_save_failed;
    TaikoPlayerView view;
} TaikoPlayer;

static void draw(Canvas* canvas, void* context) {
    TaikoPlayer* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    TaikoPlayerView view = app->view;
    furi_mutex_release(app->mutex);
    taiko_player_view_draw(canvas, &view, app->draw_text);
}

static void input(InputEvent* event, void* context) {
    TaikoPlayer* app = context;
    if((event->key == InputKeyBack && event->type == InputTypeLong) ||
       event->type == InputTypeShort ||
       (event->type == InputTypeRepeat &&
        (event->key == InputKeyUp || event->key == InputKeyDown))) {
        furi_message_queue_put(app->input, event, 0);
    }
}

static int read_tja_line(void* context, char* line, size_t capacity) {
    Stream* stream = context;
    size_t length = 0;
    uint8_t c;
    while(stream_read(stream, &c, 1) == 1) {
        if(c == '\n') {
            line[length] = '\0';
            return 1;
        }
        if(length + 1 >= capacity) return -1;
        line[length++] = (char)c;
    }
    line[length] = '\0';
    if(buffered_file_stream_get_error(stream) != FSE_OK) return -1;
    return length ? 1 : 0;
}

static bool load_chart(
    TaikoPlayer* app,
    const char* path,
    TjaCourse course,
    TjaChart* chart,
    const char** error) {
    Stream* stream = buffered_file_stream_alloc(app->storage);
    bool loaded = buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(loaded)
        loaded = tja_parse_course(read_tja_line, stream, course, chart, error);
    else
        *error = "Cannot open TJA file";
    buffered_file_stream_close(stream);
    stream_free(stream);
    if(loaded)
        loaded = tja_chart_set_roll_speed(chart, taiko_menu_playback_tenths(&app->menu), error);
    return loaded;
}

static PcmAudio* start_audio(
    TaikoPlayer* app,
    const FuriString* wave,
    uint32_t position,
    const TaikoMixer* mixer,
    const char** error) {
    PcmAudio* audio = pcm_audio_start(
        app->storage,
        furi_string_get_cstr(wave),
        position,
        mixer,
        app->volume,
        taiko_menu_playback_tenths(&app->menu));
    if(!audio) *error = "Not enough memory for audio";
    return audio;
}

static void stop_audio(PcmAudio** audio) {
    if(*audio) pcm_audio_free(*audio);
    *audio = NULL;
}

static void clear_hits(TaikoPlayer* app) {
    taiko_gpio_reset(app->gpio);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->view.hit_until = 0;
    app->view.target_hit_until = 0;
    furi_mutex_release(app->mutex);
}

static void set_chart_title(TaikoPlayer* app, const TjaChart* chart) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(
        app->view.title,
        sizeof(app->view.title),
        "%s",
        *chart->title ? chart->title : "Taiko Player");
    snprintf(app->view.course, sizeof(app->view.course), "%s", chart->course);
    app->view.bpm_tenths = (uint32_t)(tja_chart_bpm_at(chart, 0) * 10 + 0.5f);
    furi_mutex_release(app->mutex);
}

static void show_message(TaikoPlayer* app, const char* header, const char* message) {
    DialogMessage* dialog = dialog_message_alloc();
    dialog_message_set_header(dialog, header, 64, 3, AlignCenter, AlignTop);
    dialog_message_set_text(dialog, message, 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(dialog, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, dialog);
    dialog_message_free(dialog);
}

static void show_error(TaikoPlayer* app, const char* message) {
    show_message(app, "Cannot play", message);
}

/* Called only while the audio worker is stopped/finished, or before playback. */
static void save_settings(TaikoPlayer* app) {
    TaikoSettings settings;
    taiko_settings_capture(&settings, &app->menu, app->volume);
    if(app->settings_saved && taiko_settings_equal(&settings, &app->saved_settings)) return;
    if(taiko_settings_save(app->storage, &settings)) {
        app->saved_settings = settings;
        app->settings_saved = true;
        app->settings_save_failed = false;
    } else {
        app->settings_saved = false;
        if(!app->settings_save_failed) {
            app->settings_save_failed = true;
            show_message(app, "Cannot save", "Settings not saved.\nCheck the SD card.");
        }
    }
}

static FuriString* audio_path(const char* tja_path, const char* wave) {
    FuriString* path = furi_string_alloc();
    const char* slash = strrchr(tja_path, '/');
    if(slash) furi_string_set_strn(path, tja_path, slash - tja_path + 1);
    /* Conversion keeps the WAVE basename and replaces its extension with .wav. */
    char name[256];
    snprintf(name, sizeof(name), "%s", wave);
    for(char* p = name; *p; p++) {
        if(*p == '\\') *p = '/';
    }
    char* extension = strrchr(name, '.');
    char* separator = strrchr(name, '/');
    if(extension && (!separator || extension > separator)) *extension = '\0';
    furi_string_cat_str(path, name);
    furi_string_cat_str(path, ".wav");
    return path;
}

/* True means return to the file browser; false means exit the app. */
static bool play_song(TaikoPlayer* app, const char* path) {
    taiko_gpio_reset(app->gpio);
    furi_message_queue_reset(app->input);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memset(&app->view, 0, sizeof(app->view));
    snprintf(app->view.title, sizeof(app->view.title), "Loading chart...");
    app->view.loading = true;
    app->view.volume = app->volume;
    app->view.menu = app->menu;
    furi_mutex_release(app->mutex);
    view_port_enabled_set(app->viewport, true);
    view_port_update(app->viewport);

    Stream* stream = buffered_file_stream_alloc(app->storage);
    TjaChart chart = {0};
    uint8_t courses = 0;
    const char* error = NULL;
    bool loaded = buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(loaded)
        loaded = tja_list_courses(read_tja_line, stream, &courses, &error);
    else
        error = "Cannot open TJA file";
    buffered_file_stream_close(stream);
    stream_free(stream);
    if(loaded) {
        app->menu.courses = courses;
        app->menu.course = tja_default_course(courses);
        loaded = load_chart(app, path, app->menu.course, &chart, &error);
    }
    if(!loaded) {
        tja_chart_free(&chart);
        show_error(app, error);
        return true;
    }

    FuriString* wave = audio_path(path, chart.wave);
    TaikoMixer mixer = {
        .don = app->sfx.don,
        .ka = app->sfx.ka,
        .chart = &chart,
        .don_volume = app->menu.don_volume,
        .ka_volume = app->menu.ka_volume,
    };
    PcmAudio* audio = start_audio(app, wave, 0, &mixer, &error);
    set_chart_title(app, &chart);
    TaikoHitCursor hits;
    taiko_hits_seek(&chart, &hits, 0);
    bool next_song = false;
    bool outputs_finished = false;
    bool paused = false;
    bool resume_after_menu = false;
    uint32_t paused_position = 0;
    uint32_t duration = 0;
    const uint32_t tick_frequency = furi_kernel_get_tick_frequency();
    uint32_t frame_remainder = 0;
    uint32_t next_frame = furi_get_tick();
    while(true) {
        if(error) {
            next_song = true;
            break;
        }
        PcmAudioState state = paused ? PcmAudioPaused :
                              audio  ? pcm_audio_state(audio) :
                                       PcmAudioFinished;
        if(state == PcmAudioError) {
            error = pcm_audio_error(audio);
            next_song = true;
            break;
        }
        uint32_t position = audio ? pcm_audio_position_ms(audio) : paused_position;
        if(audio) duration = pcm_audio_duration_ms(audio);
        if(state == PcmAudioFinished && !outputs_finished) {
            taiko_gpio_reset(app->gpio);
            paused_position = position;
            /* Keep the end position, not the finished worker and its DMA buffer. */
            stop_audio(&audio);
            outputs_finished = true;
        }
        if(state == PcmAudioPlaying) {
            TaikoHit hit;
            while(taiko_hits_peek(&chart, &hits, &hit) && hit.time_ms <= (int64_t)position) {
                taiko_hits_advance(&chart, &hits);
                if(hit.time_ms < 0 || (int64_t)position - hit.time_ms > 120) continue;
                taiko_gpio_pulse(app->gpio, hit.right, hit.rim);
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->view.hit_until = furi_get_tick() + furi_ms_to_ticks(HIT_FLASH_MS);
                if(!hit.roll) app->view.target_hit_until = app->view.hit_until;
                app->view.hit_right = hit.right;
                app->view.hit_rim = hit.rim;
                furi_mutex_release(app->mutex);
            }
        }
        uint32_t now = furi_get_tick();
        if((int32_t)(now - next_frame) >= 0) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->view.loading = state == PcmAudioLoading;
            app->view.finished = state == PcmAudioFinished;
            app->view.paused = paused;
            app->view.volume = app->volume;
            app->view.menu = app->menu;
            app->view.position_ms = position;
            app->view.gogo = state != PcmAudioFinished && state != PcmAudioLoading &&
                             tja_chart_gogo_at(&chart, position);
            app->view.bpm_tenths = (uint32_t)(tja_chart_bpm_at(&chart, position) * 10 + 0.5f);
            if(state == PcmAudioFinished) {
                app->view.hit_until = 0;
                app->view.target_hit_until = 0;
            }
            app->view.lane_count = taiko_lane_collect(
                chart.notes,
                chart.note_count,
                chart.min_note_speed,
                position,
                taiko_menu_scroll_tenths(&app->menu),
                app->view.lane_notes,
                TAIKO_LANE_MAX_NOTES);
            app->view.roll_count = taiko_lane_collect_rolls(
                &chart,
                position,
                taiko_menu_scroll_tenths(&app->menu),
                app->view.lane_rolls,
                TAIKO_LANE_MAX_ROLLS,
                &app->view.balloon);
            taiko_lane_collect_bars(
                &chart, position, taiko_menu_scroll_tenths(&app->menu), app->view.bar_columns);
            furi_mutex_release(app->mutex);
            view_port_update(app->viewport);
            /* Carry fractional ticks: 30 FPS at 1 kHz alternates 33/34 ticks. */
            frame_remainder += tick_frequency;
            const uint32_t frame_ticks = frame_remainder / app->menu.frame_rate;
            frame_remainder %= app->menu.frame_rate;
            next_frame += frame_ticks;
            /* Skip missed refresh slots instead of submitting a burst of old frames. */
            if((int32_t)(now - next_frame) >= 0) next_frame = now + frame_ticks;
        }

        InputEvent event;
        /* Pausing has already stopped audio and drained GPIO timers. Keep the
         * static view until input changes it instead of polling/redrawing it.
         * Every visible input change schedules an immediate frame below. */
        const uint32_t input_wait = paused ? FuriWaitForever : furi_ms_to_ticks(2);
        if(furi_message_queue_get(app->input, &event, input_wait) == FuriStatusOk) {
            if(event.key == InputKeyBack && event.type == InputTypeLong) break;
            if(!app->menu.open && (event.key == InputKeyUp || event.key == InputKeyDown)) {
                if(event.key == InputKeyUp && app->volume < TAIKO_MASTER_VOLUME_MAX)
                    app->volume += 10;
                if(event.key == InputKeyDown && app->volume > 0) app->volume -= 10;
                if(audio) pcm_audio_set_volume(audio, app->volume);
                next_frame = furi_get_tick();
                continue;
            }
            if(state == PcmAudioLoading) continue;
            if(event.key == InputKeyBack || app->menu.open) {
                if(!app->menu.open) {
                    resume_after_menu = state == PcmAudioPlaying;
                    clear_hits(app);
                    if(resume_after_menu) {
                        paused_position = pcm_audio_position_ms(audio);
                        stop_audio(&audio);
                        paused = true;
                    }
                    taiko_menu_open(&app->menu);
                    save_settings(app);
                } else {
                    TaikoMenuAction action = TaikoMenuActionNone;
                    if(event.key == InputKeyBack) {
                        action = taiko_menu_back(&app->menu);
                    } else if(event.key == InputKeyUp || event.key == InputKeyDown) {
                        taiko_menu_move(&app->menu, event.key == InputKeyDown);
                    } else if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                        taiko_menu_adjust(&app->menu, event.key == InputKeyRight);
                    } else if(event.key == InputKeyOk) {
                        action = taiko_menu_activate(&app->menu);
                    }
                    if(action == TaikoMenuActionRestoreDefaults) {
                        TaikoSettings defaults;
                        taiko_settings_defaults(&defaults);
                        taiko_settings_apply(&defaults, &app->menu, &app->volume);
                        app->settings_saved = false;
                    }
                    mixer.don_volume = app->menu.don_volume;
                    mixer.ka_volume = app->menu.ka_volume;
                    furi_check(taiko_gpio_configure(app->gpio, &app->menu.gpio));
                    if(action != TaikoMenuActionNone) save_settings(app);
                    if(action == TaikoMenuActionExit) break;
                    if(action == TaikoMenuActionNewSong) {
                        next_song = true;
                        break;
                    }
                    if((action == TaikoMenuActionReturn || action == TaikoMenuActionRestart) &&
                       app->menu.course != chart.course_id) {
                        const TjaCourse previous_course = chart.course_id;
                        /* A second chart can exceed the device heap. Stop the
                         * mixer before releasing the chart it borrows. */
                        stop_audio(&audio);
                        tja_chart_free(&chart);
                        const char* change_error = NULL;
                        if(!load_chart(app, path, app->menu.course, &chart, &change_error)) {
                            tja_chart_free(&chart);
                            app->menu.course = previous_course;
                            /* Recover by rereading the old course, never by
                             * retaining two charts while loading a new one. */
                            const char* restore_error = NULL;
                            if(!load_chart(app, path, previous_course, &chart, &restore_error)) {
                                error = restore_error;
                                next_song = true;
                                break;
                            }
                            taiko_hits_seek(&chart, &hits, paused_position);
                            app->menu.open = true;
                            app->menu.selected = TaikoMenuDifficulty;
                            show_error(app, change_error);
                            next_frame = furi_get_tick();
                            continue;
                        }
                        if(state == PcmAudioFinished) resume_after_menu = true;
                        furi_string_free(wave);
                        wave = audio_path(path, chart.wave);
                        taiko_hits_seek(&chart, &hits, 0);
                        paused_position = 0;
                        paused = true;
                        outputs_finished = false;
                        set_chart_title(app, &chart);
                    }
                    if((action == TaikoMenuActionReturn || action == TaikoMenuActionRestart) &&
                       taiko_menu_playback_tenths(&app->menu) != chart.roll_speed_tenths) {
                        const char* rate_error = NULL;
                        if(!tja_chart_set_roll_speed(
                               &chart, taiko_menu_playback_tenths(&app->menu), &rate_error)) {
                            taiko_menu_set_value(
                                &app->menu, TaikoMenuPlaybackSpeed, chart.roll_speed_tenths);
                            app->menu.open = true;
                            app->menu.selected = TaikoMenuPlaybackSpeed;
                            save_settings(app);
                            show_error(app, rate_error);
                            next_frame = furi_get_tick();
                            continue;
                        }
                        taiko_hits_seek(&chart, &hits, paused_position);
                    }
                    if(action == TaikoMenuActionRestart) {
                        stop_audio(&audio);
                        audio = start_audio(app, wave, 0, &mixer, &error);
                        paused = false;
                        taiko_hits_seek(&chart, &hits, 0);
                        outputs_finished = false;
                    } else if(action == TaikoMenuActionReturn && resume_after_menu) {
                        audio = start_audio(app, wave, paused_position, &mixer, &error);
                        paused = false;
                    }
                }
                next_frame = furi_get_tick();
                continue;
            }
            if(event.key != InputKeyOk && event.key != InputKeyLeft && event.key != InputKeyRight)
                continue;
            position = audio ? pcm_audio_position_ms(audio) : paused_position;
            clear_hits(app);
            if(event.key == InputKeyOk) {
                if(paused) {
                    audio = start_audio(app, wave, position, &mixer, &error);
                    paused = false;
                } else if(state == PcmAudioFinished) {
                    stop_audio(&audio);
                    audio = start_audio(app, wave, 0, &mixer, &error);
                    taiko_hits_seek(&chart, &hits, 0);
                } else {
                    paused_position = position;
                    stop_audio(&audio);
                    paused = true;
                }
            } else {
                bool forward = event.key == InputKeyRight;
                int64_t step = !forward ? -(int64_t)taiko_menu_backward_ms(&app->menu) :
                                          taiko_menu_forward_ms(&app->menu);
                int64_t target = app->menu.seek_mode == TaikoSeekByBars ?
                                     tja_chart_seek_bars(
                                         &chart,
                                         position,
                                         duration,
                                         forward,
                                         taiko_menu_bar_count(&app->menu, forward)) :
                                     (int64_t)position + step;
                if(target < 0) target = 0;
                if(target > duration) target = duration;
                stop_audio(&audio);
                paused_position = (uint32_t)target;
                if(!paused) audio = start_audio(app, wave, paused_position, &mixer, &error);
                taiko_hits_seek(&chart, &hits, paused_position);
            }
            outputs_finished = false;
            next_frame = furi_get_tick();
        }
    }
    taiko_gpio_reset(app->gpio);
    stop_audio(&audio);
    furi_string_free(wave);
    tja_chart_free(&chart);
    save_settings(app);
    if(error) show_error(app, error);
    return next_song;
}

int32_t taiko_player_app(void* context) {
    UNUSED(context);
    TaikoPlayer* app = malloc(sizeof(TaikoPlayer));
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->gui = furi_record_open(RECORD_GUI);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->draw_text = furi_string_alloc();
    furi_string_reserve(app->draw_text, sizeof(app->view.title));
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gpio = taiko_gpio_alloc();
    app->sfx = taiko_sfx_load(app->storage);
    taiko_menu_init(&app->menu);
    app->volume = TAIKO_DEFAULT_VOLUME;
    app->settings_save_failed = false;
    memset(&app->view, 0, sizeof(app->view));
    app->viewport = view_port_alloc();
    view_port_draw_callback_set(app->viewport, draw, app);
    view_port_input_callback_set(app->viewport, input, app);
    view_port_enabled_set(app->viewport, false);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
    storage_simply_mkdir(app->storage, PLAYER_PATH);
    storage_simply_mkdir(app->storage, SONGS_PATH);
    app->settings_saved = taiko_settings_load(app->storage, &app->saved_settings);
    taiko_settings_apply(&app->saved_settings, &app->menu, &app->volume);
    furi_check(taiko_gpio_configure(app->gpio, &app->menu.gpio));

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_display_backlight_enforce_on);
    FuriString* path = furi_string_alloc_set(SONGS_PATH);
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".tja", NULL);
    options.base_path = PLAYER_PATH;
    options.hide_ext = false;
    while(true) {
        view_port_enabled_set(app->viewport, false);
        if(!dialog_file_browser_show(app->dialogs, path, path, &options)) break;
        if(!play_song(app, furi_string_get_cstr(path))) break;
        furi_string_set(path, SONGS_PATH);
    }
    furi_string_free(path);
    save_settings(app);
    taiko_gpio_free(app->gpio);
    taiko_sfx_free(&app->sfx);
    view_port_enabled_set(app->viewport, false);
    gui_remove_view_port(app->gui, app->viewport);
    view_port_free(app->viewport);
    furi_string_free(app->draw_text);
    furi_message_queue_free(app->input);
    furi_mutex_free(app->mutex);
    notification_message(notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
