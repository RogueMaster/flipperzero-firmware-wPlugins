#include "holdem_storage.h"

#include "holdem_engine.h"

#include <string.h>

// Version 2 saves predate the fourth seat. We keep a local copy of the old layout so
// migration code can stay explicit instead of relying on fragile size assumptions.
typedef struct {
    char name[8];
    int32_t stack;
    Card hole[2];
    bool in_hand;
    bool is_bot;
    bool all_in;
    int32_t street_bet;
    int32_t hand_contrib;
} PlayerV2Legacy;

typedef struct {
    PlayerV2Legacy players[HOLDEM_LEGACY_V1_PLAYERS];
    size_t player_count;

    Card deck[HOLDEM_DECK_SIZE];
    size_t deck_pos;

    Card board[HOLDEM_BOARD_MAX];
    size_t board_count;

    int32_t pot;
    int32_t small_blind;
    int32_t big_blind;

    int button;
    int sb_idx;
    int bb_idx;
    int hand_no;
    HoldemStage stage;
    bool hand_in_progress;
} HoldemGameV2Legacy;

typedef struct {
    uint32_t magic;
    uint16_t version;
    HoldemGameV2Legacy game;
} HoldemSaveV2Legacy;

bool save_exists_on_storage(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool exists = storage_file_open(file, HOLDEM_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(exists) storage_file_close(file);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return exists;
}

void delete_save_on_storage(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;

    (void)storage_common_remove(storage, HOLDEM_SAVE_PATH);
    furi_record_close(RECORD_STORAGE);
}

bool save_progress(const HoldemGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    (void)storage_common_mkdir(storage, HOLDEM_SAVE_DIR);

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool ok = false;
    if(storage_file_open(file, HOLDEM_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        HoldemSave save;
        memset(&save, 0, sizeof(save));
        save.magic = HOLDEM_SAVE_MAGIC;
        save.version = 3;
        save.game = *game;

        // Persist the whole gameplay snapshot so resume never has to infer hidden cards or board state.
        ok = (storage_file_write(file, &save, sizeof(save)) == sizeof(save));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool load_legacy_v2_save(File* file, HoldemGame* game) {
    HoldemSaveV2Legacy legacy_save;
    memset(&legacy_save, 0, sizeof(legacy_save));
    storage_file_seek(file, 0, true);

    if(storage_file_read(file, &legacy_save, sizeof(legacy_save)) != sizeof(legacy_save)) return false;
    if(legacy_save.magic != HOLDEM_SAVE_MAGIC || legacy_save.version != 2) return false;

    init_game(game);

    for(size_t player_index = 0; player_index < HOLDEM_LEGACY_V1_PLAYERS; player_index++) {
        Player* current_player = &game->players[player_index];
        const PlayerV2Legacy* legacy_player = &legacy_save.game.players[player_index];
        *current_player = (Player){0};
        memcpy(current_player->name, legacy_player->name, sizeof(current_player->name));
        current_player->stack = legacy_player->stack;
        current_player->hole[0] = legacy_player->hole[0];
        current_player->hole[1] = legacy_player->hole[1];
        current_player->in_hand = legacy_player->in_hand;
        current_player->is_bot = legacy_player->is_bot;
        current_player->all_in = legacy_player->all_in;
        current_player->street_bet = legacy_player->street_bet;
        current_player->hand_contrib = legacy_player->hand_contrib;
    }

    // A pre-four-player save resumes safely by starting the next hand with the extra seat added.
    game->small_blind = legacy_save.game.small_blind;
    game->big_blind = legacy_save.game.big_blind;
    game->button = legacy_save.game.button;
    game->hand_no = legacy_save.game.hand_no;
    game->hand_in_progress = false;
    game->stage = StagePreflop;
    game->pot = 0;
    game->board_count = 0;
    game->deck_pos = 0;
    return true;
}

bool load_progress(HoldemGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool ok = false;
    if(storage_file_open(file, HOLDEM_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        HoldemSave save;
        memset(&save, 0, sizeof(save));
        size_t read = storage_file_read(file, &save, sizeof(save));

        if(read == sizeof(save) && save.magic == HOLDEM_SAVE_MAGIC && save.version == 3) {
            // Validate the persisted snapshot before trusting it. A corrupt save should fail closed.
            if(
                save.game.player_count >= HOLDEM_MIN_PLAYERS &&
                save.game.player_count <= HOLDEM_MAX_PLAYERS &&
                save.game.deck_pos <= HOLDEM_DECK_SIZE &&
                save.game.board_count <= HOLDEM_BOARD_MAX &&
                save.game.stage <= StageShowdown) {
                *game = save.game;
                ok = true;
            }
        } else if(load_legacy_v2_save(file, game)) {
            ok = true;
        } else {
            storage_file_seek(file, 0, true);

            HoldemSaveV1 legacy;
            memset(&legacy, 0, sizeof(legacy));
            if(storage_file_read(file, &legacy, sizeof(legacy)) == sizeof(legacy)) {
                if(legacy.magic == HOLDEM_SAVE_MAGIC && legacy.version == 1) {
                    // Version 1 only stored coarse table state, so we recover what we can and restart cleanly.
                    for(size_t player_index = 0; player_index < HOLDEM_LEGACY_V1_PLAYERS; player_index++) {
                        if(legacy.stack[player_index] > 0) {
                            game->players[player_index].stack = legacy.stack[player_index];
                        }
                    }

                    if(legacy.hand_no >= 0) game->hand_no = legacy.hand_no;
                    game->button = legacy.button;
                    if(legacy.sb > 0) game->small_blind = legacy.sb;
                    if(legacy.bb > 0) game->big_blind = legacy.bb;
                    game->hand_in_progress = false;
                    ok = true;
                }
            }
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

