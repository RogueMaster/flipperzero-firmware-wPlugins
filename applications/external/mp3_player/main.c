#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include "mp3_playback.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define TAG "Mp3Player"
#define MUSIC_DIRECTORY EXT_PATH("music")
#define MAX_SONGS 100
#define MAX_FILENAME 128
#define MAX_MUSIC_PATH 256
#define MAX_SCAN_ENTRIES 1024
#define MAX_SCAN_TIME_MS 5000U
#define SEEK_STEP_MS 5000U
#define VISIBLE_ROWS 4
#define MP3_SETTINGS_PATH APP_DATA_PATH("settings.bin")
#define MP3_LIBRARY_PATH_FILE APP_DATA_PATH("music_path.txt")
#define MP3_SETTINGS_MAGIC 0x4DU
#define MP3_SETTINGS_VERSION 1U

typedef enum {
  Mp3ScreenMain,
  Mp3ScreenSongs,
  Mp3ScreenSettings,
  Mp3ScreenAbout,
  Mp3ScreenNowPlaying,
} Mp3Screen;

typedef enum {
  Mp3RepeatOff,
  Mp3RepeatOne,
  Mp3RepeatAll,
} Mp3RepeatMode;

typedef enum {
  Mp3OutputInternal,
  Mp3OutputMax98357a,
} Mp3Output;

typedef struct {
  char filename[MAX_FILENAME];
} Mp3Song;

typedef struct {
  uint8_t volume;
  uint8_t repeat;
  uint8_t output;
  uint8_t reserved;
} Mp3SettingsV1;

typedef struct {
  FuriMessageQueue *input_queue;
  ViewPort *view_port;
  Gui *gui;
  Storage *storage;
  Mp3Playback *playback;

  Mp3Screen screen;
  uint8_t main_selection;
  uint8_t settings_selection;
  uint8_t song_selection;
  uint8_t song_offset;

  Mp3Song songs[MAX_SONGS];
  char music_directory[MAX_MUSIC_PATH];
  uint8_t song_count;
  int16_t current_song;

  uint8_t volume;
  Mp3RepeatMode repeat;
  Mp3Output output;
  bool settings_dirty;
  bool library_loaded;
  bool scanning;
  bool playing;
  bool seeking;
  bool seek_was_playing;
  InputKey seek_key;
  uint32_t seek_target_ms;
  bool show_status;
  char status[48];
} Mp3App;

static void mp3_load_settings(Mp3App *app) {
  Mp3SettingsV1 settings = {0};
  FuriString *path = furi_string_alloc_set(MP3_SETTINGS_PATH);
  storage_common_resolve_path_and_ensure_app_directory(app->storage, path);
  const bool loaded =
      saved_struct_load(furi_string_get_cstr(path), &settings, sizeof(settings),
                        MP3_SETTINGS_MAGIC, MP3_SETTINGS_VERSION);
  furi_string_free(path);

  if (loaded && settings.volume <= 100 && settings.repeat <= Mp3RepeatAll &&
      settings.output <= Mp3OutputMax98357a) {
    app->volume = settings.volume;
    app->repeat = (Mp3RepeatMode)settings.repeat;
    app->output = (Mp3Output)settings.output;
  }
  app->settings_dirty = false;
}

static bool mp3_save_settings(Mp3App *app) {
  if (!app->settings_dirty)
    return true;

  const Mp3SettingsV1 settings = {
      .volume = app->volume,
      .repeat = (uint8_t)app->repeat,
      .output = (uint8_t)app->output,
      .reserved = 0,
  };
  FuriString *path = furi_string_alloc_set(MP3_SETTINGS_PATH);
  storage_common_resolve_path_and_ensure_app_directory(app->storage, path);
  const bool saved =
      saved_struct_save(furi_string_get_cstr(path), &settings, sizeof(settings),
                        MP3_SETTINGS_MAGIC, MP3_SETTINGS_VERSION);
  furi_string_free(path);
  if (saved)
    app->settings_dirty = false;
  return saved;
}

static const char *const main_items[] = {
    "Now Playing",
    "Song List",
    "Settings",
    "About",
};

