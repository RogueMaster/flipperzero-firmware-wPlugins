#include "../flipbip.h"
#include <furi.h>
#include <input/input.h>
#include <string.h>
#include "../helpers/flipbip_string.h"
#include "../helpers/flipbip_file.h"
// From: /lib/crypto
#include <memzero.h>
#include <curves.h>
#include <bip32.h>
#include <bip39.h>
#include <segwit_addr.h>
#include <monero/monero.h>

#define SEGWIT_HRP    "bc"
#define DERIV_ACCOUNT 0
#define DERIV_CHANGE  0

#define LINE_BUF     32 // longest rendered line (30) + null, rounded up
#define NUM_LINES    6 // text lines that fit on screen
#define XKEY_BUF     (111 + 1) // base58check of a 78 byte BIP32 key is 111 chars
#define MAX_ADDR_BUF (95 + 1) // XMR standard address/subaddress is 95 chars
#define NUM_ADDRS    6

#define PAGE_LOADING    0
#define PAGE_INFO       1
#define PAGE_MNEMONIC   2
#define PAGE_SEED       3
#define PAGE_XPRV_ROOT  4
#define PAGE_XPRV_ACCT  5
#define PAGE_XPUB_ACCT  6
#define PAGE_XPRV_EXTD  7
#define PAGE_XPUB_EXTD  8
#define PAGE_ADDR_BEGIN 9
#define PAGE_ADDR_END   (PAGE_ADDR_BEGIN + NUM_ADDRS - 1)
#define PAGE_ERROR      (PAGE_ADDR_END + 1)

#define TEXT_LOADING         "Loading..."
#define TEXT_NEW_WALLET      "New wallet"
#define TEXT_RECEIVE_ADDRESS "receive address:"
#define TEXT_XMR_SPEND_KEY   "Private spend key:"
#define TEXT_XMR_VIEW_KEY    "Private view key:"
#define TEXT_QRFILE_EXT      ".qrcode"
#define WARN_INSECURE_TEXT_1 "Recommendation:"
#define WARN_INSECURE_TEXT_2 "Set BIP39 Passphrase"

static const char TEXT_INFO[] = "-Scroll pages with up/down-"
                                "p1,2)   BIP39 Mnemonic/Seed"
                                "p3)       BIP32 Root Key   "
                                "p4,5)  Prv/Pub Account Keys"
                                "p6,7)  Prv/Pub BIP32 Keys  "
                                "p8+)    Receive Addresses  ";
// Same layout, Monero shows its own spend/view keys instead of BIP32 keys
static const char TEXT_INFO_XMR[] = "-Scroll pages with up/down-"
                                    "p1,2)   BIP39 Mnemonic/Seed"
                                    "p3)       BIP32 Root Key   "
                                    "p4,5)  Prv/Pub Account Keys"
                                    "p6,7)  Prv Spend/View Keys "
                                    "p8+)  Address/Subaddresses ";

struct FlipBipScene1 {
    View* view;
    FlipBipScene1Callback callback;
    void* context;
};

// Everything derived from the mnemonic, in a single allocation that only
// exists while the wallet is on screen.
typedef struct {
    CONFIDENTIAL char mnemonic[TEXT_BUFFER_SIZE];
    CONFIDENTIAL uint8_t seed[64];
    CONFIDENTIAL char xprv_root[XKEY_BUF];
    CONFIDENTIAL char xprv_account[XKEY_BUF];
    char xpub_account[XKEY_BUF];
    // m/44'/coin'/0'/0 xprv and xpub; for XMR the private spend and view keys (hex)
    CONFIDENTIAL char xprv_extended[XKEY_BUF];
    CONFIDENTIAL char xpub_extended[XKEY_BUF];
    char recv_addresses[NUM_ADDRS][MAX_ADDR_BUF];
} FlipBipWallet;

