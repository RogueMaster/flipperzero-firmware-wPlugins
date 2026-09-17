#pragma once

#include <stdint.h>

#define FONT_WIDTH 4
#define FONT_HEIGHT 6

void DrawString(const char* str, uint8_t x, uint8_t y);
void DrawInt(int16_t val, uint8_t x, uint8_t y);
uint8_t DrawCurrency(int32_t val, uint8_t x, uint8_t y);

