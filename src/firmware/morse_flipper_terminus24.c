/*
 * Purpose: Load the few currently visible Terminus 24 glyphs from the FAP asset.
 * Owns: asset validation and the app-owned, allocation-free prompt workspace.
 * Draw callbacks only consult the workspace; they never touch storage.
 */

#include "morse_flipper_app_i.h"
#include "fonts/morse_flipper_terminus24.h"

#define MORSE_FLIPPER_TERMINUS24_ASSET APP_ASSETS_PATH("terminus24.bin")
#define MORSE_FLIPPER_TERMINUS24_ASSET_SIGNATURE APP_ASSETS_PATH(".assets.signature")
#define MORSE_FLIPPER_TERMINUS24_MAGIC "MF24"
#define MORSE_FLIPPER_TERMINUS24_VERSION 1U
#define MORSE_FLIPPER_TERMINUS24_HEADER_SIZE 8U
#define MORSE_FLIPPER_TERMINUS24_GLYPH_COUNT 61U
#define MORSE_FLIPPER_TERMINUS24_ASSET_SIZE \
    (MORSE_FLIPPER_TERMINUS24_HEADER_SIZE + \
     (256U * MORSE_FLIPPER_TERMINUS24_PACKED_SIZE))

const MorseFlipperTerminus24PreparedGlyph*
    morse_flipper_terminus24_prepared(const MorseFlipperTerminus24Cache* cache, uint8_t ch) {
    if(cache == NULL || !cache->prepared) return NULL;
    for(uint8_t i = 0U; i < cache->count; i++) {
        if(cache->slots[i].ch == ch) return &cache->slots[i];
    }
    return NULL;
}

uint16_t morse_flipper_terminus24_prepared_row(
    const MorseFlipperTerminus24PreparedGlyph* glyph,
    size_t row) {
    size_t pos;

    if(glyph == NULL || row >= MORSE_FLIPPER_TERMINUS24_HEIGHT) return 0U;
    pos = (row / 2U) * 3U;
    if((row & 1U) == 0U)
        return (uint16_t)(((uint16_t)glyph->rows[pos] << 4U) | (glyph->rows[pos + 1U] >> 4U));
    return (uint16_t)(((uint16_t)(glyph->rows[pos + 1U] & 0x0FU) << 8U) |
                      glyph->rows[pos + 2U]);
}

static bool morse_flipper_terminus24_push(uint8_t chars[10], uint8_t* count, uint8_t ch) {
    if(count == NULL || *count >= MORSE_FLIPPER_TERMINUS24_CACHE_SLOTS) return false;
    chars[(*count)++] = ch;
    return true;
}

static bool morse_flipper_terminus24_visible(const MorseFlipperApp* app) {
    return app != NULL &&
           ((app->screen == MorseFlipperScreenStraight && app->straight_started &&
             !morse_flipper_straight_countdown_active(app)) ||
            app->screen == MorseFlipperScreenTxGroups ||
            app->screen == MorseFlipperScreenStreakIntro);
}

static bool morse_flipper_terminus24_visible_chars(
    const MorseFlipperApp* app,
    uint8_t chars[10],
    uint8_t* count) {
    *count = 0U;
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenStraight && app->straight_started &&
       !morse_flipper_straight_countdown_active(app)) {
        return morse_flipper_terminus24_push(
            chars, count, morse_flipper_straight_trainer_target_char(&app->straight_trainer));
    } else if(app->screen == MorseFlipperScreenTxGroups) {
        uint8_t answer_len = 0U;
        for(uint8_t i = 0U;
            i < MORSE_FLIPPER_TX_GROUP_LEN && app->tx_group.target[i] != '\0';
            i++) {
            if(!morse_flipper_terminus24_push(chars, count, (uint8_t)app->tx_group.target[i])) return false;
        }
        while(answer_len < MORSE_FLIPPER_TX_GROUP_LEN && app->tx_group.answer[answer_len] != '\0') {
            if(!morse_flipper_terminus24_push(
                   chars, count, (uint8_t)app->tx_group.answer[answer_len]))
                return false;
            answer_len++;
        }
        if(answer_len < MORSE_FLIPPER_TX_GROUP_LEN && app->txg_wait_answer) {
            uint8_t preview =
                morse_flipper_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
            if(preview != 0U && preview != ' ' && preview != '|')
                return morse_flipper_terminus24_push(chars, count, preview);
        }
        return true;
    } else if(app->screen == MorseFlipperScreenStreakIntro) {
        char days[6];
        snprintf(days, sizeof(days), "%u", (unsigned)app->streak_intro_days);
        for(uint8_t i = 0U; i < sizeof(days) && days[i] != '\0'; i++) {
            if(!morse_flipper_terminus24_push(chars, count, (uint8_t)days[i])) return false;
        }
        return true;
    }
    return true;
}

