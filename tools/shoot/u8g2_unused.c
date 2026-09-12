/*
 * The Flipper's copy of u8g2 leaves out the capture and kerning source
 * files, because the firmware never calls them - but u8g2_buffer.c and
 * u8g2_font.c still reference them, so a host link needs something to
 * point at. Nothing below is reachable from a canvas_* call.
 */
#include <u8g2.h>

uint8_t u8x8_capture_get_pixel_1(uint16_t x, uint16_t y, uint8_t* dest, uint8_t tile_width) {
    (void)x;
    (void)y;
    (void)dest;
    (void)tile_width;
    return 0;
}
uint8_t u8x8_capture_get_pixel_2(uint16_t x, uint16_t y, uint8_t* dest, uint8_t tile_width) {
    (void)x;
    (void)y;
    (void)dest;
    (void)tile_width;
    return 0;
}
void u8x8_capture_write_pbm_pre(uint8_t w, uint8_t h, void (*out)(const char* s)) {
    (void)w;
    (void)h;
    (void)out;
}
void u8x8_capture_write_pbm_buffer(
    uint8_t* buffer,
    uint8_t w,
    uint8_t h,
    uint8_t (*get_pixel)(uint16_t, uint16_t, uint8_t*, uint8_t),
    void (*out)(const char* s)) {
    (void)buffer;
    (void)w;
    (void)h;
    (void)get_pixel;
    (void)out;
}
void u8x8_capture_write_xbm_pre(uint8_t w, uint8_t h, void (*out)(const char* s)) {
    (void)w;
    (void)h;
    (void)out;
}
void u8x8_capture_write_xbm_buffer(
    uint8_t* buffer,
    uint8_t w,
    uint8_t h,
    uint8_t (*get_pixel)(uint16_t, uint16_t, uint8_t*, uint8_t),
    void (*out)(const char* s)) {
    (void)buffer;
    (void)w;
    (void)h;
    (void)get_pixel;
    (void)out;
}
uint8_t u8g2_GetKerning(u8g2_t* u8g2, u8g2_kerning_t* k, uint16_t e1, uint16_t e2) {
    (void)u8g2;
    (void)k;
    (void)e1;
    (void)e2;
    return 0;
}
uint8_t u8g2_GetKerningByTable(u8g2_t* u8g2, const uint16_t* kt, uint16_t e1, uint16_t e2) {
    (void)u8g2;
    (void)kt;
    (void)e1;
    (void)e2;
    return 0;
}
