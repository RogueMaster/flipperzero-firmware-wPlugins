/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Which files the reader will attempt.
 *
 * Every elementary file of ICAO Doc 9303 part 10 is listed, including the
 * ones that cannot be switched, because the question this screen is opened
 * with is usually "why did it not read DG3" and the answer has to be on it.
 */
#include "../emrtd_i.h"

/*
 * The list keeps the label pointer it is given rather than a copy, so the
 * labels have to outlive the screen. "DG1 MRZ" says more than either half of
 * it alone, so they are composed once, here.
 */
static char emrtd_scene_data_groups_labels[EmrtdFileCount][24];

/*
 * What an item needs to know about itself.
 *
 * The list also keeps the context pointer rather than a copy, and hands it
 * back to the callback, so telling each item which file it stands for costs
 * nothing and saves the callback from having to ask the list where the cursor
 * is - which would mean reaching back into the list from inside its own
 * callback, at a moment when the list is holding its model open.
 */
typedef struct {
    Emrtd* app;
    uint8_t id;
} EmrtdDataGroupItem;

static EmrtdDataGroupItem emrtd_scene_data_groups_context[EmrtdFileCount];

static bool emrtd_scene_data_groups_selectable(const EmrtdFileInfo* info) {
    return info != NULL && info->dg_number >= 0 && !info->eac_protected;
}

static void emrtd_scene_data_groups_set(Emrtd* app, size_t id, bool selected) {
    if(selected) {
        app->config.files |= EMRTD_FILE_BIT(id);
    } else {
        app->config.files &= ~EMRTD_FILE_BIT(id);
    }
}

/*
 * Left and right. The list has already moved the index by the time it calls
 * us, and the item it hands over is the only pointer to that item which is
 * safe to hold: see the note above emrtd_scene_data_groups_populate.
 */
static void emrtd_scene_data_groups_changed(VariableItem* item) {
    EmrtdDataGroupItem* entry = variable_item_get_context(item);
    const bool selected = variable_item_get_current_value_index(item) != 0;

    emrtd_scene_data_groups_set(entry->app, entry->id, selected);
    variable_item_set_current_value_text(item, selected ? "Read" : "Skip");
}

/*
 * Fill the list from app->config.files.
 *
 * Nothing here keeps the VariableItem pointers the list hands back, and
 * nothing may: the list holds its items in a single array which it grows by
 * reallocating, sixteen items to the first allocation. This screen adds
 * eighteen, so the seventeenth add copies the array elsewhere and frees the
 * block the first sixteen items lived in - and the firmware clears a block as
 * it frees it. A pointer kept from before that add therefore reads a null
 * value text, and writing through it faults the whole system, not just the
 * application. Every item is addressed through the argument the list passes
 * to the callback instead.
 */
static void emrtd_scene_data_groups_populate(Emrtd* app) {
    VariableItemList* list = app->variable_item_list;

    for(size_t id = 0; id < EmrtdFileCount; id++) {
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)id);
        char* label = emrtd_scene_data_groups_labels[id];
        const size_t label_size = sizeof(emrtd_scene_data_groups_labels[0]);
        EmrtdDataGroupItem* entry = &emrtd_scene_data_groups_context[id];

        entry->app = app;
        entry->id = (uint8_t)id;

        if(info == NULL) {
            snprintf(label, label_size, "File %u", (unsigned)id);
            variable_item_list_add(list, label, 1, NULL, entry);
            continue;
        }

        /* "EF.DG1" reads better without the prefix the standard gives it. */
        const char* short_name = info->name;
        if(strncmp(short_name, "EF.", 3) == 0) {
            short_name += 3;
        }
        snprintf(label, label_size, "%s %s", short_name, info->label);

        VariableItem* item;
        if(info->dg_number < 0) {
            /* EF.COM lists what is on the chip and EF.SOD carries the hashes
             * every other file is checked against; neither is optional. */
            item = variable_item_list_add(list, label, 1, NULL, entry);
            variable_item_set_current_value_text(item, "Always");
        } else if(info->eac_protected) {
            /* Fingerprints and iris images need a certificate issued by the
             * state that made the document, which this reader has no way of
             * holding. ICAO 9303-11, 4.6. */
            item = variable_item_list_add(list, label, 1, NULL, entry);
            variable_item_set_current_value_text(item, "EAC");
        } else {
            const bool selected = (app->config.files & EMRTD_FILE_BIT(id)) != 0;
            item = variable_item_list_add(list, label, 2, emrtd_scene_data_groups_changed, entry);
            variable_item_set_current_value_index(item, selected ? 1 : 0);
            variable_item_set_current_value_text(item, selected ? "Read" : "Skip");
        }
    }
}

static void emrtd_scene_data_groups_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void emrtd_scene_data_groups_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    VariableItemList* list = app->variable_item_list;

    /*
     * The list is shared with the options screen, which empties it on its way
     * out. Emptying it again here costs nothing and makes the one thing the
     * centre key depends on - that row N is file N - true by construction
     * rather than by the good behaviour of whoever came before.
     */
    variable_item_list_reset(list);
    emrtd_scene_data_groups_populate(app);

    /* The list refuses a null callback, and it is shared with the options
     * screen, so it is always given one. Here it makes the centre key do what
     * left and right do, which is what a list of switches invites. */
    variable_item_list_set_enter_callback(list, emrtd_scene_data_groups_enter_callback, app);
    variable_item_list_set_selected_item(
        list, (uint8_t)scene_manager_get_scene_state(app->scene_manager, EmrtdSceneDataGroups));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewVariableItemList);
}

bool emrtd_scene_data_groups_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < EmrtdFileCount) {
        const size_t id = event.event;
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)id);

        if(emrtd_scene_data_groups_selectable(info)) {
            emrtd_scene_data_groups_set(app, id, (app->config.files & EMRTD_FILE_BIT(id)) == 0);

            /*
             * The centre key reports which row was pressed, not which item,
             * and an item may not be held on to. Building the list again is
             * what is left, and it costs nothing visible: emptying the list
             * leaves the cursor and the scroll offset where they were, so the
             * screen does not move under the user.
             */
            variable_item_list_reset(app->variable_item_list);
            emrtd_scene_data_groups_populate(app);
        }
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_data_groups_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    scene_manager_set_scene_state(
        app->scene_manager,
        EmrtdSceneDataGroups,
        variable_item_list_get_selected_item_index(app->variable_item_list));

    variable_item_list_reset(app->variable_item_list);

    emrtd_settings_save(app);
}
