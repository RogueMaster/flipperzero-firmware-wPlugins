#ifndef TRADE_PATCH_LIST_H
#define TRADE_PATCH_LIST_H

#pragma once

#include <gui/view.h>
#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>

struct patch_list;

struct patch_list* plist_alloc(void);

void plist_append(struct patch_list* plist, uint8_t index);

void plist_free(struct patch_list* plist);

uint8_t plist_index_get(struct patch_list* plist, int offset);

void plist_create(struct patch_list** pplist, PokemonData* pdata);

#endif /* TRADE_PATCH_LIST_H */
