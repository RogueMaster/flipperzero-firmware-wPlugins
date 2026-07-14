#pragma once

#include <stdint.h>

class Platform {
public:
    static uint8_t GetInput(void);
    static uint8_t* GetScreenBuffer();

    static void PlaySound(const uint16_t* audioPattern);
    static bool IsAudioEnabled();
    static void SetAudioEnabled(bool isEnabled);

    static void FillScreen(uint8_t col);
    static void PutPixel(uint8_t x, uint8_t y, uint8_t colour);
    static void DrawBitmap(int16_t x, int16_t y, const uint8_t* bitmap);
    static void DrawSolidBitmap(int16_t x, int16_t y, const uint8_t* bitmap);
    static void DrawSprite(
        int16_t x,
        int16_t y,
        const uint8_t* bitmap,
        const uint8_t* mask,
        uint8_t frame,
        uint8_t mask_frame);
    static void DrawSprite(int16_t x, int16_t y, const uint8_t* bitmap, uint8_t frame);

    static void DrawVLine(uint8_t x, int8_t y1, int8_t y2, uint8_t pattern);
    static void DrawBackground();
};