// Shared between the GUI thread (draw) and the app thread (input/enter/exit),
// so it is only ever touched through with_view_model().
typedef struct {
    int page;
    uint32_t coin_type;
    bool warn_insecure;
    const char* derivation_text;
    const char* error; // set when page == PAGE_ERROR
    FlipBipWallet* wallet; // NULL unless fully derived
} FlipBipScene1Model;

static uint32_t flipbip_coin_fmt(uint32_t coin_type) {
    return COIN_INFO_ARRAY[coin_type][COIN_INFO_ADDR_FMT];
}

void flipbip_scene_1_set_callback(
    FlipBipScene1* instance,
    FlipBipScene1Callback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

static void flipbip_wallet_free(FlipBipWallet* wallet) {
    if(wallet) {
        memzero(wallet, sizeof(FlipBipWallet));
        free(wallet);
    }
}

/* ---------------------------------------------------------------------------
 * Drawing. Runs on the GUI thread, so it only uses stack buffers: nothing
 * here may be shared with the app thread outside of the model lock.
 * ------------------------------------------------------------------------- */

// Copy line `index` of `text` (fixed `line_len` characters per line) into `out`.
// When `chunk` is set, a space is inserted before every group of 4 characters.
static void flipbip_scene_1_line(
    char* out,
    const char* text,
    size_t text_len,
    size_t line_len,
    size_t index,
    bool chunk) {
    out[0] = '\0';
    const size_t start = index * line_len;
    if(start >= text_len) return;
    size_t n = text_len - start;
    if(n > line_len) n = line_len;

    size_t o = 0;
    for(size_t i = 0; i < n && o < LINE_BUF - 2; i++) {
        if(chunk && i % 4 == 0) out[o++] = ' ';
        out[o++] = text[start + i];
    }
    out[o] = '\0';
}

static void flipbip_scene_1_draw_lines(Canvas* canvas, const char* text, size_t line_len) {
    char line[LINE_BUF];
    const size_t len = strlen(text);
    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < NUM_LINES; i++) {
        flipbip_scene_1_line(line, text, len, line_len, i, false);
        canvas_draw_str_aligned(canvas, 1, 2 + i * 10, AlignLeft, AlignTop, line);
    }
    memzero(line, sizeof(line));
}

static void flipbip_scene_1_draw_mnemonic(Canvas* canvas, const char* mnemonic) {
    // 4 words per line
    char line[LINE_BUF];
    canvas_set_font(canvas, FontSecondary);
    const char* p = mnemonic;
    for(size_t i = 0; i < NUM_LINES && *p; i++) {
        size_t o = 0;
        int words = 0;
        while(*p) {
            if(*p == ' ' && ++words == 4) {
                p++;
                break;
            }
            if(o < LINE_BUF - 2) line[o++] = *p;
            p++;
        }
        line[o] = '\0';
        canvas_draw_str_aligned(canvas, 1, 2 + i * 10, AlignLeft, AlignTop, line);
    }
    memzero(line, sizeof(line));
}

static void flipbip_scene_1_draw_seed(Canvas* canvas, const uint8_t* seed) {
    // 11 bytes = 22 hex chars per line
    char line[LINE_BUF];
    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < NUM_LINES; i++) {
        size_t start = i * 11;
        size_t n = 64 - start < 11 ? 64 - start : 11;
        flipbip_btox(seed + start, n, line);
        canvas_draw_str_aligned(canvas, 1, 2 + i * 10, AlignLeft, AlignTop, line);
    }
    memzero(line, sizeof(line));
}

static void flipbip_scene_1_draw_xmr_key(Canvas* canvas, const char* title, const char* hex) {
    char line[LINE_BUF];
    const size_t len = strlen(hex);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 1, 2, AlignLeft, AlignTop, title);
    for(size_t i = 0; i < 3; i++) {
        flipbip_scene_1_line(line, hex, len, 22, i, false);
        canvas_draw_str_aligned(canvas, 1, 14 + i * 10, AlignLeft, AlignTop, line);
    }
    memzero(line, sizeof(line));
}

