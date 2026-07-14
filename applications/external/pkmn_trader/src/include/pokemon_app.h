#ifndef POKEMON_APP_H
#define POKEMON_APP_H

#pragma once

#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/icon.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gblink.h>

#include <src/include/pokemon_data.h>

#define TAG "Pokemon"

struct pokemon_fap {
    /* Various anonymous pointers for flipper UI/navigation */
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    void* select;
    void* trade;
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* variable_item_list;
    DialogEx* dialog_ex;

    /* Struct for holding trade data */
    /* NOTE: There may be some runtime memory savings by adding more intelligence
     * to views/trade and slimming down this struct to only contain the single
     * pokemon data rather than the full 6 member party data.
     */
    PokemonData* pdata;

    /* gblink interface */
    void* gblink_handle;
};

typedef struct pokemon_fap PokemonFap;

typedef enum {
    AppViewSubmenu,
    AppViewTextInput,
    AppViewVariableItem,
    AppViewDialogEx,
    AppViewSelectPokemon,
    AppViewTrade,
} AppView;

#endif /* POKEMON_APP_H */