static bool mp3_has_extension(const char *name) {
  const size_t length = strlen(name);
  return length > 4 && name[length - 4] == '.' &&
         tolower((unsigned char)name[length - 3]) == 'm' &&
         tolower((unsigned char)name[length - 2]) == 'p' &&
         name[length - 1] == '3';
}

static void mp3_make_title(const char *filename, char *title,
                           size_t title_size) {
  strlcpy(title, filename, title_size);
  const size_t length = strlen(title);
  if (length > 4)
    title[length - 4] = '\0';
  for (char *cursor = title; *cursor; cursor++) {
    if (*cursor == '_')
      *cursor = ' ';
  }
}

static void mp3_set_status(Mp3App *app, const char *message) {
  strlcpy(app->status, message, sizeof(app->status));
  app->show_status = true;
}

static void mp3_load_music_directory(Mp3App *app) {
  strlcpy(app->music_directory, MUSIC_DIRECTORY, sizeof(app->music_directory));

  FuriString *config_path = furi_string_alloc_set(MP3_LIBRARY_PATH_FILE);
  storage_common_resolve_path_and_ensure_app_directory(app->storage,
                                                       config_path);
  File *file = storage_file_alloc(app->storage);
  const char *config_cstr = furi_string_get_cstr(config_path);
  const bool config_exists = storage_file_exists(app->storage, config_cstr);
  if (config_exists) {
    if (!storage_file_open(file, config_cstr, FSAM_READ, FSOM_OPEN_EXISTING)) {
      mp3_set_status(app, "Cannot read music_path.txt");
      storage_file_free(file);
      furi_string_free(config_path);
      return;
    }
    const size_t read = storage_file_read(file, app->music_directory,
                                          sizeof(app->music_directory) - 1U);
    app->music_directory[read] = '\0';
    storage_file_close(file);

    char *start = app->music_directory;
    if (read >= 3U && (uint8_t)start[0] == 0xEFU &&
        (uint8_t)start[1] == 0xBBU && (uint8_t)start[2] == 0xBFU)
      start += 3;
    while (*start && isspace((unsigned char)*start))
      start++;
    if (start != app->music_directory)
      memmove(app->music_directory, start, strlen(start) + 1U);

    char *end = app->music_directory;
    while (*end && *end != '\r' && *end != '\n')
      end++;
    *end = '\0';
    while (end > app->music_directory && isspace((unsigned char)end[-1]))
      *--end = '\0';
    while (end > app->music_directory + 4U && end[-1] == '/')
      *--end = '\0';

    if (strncmp(app->music_directory, "/ext", 4) != 0 ||
        (app->music_directory[4] != '\0' && app->music_directory[4] != '/')) {
      strlcpy(app->music_directory, MUSIC_DIRECTORY,
              sizeof(app->music_directory));
      mp3_set_status(app, "Invalid music_path.txt");
    }
  } else {
    storage_file_free(file);
    file = storage_file_alloc(app->storage);
    if (storage_file_open(file, config_cstr, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
      static const char default_path[] = MUSIC_DIRECTORY "\n";
      storage_file_write(file, default_path, sizeof(default_path) - 1U);
      storage_file_close(file);
    }
  }
  storage_file_free(file);
  furi_string_free(config_path);
}

static void mp3_scan_songs(Mp3App *app) {
  app->scanning = true;
  view_port_update(app->view_port);
  app->song_count = 0;
  mp3_load_music_directory(app);

  File *directory = storage_file_alloc(app->storage);
  uint16_t scanned_entries = 0;
  bool scan_limited = false;
  const uint32_t started = furi_get_tick();
  const uint32_t timeout = furi_ms_to_ticks(MAX_SCAN_TIME_MS);
  if (storage_dir_open(directory, app->music_directory)) {
    FileInfo info;
    char filename[MAX_FILENAME];
    while (app->song_count < MAX_SONGS &&
           storage_dir_read(directory, &info, filename, sizeof(filename))) {
      scanned_entries++;
      if (scanned_entries >= MAX_SCAN_ENTRIES ||
          (uint32_t)(furi_get_tick() - started) >= timeout) {
        scan_limited = true;
        break;
      }
      if (!file_info_is_dir(&info) && mp3_has_extension(filename)) {
        Mp3Song *song = &app->songs[app->song_count++];
        strlcpy(song->filename, filename, sizeof(song->filename));
      }
    }
  }
  storage_dir_close(directory);
  storage_file_free(directory);
  app->library_loaded = true;
  app->scanning = false;

  if (scan_limited)
    mp3_set_status(app, "Scan stopped at safety limit");
  else if (app->song_count == 0 && !app->show_status)
    mp3_set_status(app, "No MP3s in music path");

  if (app->song_selection >= app->song_count)
    app->song_selection = 0;
  app->song_offset = 0;
}

static const char *mp3_repeat_name(Mp3RepeatMode repeat) {
  switch (repeat) {
  case Mp3RepeatOne:
    return "One";
  case Mp3RepeatAll:
    return "All";
  default:
    return "Off";
  }
}

static const char *mp3_output_name(Mp3Output output) {
  switch (output) {
  case Mp3OutputMax98357a:
    return "External";
  default:
    return "Internal";
  }
}

static void mp3_draw_header(Canvas *canvas, const char *title) {
  canvas_set_color(canvas, ColorBlack);
  canvas_set_font(canvas, FontPrimary);
  canvas_draw_str(canvas, 3, 10, title);
  canvas_draw_line(canvas, 0, 13, 127, 13);
}

static void mp3_draw_row(Canvas *canvas, uint8_t y, const char *text,
                         bool selected) {
  if (selected) {
    canvas_draw_rbox(canvas, 1, y - 9, 126, 12, 2);
    canvas_set_color(canvas, ColorWhite);
  }
  canvas_set_font(canvas, FontSecondary);
  canvas_draw_str(canvas, 5, y, text);
  canvas_set_color(canvas, ColorBlack);
}

static void mp3_draw_main(Canvas *canvas, const Mp3App *app) {
  mp3_draw_header(canvas, "MP3 Player");
  for (uint8_t i = 0; i < COUNT_OF(main_items); i++) {
    mp3_draw_row(canvas, 25 + i * 12, main_items[i], i == app->main_selection);
  }
}

static void mp3_draw_songs(Canvas *canvas, const Mp3App *app) {
  char header[28];
  snprintf(header, sizeof(header), "Songs  %u", app->song_count);
  mp3_draw_header(canvas, header);

  if (app->scanning) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom,
                            "Scanning folders...");
    canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom,
                            "Please wait");
    return;
  }

  if (app->song_count == 0) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 29, "No MP3 files found");
    canvas_draw_str(canvas, 4, 43, "Check music_path.txt");
    canvas_draw_str(canvas, 4, 56, "in apps_data/mp3_player");
    return;
  }

  for (uint8_t row = 0; row < VISIBLE_ROWS; row++) {
    const uint8_t index = app->song_offset + row;
    if (index >= app->song_count)
      break;
    mp3_draw_row(canvas, 25 + row * 12, app->songs[index].filename,
                 index == app->song_selection);
  }
}