static void flipbip_scene_1_draw_address(Canvas* canvas, const FlipBipScene1Model* model) {
    const int index = model->page - PAGE_ADDR_BEGIN;
    const char* label = COIN_TEXT_ARRAY[model->coin_type][COIN_TEXT_LABEL];
    const char* file = COIN_TEXT_ARRAY[model->coin_type][COIN_TEXT_FILE];
    const char* addr = model->wallet->recv_addresses[index];
    char line[LINE_BUF];

    // header: "<coin> receive address:"            "/N"
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 2, 2, AlignLeft, AlignTop, label);
    canvas_draw_str_aligned(
        canvas, strlen(label) * 7 + 1, 2, AlignLeft, AlignTop, TEXT_RECEIVE_ADDRESS);
    snprintf(line, sizeof(line), "/%d", index);
    canvas_draw_str_aligned(canvas, 125, 2, AlignRight, AlignTop, line);

    // footer: QR code file name
    const bool xmr = flipbip_coin_fmt(model->coin_type) == CoinTypeXMR128;
    snprintf(line, sizeof(line), "%s%02x%s", file, index, TEXT_QRFILE_EXT);
    canvas_draw_str_aligned(canvas, 125, xmr ? 56 : 53, AlignRight, AlignTop, line);

    if(xmr) {
        // 95 chars: 5 lines of 19 in the small font, no grouping
        const size_t len = strlen(addr);
        for(size_t i = 0; i < 5; i++) {
            flipbip_scene_1_line(line, addr, len, 19, i, false);
            canvas_draw_str_aligned(canvas, 2, 11 + i * 9, AlignLeft, AlignTop, line);
        }
        return;
    }

    // address, in groups of 4 characters: 3-4 lines of 12 for base58
    // (34-35 chars), 3 lines of 14 for ETH and bech32 (42 chars)
    const size_t len = strlen(addr);
    const size_t line_len = len > 36 ? 14 : 12;
    canvas_set_font(canvas, FontPrimary);
    for(size_t i = 0; i < 4; i++) {
        flipbip_scene_1_line(line, addr, len, line_len, i, true);
        canvas_draw_str(canvas, 7, 22 + i * 12, line);
    }
}

static void flipbip_scene_1_draw(Canvas* canvas, void* _model) {
    FlipBipScene1Model* model = _model;
    const FlipBipWallet* w = model->wallet;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->page == PAGE_ERROR) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "ERROR:");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 26, model->error);
        return;
    }

    // Every other page needs a fully derived wallet
    if(model->page == PAGE_LOADING || w == NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, TEXT_LOADING);
        if(model->derivation_text) canvas_draw_str(canvas, 7, 30, model->derivation_text);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 125, 2, AlignRight, AlignTop, FLIPBIP_VERSION);
        if(model->warn_insecure) {
            canvas_draw_str(canvas, 2, 50, WARN_INSECURE_TEXT_1);
            canvas_draw_str(canvas, 2, 60, WARN_INSECURE_TEXT_2);
        }
        return;
    }

    const bool xmr = flipbip_coin_fmt(model->coin_type) == CoinTypeXMR128;
    switch(model->page) {
    case PAGE_INFO:
        flipbip_scene_1_draw_lines(canvas, xmr ? TEXT_INFO_XMR : TEXT_INFO, 27);
        break;
    case PAGE_MNEMONIC:
        flipbip_scene_1_draw_mnemonic(canvas, w->mnemonic);
        break;
    case PAGE_SEED:
        flipbip_scene_1_draw_seed(canvas, w->seed);
        break;
    case PAGE_XPRV_ROOT:
        flipbip_scene_1_draw_lines(canvas, w->xprv_root, 20);
        break;
    case PAGE_XPRV_ACCT:
        flipbip_scene_1_draw_lines(canvas, w->xprv_account, 20);
        break;
    case PAGE_XPUB_ACCT:
        flipbip_scene_1_draw_lines(canvas, w->xpub_account, 20);
        break;
    case PAGE_XPRV_EXTD:
        if(xmr) {
            flipbip_scene_1_draw_xmr_key(canvas, TEXT_XMR_SPEND_KEY, w->xprv_extended);
        } else {
            flipbip_scene_1_draw_lines(canvas, w->xprv_extended, 20);
        }
        break;
    case PAGE_XPUB_EXTD:
        if(xmr) {
            flipbip_scene_1_draw_xmr_key(canvas, TEXT_XMR_VIEW_KEY, w->xpub_extended);
        } else {
            flipbip_scene_1_draw_lines(canvas, w->xpub_extended, 20);
        }
        break;
    default:
        flipbip_scene_1_draw_address(canvas, model);
        break;
    }
}

