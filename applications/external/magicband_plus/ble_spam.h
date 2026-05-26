#pragma once

#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>

#include "scenes/_setup.h"

// ─── Views ───────────────────────────────────────────────────────────────────
enum {
    ViewMain,
    ViewByteInput,
    ViewSubmenu,
    ViewTextInput,
    ViewVariableItemList,
};

// ─── Pre-warm options (shared between ble_spam.c and scenes/settings.c) ──────
#define MB_PREWARM_OPT_COUNT 5
static const uint8_t mb_prewarm_secs[MB_PREWARM_OPT_COUNT] = {0, 5, 10, 15, 30};
static const char* const mb_prewarm_names[MB_PREWARM_OPT_COUNT] =
    {"Off", "5s", "10s", "15s", "30s"};

// Index of the Custom Hex attack in the attacks[] array
#define CUSTOM_ATTACK_IDX 4

typedef struct Attack Attack;

// ─── Context (first field of State → safe to cast Ctx* ↔ State*) ─────────────
typedef struct {
    Attack* attack; // populated before entering SceneConfig

    // Buffer for byte-input scenes (custom hex, etc.)
    uint8_t byte_store[16];

    // Fallback enter callback used by protocol extra_config
    VariableItemListEnterCallback fallback_config_enter;

    // ── Global settings (readable from any scene) ────────────────────────────
    bool led_indicator;
    bool lock_keyboard;
    bool fast_mode; // interleave CC fast-mode pings
    uint8_t fast_mode_prewarm_idx; // index into mb_prewarm_secs[]

    // Current attack index shown in the preset browser
    int8_t preset_index;

    // ── SDK objects ───────────────────────────────────────────────────────────
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    ByteInput* byte_input;
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* variable_item_list;

    // OFW patch
    VariableItem* item_pp_color;
    VariableItem*     item_send;
    bool              config_in_prewarm;
} Ctx;

// ─── Helpers exposed for scene files ─────────────────────────────────────────

// Stop advertising if currently running (safe to call from any scene)
void ble_spam_stop_adv(Ctx* ctx);

// Update the Custom Hex attack payload and jump preset_index to it
void ble_spam_set_custom_payload(Ctx* ctx, const uint8_t* data, uint8_t len);
void ble_spam_select_attack(Ctx* ctx, int8_t idx);
void ble_spam_toggle_adv(Ctx* ctx);
bool ble_spam_is_advertising(Ctx* ctx);
bool ble_spam_is_warming(Ctx* ctx);