static void mp3_draw_settings(Canvas *canvas, const Mp3App *app) {
  mp3_draw_header(canvas, "Settings");
  char value[32];

  snprintf(value, sizeof(value), "Volume       %u%%", app->volume);
  mp3_draw_row(canvas, 25, value, app->settings_selection == 0);
  snprintf(value, sizeof(value), "Repeat       %s",
           mp3_repeat_name(app->repeat));
  mp3_draw_row(canvas, 37, value, app->settings_selection == 1);
  snprintf(value, sizeof(value), "Output       %s",
           mp3_output_name(app->output));
  mp3_draw_row(canvas, 49, value, app->settings_selection == 2);
  mp3_draw_row(canvas, 61, "Rescan library", app->settings_selection == 3);
}

static void mp3_draw_about(Canvas *canvas) {
  mp3_draw_header(canvas, "About");
  canvas_set_font(canvas, FontSecondary);
  canvas_draw_str(canvas, 4, 21, "MP3 Player v2.12");
  canvas_draw_str(canvas, 4, 29, "Created by Coolshrimp");
  canvas_draw_str(canvas, 4, 37, "External MAX98357A:");
  canvas_draw_str(canvas, 4, 45, "BCLK 5/PB3  DIN 2/PA7");
  canvas_draw_str(canvas, 4, 53, "LRC 4/PA4   VIN 1/5V");
  canvas_draw_str(canvas, 4, 61, "GND -> any GND pin");
}