/* ---------------------------------------------------------------------------
 * Wallet derivation. Runs on the app thread WITHOUT the model lock held, so
 * the GUI thread can keep drawing the loading screen meanwhile.
 * ------------------------------------------------------------------------- */

static void flipbip_scene_1_init_address(
    char* addr_text,
    HDNode* addr_node,
    const HDNode* node,
    uint32_t coin_type,
    uint32_t addr_index) {
    memcpy(addr_node, node, sizeof(HDNode));
    hdnode_private_ckd(addr_node, addr_index);
    hdnode_fill_public_key(addr_node);

    const uint32_t fmt = flipbip_coin_fmt(coin_type);
    if(fmt == CoinTypeBTC84) {
        // Native SegWit P2WPKH: bech32(hrp, v0, hash160(pubkey))
        uint8_t hash[20];
        ecdsa_get_pubkeyhash(addr_node->public_key, HASHER_SHA2_RIPEMD, hash);
        segwit_addr_encode(addr_text, SEGWIT_HRP, 0, hash, sizeof(hash));
    } else if(fmt == CoinTypeETH60) {
        // ETH style address: "0x" + hex(keccak(pubkey)[12:])
        uint8_t hash[20];
        hdnode_get_ethereum_pubkeyhash(addr_node, hash);
        addr_text[0] = '0';
        addr_text[1] = 'x';
        flipbip_btox(hash, sizeof(hash), addr_text + 2);
    } else {
        // BTC / DOGE / ZEC style address (version bytes produce the prefix)
        ecdsa_get_address(
            addr_node->public_key,
            COIN_INFO_ARRAY[coin_type][COIN_INFO_ADDR_VERS],
            HASHER_SHA2_RIPEMD,
            HASHER_SHA2D,
            addr_text,
            MAX_ADDR_BUF);
    }

    memzero(addr_node, sizeof(HDNode));
}

// Monero, Ledger compatible (same keys as a Ledger with the same BIP39 seed):
//   k     = BIP32 secp256k1 private key at m/44'/128'/0'/0/0
//   spend = sc_reduce(keccak256(k)), view = sc_reduce(keccak256(spend))
// Address 0 is the primary address, 1..N-1 are subaddresses (account 0).
typedef struct {
    bignum256modm spend, view, m;
    ge25519 B, A, D, P;
    uint8_t buf[64];
} FlipBipXmrScratch;

