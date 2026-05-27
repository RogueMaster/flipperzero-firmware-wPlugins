/* Keys scene — read-only browser of the AES-128 key store.
 *
 * Editing keys on a 5-button device is genuinely painful (32 hex
 * characters, no on-screen keyboard for hex by default), so the
 * authoritative editor for `keys.csv` is a text editor on a desktop:
 *
 *     /ext/apps_data/wmbuster/keys.csv
 *     # MAN,IDHEX,KEY32HEX
 *     TCH,27404216,0123456789ABCDEF0123456789ABCDEF
 *     KAM,12345678,FEDCBA9876543210FEDCBA9876543210
 *
 * The app reloads this file at startup. This scene shows the entries
 * that are currently loaded so the user can verify their additions
 * landed correctly. */

#include "../wmbus_app_i.h"
#include "../key_store.h"
#include "../protocol/wmbus_manuf.h"
#include <stdio.h>
#include <string.h>

#define KEYS_TEXT_CAP 1024

static char s_keys_text[KEYS_TEXT_CAP];

typedef struct {
    char* out;
    size_t pos;
    size_t cap;
    size_t count;
} BuildCtx;

static void build_cb(uint16_t m, uint32_t id, const uint8_t key[16], void* ctx) {
    BuildCtx* b = ctx;
    if(b->pos >= b->cap) return;
    char mc[4]; wmbus_manuf_decode(m, mc);
    /* Show the first 4 and last 2 bytes of the key as a fingerprint —
     * full key is sensitive, fingerprint lets the user confirm the
     * right value loaded without exposing the whole secret on screen. */
    int n = snprintf(b->out + b->pos, b->cap - b->pos,
                     "%s %08lX\n  %02X%02X%02X%02X.." "%02X%02X\n",
                     mc, (unsigned long)id,
                     key[0], key[1], key[2], key[3],
                     key[14], key[15]);
    if(n > 0) {
        b->pos += (size_t)n;
        b->count++;
    }
}

void wmbus_view_keys_enter(void* ctx) {
    WmbusApp* app = ctx;
    BuildCtx b = { s_keys_text, 0, sizeof(s_keys_text), 0 };
    s_keys_text[0] = 0;
    if(app->key_store) key_store_iter(app->key_store, build_cb, &b);

    text_box_reset(app->text_box);
    if(b.count == 0) {
        snprintf(s_keys_text, sizeof(s_keys_text),
                 "No keys loaded.\n\n"
                 "Drop keys.csv on the SD card at:\n"
                 "/ext/apps_data/wmbuster/keys.csv\n\n"
                 "Format (one line each):\n"
                 "MAN,IDHEX,KEY32HEX\n"
                 "TCH,27404216,0011...EEFF\n\n"
                 "Restart the app to reload.");
    }
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, s_keys_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WmbusViewDetail);
}

bool wmbus_view_keys_event(void* ctx, SceneManagerEvent ev) {
    (void)ctx; (void)ev; return false;
}

void wmbus_view_keys_exit(void* ctx) {
    WmbusApp* app = ctx;
    text_box_reset(app->text_box);
}