static void mp3_draw_battery(Canvas *canvas) {
  const uint8_t percent = furi_hal_power_get_pct();
  canvas_draw_frame(canvas, 109, 2, 16, 8);
  canvas_draw_box(canvas, 125, 4, 2, 4);
  const uint8_t width = (uint8_t)((percent * 12U + 99U) / 100U);
  if (width)
    canvas_draw_box(canvas, 111, 4, width, 4);
}

static void mp3_draw_play_triangle(Canvas *canvas, int16_t center_x,
                                   int16_t center_y, bool points_right) {
  for (int8_t y = -5; y <= 5; y++) {
    const int8_t extent = (int8_t)(9 - ((y < 0 ? -y : y) * 9) / 5);
    if (points_right)
      canvas_draw_line(canvas, center_x - 4, center_y + y,
                       center_x - 4 + extent, center_y + y);
    else
      canvas_draw_line(canvas, center_x + 4 - extent, center_y + y,
                       center_x + 4, center_y + y);
  }
}

static void mp3_format_time(char *text, size_t size, uint32_t milliseconds) {
  const uint32_t seconds = milliseconds / 1000U;
  snprintf(text, size, "%lu:%02lu", (unsigned long)(seconds / 60U),
           (unsigned long)(seconds % 60U));
}

static void mp3_draw_now_playing(Canvas *canvas, const Mp3App *app) {
  canvas_set_color(canvas, ColorBlack);
  canvas_set_font(canvas, FontSecondary);

  canvas_draw_frame(canvas, 2, 1, 25, 11);
  canvas_draw_str(canvas, 5, 10, "MP3");
  mp3_draw_battery(canvas);

  if (app->current_song < 0 || app->song_count == 0) {
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom,
                            "Nothing selected");
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignBottom,
                            "Choose a song first");
    return;
  }

  char track[16];
  snprintf(track, sizeof(track), "%u/%u", (unsigned)(app->current_song + 1),
           (unsigned)app->song_count);
  canvas_draw_str_aligned(canvas, 52, 10, AlignCenter, AlignBottom, track);
  char volume[10];
  snprintf(volume, sizeof(volume), "V%u", app->volume);
  canvas_draw_str_aligned(canvas, 106, 10, AlignRight, AlignBottom, volume);

  const Mp3Song *song = &app->songs[app->current_song];
  char title[MAX_FILENAME];
  mp3_make_title(song->filename, title, sizeof(title));
  canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, title);

  const uint32_t position = app->seeking
                                ? app->seek_target_ms
                                : mp3_playback_get_position_ms(app->playback);
  const uint32_t duration = mp3_playback_get_duration_ms(app->playback);
  char elapsed[12];
  char total[12];
  char times[28];
  mp3_format_time(elapsed, sizeof(elapsed), position);
  if (duration)
    mp3_format_time(total, sizeof(total), duration);
  else
    strlcpy(total, "--:--", sizeof(total));
  snprintf(times, sizeof(times), "%s / %s", elapsed, total);
  canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, times);

  canvas_draw_frame(canvas, 3, 39, 122, 7);
  if (duration) {
    const uint32_t bounded = position < duration ? position : duration;
    const uint8_t progress = (uint8_t)(((uint64_t)bounded * 118U) / duration);
    if (progress)
      canvas_draw_box(canvas, 5, 41, progress, 3);
  }

  canvas_draw_line(canvas, 8, 52, 8, 62);
  mp3_draw_play_triangle(canvas, 15, 57, false);
  mp3_draw_play_triangle(canvas, 113, 57, true);
  canvas_draw_line(canvas, 120, 52, 120, 62);

  if (mp3_playback_is_running(app->playback) &&
      !mp3_playback_is_paused(app->playback)) {
    canvas_draw_box(canvas, 60, 52, 3, 11);
    canvas_draw_box(canvas, 66, 52, 3, 11);
  } else {
    mp3_draw_play_triangle(canvas, 64, 57, true);
  }
}