static void flipbip_scene_1_init_xmr(
    FlipBipWallet* w,
    HDNode* addr_node,
    const HDNode* node,
    uint32_t coin_type) {
    FlipBipXmrScratch* x = malloc(sizeof(FlipBipXmrScratch));

    memcpy(addr_node, node, sizeof(HDNode));
    hdnode_private_ckd(addr_node, 0);
    xmr_hash_to_scalar(x->spend, addr_node->private_key, 32);
    memzero(addr_node, sizeof(HDNode));
    contract256_modm(x->buf, x->spend);
    xmr_hash_to_scalar(x->view, x->buf, 32);

    // Private keys, shown on the spend/view key pages
    flipbip_btox(x->buf, 32, w->xprv_extended);
    contract256_modm(x->buf, x->view);
    flipbip_btox(x->buf, 32, w->xpub_extended);

    // Public spend key B and view key A
    ge25519_scalarmult_base_wrapper(&x->B, x->spend);
    ge25519_scalarmult_base_wrapper(&x->A, x->view);

    for(uint32_t i = 0; i < NUM_ADDRS; i++) {
        uint64_t tag = COIN_INFO_ARRAY[coin_type][COIN_INFO_ADDR_VERS];
        if(i == 0) {
            ge25519_pack(x->buf, &x->B);
            ge25519_pack(x->buf + 32, &x->A);
        } else {
            // m = Hs("SubAddr" || view || 0 || i)
            // D = B + mG = (spend + m)G,  C = view * D = (view * (spend + m))G
            // Base point multiplies only: the variable base ge25519_scalarmult
            // needs ~2KB of stack, too much for the 3KB app stack.
            tag = 42;
            xmr_get_subaddress_secret_key(x->m, 0, i, x->view);
            add256_modm(x->m, x->m, x->spend);
            ge25519_scalarmult_base_wrapper(&x->D, x->m);
            mul256_modm(x->m, x->m, x->view);
            ge25519_scalarmult_base_wrapper(&x->P, x->m);
            ge25519_pack(x->buf, &x->D);
            ge25519_pack(x->buf + 32, &x->P);
        }
        xmr_base58_addr_encode_check(tag, x->buf, 64, w->recv_addresses[i], MAX_ADDR_BUF);
    }

    memzero(x, sizeof(FlipBipXmrScratch));
    free(x);
}

