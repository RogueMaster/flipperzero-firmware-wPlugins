#include "../animation_switcher.h"
#include "fas_scene.h"

/* ── Range definitions ───────────────────────────────────────────────── */
#define MIN_BUTTHURT_MIN 0
#define MIN_BUTTHURT_MAX 14
#define MAX_BUTTHURT_MIN 0
#define MAX_BUTTHURT_MAX 14
#define MIN_LEVEL_MIN    1
#define MIN_LEVEL_MAX    30
#define MAX_LEVEL_MIN    1
#define MAX_LEVEL_MAX    30
#define WEIGHT_MIN       1
#define WEIGHT_MAX       99

/* ── Helper: write the numeric label for an item ─────────────────────── */
static void set_text(VariableItem* item, int val) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", val);
    variable_item_set_current_value_text(item, buf);
}

/* ── Change callbacks ────────────────────────────────────────────────── */
static void cb_min_butthurt(VariableItem* item) {
    FasApp* app = variable_item_get_context(item);
    int     val = (int)variable_item_get_current_value_index(item) + MIN_BUTTHURT_MIN;
    app->animations[app->current_anim_index].min_butthurt = val;
    set_text(item, val);
}

static void cb_max_butthurt(VariableItem* item) {
    FasApp* app = variable_item_get_context(item);
    int     val = (int)variable_item_get_current_value_index(item) + MAX_BUTTHURT_MIN;
    app->animations[app->current_anim_index].max_butthurt = val;
    set_text(item, val);
}

static void cb_min_level(VariableItem* item) {
    FasApp* app = variable_item_get_context(item);
    int     val = (int)variable_item_get_current_value_index(item) + MIN_LEVEL_MIN;
    app->animations[app->current_anim_index].min_level = val;
    set_text(item, val);
}

static void cb_max_level(VariableItem* item) {
    FasApp* app = variable_item_get_context(item);
    int     val = (int)variable_item_get_current_value_index(item) + MAX_LEVEL_MIN;
    app->animations[app->current_anim_index].max_level = val;
    set_text(item, val);
}

static void cb_weight(VariableItem* item) {
    FasApp* app = variable_item_get_context(item);
    int     val = (int)variable_item_get_current_value_index(item) + WEIGHT_MIN;
    app->animations[app->current_anim_index].weight = val;
    set_text(item, val);
}

/* ── Scene handlers ───────────────────────────────────────────────────── */
void fas_scene_anim_settings_on_enter(void* context) {
    FasApp*    app  = context;
    AnimEntry* anim = &app->animations[app->current_anim_index];
    VariableItemList* vl = app->var_list;
    VariableItem*     item;

    variable_item_list_reset(vl);

    /* Display the animation name as a non-interactive label at the top.
     * One value count with no callback makes it unselectable visually. */
    VariableItem* name_item = variable_item_list_add(vl, anim->name, 1, NULL, NULL);
    variable_item_set_current_value_text(name_item, "");

    /* Min Butthurt */
    item = variable_item_list_add(
        vl, "Min Butthurt",
        MIN_BUTTHURT_MAX - MIN_BUTTHURT_MIN + 1,
        cb_min_butthurt, app);
    variable_item_set_current_value_index(item, anim->min_butthurt - MIN_BUTTHURT_MIN);
    set_text(item, anim->min_butthurt);

    /* Max Butthurt */
    item = variable_item_list_add(
        vl, "Max Butthurt",
        MAX_BUTTHURT_MAX - MAX_BUTTHURT_MIN + 1,
        cb_max_butthurt, app);
    variable_item_set_current_value_index(item, anim->max_butthurt - MAX_BUTTHURT_MIN);
    set_text(item, anim->max_butthurt);

    /* Min Level */
    item = variable_item_list_add(
        vl, "Min Level",
        MIN_LEVEL_MAX - MIN_LEVEL_MIN + 1,
        cb_min_level, app);
    variable_item_set_current_value_index(item, anim->min_level - MIN_LEVEL_MIN);
    set_text(item, anim->min_level);

    /* Max Level */
    item = variable_item_list_add(
        vl, "Max Level",
        MAX_LEVEL_MAX - MAX_LEVEL_MIN + 1,
        cb_max_level, app);
    variable_item_set_current_value_index(item, anim->max_level - MAX_LEVEL_MIN);
    set_text(item, anim->max_level);

    /* Weight */
    item = variable_item_list_add(
        vl, "Weight",
        WEIGHT_MAX - WEIGHT_MIN + 1,
        cb_weight, app);
    variable_item_set_current_value_index(item, anim->weight - WEIGHT_MIN);
    set_text(item, anim->weight);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewVarList);
}

bool fas_scene_anim_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; /* Back button handled by scene manager automatically */
}

void fas_scene_anim_settings_on_exit(void* context) {
    FasApp* app = context;
    variable_item_list_reset(app->var_list);
}