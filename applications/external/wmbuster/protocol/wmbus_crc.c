#include "wmbus_crc.h"
#include <string.h>

uint16_t wmbus_crc(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for(size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(int b = 0; b < 8; b++) {
            if(crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x3D65);
            else             crc = (uint16_t)(crc << 1);
        }
    }
    return (uint16_t)(crc ^ 0xFFFF);
}

/* Format A block sizes: first block is 10 data bytes + 2 CRC = 12.
 * Following blocks are up to 16 + 2 = 18, and the final one may be shorter. */
bool wmbus_crc_verify_format_a(
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len) {
    if(frame_len < 12) return false;
    /* L-field is frame[0]; total packet length = L + 1 (L counts everything
     * after itself, excluding CRCs). */
    size_t L = frame[0];
    /* Total raw length (with CRCs) for Format A: */
    size_t blocks = 1 + ((L > 9) ? ((L - 9 + 15) / 16) : 0);
    size_t needed = 12 + (blocks > 1 ? (blocks - 1) * 18 : 0);
    /* Last block may be shorter — recompute exactly. */
    if(L > 9) {
        size_t rest = L - 9;
        size_t full = rest / 16;
        size_t tail = rest % 16;
        needed = 12 + full * 18 + (tail ? (tail + 2) : 0);
    } else {
        needed = 12; /* only first block */
    }
    if(frame_len < needed) return false;

    size_t out_pos = 0;
    /* Block 1 */
    uint16_t c = wmbus_crc(frame, 10);
    uint16_t got = (uint16_t)((frame[10] << 8) | frame[11]);
    if(c != got) return false;
    if(out_capacity < 10) return false;
    memcpy(out, frame, 10);
    out_pos = 10;

    size_t pos = 12;
    while(pos < needed) {
        size_t remain = needed - pos;
        size_t blk = (remain >= 18) ? 16 : (remain - 2);
        c = wmbus_crc(&frame[pos], blk);
        got = (uint16_t)((frame[pos + blk] << 8) | frame[pos + blk + 1]);
        if(c != got) return false;
        if(out_pos + blk > out_capacity) return false;
        memcpy(&out[out_pos], &frame[pos], blk);
        out_pos += blk;
        pos += blk + 2;
    }
    if(out_len) *out_len = out_pos;
    return true;
}

bool wmbus_crc_verify_format_b(const uint8_t* frame, size_t frame_len) {
    if(frame_len < 3) return false;
    uint16_t c = wmbus_crc(frame, frame_len - 2);
    uint16_t got = (uint16_t)((frame[frame_len - 2] << 8) | frame[frame_len - 1]);
    return c == got;
}