static void mp3_draw_callback(Canvas *canvas, void *context) {
  Mp3App *app = context;
  canvas_clear(canvas);

  switch (app->screen) {
  case Mp3ScreenSongs:
    mp3_draw_songs(canvas, app);
    break;
  case Mp3ScreenSettings:
    mp3_draw_settings(canvas, app);
    break;
  case Mp3ScreenAbout:
    mp3_draw_about(canvas);
    break;
  case Mp3ScreenNowPlaying:
    mp3_draw_now_playing(canvas, app);
    break;
  default:
    mp3_draw_main(canvas, app);
    break;
  }

  if (app->show_status) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 3, 43, 122, 18);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 2, 42, 124, 20);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 6, 56, app->status);
  }
}

static void mp3_input_callback(InputEvent *event, void *context) {
  FuriMessageQueue *queue = context;
  furi_message_queue_put(queue, event, 0);
}

static bool mp3_build_song_path(const Mp3App *app, char *path,
                                size_t path_size) {
  if (app->current_song < 0 || app->current_song >= app->song_count)
    return false;
  const int length = snprintf(path, path_size, "%s/%s", app->music_directory,
                              app->songs[app->current_song].filename);
  return length >= 0 && (size_t)length < path_size;
}

static bool mp3_start_current(Mp3App *app) {
  char path[MAX_MUSIC_PATH];
  if (!mp3_build_song_path(app, path, sizeof(path)))
    return false;

  const AudioOutput output = app->output == Mp3OutputInternal
                                 ? AudioOutputInternal
                                 : AudioOutputMax98357a;
  app->playing = mp3_playback_start(app->playback, path, output, app->volume);
  if (!app->playing) {
    const char *error = mp3_playback_get_error(app->playback);
    mp3_set_status(app, *error ? error : "Could not start player");
  }
  return app->playing;
}

static bool mp3_start_current_at(Mp3App *app, uint32_t position_ms,
                                 uint32_t duration_ms) {
  char path[MAX_MUSIC_PATH];
  if (!mp3_build_song_path(app, path, sizeof(path)))
    return false;

  const AudioOutput output = app->output == Mp3OutputInternal
                                 ? AudioOutputInternal
                                 : AudioOutputMax98357a;
  app->playing = mp3_playback_start_at(app->playback, path, output, app->volume,
                                       position_ms, duration_ms);
  if (!app->playing) {
    const char *error = mp3_playback_get_error(app->playback);
    mp3_set_status(app, *error ? error : "Could not seek");
  }
  return app->playing;
}

static void mp3_change_song(Mp3App *app, int8_t direction) {
  if (app->song_count == 0)
    return;
  const bool continue_playing = mp3_playback_is_running(app->playback) &&
                                !mp3_playback_is_paused(app->playback);
  mp3_playback_stop(app->playback);
  int16_t next = app->current_song;
  if (next < 0)
    next = 0;
  else
    next += direction;
  if (next < 0)
    next = app->song_count - 1;
  if (next >= app->song_count)
    next = 0;
  app->current_song = next;
  app->song_selection = next;
  app->playing = false;
  if (continue_playing)
    mp3_start_current(app);
}

static void mp3_toggle_playback(Mp3App *app) {
  if (app->current_song < 0) {
    mp3_set_status(app, "Select a song first");
    return;
  }

  if (mp3_playback_is_running(app->playback)) {
    const bool resume = mp3_playback_is_paused(app->playback);
    mp3_playback_set_paused(app->playback, !resume);
    app->playing = resume;
    return;
  }
  mp3_start_current(app);
}

static void mp3_poll_playback(Mp3App *app) {
  if (app->playing && !mp3_playback_is_running(app->playback)) {
    app->playing = false;
    const char *error = mp3_playback_get_error(app->playback);
    if (*error) {
      mp3_set_status(app, error);
    } else if (app->repeat == Mp3RepeatOne) {
      mp3_start_current(app);
    } else if (app->repeat == Mp3RepeatAll) {
      app->current_song = (app->current_song + 1) % app->song_count;
      app->song_selection = app->current_song;
      mp3_start_current(app);
    }
  }
}

static bool mp3_handle_main(Mp3App *app, InputKey key) {
  if (key == InputKeyUp) {
    app->main_selection = app->main_selection
                              ? app->main_selection - 1
                              : (uint8_t)(COUNT_OF(main_items) - 1);
  } else if (key == InputKeyDown) {
    app->main_selection = (app->main_selection + 1) % COUNT_OF(main_items);
  } else if (key == InputKeyOk) {
    static const Mp3Screen screens[] = {
        Mp3ScreenNowPlaying,
        Mp3ScreenSongs,
        Mp3ScreenSettings,
        Mp3ScreenAbout,
    };
    app->screen = screens[app->main_selection];
    if (app->screen == Mp3ScreenSongs && !app->library_loaded)
      mp3_scan_songs(app);
  } else if (key == InputKeyBack) {
    return false;
  }
  return true;
}