static FlipBipStatus flipbip_wallet_init(
    FlipBipWallet* w,
    const int strength,
    const uint32_t coin_type,
    const bool overwrite,
    const char* passphrase_text) {
    // Generate and save a new mnemonic if asked to, or if none is saved yet
    bool mnemonic_only = false;
    if(overwrite || (!flipbip_has_file(FlipBipFileKey, NULL, false) &&
                     !flipbip_has_file(FlipBipFileDat, NULL, false))) {
        mnemonic_only = true;
        const char* mnemonic_gen = mnemonic_generate(strength);
        FlipBipStatus status = FlipBipStatusSuccess;
        if(mnemonic_check(mnemonic_gen) == 0) {
            status = FlipBipStatusMnemonicCheckError;
        } else if(!flipbip_save_file_secure(mnemonic_gen)) {
            status = FlipBipStatusSaveError;
        }
        mnemonic_clear();
        if(status != FlipBipStatusSuccess) return status;
    }

    // Load the mnemonic from persistent storage
    if(!flipbip_load_file_secure(w->mnemonic)) return FlipBipStatusLoadError;
    if(mnemonic_check(w->mnemonic) == 0) return FlipBipStatusMnemonicCheckError;

    // Only generating a new mnemonic: go straight back to the menu
    if(mnemonic_only) return FlipBipStatusReturn;

    // BIP39 seed
    mnemonic_to_seed(w->mnemonic, passphrase_text, w->seed, 0);

    // Scratch nodes live on the heap to keep the 3K app stack free for the crypto
    HDNode* nodes = malloc(2 * sizeof(HDNode));
    HDNode* node = &nodes[0];
    HDNode* addr_node = &nodes[1];
    const uint32_t xprv_vers = COIN_INFO_ARRAY[coin_type][COIN_INFO_XPRV_VERS];
    const uint32_t xpub_vers = COIN_INFO_ARRAY[coin_type][COIN_INFO_XPUB_VERS];
    uint32_t fingerprint = 0;

    // m
    hdnode_from_seed(w->seed, 64, SECP256K1_NAME, node);
    hdnode_serialize_private(node, fingerprint, xprv_vers, w->xprv_root, XKEY_BUF);

    // m/purpose' (44, or 84 for native SegWit)
    hdnode_private_ckd_prime(node, COIN_INFO_ARRAY[coin_type][COIN_INFO_PURPOSE]);
    // m/purpose'/coin'
    hdnode_private_ckd_prime(node, COIN_INFO_ARRAY[coin_type][COIN_INFO_BIP44_COIN]);
    // m/purpose'/coin'/0'
    fingerprint = hdnode_fingerprint(node);
    hdnode_private_ckd_prime(node, DERIV_ACCOUNT);
    hdnode_serialize_private(node, fingerprint, xprv_vers, w->xprv_account, XKEY_BUF);
    // private_ckd leaves public_key stale: recompute before serializing the xpub
    hdnode_fill_public_key(node);
    hdnode_serialize_public(node, fingerprint, xpub_vers, w->xpub_account, XKEY_BUF);

    // m/purpose'/coin'/0'/0
    fingerprint = hdnode_fingerprint(node);
    hdnode_private_ckd(node, DERIV_CHANGE);
    const bool xmr = flipbip_coin_fmt(coin_type) == CoinTypeXMR128;
    if(xmr) {
        flipbip_scene_1_init_xmr(w, addr_node, node, coin_type);
    } else {
        hdnode_serialize_private(node, fingerprint, xprv_vers, w->xprv_extended, XKEY_BUF);
        hdnode_fill_public_key(node);
        hdnode_serialize_public(node, fingerprint, xpub_vers, w->xpub_extended, XKEY_BUF);
    }

    // Receive addresses m/purpose'/coin'/0'/0/i, each also saved as a QR code file
    char file_name[LINE_BUF];
    for(uint32_t a = 0; a < NUM_ADDRS; a++) {
        if(!xmr) {
            flipbip_scene_1_init_address(w->recv_addresses[a], addr_node, node, coin_type, a);
        }
        snprintf(
            file_name,
            sizeof(file_name),
            "%s%02lx%s",
            COIN_TEXT_ARRAY[coin_type][COIN_TEXT_FILE],
            (unsigned long)a,
            TEXT_QRFILE_EXT);
        flipbip_save_qrfile(
            COIN_TEXT_ARRAY[coin_type][COIN_TEXT_NAME], w->recv_addresses[a], file_name);
    }

    memzero(nodes, 2 * sizeof(HDNode));
    free(nodes);

#if USE_BIP39_CACHE
    bip39_cache_clear();
#endif

    return FlipBipStatusSuccess;
}

/* ---------------------------------------------------------------------------
 * View callbacks (app thread)
 * ------------------------------------------------------------------------- */

static bool flipbip_scene_1_input(InputEvent* event, void* context) {
    furi_assert(context);
    FlipBipScene1* instance = context;

    if(event->type != InputTypeRelease) return true;

    if(event->key == InputKeyBack) {
        // Never call out of the view while holding the model lock
        instance->callback(FlipBipCustomEventScene1Back, instance->context);
        return true;
    }

    int step = 0;
    if(event->key == InputKeyDown || event->key == InputKeyRight) step = 1;
    if(event->key == InputKeyUp || event->key == InputKeyLeft) step = -1;
    if(step == 0) return true;

    with_view_model(
        instance->view,
        FlipBipScene1Model * model,
        {
            // Only page through a fully derived wallet (not loading/error)
            if(model->wallet && model->page >= PAGE_INFO && model->page <= PAGE_ADDR_END) {
                model->page += step;
                if(model->page > PAGE_ADDR_END) model->page = PAGE_INFO;
                if(model->page < PAGE_INFO) model->page = PAGE_ADDR_END;
            }
        },
        true);
    return true;
}

