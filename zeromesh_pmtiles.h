#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct PmTiles PmTiles;

PmTiles* pmtiles_open(const char* path);

void pmtiles_close(PmTiles* p);

bool pmtiles_get_tile(
    PmTiles* p,
    uint8_t z,
    uint32_t x,
    uint32_t y,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

bool pmtiles_has_tile(PmTiles* p, uint8_t z, uint32_t x, uint32_t y);

uint8_t pmtiles_min_zoom(const PmTiles* p);
uint8_t pmtiles_max_zoom(const PmTiles* p);
uint32_t pmtiles_tile_count(const PmTiles* p);

uint32_t pmtiles_max_tile_len(const PmTiles* p);