static bool morse_flipper_terminus24_load(
    MorseFlipperTerminus24Cache* cache,
    const uint8_t chars[10],
    uint8_t wanted) {
    Storage* storage;
    File* file;
    uint8_t header[MORSE_FLIPPER_TERMINUS24_HEADER_SIZE];
    bool ok = false;

    if(cache == NULL) return false;
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    if(file == NULL) goto record_done;
    if(!storage_file_open(file, MORSE_FLIPPER_TERMINUS24_ASSET, FSAM_READ, FSOM_OPEN_EXISTING)) goto done;
    if(storage_file_size(file) != MORSE_FLIPPER_TERMINUS24_ASSET_SIZE ||
       storage_file_read(file, header, sizeof(header)) != sizeof(header) ||
       memcmp(header, MORSE_FLIPPER_TERMINUS24_MAGIC, 4U) != 0 ||
       header[4] != MORSE_FLIPPER_TERMINUS24_VERSION ||
       header[5] != MORSE_FLIPPER_TERMINUS24_GLYPH_COUNT || header[6] != 0U || header[7] != 0U)
        goto done;
    for(uint8_t slot = 0U; slot < wanted; slot++) {
        if(!storage_file_seek(
               file,
               MORSE_FLIPPER_TERMINUS24_HEADER_SIZE +
                   ((uint32_t)chars[slot] * MORSE_FLIPPER_TERMINUS24_PACKED_SIZE),
               true) ||
           storage_file_read(
               file, cache->slots[slot].rows, MORSE_FLIPPER_TERMINUS24_PACKED_SIZE) !=
               MORSE_FLIPPER_TERMINUS24_PACKED_SIZE)
            goto done;
        cache->slots[slot].ch = chars[slot];
    }
    cache->count = wanted;
    cache->asset_ok = true;
    cache->prepared = true;
    ok = true;
done:
    storage_file_close(file);
    storage_file_free(file);
record_done:
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void morse_flipper_terminus24_request_asset_unpack(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_remove(storage, MORSE_FLIPPER_TERMINUS24_ASSET_SIGNATURE);
    furi_record_close(RECORD_STORAGE);
}

void morse_flipper_terminus24_prepare(MorseFlipperApp* app) {
    uint8_t chars[MORSE_FLIPPER_TERMINUS24_CACHE_SLOTS] = {0};
    uint8_t wanted;

    if(app == NULL) return;
    /* This cache shares storage with the RfRx ticker: leave it entirely alone
     * whenever no Terminus prompt is on the framebuffer. */
    if(!morse_flipper_terminus24_visible(app)) {
        app->terminus24_active = false;
        return;
    }
    if(!app->terminus24_active) {
        app->terminus24_active = true;
        app->terminus24 = (MorseFlipperTerminus24Cache){0};
    }
    if(!morse_flipper_terminus24_visible_chars(app, chars, &wanted)) {
        app->terminus24 = (MorseFlipperTerminus24Cache){.asset_ok = false, .prepared = true};
        return;
    }
    if(wanted == 0U) return;
    if(app->terminus24.prepared && app->terminus24.count == wanted) {
        bool same = true;
        for(uint8_t i = 0U; i < wanted; i++) {
            if(app->terminus24.slots[i].ch != chars[i]) same = false;
        }
        if(same) return;
    }
    /* A concurrent redraw can only render the visible failure while this cache changes. */
    app->terminus24.asset_ok = false;
    app->terminus24.prepared = false;
    if(!morse_flipper_terminus24_load(&app->terminus24, chars, wanted)) {
        morse_flipper_terminus24_request_asset_unpack();
        app->terminus24 = (MorseFlipperTerminus24Cache){
            .count = wanted,
            .asset_ok = false,
            .prepared = true,
        };
        for(uint8_t i = 0U; i < wanted; i++) app->terminus24.slots[i].ch = chars[i];
    }
}
