#include "include/infrastructure/saved_games.h"
#include "include/domain/game_limits.h"
#include <furi.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

void saved_games_record_from_setup(SavedGameRecord *out, const GameSetup *setup,
                                   uint32_t created_ts, const char *title) {
  if (!out || !setup) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->created_ts = created_ts;
  out->player_count = setup->player_count;
  out->impostor_count = setup->impostor_count;
  out->word_index = setup->word_index;
  snprintf(out->title, sizeof(out->title), "%s", title ? title : "");
  memcpy(out->names, setup->names, sizeof(out->names));
}

void saved_games_record_refresh_kept_meta(SavedGameRecord *out,
                                          const SavedGameRecord *meta_from,
                                          const GameSetup *setup) {
  if (!out || !meta_from || !setup) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->created_ts = meta_from->created_ts;
  memcpy(out->title, meta_from->title, sizeof(out->title));
  out->player_count = setup->player_count;
  out->impostor_count = setup->impostor_count;
  out->word_index = setup->word_index;
  memcpy(out->names, setup->names, sizeof(out->names));
}

#define APPS_DATA_DIR "/ext/apps_data/impostor_game"
#define GAMES_PATH APPS_DATA_DIR "/games.bin"

#define GAMES_MAGIC 0x494D5047u
#define GAMES_VERSION 2u

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t count;
  uint8_t reserved[2];
} GamesHeader;

typedef struct {
  uint32_t created_ts;
  uint8_t player_count;
  uint8_t impostor_count;
  uint16_t word_index;
  char names[IMPOSTOR_MAX_PLAYERS][IMPOSTOR_MAX_NAME_LEN];
} SavedGameDiskV1;

typedef struct {
  uint32_t created_ts;
  uint8_t player_count;
  uint8_t impostor_count;
  uint16_t word_index;
  char title[IMPOSTOR_MAX_SESSION_TITLE_LEN];
  char names[IMPOSTOR_MAX_PLAYERS][IMPOSTOR_MAX_NAME_LEN];
} SavedGameDiskV2;

static bool saved_games_write_all(const SavedGameRecord *list, uint8_t count) {
  if (!list || count > IMPOSTOR_MAX_SAVED_GAMES) {
    return false;
  }

  Storage *storage = furi_record_open(RECORD_STORAGE);
  File *file = storage_file_alloc(storage);

  storage_common_mkdir(storage, APPS_DATA_DIR);
  if (!storage_file_open(file, GAMES_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return false;
  }

  const GamesHeader hdr = {
      .magic = GAMES_MAGIC,
      .version = GAMES_VERSION,
      .count = count,
      .reserved = {0},
  };

  if (storage_file_write(file, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return false;
  }

  for (uint8_t i = 0; i < count; ++i) {
    SavedGameDiskV2 disk;
    disk.created_ts = list[i].created_ts;
    disk.player_count = list[i].player_count;
    disk.impostor_count = list[i].impostor_count;
    disk.word_index = list[i].word_index;
    memcpy(disk.title, list[i].title, sizeof(disk.title));
    memcpy(disk.names, list[i].names, sizeof(disk.names));

    if (storage_file_write(file, &disk, sizeof(disk)) != sizeof(disk)) {
      storage_file_close(file);
      storage_file_free(file);
      furi_record_close(RECORD_STORAGE);
      return false;
    }
  }

  storage_file_close(file);
  storage_file_free(file);
  furi_record_close(RECORD_STORAGE);
  return true;
}

bool saved_games_load(SavedGameRecord *out, uint8_t *count_out) {
  if (!out || !count_out) {
    return false;
  }
  *count_out = 0;

  Storage *storage = furi_record_open(RECORD_STORAGE);
  File *file = storage_file_alloc(storage);

  if (!storage_file_open(file, GAMES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
  }

  GamesHeader hdr;
  if (storage_file_read(file, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return false;
  }

  if (hdr.magic != GAMES_MAGIC || hdr.count > IMPOSTOR_MAX_SAVED_GAMES) {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return false;
  }

  if (hdr.version == 1u) {
    for (uint8_t i = 0; i < hdr.count; ++i) {
      SavedGameDiskV1 v1;
      if (storage_file_read(file, &v1, sizeof(v1)) != sizeof(v1)) {
        break;
      }
      out[i].created_ts = v1.created_ts;
      out[i].player_count = v1.player_count;
      out[i].impostor_count = v1.impostor_count;
      out[i].word_index = v1.word_index;
      memset(out[i].title, 0, sizeof(out[i].title));
      snprintf(out[i].title, sizeof(out[i].title), "Game %u",
               (unsigned)(i + 1u));
      memcpy(out[i].names, v1.names, sizeof(v1.names));
    }
    *count_out = hdr.count;
  } else if (hdr.version == 2u) {
    for (uint8_t i = 0; i < hdr.count; ++i) {
      SavedGameDiskV2 disk;
      if (storage_file_read(file, &disk, sizeof(disk)) != sizeof(disk)) {
        break;
      }
      out[i].created_ts = disk.created_ts;
      out[i].player_count = disk.player_count;
      out[i].impostor_count = disk.impostor_count;
      out[i].word_index = disk.word_index;
      memcpy(out[i].title, disk.title, sizeof(out[i].title));
      memcpy(out[i].names, disk.names, sizeof(disk.names));
    }
    *count_out = hdr.count;
  } else {
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return false;
  }

  storage_file_close(file);
  storage_file_free(file);
  furi_record_close(RECORD_STORAGE);
  return true;
}

bool saved_games_append(SavedGameRecord *list, uint8_t *count_io,
                        const SavedGameRecord *record) {
  if (!list || !count_io || !record) {
    return false;
  }

  if (!saved_games_load(list, count_io)) {
    *count_io = 0;
  }

  uint8_t count = *count_io;
  if (count >= IMPOSTOR_MAX_SAVED_GAMES) {
    memmove(&list[0], &list[1],
            (size_t)(sizeof(SavedGameRecord) * (IMPOSTOR_MAX_SAVED_GAMES - 1)));
    count = IMPOSTOR_MAX_SAVED_GAMES - 1;
  }

  list[count] = *record;
  count++;
  *count_io = count;
  return saved_games_write_all(list, count);
}

bool saved_games_replace_at(SavedGameRecord *list, uint8_t *count_io,
                            uint8_t index, const SavedGameRecord *record) {
  if (!list || !count_io || !record) {
    return false;
  }

  if (!saved_games_load(list, count_io)) {
    return false;
  }
  if (index >= *count_io) {
    return false;
  }
  list[index] = *record;
  return saved_games_write_all(list, *count_io);
}

bool saved_games_delete_at(SavedGameRecord *list, uint8_t *count_io,
                           uint8_t index) {
  if (!list || !count_io) {
    return false;
  }

  if (!saved_games_load(list, count_io)) {
    return false;
  }
  if (index >= *count_io) {
    return false;
  }
  for (uint8_t i = index; i + 1u < *count_io; ++i) {
    list[i] = list[i + 1u];
  }
  (*count_io)--;
  return saved_games_write_all(list, *count_io);
}
