/**
 * @file tamagometer_companion.c
 *
 * This application will display a text box with some scrollable text in it.
 * Press the Back key to exit the application.
 * It will also add the command "tamagometer" to the CLI for use with
 * https://zacharesmer.github.io/tamagometer/
 */

#include <furi.h>

#include <gui/gui.h>
#include <gui/modules/text_box.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/view_holder.h>

#include <storage/storage.h>

#include <cli/cli.h>
#include <furi_hal_infrared.h>
#include <infrared.h>
#include <infrared_transmit.h>
#include <infrared_worker.h>

#include <api_lock.h>

#define TAMA_SETTINGS_PATH "/ext/apps_data/tama_connect/settings.bin"

static void tama_set_byte(char* packet, size_t byte_number, uint8_t value);

typedef struct {
    char name[6];
    uint8_t character;
    bool girl;
} TamaProfile;

static void tama_profile_defaults(TamaProfile* profile) {
    strcpy(profile->name, "FLIP");
    profile->character = 19;
    profile->girl = false;
}

static bool tama_profile_save(const TamaProfile* profile) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_mkdir(storage, "/ext/apps_data/tama_connect");

    File* file = storage_file_alloc(storage);

    bool ok = storage_file_open(file, TAMA_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(ok) {
        ok = storage_file_write(file, profile, sizeof(TamaProfile)) == sizeof(TamaProfile);
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return ok;
}

static bool tama_profile_load(TamaProfile* profile) {
    tama_profile_defaults(profile);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool ok = storage_file_open(file, TAMA_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING);

    if(ok) {
        TamaProfile loaded;

        ok = storage_file_read(file, &loaded, sizeof(TamaProfile)) == sizeof(TamaProfile);

        if(ok) {
            loaded.name[5] = '\0';

            if(loaded.character <= 63) {
                *profile = loaded;
            } else {
                ok = false;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return ok;
}

/*
 * Tamagotchi Connection name encoding:
 *
 * A = 0
 * B = 1
 * ...
 * Z = 25
 * _ = 160
 */
static uint8_t tama_encode_name_char(char c) {
    if(c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    if(c >= 'A' && c <= 'Z') {
        return (uint8_t)(c - 'A');
    }

    return 160;
}

static void tama_set_name(char* packet, const char* name) {
    for(size_t i = 0; i < 5; i++) {
        uint8_t value = 160;

        if(name[i] != '\0') {
            value = tama_encode_name_char(name[i]);
        }

        tama_set_byte(packet, 6 + i, value);

        if(name[i] == '\0') {
            for(size_t j = i + 1; j < 5; j++) {
                tama_set_byte(packet, 6 + j, 160);
            }
            break;
        }
    }
}

// borrowed from infrared_common_i.h
#define MATCH_TIMING(x, v, delta) (((x) < ((v) + (delta))) && ((x) > ((v) - (delta))))

static struct {
    bool command_decoded;
    bool timed_out;
    FuriApiLock cli_lock;
} app_state;

typedef struct {
    uint32_t header_mark;
    uint32_t header_mark_tolerance;
    uint32_t header_space;
    uint32_t header_space_tolerance;
    uint32_t data_mark;
    uint32_t data_mark_tolerance;
    uint32_t data_0_space;
    uint32_t data_0_space_tolerance;
    uint32_t data_1_space;
    uint32_t data_1_space_tolerance;
    uint32_t ending_mark;
    uint32_t ending_mark_tolerance;
} DecoderStates;

// all in micro-seconds
DecoderStates decoder_states = {
    .header_mark = 9600,
    .header_mark_tolerance = 2000,
    .header_space = 5000,
    .header_space_tolerance = 1500,
    .data_mark = 550,
    .data_mark_tolerance = 300, // max 850
    .data_0_space = 600,
    .data_0_space_tolerance = 400, // max 1000
    .data_1_space = 1500,
    .data_1_space_tolerance = 500, // match the min long gap to max short gap: 1000
    .ending_mark = 1100,
    .ending_mark_tolerance = 250, // match the min to max of data mark: 850
};

// This function will be called when the user presses the Back button.

// take in a signal from the IR worker, and decode it into a 160 bit tamagotchi
// bit-string and store that in the provided buffer. Return true if success,
// false if it was not a decodable message.
//
// This function does not check the checksum for validity.
bool decode_signal_to_tamabits(InfraredWorkerSignal* received_signal, unsigned char* data_bits) {
    furi_assert(received_signal);
    // Get the timings from the recorded signal
    const uint32_t* timings;
    size_t timings_cnt;
    infrared_worker_get_raw_signal(received_signal, &timings, &timings_cnt);
    // Check if there are at least 323 timings, otherwise the message won't fit
    // and there's no possible way it's valid
    if(timings_cnt < 323) {
        return false;
    }
    // Check first 2 values to see if they're a valid preamble
    if(!(MATCH_TIMING(
             timings[0], decoder_states.header_mark, decoder_states.header_mark_tolerance) &&
         MATCH_TIMING(
             timings[1], decoder_states.header_space, decoder_states.header_space_tolerance))) {
        return false;
    }
    // Decode the next 160 pairs of bits, and store the result in data_bits
    size_t timing = 2;
    for(size_t data_bit = 0; data_bit < 160; data_bit++) {
        if(MATCH_TIMING(
               timings[timing], decoder_states.data_mark, decoder_states.data_mark_tolerance)) {
            if(MATCH_TIMING(
                   timings[timing + 1],
                   decoder_states.data_0_space,
                   decoder_states.data_0_space_tolerance)) {
                // It's a 0, store it
                data_bits[data_bit] = '0';
            } else if(MATCH_TIMING(
                          timings[timing + 1],
                          decoder_states.data_1_space,
                          decoder_states.data_1_space_tolerance)) {
                // It's a 1, store it
                data_bits[data_bit] = '1';
            } else {
                // It's not a valid 1 or 0, give up
                return false;
            }
        }
        timing += 2;
    }
    // I could check that the last mark is the end mark length (longer than a data
    // bit mark) but if we got 160 data bits that's good enough
    return true;
}

static void signal_received_callback(void* pipe, InfraredWorkerSignal* received_signal) {
    // todo: set processing_started flag
    furi_assert(received_signal);
    unsigned char tamabits[160];

    if(decode_signal_to_tamabits(received_signal, tamabits)) {
        // print out the signal
        // cli_write(cli, (uint8_t*)tamabits, 160);
        FURI_LOG_I("TEST", "I saw a signal!!!!");
        pipe_send(pipe, (unsigned char*)"[PICO]", 6);
        pipe_send(pipe, tamabits, 160);
        pipe_send(pipe, (unsigned char*)"[END]", 6);
        app_state.command_decoded = true;

    } else {
        // Do nothing I guess
        printf("Invalid signal received");
    }
    // // todo: set processing finished flag
}

static void timed_out_callback(void* arg) {
    UNUSED(arg);
    app_state.timed_out = true;
}

static void listen(void* context) {
    // set a timeout so the command will exit after 1 second
    FuriTimer* timer = furi_timer_alloc(timed_out_callback, FuriTimerTypeOnce, context);
    furi_timer_start(timer, furi_ms_to_ticks(1000));
    // furi_timer_restart(timer, furi_ms_to_ticks(1000));

    InfraredWorker* worker = infrared_worker_alloc();
    infrared_worker_rx_set_received_signal_callback(worker, signal_received_callback, context);
    infrared_worker_rx_start(worker);
    // default timeout value is 150,000 us, I need it shorter.
    furi_hal_infrared_async_rx_set_timeout(
        decoder_states.header_space + decoder_states.header_space_tolerance);

    // printf("Receiving %s INFRARED...\r\nPress Ctrl+C to abort\r\n", "RAW");
    while(
        !(app_state.command_decoded || app_state.timed_out ||
          cli_is_pipe_broken_or_is_etx_next_char(context))) {
        furi_delay_ms(1);
    }

    // TODO: worry about the race condition where the timer times out while the
    // signal received callback is running and processing the signal. Could add a
    // flag that's set when a command starts processing and then make it wait
    // until that's done. If it's successful, do nothing because it will print the
    // decoded signal. If it was unsuccessful, print timed out message. There is
    // still a possibility that the signal is being received while the timer times
    // out. What happens then? At best it will get lost, at worst the callback
    // will be called and cause a null pointer dereference. Hmph.

    if(app_state.timed_out) {
        printf("[PICO]timed out[END]");
    }

    infrared_worker_rx_stop(worker);
    infrared_worker_free(worker);
    furi_timer_stop(timer);
    furi_timer_free(timer);
}

typedef struct {
    volatile bool received;
    unsigned char bits[160];
} TamaRxState;

static void tama_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    TamaRxState* state = context;
    furi_assert(received_signal);

    if(decode_signal_to_tamabits(received_signal, state->bits)) {
        state->received = true;
    }
}

static bool tama_receive(unsigned char* output, uint32_t timeout_ms) {
    TamaRxState state = {
        .received = false,
    };

    InfraredWorker* worker = infrared_worker_alloc();

    infrared_worker_rx_set_received_signal_callback(worker, tama_rx_callback, &state);

    infrared_worker_rx_start(worker);

    furi_hal_infrared_async_rx_set_timeout(
        decoder_states.header_space + decoder_states.header_space_tolerance);

    uint32_t waited = 0;

    while(!state.received && waited < timeout_ms) {
        furi_delay_ms(1);
        waited++;
    }

    infrared_worker_rx_stop(worker);
    infrared_worker_free(worker);

    if(!state.received) {
        return false;
    }

    memcpy(output, state.bits, 160);
    return true;
}

static bool tamabits_to_timings(char* bitstring, uint32_t* timings) {
    // check if bitstring is 160 chars
    if(strlen(bitstring) != 160) {
        return false;
    }
    timings[0] = decoder_states.header_mark;
    timings[1] = decoder_states.header_space;
    size_t t = 2;
    for(size_t i = 0; i < 160; i++) {
        timings[t] = decoder_states.data_mark;
        if(bitstring[i] == '0') {
            timings[t + 1] = decoder_states.data_0_space;
        } else if(bitstring[i] == '1') {
            timings[t + 1] = decoder_states.data_1_space;
        } else {
            return false;
        }
        t += 2;
    }
    timings[t] = decoder_states.ending_mark;
    return true;
}

static void send(char* bitstring) {
    // 2 timings for preamble, 320 for bits, 1 for ending mark
    uint32_t timings[2 + 320 + 1];
    if(tamabits_to_timings(bitstring, timings)) {
        infrared_send_raw(timings, 323, true);
    }
}

static void __attribute__((unused))
tamagometer_start_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    // Acquire the cli_lock so that the GUI part of the app will wait to exit
    // if the CLI is still running something. This should hopefully reduce the
    // number of null pointer dereferences on exit
    api_lock_relock(app_state.cli_lock);
    FURI_LOG_I("TEST", "CLI ran...");

    app_state.command_decoded = false;
    app_state.timed_out = false;

    const char* args_string = furi_string_get_cstr(args);
    char bitstring[161];
    if(sscanf(args_string, "send%s", bitstring)) {
        // send the bitstring
        send(bitstring);
    } else if(strcmp(args_string, "listen") == 0) {
        // listen
        listen(pipe);
    } else {
        printf("Arguments: \"%s\"\n", args_string);
        printf("Invalid argument(s).\n");
    }
    api_lock_unlock(app_state.cli_lock);
    return;
}

static char __attribute__((unused)) music_visit_message1[] =
    "0000111000000000110111100101101000101001000001110000100010000000011111111000000100000010000000000010001100000000000001100000000000000000000000000000101000110011";

static char __attribute__((unused)) music_visit_message2[] =
    "0000111000001000110111100101101000101001000001110000100010000000011111111000000100000011000000000000000000000000000000000000000000000000000000000000000000001001";

static void tama_set_byte(char* packet, size_t byte_number, uint8_t value) {
    if(byte_number < 1 || byte_number > 20) return;

    size_t start = (byte_number - 1) * 8;

    for(size_t bit = 0; bit < 8; bit++) {
        packet[start + bit] = (value & (1U << (7 - bit))) ? '1' : '0';
    }
}

static void tama_set_bit(char* packet, size_t byte_number, size_t bit_number, bool value) {
    if(byte_number < 1 || byte_number > 20) return;
    if(bit_number < 1 || bit_number > 8) return;

    size_t position = ((byte_number - 1) * 8) + (bit_number - 1);
    packet[position] = value ? '1' : '0';
}

static void tama_update_checksum(char* packet) {
    uint16_t sum = 0;

    /* Bytes 1-19 are summed modulo 256.
       Byte 20 contains the checksum. */
    for(size_t byte = 0; byte < 19; byte++) {
        uint8_t value = 0;

        for(size_t bit = 0; bit < 8; bit++) {
            value <<= 1;
            if(packet[(byte * 8) + bit] == '1') {
                value |= 1;
            }
        }

        sum = (sum + value) & 0xFF;
    }

    tama_set_byte(packet, 20, (uint8_t)sum);
}

static char points_game_message1[] =
    "0000111000000000100011001111011000101000000000000000101100001000000000100000010000000011000000000010000100000000000000000000000000000000000001110000000011111100";

static char points_game_message3[] =
    "0000111000000010100011001111011000101000000000000000101100001000000000100000010000000000000000000000000000000000000000000000000000011110000001110000000111111001";

typedef enum {
    TamaGameWin,
    TamaGameLose,
    TamaGameRandom,
} TamaGameResult;

typedef enum {
    TamaGamePoints = 7,
} TamaGame;

/* ---------- Gift test: Cone ---------- */

static uint8_t __attribute__((unused))
tama_received_byte(const unsigned char* bits, size_t byte_number) {
    uint8_t value = 0;
    size_t start = (byte_number - 1) * 8;

    for(size_t i = 0; i < 8; i++) {
        value <<= 1;

        if(bits[start + i] == '1') {
            value |= 1;
        }
    }

    return value;
}

static void tama_log_packet(const char* label, const unsigned char* bits) {
    char packet[161];

    memcpy(packet, bits, 160);
    packet[160] = '\0';

    FURI_LOG_I("TamaConnect", "%s=%s", label, packet);
}

static bool tama_send_gift(uint8_t character, bool girl, const char* name, uint8_t gift_id) {
    unsigned char response1[160];
    unsigned char response3[160];

    /*
     * Known-good responder packets.
     *
     * Gift conversation:
     * RX M1 (0)
     * TX M2 (1)
     * RX M3 (6)
     * TX M4 (7)
     *
     * Byte 15 of M4 = Gift ID.
     */
    char message2[] =
        "0000111000000001101111110010001000101100000000010000111000000001101000001010000000000000011001000010001000000000000001000000000000000000000000000000000011110110";

    char message4[] =
        "0000111000000101101111110010001000101100000000010000111000000001101000001010000000000000000000000000000000000000100001000000000000000000000000000000000011110100";

    /* Virtual Tama identity */
    tama_set_byte(message2, 5, character);
    tama_set_byte(message4, 5, character);

    tama_set_name(message2, name);
    tama_set_name(message4, name);

    tama_set_bit(message2, 11, 8, girl);

    /*
     * Connection 2024 Present+Visit responder.
     */
    tama_set_byte(message4, 2, 7);

    /*
     * The actual present.
     */
    tama_set_byte(message4, 15, gift_id);

    tama_update_checksum(message2);
    tama_update_checksum(message4);

    FURI_LOG_I("TamaConnect", "Gift responder: gift ID=%u", gift_id);

    FURI_LOG_I("TamaConnect", "Gift RX M1");

    if(!tama_receive(response1, 5000)) {
        FURI_LOG_E("TamaConnect", "Gift: M1 timeout");
        return false;
    }

    tama_log_packet("M1", response1);

    FURI_LOG_I("TamaConnect", "Gift TX M2");
    send(message2);

    FURI_LOG_I("TamaConnect", "Gift RX M3");

    if(!tama_receive(response3, 5000)) {
        FURI_LOG_E("TamaConnect", "Gift: M3 timeout");
        return false;
    }

    tama_log_packet("M3", response3);

    FURI_LOG_I("TamaConnect", "Gift TX M4: B2=7 B15=%u", gift_id);

    send(message4);

    return true;
}

static bool tama_game(
    uint8_t character,
    bool girl,
    const char* name,
    TamaGame game,
    uint8_t amount,
    TamaGameResult result) {
    unsigned char response2[160];
    unsigned char response4[160];

    char message1[161];
    char message3[161];

    memcpy(message1, points_game_message1, 161);
    memcpy(message3, points_game_message3, 161);

    /* Character appearance */
    tama_set_byte(message1, 5, character);
    tama_set_byte(message3, 5, character);

    /* Name: Bytes 6-10 */
    tama_set_name(message1, name);
    tama_set_name(message3, name);

    /* Gender: Byte 11 bit 8 */
    tama_set_bit(message1, 11, 8, girl);
    tama_set_bit(message3, 11, 8, girl);

    /*
     * Byte 18 selects the game.
     * Current captured packet has upper five bits clear.
     */
    tama_set_byte(message1, 18, (uint8_t)game);
    tama_set_byte(message3, 18, (uint8_t)game);

    /* Byte 17 = Gotchi Points wager */
    tama_set_byte(message3, 17, amount);

    /*
     * Points game result:
     *
     * Byte 19 even:
     *   sender of messages 1+3 wins = Flipper wins
     *   real Tamagotchi loses
     *
     * Byte 19 odd:
     *   real Tamagotchi wins
     */
    bool tama_wins;

    if(result == TamaGameWin) {
        tama_wins = true;
    } else if(result == TamaGameLose) {
        tama_wins = false;
    } else {
        tama_wins = (furi_hal_random_get() & 1U) != 0;
    }

    /*
     * Points result is encoded by parity of Byte 19.
     * Change only the least-significant bit and preserve
     * the remaining seven bits from the captured packet.
     *
     * odd  = real Tamagotchi wins
     * even = real Tamagotchi loses
     */
    if(game == TamaGamePoints) {
        tama_set_bit(message3, 19, 8, tama_wins);
    }

    tama_update_checksum(message1);
    tama_update_checksum(message3);

    FURI_LOG_I("TamaConnect", "Points Game: Tama %s", tama_wins ? "WIN" : "LOSE");

    send(message1);

    if(!tama_receive(response2, 3000)) {
        FURI_LOG_E("TamaConnect", "Game: message 2 timeout");
        return false;
    }

    send(message3);

    if(!tama_receive(response4, 3000)) {
        FURI_LOG_E("TamaConnect", "Game: message 4 timeout");
        return false;
    }

    return true;
}

static bool music_visit(uint8_t character, bool girl, const char* name) {
    unsigned char response1[160];
    unsigned char response2[160];

    char message1[161];
    char message2[161];

    memcpy(message1, music_visit_message1, 161);
    memcpy(message2, music_visit_message2, 161);

    /* Byte 5 = character appearance.
       19 = Mametchi */
    tama_set_byte(message1, 5, character);
    tama_set_byte(message2, 5, character);

    /* Bytes 6-10 = name */
    tama_set_name(message1, name);
    tama_set_name(message2, name);

    /* Byte 11, bit 8:
       0 = boy
       1 = girl */
    tama_set_bit(message1, 11, 8, girl);
    tama_set_bit(message2, 11, 8, girl);

    tama_update_checksum(message1);
    tama_update_checksum(message2);

    FURI_LOG_I("TamaConnect", "Sending message 1");
    send(message1);

    FURI_LOG_I("TamaConnect", "Waiting for response 1");
    if(!tama_receive(response1, 3000)) {
        FURI_LOG_E("TamaConnect", "Response 1 timeout");
        return false;
    }

    FURI_LOG_I("TamaConnect", "Response 1 received");

    FURI_LOG_I("TamaConnect", "Sending message 2");
    send(message2);

    FURI_LOG_I("TamaConnect", "Waiting for response 2");
    if(!tama_receive(response2, 3000)) {
        FURI_LOG_E("TamaConnect", "Response 2 timeout");
        return false;
    }

    FURI_LOG_I("TamaConnect", "Response 2 received");
    return true;
}

typedef enum {
    TamaActionNone,
    TamaActionSelect,
    TamaActionBack,
} TamaAction;

typedef enum {
    TamaModeVisit,
    TamaModeGame,
    TamaModeGift,
    TamaModeSettings,
} TamaMode;

typedef struct {
    FuriApiLock lock;
    TamaAction action;

    TamaMode mode;
    TamaProfile profile;
    TamaGame game;
    TamaGameResult game_result;
    uint8_t game_amount;

    uint8_t gift_id;

    char name_edit[6];
} TamaMenuState;

/* ---------- generic Back ---------- */

static void tama_menu_back(void* context) {
    TamaMenuState* state = context;

    if(state->action == TamaActionNone) {
        state->action = TamaActionBack;
        api_lock_unlock(state->lock);
    }
}

/* ---------- Main menu ---------- */

static void tama_main_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->mode = (TamaMode)index;
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

/* ---------- Character ---------- */

static void tama_character_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->profile.character = (uint8_t)index;
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

/* ---------- Gender ---------- */

static void tama_gender_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->profile.girl = (index == 1);
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

/* ---------- Game result ---------- */

static void tama_game_result_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->game_result = (TamaGameResult)index;
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

/*
 * Wait for one menu action.
 */
static TamaAction tama_wait(TamaMenuState* state, ViewHolder* view_holder, View* view) {
    state->action = TamaActionNone;
    state->lock = api_lock_alloc_locked();

    view_holder_set_back_callback(view_holder, tama_menu_back, state);

    view_holder_set_view(view_holder, view);

    api_lock_wait_unlock_and_free(state->lock);

    view_holder_set_view(view_holder, NULL);

    return state->action;
}

/* ---------- Character menu ---------- */

static TamaAction tama_choose_character(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Choose character");

    submenu_add_item(menu, "Teletchi", 1, tama_character_selected, state);
    submenu_add_item(menu, "ShiroTeletchi", 2, tama_character_selected, state);
    submenu_add_item(menu, "Tamatchi", 3, tama_character_selected, state);
    submenu_add_item(menu, "MizuTamatchi", 4, tama_character_selected, state);
    submenu_add_item(menu, "Kuchitamatchi", 5, tama_character_selected, state);
    submenu_add_item(menu, "Mohitamatchi", 6, tama_character_selected, state);
    submenu_add_item(menu, "Obotchi", 7, tama_character_selected, state);
    submenu_add_item(menu, "Young Mametchi", 8, tama_character_selected, state);
    submenu_add_item(menu, "Batabatchi", 9, tama_character_selected, state);
    submenu_add_item(menu, "Ichigotchi", 10, tama_character_selected, state);
    submenu_add_item(menu, "Nikatchi", 11, tama_character_selected, state);
    submenu_add_item(menu, "Pirorirotchi", 12, tama_character_selected, state);
    submenu_add_item(menu, "Hikotchi", 13, tama_character_selected, state);
    submenu_add_item(menu, "Hinatchi", 14, tama_character_selected, state);
    submenu_add_item(menu, "Young Mimitchi", 15, tama_character_selected, state);
    submenu_add_item(menu, "Ringotchi", 16, tama_character_selected, state);
    submenu_add_item(menu, "Hinotamatchi", 17, tama_character_selected, state);
    submenu_add_item(menu, "Hashitamatchi", 18, tama_character_selected, state);
    submenu_add_item(menu, "Mametchi", 19, tama_character_selected, state);
    submenu_add_item(menu, "Flowertchi", 20, tama_character_selected, state);
    submenu_add_item(menu, "Pyonkotchi", 21, tama_character_selected, state);
    submenu_add_item(menu, "Kuchipatchi", 22, tama_character_selected, state);
    submenu_add_item(menu, "Memetchi", 23, tama_character_selected, state);
    submenu_add_item(menu, "Billotchi", 24, tama_character_selected, state);
    submenu_add_item(menu, "Tarakotchi", 25, tama_character_selected, state);
    submenu_add_item(menu, "Paparatchi", 26, tama_character_selected, state);
    submenu_add_item(menu, "Mimiyoritchi", 27, tama_character_selected, state);
    submenu_add_item(menu, "Hanatchi", 28, tama_character_selected, state);
    submenu_add_item(menu, "Hashizotchi", 29, tama_character_selected, state);
    submenu_add_item(menu, "Tsunotchi", 30, tama_character_selected, state);
    submenu_add_item(menu, "Masktchi", 31, tama_character_selected, state);
    submenu_add_item(menu, "Megatchi", 32, tama_character_selected, state);

    /* 33 is also marked Mailman in Protocol.md */

    submenu_add_item(menu, "Mimitchi", 34, tama_character_selected, state);
    submenu_add_item(menu, "ChoMametchi", 35, tama_character_selected, state);
    submenu_add_item(menu, "Decotchi", 36, tama_character_selected, state);
    submenu_add_item(menu, "Hidatchi", 37, tama_character_selected, state);
    submenu_add_item(menu, "Debatchi", 38, tama_character_selected, state);
    submenu_add_item(menu, "Bunbuntchi", 39, tama_character_selected, state);
    submenu_add_item(menu, "Pipotchi", 40, tama_character_selected, state);
    submenu_add_item(menu, "Dorotchi", 41, tama_character_selected, state);
    submenu_add_item(menu, "Bill", 42, tama_character_selected, state);
    submenu_add_item(menu, "Robotchi", 43, tama_character_selected, state);
    submenu_add_item(menu, "Wooltchi", 44, tama_character_selected, state);
    submenu_add_item(menu, "Teketchi", 45, tama_character_selected, state);
    submenu_add_item(menu, "Gozarutchi", 46, tama_character_selected, state);
    submenu_add_item(menu, "Warusotchi", 47, tama_character_selected, state);
    submenu_add_item(menu, "Sekitoritchi", 48, tama_character_selected, state);
    submenu_add_item(menu, "Oyajitchi", 49, tama_character_selected, state);
    submenu_add_item(menu, "Ojitchi", 50, tama_character_selected, state);
    submenu_add_item(menu, "Otokitchi", 51, tama_character_selected, state);
    submenu_add_item(menu, "Nyatchi", 52, tama_character_selected, state);
    submenu_add_item(menu, "Hohotchi", 53, tama_character_selected, state);

    /*
     * Special/non-standard protocol entries.
     */
    submenu_add_item(menu, "Mailman [0]", 0, tama_character_selected, state);
    submenu_add_item(menu, "Mailman [33]", 33, tama_character_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

/* ---------- Gender menu ---------- */

static TamaAction tama_choose_gender(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Choose gender");

    submenu_add_item(menu, "Boy", 0, tama_gender_selected, state);

    submenu_add_item(menu, "Girl", 1, tama_gender_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

/* ---------- Game result menu ---------- */

/* =========================================================
 * Gift menus
 * ========================================================= */

typedef struct {
    const char* name;
    uint8_t id;
} TamaGiftEntry;

static const TamaGiftEntry tama_gift_food[] = {
    {"Scone", 0},
    {"Sushi", 1},
    {"Bread", 2},
    {"Cereal", 3},
    {"Omelet", 4},
    {"Milk", 5},
    {"Hamburger", 6},
    {"BBQ", 7},
    {"Sandwich", 8},
    {"Beef Bowl", 9},
    {"Cheese", 10},
    {"Pizza", 11},
    {"Steak", 12},
    {"Taco", 13},
    {"Sausage Stick", 14},
    {"Hot Dog", 15},
    {"Pasta", 16},
    {"Corn", 17},
    {"Turkey", 18},
    {"Noodle", 19},
    {"Fried Chicken", 20},
    {"Waffle", 21},
    {"Choco Bar", 22},
    {"Escargot", 23},
    {"Octopus Sausage", 24},
    {"Chikuwa", 25},
    {"Rice Ball", 26},
    {"Curry", 27},
    {"Kobu Maki", 28},
    {"Umeboshi", 29},
    {"Natto", 30},
    {"Fried Shrimp", 31},
    {"Takoyaki", 32},
    {"Oyster", 33},
    {"Naruto", 34},
    {"Pigs Feet", 35},
};

static const TamaGiftEntry tama_gift_snacks[] = {
    {"Cone", 36},
    {"Pudding", 37},
    {"Cake", 38},
    {"Apple", 39},
    {"Sundae", 40},
    {"Banana", 41},
    {"Fries", 42},
    {"Roll Cake", 43},
    {"Cupcake", 44},
    {"Fruit Juice", 45},
    {"Ice Cream", 46},
    {"Cheese Cake", 47},
    {"Apple Pie", 48},
    {"Energy Drink", 49},
    {"Corn Dog", 50},
    {"Donut", 51},
    {"Soda", 52},
    {"Popcorn", 53},
    {"Pear", 54},
    {"Pineapple", 55},
    {"Melon", 56},
    {"Grapes", 57},
    {"Heart Chocolate", 58},
    {"Cookie", 59},
    {"Whole Cake", 60},
    {"Yogurt", 61},
    {"Lollipop", 62},
    {"Candy", 63},
    {"Crepe Suzette", 64},
    {"Cherry", 65},
    {"Biscuit", 66},
    {"Marron Cake", 67},
    {"Cream Puff", 68},
    {"Gum", 69},
    {"Dango", 70},
    {"Shaved Ice", 71},
    {"Sweet Potato", 72},
    {"Mochi", 73},
    {"Peanuts", 74},
    {"Toast", 75},
    {"Crackers", 76},
    {"Water", 77},
};

static const TamaGiftEntry tama_gift_items[] = {
    {"Ball", 78},
    {"Pencil", 79},
    {"Wig", 80},
    {"Sunglasses", 81},
    {"RC Car 1", 82},
    {"Pen", 83},
    {"Weights", 84},
    {"RC Car 2", 85},
    {"RC Car 3", 86},
    {"Bow", 87},
    {"Darts", 88},
    {"Bldg Block", 89},
    {"Cap", 90},
    {"Bow Tie", 91},
    {"Wings", 92},
    {"Hair Gel", 93},
    {"Clock", 94},
    {"Chest", 95},
    {"Phonograph", 96},
    {"Fishing Pole", 97},
    {"Mirror", 98},
    {"Make Up", 99},
    {"Boom Box", 100},
    {"Music Disc", 101},
    {"Shirt", 102},
    {"Shoes", 103},
    {"Ticket 1", 104},
    {"Ticket 2", 105},
    {"Ticket 3", 106},
    {"Ticket 4", 107},
    {"Ticket 5", 108},
    {"Doll 1", 109},
    {"Umbrella", 110},
    {"Lamp", 111},
    {"Roller Blades", 112},
    {"Action Figure", 113},
    {"Stuffed Tama 1", 114},
    {"Stuffed Tama 2", 115},
    {"Trumpet", 116},
    {"Drum", 117},
    {"Throne", 118},
    {"Music", 119},
    {"Plant", 120},
    {"Shovel", 121},
    {"TV", 122},
    {"Honey", 123},
    {"Royal Costume", 124},
    {"Clone !!", 125},
    {"Balloon", 126},
    {"Rope", 127},
    {"Doll 2", 128},
    {"Tama Drink", 129},
    {"Castle", 130},
    {"Shaver", 131},
};

static const TamaGiftEntry tama_gift_special[] = {
    {"Cone [effect]", 132},
    {"Flower [effect]", 133},
    {"Poop [effect]", 134},
    {"Jack in Box", 135},
    {"Cake [effect]", 136},
    {"Heart [effect]", 137},
    {"Snake [effect]", 138},
    {"Blank / Nothing", 139},
    {"Ghost [effect]", 140},
    {"Sickness [effect]", 141},
};

typedef enum {
    TamaGiftCategoryFood,
    TamaGiftCategorySnacks,
    TamaGiftCategoryItems,
    TamaGiftCategorySpecial,
} TamaGiftCategory;

static void tama_gift_category_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->gift_id = (uint8_t)index;
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

static void tama_gift_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    if(index == 256) {
        /* Special: 132-141 */
        state->gift_id = (uint8_t)(132 + (furi_hal_random_get() % 10));
    } else if(index == 257) {
        /* Any normal gift: 0-131 */
        state->gift_id = (uint8_t)(furi_hal_random_get() % 132);
    } else if(index == 258) {
        /* Food: 0-35 */
        state->gift_id = (uint8_t)(furi_hal_random_get() % 36);
    } else if(index == 259) {
        /* Snacks: 36-77 */
        state->gift_id = (uint8_t)(36 + (furi_hal_random_get() % 42));
    } else if(index == 260) {
        /* Items: 78-131 */
        state->gift_id = (uint8_t)(78 + (furi_hal_random_get() % 54));
    } else {
        state->gift_id = (uint8_t)index;
    }

    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

static TamaAction tama_choose_gift_category(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Gift");

    submenu_add_item(menu, "Random Gift", 257, tama_gift_selected, state);

    submenu_add_item(menu, "Food", TamaGiftCategoryFood, tama_gift_category_selected, state);

    submenu_add_item(menu, "Snacks", TamaGiftCategorySnacks, tama_gift_category_selected, state);

    submenu_add_item(menu, "Items", TamaGiftCategoryItems, tama_gift_category_selected, state);

    submenu_add_item(menu, "Special", TamaGiftCategorySpecial, tama_gift_category_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

static TamaAction tama_choose_gift_from_table(
    TamaMenuState* state,
    ViewHolder* view_holder,
    const char* title,
    const TamaGiftEntry* entries,
    size_t count,
    uint32_t random_command) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, title);

    if(random_command != 0) {
        const char* random_name = "Random";

        if(random_command == 258) {
            random_name = "Random Food";
        } else if(random_command == 259) {
            random_name = "Random Snack";
        } else if(random_command == 260) {
            random_name = "Random Item";
        }

        submenu_add_item(menu, random_name, random_command, tama_gift_selected, state);
    }

    for(size_t i = 0; i < count; i++) {
        submenu_add_item(menu, entries[i].name, entries[i].id, tama_gift_selected, state);
    }

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

static TamaAction tama_choose_special(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Special");

    /*
     * 256 is outside uint8_t Gift IDs and therefore
     * safely acts as the Random menu command.
     */
    submenu_add_item(menu, "Random Special", 256, tama_gift_selected, state);

    for(size_t i = 0; i < sizeof(tama_gift_special) / sizeof(tama_gift_special[0]); i++) {
        submenu_add_item(
            menu, tama_gift_special[i].name, tama_gift_special[i].id, tama_gift_selected, state);
    }

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

static void tama_game_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->game = (TamaGame)index;
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

static TamaAction tama_choose_game(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Choose game");

    submenu_add_item(menu, "Points", TamaGamePoints, tama_game_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

static void tama_game_amount_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    state->game_amount = (uint8_t)index;
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

static TamaAction tama_choose_game_amount(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Points wager");

    submenu_add_item(menu, "10 GP", 10, tama_game_amount_selected, state);

    submenu_add_item(menu, "30 GP", 30, tama_game_amount_selected, state);

    submenu_add_item(menu, "50 GP", 50, tama_game_amount_selected, state);

    submenu_add_item(menu, "99 GP", 99, tama_game_amount_selected, state);

    submenu_add_item(menu, "255 GP", 255, tama_game_amount_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

static TamaAction tama_choose_game_result(TamaMenuState* state, ViewHolder* view_holder) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Game result");

    submenu_add_item(menu, "Win", TamaGameWin, tama_game_result_selected, state);

    submenu_add_item(menu, "Lose", TamaGameLose, tama_game_result_selected, state);

    submenu_add_item(menu, "Random", TamaGameRandom, tama_game_result_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    submenu_free(menu);

    return action;
}

/* ---------- Name editor ---------- */

static void tama_name_saved(void* context) {
    TamaMenuState* state = context;

    /*
     * Empty name is allowed and becomes underscores
     * in the Tamagotchi protocol.
     */
    strncpy(state->profile.name, state->name_edit, 5);
    state->profile.name[5] = '\0';

    tama_profile_save(&state->profile);

    state->action = TamaActionSelect;
    api_lock_unlock(state->lock);
}

static TamaAction tama_edit_name(TamaMenuState* state, ViewHolder* view_holder) {
    strncpy(state->name_edit, state->profile.name, 5);
    state->name_edit[5] = '\0';

    TextInput* input = text_input_alloc();

    text_input_set_header_text(input, "Tamagotchi name");

    text_input_set_result_callback(
        input, tama_name_saved, state, state->name_edit, sizeof(state->name_edit), false);

    TamaAction action = tama_wait(state, view_holder, text_input_get_view(input));

    text_input_free(input);

    return action;
}

/* ---------- Settings menu ---------- */

typedef enum {
    TamaSettingName,
    TamaSettingCharacter,
    TamaSettingGender,
} TamaSetting;

static void tama_setting_selected(void* context, uint32_t index) {
    TamaMenuState* state = context;

    /*
     * Reuse mode temporarily to return the selected setting.
     * Values are outside the normal Visit/Game range.
     */
    state->mode = (TamaMode)(100 + index);
    state->action = TamaActionSelect;

    api_lock_unlock(state->lock);
}

static TamaAction
    tama_settings_menu(TamaMenuState* state, ViewHolder* view_holder, TamaSetting* selected) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Settings");

    submenu_add_item(menu, "Name", TamaSettingName, tama_setting_selected, state);

    submenu_add_item(menu, "Character", TamaSettingCharacter, tama_setting_selected, state);

    submenu_add_item(menu, "Gender", TamaSettingGender, tama_setting_selected, state);

    TamaAction action = tama_wait(state, view_holder, submenu_get_view(menu));

    if(action == TamaActionSelect) {
        *selected = (TamaSetting)((uint32_t)state->mode - 100);
    }

    submenu_free(menu);

    return action;
}

/* ---------- Complete Settings screen ---------- */

static void tama_run_settings(TamaMenuState* state, ViewHolder* view_holder) {
    bool settings_open = true;

    while(settings_open) {
        TamaSetting selected = TamaSettingName;

        TamaAction action = tama_settings_menu(state, view_holder, &selected);

        if(action == TamaActionBack) {
            settings_open = false;
            continue;
        }

        if(selected == TamaSettingName) {
            tama_edit_name(state, view_holder);

        } else if(selected == TamaSettingCharacter) {
            action = tama_choose_character(state, view_holder);

            if(action == TamaActionSelect) {
                tama_profile_save(&state->profile);
            }

        } else if(selected == TamaSettingGender) {
            action = tama_choose_gender(state, view_holder);

            if(action == TamaActionSelect) {
                tama_profile_save(&state->profile);
            }
        }
    }
}

/* ---------- Run connection ---------- */

static void tama_run_connection(TamaMenuState* state, ViewHolder* view_holder) {
    TextBox* text_box = text_box_alloc();

    if(state->mode == TamaModeVisit) {
        text_box_set_text(
            text_box,
            "TamaConnect\n\n"
            "Connect > Visit\n"
            "Stand by\n\n"
            "Starting...");
    } else if(state->mode == TamaModeGift) {
        text_box_set_text(
            text_box,
            "TamaConnect\n\n"
            "Connect > Present\n"
            "Press B at STAND BY\n\n"
            "Waiting...");
    } else {
        text_box_set_text(
            text_box,
            "TamaConnect\n\n"
            "Connect > Game\n"
            "Points\n\n"
            "Starting...");
    }

    /*
     * Disable Back while IR transaction is running.
     */
    view_holder_set_back_callback(view_holder, NULL, NULL);

    view_holder_set_view(view_holder, text_box_get_view(text_box));

    furi_delay_ms(1000);

    bool success;

    if(state->mode == TamaModeVisit) {
        success = music_visit(state->profile.character, state->profile.girl, state->profile.name);
    } else if(state->mode == TamaModeGift) {
        success = tama_send_gift(
            state->profile.character, state->profile.girl, state->profile.name, state->gift_id);
    } else {
        success = tama_game(
            state->profile.character,
            state->profile.girl,
            state->profile.name,
            state->game,
            state->game_amount,
            state->game_result);
    }

    if(success) {
        text_box_set_text(
            text_box,
            "SUCCESS!\n\n"
            "Connection complete.\n\n"
            "Returning to menu...");
    } else {
        text_box_set_text(
            text_box,
            "TIMEOUT\n\n"
            "No response from Tama.\n\n"
            "Returning to menu...");
    }

    /*
     * Let the result be visible briefly, then automatically
     * return to the main menu.
     */
    furi_delay_ms(1500);

    view_holder_set_view(view_holder, NULL);
    text_box_free(text_box);
}

/* =========================================================
 * Main application
 * ========================================================= */

int32_t tama_connect(void* arg) {
    UNUSED(arg);

    Gui* gui = furi_record_open(RECORD_GUI);

    ViewHolder* view_holder = view_holder_alloc();
    view_holder_attach_to_gui(view_holder, gui);

    TamaMenuState state = {
        .mode = TamaModeVisit,
        .game = TamaGamePoints,
        .game_result = TamaGameRandom,
        .game_amount = 30,
        .gift_id = 36,
    };

    /*
     * Load persistent profile.
     */
    if(!tama_profile_load(&state.profile)) {
        tama_profile_defaults(&state.profile);
        tama_profile_save(&state.profile);
    }

    strncpy(state.name_edit, state.profile.name, 5);
    state.name_edit[5] = '\0';

    bool running = true;

    while(running) {
        Submenu* main_menu = submenu_alloc();

        submenu_set_header(main_menu, "TamaConnect");

        submenu_add_item(main_menu, "Visit", TamaModeVisit, tama_main_selected, &state);

        submenu_add_item(main_menu, "Game", TamaModeGame, tama_main_selected, &state);

        submenu_add_item(main_menu, "Gift", TamaModeGift, tama_main_selected, &state);

        submenu_add_item(main_menu, "Settings", TamaModeSettings, tama_main_selected, &state);

        TamaAction action = tama_wait(&state, view_holder, submenu_get_view(main_menu));

        submenu_free(main_menu);

        if(action == TamaActionBack) {
            running = false;
            continue;
        }

        /*
         * SETTINGS
         */
        if(state.mode == TamaModeSettings) {
            tama_run_settings(&state, view_holder);
            continue;
        }

        /*
         * VISIT
         *
         * Character, gender and name now come directly
         * from the persistent profile.
         */
        if(state.mode == TamaModeVisit) {
            tama_run_connection(&state, view_holder);
            continue;
        }

        /*
         * GIFT
         */
        if(state.mode == TamaModeGift) {
            action = tama_choose_gift_category(&state, view_holder);

            if(action == TamaActionBack) {
                continue;
            }

            TamaGiftCategory category = (TamaGiftCategory)state.gift_id;

            if(category == TamaGiftCategoryFood) {
                action = tama_choose_gift_from_table(
                    &state,
                    view_holder,
                    "Food",
                    tama_gift_food,
                    sizeof(tama_gift_food) / sizeof(tama_gift_food[0]),
                    258);
            } else if(category == TamaGiftCategorySnacks) {
                action = tama_choose_gift_from_table(
                    &state,
                    view_holder,
                    "Snacks",
                    tama_gift_snacks,
                    sizeof(tama_gift_snacks) / sizeof(tama_gift_snacks[0]),
                    259);
            } else if(category == TamaGiftCategoryItems) {
                action = tama_choose_gift_from_table(
                    &state,
                    view_holder,
                    "Items",
                    tama_gift_items,
                    sizeof(tama_gift_items) / sizeof(tama_gift_items[0]),
                    260);
            } else {
                action = tama_choose_special(&state, view_holder);
            }

            if(action == TamaActionBack) {
                continue;
            }

            tama_run_connection(&state, view_holder);

            continue;
        }

        /*
         * GAME
         */
        if(state.mode == TamaModeGame) {
            action = tama_choose_game(&state, view_holder);

            if(action == TamaActionBack) {
                continue;
            }

            action = tama_choose_game_amount(&state, view_holder);

            if(action == TamaActionBack) {
                continue;
            }

            action = tama_choose_game_result(&state, view_holder);

            if(action == TamaActionBack) {
                continue;
            }

            tama_run_connection(&state, view_holder);
            continue;
        }
    }

    view_holder_set_view(view_holder, NULL);
    view_holder_free(view_holder);

    furi_record_close(RECORD_GUI);

    return 0;
}
