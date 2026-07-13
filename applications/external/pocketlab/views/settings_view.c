#include "settings_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <input/input.h>

#include "../helpers/pocketlab_fonts.h"
#include "../helpers/pocketlab_i18n.h"
#include "../helpers/pocketlab_sound.h"

// This view is a pixel-faithful reimplementation of the stock VariableItemList
// (same 16px rows, x=6 label, arrows at 73/115, value centred at 94, scrollbar)
// drawn with the app font so it can render Cyrillic/Spanish, which the stock
// module cannot. In English the font resolves to the stock one, so it looks
// identical to the original list.
#define SETTINGS_ITEM_H    16
#define SETTINGS_ITEM_W    123
#define SETTINGS_ON_SCREEN 4

typedef enum {
    SettingsItemSound,
    SettingsItemLed,
    SettingsItemVibro,
    SettingsItemLanguage,
    SettingsItemReset,
    SettingsItemCount,
} SettingsItem;

// Short ASCII codes so the value always fits between the < > arrows, in any language.
static const char* const settings_lang_names[PocketLabLangCount] = {"EN", "RU"};

struct SettingsView {
    View* view;
    NotificationApp* notifications;
    SettingsViewResetCallback reset_callback;
    void* reset_context;
};

typedef struct {
    PocketLabState* state;
    uint8_t position; // selected item
    uint8_t window; // first visible item
} SettingsModel;

static const char* settings_item_label(size_t i) {
    switch(i) {
    case SettingsItemSound:
        return pocketlab_text(PocketLabTextSettingsSound);
    case SettingsItemLed:
        return pocketlab_text(PocketLabTextSettingsLed);
    case SettingsItemVibro:
        return pocketlab_text(PocketLabTextSettingsVibro);
    case SettingsItemLanguage:
        return pocketlab_text(PocketLabTextSettingsLanguage);
    default:
        return pocketlab_text(PocketLabTextSettingsReset);
    }
}

static uint8_t settings_item_count(size_t i) {
    switch(i) {
    case SettingsItemLanguage:
        return PocketLabLangCount;
    case SettingsItemReset:
        return 1;
    default:
        return 2; // On/Off toggles
    }
}

static uint8_t settings_item_index(const PocketLabState* s, size_t i) {
    switch(i) {
    case SettingsItemSound:
        return s->sound ? 1 : 0;
    case SettingsItemLed:
        return s->led ? 1 : 0;
    case SettingsItemVibro:
        return s->vibro ? 1 : 0;
    case SettingsItemLanguage:
        return s->lang < PocketLabLangCount ? s->lang : 0;
    default:
        return 0;
    }
}

static const char* settings_item_value(const PocketLabState* s, size_t i) {
    switch(i) {
    case SettingsItemSound:
        return pocketlab_text(s->sound ? PocketLabTextOn : PocketLabTextOff);
    case SettingsItemLed:
        return pocketlab_text(s->led ? PocketLabTextOn : PocketLabTextOff);
    case SettingsItemVibro:
        return pocketlab_text(s->vibro ? PocketLabTextOn : PocketLabTextOff);
    case SettingsItemLanguage:
        return settings_lang_names[settings_item_index(s, i)];
    default:
        return NULL; // Reset has no value
    }
}

static void settings_view_draw_callback(Canvas* canvas, void* context) {
    SettingsModel* model = context;
    canvas_clear(canvas);
    pocketlab_font_apply_small(canvas);

    for(size_t i = 0; i < SettingsItemCount; i++) {
        const int pos = (int)i - (int)model->window;
        if(pos < 0 || pos >= SETTINGS_ON_SCREEN) continue;

        const uint8_t item_y = (uint8_t)(pos * SETTINGS_ITEM_H);
        const uint8_t text_y = (uint8_t)(item_y + SETTINGS_ITEM_H - 4);

        if(i == model->position) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(
                canvas, 0, item_y + 1, SETTINGS_ITEM_W, SETTINGS_ITEM_H - 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, 6, text_y, settings_item_label(i));

        const char* value = settings_item_value(model->state, i);
        if(value) {
            const uint8_t idx = settings_item_index(model->state, i);
            const uint8_t count = settings_item_count(i);
            if(idx > 0) canvas_draw_str(canvas, 73, text_y, "<");
            canvas_draw_str_aligned(
                canvas, (115 + 73) / 2 + 1, text_y, AlignCenter, AlignBottom, value);
            if(idx < count - 1) canvas_draw_str(canvas, 115, text_y, ">");
        }
    }

    canvas_set_color(canvas, ColorBlack);
    elements_scrollbar(canvas, model->position, SettingsItemCount);
}