static void flipbip_scene_1_enter(void* context) {
    furi_assert(context);
    FlipBipScene1* instance = context;
    FlipBip* app = instance->context;

    // BIP39 strength setting
    int strength = 256; // 24 words
    if(app->bip39_strength == FlipBipStrength128) {
        strength = 128; // 12 words
    } else if(app->bip39_strength == FlipBipStrength192) {
        strength = 192; // 18 words
    }

    // BIP39 passphrase setting
    const bool has_passphrase = app->passphrase == FlipBipPassphraseOn &&
                                strlen(app->passphrase_text) > 0;
    const char* passphrase_text = has_passphrase ? app->passphrase_text : "";

    const uint32_t coin_type = app->coin_type;
    const bool overwrite = app->overwrite_saved_seed != 0;

    // Publish the loading screen and release the lock so the GUI can draw it
    with_view_model(
        instance->view,
        FlipBipScene1Model * model,
        {
            model->page = PAGE_LOADING;
            model->coin_type = coin_type;
            model->warn_insecure = !has_passphrase;
            model->derivation_text =
                overwrite ? TEXT_NEW_WALLET : COIN_TEXT_ARRAY[coin_type][COIN_TEXT_DERIV];
            model->error = NULL;
            model->wallet = NULL;
        },
        true);

    // Slow part: PBKDF2, BIP32 derivation, file I/O. No lock held.
    FlipBipWallet* wallet = malloc(sizeof(FlipBipWallet));
    memzero(wallet, sizeof(FlipBipWallet));
    const FlipBipStatus status =
        flipbip_wallet_init(wallet, strength, coin_type, overwrite, passphrase_text);

    if(status == FlipBipStatusReturn) {
        // New mnemonic generated and saved, go back to the menu
        flipbip_wallet_free(wallet);
        instance->callback(FlipBipCustomEventScene1Back, instance->context);
        return;
    }

    const char* error = NULL;
    if(status == FlipBipStatusSaveError) {
        error = "Save error";
    } else if(status == FlipBipStatusLoadError) {
        error = "Load error";
    } else if(status == FlipBipStatusMnemonicCheckError) {
        error = "Mnemonic check error";
    }
    if(error) {
        flipbip_wallet_free(wallet);
        wallet = NULL;
    }

    // Publish the result in one short critical section
    with_view_model(
        instance->view,
        FlipBipScene1Model * model,
        {
            model->wallet = wallet;
            model->error = error;
            model->page = error ? PAGE_ERROR : PAGE_INFO;
        },
        true);
}

static void flipbip_scene_1_exit(void* context) {
    furi_assert(context);
    FlipBipScene1* instance = context;
    FlipBipWallet* wallet = NULL;

    // Detach the wallet under the lock; no redraw, this view is going away
    with_view_model(
        instance->view,
        FlipBipScene1Model * model,
        {
            wallet = model->wallet;
            model->wallet = NULL;
            model->error = NULL;
            model->page = PAGE_LOADING;
        },
        false);

    // The GUI thread can no longer reach it, safe to wipe outside the lock
    flipbip_wallet_free(wallet);
}

FlipBipScene1* flipbip_scene_1_alloc(void) {
    FlipBipScene1* instance = malloc(sizeof(FlipBipScene1));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(FlipBipScene1Model));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, flipbip_scene_1_draw);
    view_set_input_callback(instance->view, flipbip_scene_1_input);
    view_set_enter_callback(instance->view, flipbip_scene_1_enter);
    view_set_exit_callback(instance->view, flipbip_scene_1_exit);
    return instance;
}

void flipbip_scene_1_free(FlipBipScene1* instance) {
    furi_assert(instance);
    FlipBipWallet* wallet = NULL;
    with_view_model(
        instance->view,
        FlipBipScene1Model * model,
        {
            wallet = model->wallet;
            model->wallet = NULL;
        },
        false);
    flipbip_wallet_free(wallet);
    view_free(instance->view);
    free(instance);
}

View* flipbip_scene_1_get_view(FlipBipScene1* instance) {
    furi_assert(instance);
    return instance->view;
}
