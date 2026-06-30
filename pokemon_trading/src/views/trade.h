#ifndef TRADE_H
#define TRADE_H

#pragma once

#include <gui/view.h>
#include <src/include/pokemon_data.h>

void* trade_alloc(
    PokemonData* pdata,
    void* gblink_handle,
    ViewDispatcher* view_dispatcher,
    uint32_t view_id);

void trade_free(ViewDispatcher* view_dispatcher, uint32_t view_id, void* trade_ctx);

void trade_reset_connection(void* trade_ctx);

bool trade_connected(void* trade_ctx);

#endif /* TRADE_H */