// Change an adjustable item by +1/-1 (clamped, like the stock list). Returns
// whether a confirmation sound should play and which one.
static void settings_view_adjust(
    PocketLabState* s,
    uint8_t item,
    int delta,
    bool* play,
    PocketLabSoundId* sound) {
    const uint8_t count = settings_item_count(item);
    int idx = (int)settings_item_index(s, item) + delta;
    if(idx < 0) idx = 0;
    if(idx > count - 1) idx = count - 1;
    const uint8_t v = (uint8_t)idx;

    switch(item) {
    case SettingsItemSound:
        s->sound = v;
        *play = v != 0;
        *sound = PocketLabSoundClick;
        break;
    case SettingsItemLed:
        s->led = v;
        pocketlab_sound_configure_fx(s->led, s->vibro);
        *play = v != 0;
        *sound = PocketLabSoundComplete;
        break;
    case SettingsItemVibro:
        s->vibro = v;
        pocketlab_sound_configure_fx(s->led, s->vibro);
        *play = v != 0;
        *sound = PocketLabSoundThud;
        break;
    case SettingsItemLanguage:
        s->lang = v;
        pocketlab_i18n_set_lang((PocketLabLang)v);
        // English uses the stock font; other languages need the Universal one.
        pocketlab_font_set_universal(v != PocketLabLangEn);
        *play = true;
        *sound = PocketLabSoundClick;
        break;
    default:
        break;
    }
}

static bool settings_view_input_callback(InputEvent* event, void* context) {
    SettingsView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool play = false;
    bool do_reset = false;
    uint8_t sound_on = 0;
    PocketLabSoundId sound = PocketLabSoundClick;

    with_view_model(
        instance->view,
        SettingsModel * model,
        {
            PocketLabState* s = model->state;
            if(event->key == InputKeyUp) {
                if(model->position > 0) model->position--;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                if(model->position + 1 < SettingsItemCount) model->position++;
                consumed = true;
            } else if(event->key == InputKeyLeft) {
                settings_view_adjust(s, model->position, -1, &play, &sound);
                consumed = true;
            } else if(event->key == InputKeyRight) {
                settings_view_adjust(s, model->position, +1, &play, &sound);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                if(model->position == SettingsItemReset) do_reset = true;
                consumed = true;
            }

            // Keep the selection inside the 4-row window.
            if(model->position < model->window) {
                model->window = model->position;
            } else if(model->position >= model->window + SETTINGS_ON_SCREEN) {
                model->window = (uint8_t)(model->position - SETTINGS_ON_SCREEN + 1);
            }
            sound_on = s->sound;
        },
        true);

    if(play) {
        pocketlab_sound_play(instance->notifications, sound_on != 0, sound);
    }
    if(do_reset && instance->reset_callback) {
        instance->reset_callback(instance->reset_context);
    }

    return consumed;
}

SettingsView* settings_view_alloc(void) {
    SettingsView* instance = malloc(sizeof(SettingsView));
    instance->view = view_alloc();
    instance->notifications = NULL;
    instance->reset_callback = NULL;
    instance->reset_context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SettingsModel));
    view_set_draw_callback(instance->view, settings_view_draw_callback);
    view_set_input_callback(instance->view, settings_view_input_callback);

    return instance;
}

void settings_view_free(SettingsView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* settings_view_get_view(SettingsView* instance) {
    furi_assert(instance);
    return instance->view;
}

void settings_view_set_reset_callback(
    SettingsView* instance,
    SettingsViewResetCallback callback,
    void* context) {
    furi_assert(instance);
    instance->reset_callback = callback;
    instance->reset_context = context;
}

void settings_view_configure(
    SettingsView* instance,
    PocketLabState* state,
    NotificationApp* notifications) {
    furi_assert(instance);
    instance->notifications = notifications;
    with_view_model(
        instance->view,
        SettingsModel * model,
        {
            model->state = state;
            model->position = 0;
            model->window = 0;
        },
        true);
}
