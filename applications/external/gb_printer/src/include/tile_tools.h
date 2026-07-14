// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#ifndef TILE_TOOLS_H
#define TILE_TOOLS_H

#pragma once

void tile_to_scanline(uint8_t* src, size_t tiles_w, size_t tiles_h);

void scanline_to_tile(uint8_t* dst, uint8_t* src, size_t tiles_w, size_t tiles_h);

#endif // TILE_TOOLS_H
