#include "maze3d.h"

// Minimal: single 8x8 brick wall texture.
// Pixel layout: bit 0 of each byte = leftmost pixel (tx=0).
const uint8_t TEXTURES[TEX_COUNT][8] = {
    // [0] Brick wall - stretcher bond with mortar lines.
    {0xFF, 0x81, 0x7B, 0x7B, 0xFF, 0xDB, 0xDE, 0xDE},
};

uint8_t texture_sample(int tex_id, int tx, int ty) {
    if(tex_id < 0 || tex_id >= TEX_COUNT) return 1;
    tx &= 7;
    ty &= 7;
    return (TEXTURES[tex_id][ty] >> (tx & 7)) & 1;
}