static void mp3_handle_songs(Mp3App *app, InputKey key) {
  if (key == InputKeyBack) {
    app->screen = Mp3ScreenMain;
  } else if (key == InputKeyUp && app->song_count) {
    app->song_selection =
        app->song_selection ? app->song_selection - 1 : app->song_count - 1;
  } else if (key == InputKeyDown && app->song_count) {
    app->song_selection = (app->song_selection + 1) % app->song_count;
  } else if (key == InputKeyOk && app->song_count) {
    mp3_playback_stop(app->playback);
    app->current_song = app->song_selection;
    app->playing = false;
    app->screen = Mp3ScreenNowPlaying;
    mp3_start_current(app);
  }

  if (app->song_selection < app->song_offset)
    app->song_offset = app->song_selection;
  if (app->song_selection >= app->song_offset + VISIBLE_ROWS) {
    app->song_offset = app->song_selection - VISIBLE_ROWS + 1;
  }
}

static void mp3_handle_settings(Mp3App *app, InputKey key) {
  if (key == InputKeyBack) {
    mp3_save_settings(app);
    app->screen = Mp3ScreenMain;
  } else if (key == InputKeyUp) {
    app->settings_selection =
        app->settings_selection ? app->settings_selection - 1 : 3;
  } else if (key == InputKeyDown) {
    app->settings_selection = (app->settings_selection + 1) % 4;
  } else if (app->settings_selection == 0 &&
             (key == InputKeyLeft || key == InputKeyRight)) {
    const uint8_t previous = app->volume;
    if (key == InputKeyRight && app->volume < 100)
      app->volume += 10;
    if (key == InputKeyLeft && app->volume > 0)
      app->volume -= 10;
    if (app->volume != previous)
      app->settings_dirty = true;
    mp3_playback_set_volume(app->playback, app->volume);
  } else if (app->settings_selection == 1 &&
             (key == InputKeyLeft || key == InputKeyRight)) {
    app->repeat = (app->repeat + (key == InputKeyRight ? 1 : 2)) % 3;
    app->settings_dirty = true;
  } else if (app->settings_selection == 2 &&
             (key == InputKeyLeft || key == InputKeyRight)) {
    mp3_playback_stop(app->playback);
    app->playing = false;
    app->output = app->output == Mp3OutputInternal ? Mp3OutputMax98357a
                                                   : Mp3OutputInternal;
    app->settings_dirty = true;
  } else if (app->settings_selection == 3 && key == InputKeyOk) {
    mp3_playback_stop(app->playback);
    app->playing = false;
    app->current_song = -1;
    mp3_scan_songs(app);
    if (app->song_count)
      mp3_set_status(app, "Library refreshed");
  }
}

static void mp3_seek_step(Mp3App *app, InputKey key) {
  const uint32_t duration = mp3_playback_get_duration_ms(app->playback);
  if (!duration) {
    mp3_set_status(app, "Wait for song time");
    return;
  }

  if (!app->seeking) {
    app->seeking = true;
    app->seek_key = key;
    app->seek_was_playing = mp3_playback_is_running(app->playback) &&
                            !mp3_playback_is_paused(app->playback);
    app->seek_target_ms = mp3_playback_get_position_ms(app->playback);
    if (mp3_playback_is_running(app->playback))
      mp3_playback_set_paused(app->playback, true);
  }

  if (key == InputKeyLeft) {
    app->seek_target_ms = app->seek_target_ms > SEEK_STEP_MS
                              ? app->seek_target_ms - SEEK_STEP_MS
                              : 0;
  } else {
    const uint32_t last_position = duration > 1000U ? duration - 1000U : 0;
    app->seek_target_ms = app->seek_target_ms + SEEK_STEP_MS < last_position
                              ? app->seek_target_ms + SEEK_STEP_MS
                              : last_position;
  }
}

static void mp3_finish_seek(Mp3App *app) {
  if (!app->seeking)
    return;

  const uint32_t target = app->seek_target_ms;
  const uint32_t duration = mp3_playback_get_duration_ms(app->playback);
  const bool resume = app->seek_was_playing;
  app->seeking = false;
  mp3_playback_stop(app->playback);
  app->playing = false;
  if (mp3_start_current_at(app, target, duration) && !resume) {
    mp3_playback_set_paused(app->playback, true);
    app->playing = false;
  }
}

static void mp3_handle_now_playing(Mp3App *app, const InputEvent *event) {
  const InputKey key = event->key;
  if ((event->type == InputTypeLong || event->type == InputTypeRepeat) &&
      (key == InputKeyLeft || key == InputKeyRight)) {
    mp3_seek_step(app, key);
    return;
  }
  if (event->type == InputTypeRelease && app->seeking && key == app->seek_key) {
    mp3_finish_seek(app);
    return;
  }
  if (event->type != InputTypeShort && event->type != InputTypeRepeat)
    return;

  if (key == InputKeyBack) {
    app->screen = Mp3ScreenMain;
  } else if (key == InputKeyOk) {
    mp3_toggle_playback(app);
  } else if (key == InputKeyLeft) {
    mp3_change_song(app, -1);
  } else if (key == InputKeyRight) {
    mp3_change_song(app, 1);
  } else if (key == InputKeyUp && app->volume < 100) {
    app->volume += 10;
    app->settings_dirty = true;
    mp3_playback_set_volume(app->playback, app->volume);
  } else if (key == InputKeyDown && app->volume > 0) {
    app->volume -= 10;
    app->settings_dirty = true;
    mp3_playback_set_volume(app->playback, app->volume);
  }
}

static bool mp3_handle_input(Mp3App *app, const InputEvent *event) {
  if (event->type != InputTypeShort && event->type != InputTypeLong &&
      event->type != InputTypeRepeat && event->type != InputTypeRelease)
    return true;
  if (app->screen != Mp3ScreenNowPlaying && event->type != InputTypeShort &&
      event->type != InputTypeRepeat)
    return true;

  if (app->show_status) {
    app->show_status = false;
    return true;
  }

  switch (app->screen) {
  case Mp3ScreenSongs:
    mp3_handle_songs(app, event->key);
    break;
  case Mp3ScreenSettings:
    mp3_handle_settings(app, event->key);
    break;
  case Mp3ScreenAbout:
    if (event->key == InputKeyBack)
      app->screen = Mp3ScreenMain;
    break;
  case Mp3ScreenNowPlaying:
    mp3_handle_now_playing(app, event);
    break;
  default:
    return mp3_handle_main(app, event->key);
  }
  return true;
}

static Mp3App *mp3_app_alloc(void) {
  Mp3App *app = malloc(sizeof(Mp3App));
  memset(app, 0, sizeof(Mp3App));
  app->current_song = -1;
  app->volume = 50;
  app->storage = furi_record_open(RECORD_STORAGE);
  mp3_load_music_directory(app);
  mp3_load_settings(app);
  app->playback = mp3_playback_alloc();
  app->input_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
  app->gui = furi_record_open(RECORD_GUI);
  app->view_port = view_port_alloc();
  view_port_draw_callback_set(app->view_port, mp3_draw_callback, app);
  view_port_input_callback_set(app->view_port, mp3_input_callback,
                               app->input_queue);
  gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
  return app;
}

static void mp3_app_free(Mp3App *app) {
  mp3_playback_free(app->playback);
  mp3_save_settings(app);
  gui_remove_view_port(app->gui, app->view_port);
  view_port_free(app->view_port);
  furi_record_close(RECORD_GUI);
  furi_record_close(RECORD_STORAGE);
  furi_message_queue_free(app->input_queue);
  free(app);
}

int32_t app_main(void *parameter) {
  UNUSED(parameter);
  Mp3App *app = mp3_app_alloc();
  bool running = true;
  InputEvent event;

  while (running) {
    if (furi_message_queue_get(app->input_queue, &event,
                               furi_ms_to_ticks(250)) == FuriStatusOk) {
      running = mp3_handle_input(app, &event);
    }
    mp3_poll_playback(app);
    view_port_update(app->view_port);
  }

  mp3_app_free(app);
  return 0;
}
