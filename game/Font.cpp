// Compact font taken from
// https://hackaday.io/project/6309-vga-graphics-over-spi-and-serial-vgatonic/log/20759-a-tiny-4x6-pixel-font-that-will-fit-on-almost-any-microcontroller-license-mit

#include <stdint.h>
#include "Defines.h"
#include "Font.h"
#include "Draw.h"

// Font Definition
const uint8_t font4x6[96][2] PROGMEM = {
	{ 0x00  ,  0x00 },   /*SPACE*/
	{ 0x49  ,  0x08 },   /*'!'*/
	{ 0xb4  ,  0x00 },   /*'"'*/
	{ 0xbe  ,  0xf6 },   /*'#'*/
	{ 0x7b  ,  0x7a },   /*'$'*/
	{ 0xa5  ,  0x94 },   /*'%'*/
	{ 0x55  ,  0xb8 },   /*'&'*/
	{ 0x48  ,  0x00 },   /*'''*/
	{ 0x29  ,  0x44 },   /*'('*/
	{ 0x44  ,  0x2a },   /*')'*/
	{ 0x15  ,  0xa0 },   /*'*'*/
	{ 0x0b  ,  0x42 },   /*'+'*/
	{ 0x00  ,  0x50 },   /*','*/
	{ 0x03  ,  0x02 },   /*'-'*/
	{ 0x00  ,  0x08 },   /*'.'*/
	{ 0x25  ,  0x90 },   /*'/'*/
	{ 0x76  ,  0xba },   /*'0'*/
	{ 0x59  ,  0x5c },   /*'1'*/
	{ 0xc5  ,  0x9e },   /*'2'*/
	{ 0xc5  ,  0x38 },   /*'3'*/
	{ 0x92  ,  0xe6 },   /*'4'*/
	{ 0xf3  ,  0x3a },   /*'5'*/
	{ 0x73  ,  0xba },   /*'6'*/
	{ 0xe5  ,  0x90 },   /*'7'*/
	{ 0x77  ,  0xba },   /*'8'*/
	{ 0x77  ,  0x3a },   /*'9'*/
	{ 0x08  ,  0x40 },   /*':'*/
	{ 0x08  ,  0x50 },   /*';'*/
	{ 0x2a  ,  0x44 },   /*'<'*/
	{ 0x1c  ,  0xe0 },   /*'='*/
	{ 0x88  ,  0x52 },   /*'>'*/
	{ 0xe5  ,  0x08 },   /*'?'*/
	{ 0x56  ,  0x8e },   /*'@'*/
	{ 0x77  ,  0xb6 },   /*'A'*/
	{ 0x77  ,  0xb8 },   /*'B'*/
	{ 0x72  ,  0x8c },   /*'C'*/
	{ 0xd6  ,  0xba },   /*'D'*/
	{ 0x73  ,  0x9e },   /*'E'*/
	{ 0x73  ,  0x92 },   /*'F'*/
	{ 0x72  ,  0xae },   /*'G'*/
	{ 0xb7  ,  0xb6 },   /*'H'*/
	{ 0xe9  ,  0x5c },   /*'I'*/
	{ 0x64  ,  0xaa },   /*'J'*/
	{ 0xb7  ,  0xb4 },   /*'K'*/
	{ 0x92  ,  0x9c },   /*'L'*/
	{ 0xbe  ,  0xb6 },   /*'M'*/
	{ 0xd6  ,  0xb6 },   /*'N'*/
	{ 0x56  ,  0xaa },   /*'O'*/
	{ 0xd7  ,  0x92 },   /*'P'*/
	{ 0x76  ,  0xee },   /*'Q'*/
	{ 0x77  ,  0xb4 },   /*'R'*/
	{ 0x71  ,  0x38 },   /*'S'*/
	{ 0xe9  ,  0x48 },   /*'T'*/
	{ 0xb6  ,  0xae },   /*'U'*/
	{ 0xb6  ,  0xaa },   /*'V'*/
	{ 0xb6  ,  0xf6 },   /*'W'*/
	{ 0xb5  ,  0xb4 },   /*'X'*/
	{ 0xb5  ,  0x48 },   /*'Y'*/
	{ 0xe5  ,  0x9c },   /*'Z'*/
	{ 0x69  ,  0x4c },   /*'['*/
	{ 0x91  ,  0x24 },   /*'\'*/
	{ 0x64  ,  0x2e },   /*']'*/
	{ 0x54  ,  0x00 },   /*'^'*/
	{ 0x00  ,  0x1c },   /*'_'*/
	{ 0x44  ,  0x00 },   /*'`'*/
	{ 0x0e  ,  0xae },   /*'a'*/
	{ 0x9a  ,  0xba },   /*'b'*/
	{ 0x0e  ,  0x8c },   /*'c'*/
	{ 0x2e  ,  0xae },   /*'d'*/
	{ 0x0e  ,  0xce },   /*'e'*/
	{ 0x56  ,  0xd0 },   /*'f'*/
	{ 0x55  ,  0x3B },   /*'g'*/
	{ 0x93  ,  0xb4 },   /*'h'*/
	{ 0x41  ,  0x44 },   /*'i'*/
	{ 0x41  ,  0x51 },   /*'j'*/
	{ 0x97  ,  0xb4 },   /*'k'*/
	{ 0x49  ,  0x44 },   /*'l'*/
	{ 0x17  ,  0xb6 },   /*'m'*/
	{ 0x1a  ,  0xb6 },   /*'n'*/
	{ 0x0a  ,  0xaa },   /*'o'*/
	{ 0xd6  ,  0xd3 },   /*'p'*/
	{ 0x76  ,  0x67 },   /*'q'*/
	{ 0x17  ,  0x90 },   /*'r'*/
	{ 0x0f  ,  0x38 },   /*'s'*/
	{ 0x9a  ,  0x8c },   /*'t'*/
	{ 0x16  ,  0xae },   /*'u'*/
	{ 0x16  ,  0xba },   /*'v'*/
	{ 0x16  ,  0xf6 },   /*'w'*/
	{ 0x15  ,  0xb4 },   /*'x'*/
	{ 0xb5  ,  0x2b },   /*'y'*/
	{ 0x1c  ,  0x5e },   /*'z'*/
	{ 0x6b  ,  0x4c },   /*'{'*/
	{ 0x49  ,  0x48 },   /*'|'*/
	{ 0xc9  ,  0x5a },   /*'}'*/
	{ 0x54  ,  0x00 },   /*'~'*/
	{ 0x56  ,  0xe2 }    /*''*/
};

// Font retreival function - ugly, but needed.
unsigned char getFontLine(unsigned char data, int line_num) {
	const uint8_t index = (data - 32);
	unsigned char pixel = 0;
	if ((pgm_read_byte(&font4x6[index][1]) & 1) == 1) line_num -= 1;
	if (line_num == 0) {
		pixel = (pgm_read_byte(&font4x6[index][0])) >> 4;
	}
	else if (line_num == 1) {
		pixel = (pgm_read_byte(&font4x6[index][0])) >> 1;
	}
	else if (line_num == 2) {
		// Split over 2 bytes
		return (((pgm_read_byte(&font4x6[index][0])) & 0x03) << 2) | (((pgm_read_byte(&font4x6[index][1])) & 0x02));
	}
	else if (line_num == 3) {
		pixel = (pgm_read_byte(&font4x6[index][1])) >> 4;
	}
	else if (line_num == 4) {
		pixel = (pgm_read_byte(&font4x6[index][1])) >> 1;
	}
	return pixel & 0xE;
}

static uint8_t PrintX, PrintY;

void DrawChar(char c)
{
	const uint8_t index = ((unsigned char)(c)) - 32;
	uint8_t data1 = pgm_read_byte(&font4x6[index][0]);
	uint8_t data2 = pgm_read_byte(&font4x6[index][1]);
	uint8_t y = PrintY;

	if (data2 & 1)	// Descender e.g. j, g
	{
		PutPixel(PrintX, y, 1);
		PutPixel(PrintX + 1, y, 1);
		PutPixel(PrintX + 2, y, 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data1 & 0x80) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data1 & 0x40) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data1 & 0x20) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data1 & 0x10) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data1 & 0x8) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data1 & 0x4) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data1 & 0x2) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data1 & 0x1) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data2 & 0x2) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data2 & 0x80) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data2 & 0x40) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data2 & 0x20) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data2 & 0x10) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data2 & 0x8) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data2 & 0x4) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;
	}
	else
	{
		PutPixel(PrintX, y, (data1 & 0x80) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data1 & 0x40) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data1 & 0x20) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data1 & 0x10) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data1 & 0x8) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data1 & 0x4) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data1 & 0x2) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data1 & 0x1) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data2 & 0x2) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data2 & 0x80) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data2 & 0x40) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data2 & 0x20) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, (data2 & 0x10) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data2 & 0x8) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data2 & 0x4) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;

		PutPixel(PrintX, y, 1);
		PutPixel(PrintX + 1, y, 1);
		PutPixel(PrintX + 2, y, 1);
		PutPixel(PrintX + 3, y, 1);
	}
}

void DrawCharOld(char c)
{
	uint8_t y = PrintY;

	for (int row = 0; row < FONT_HEIGHT; row++)
	{
		uint8_t data = getFontLine((unsigned char)c, row);
		PutPixel(PrintX, y, (data & 8) ? 0 : 1);
		PutPixel(PrintX + 1, y, (data & 4) ? 0 : 1);
		PutPixel(PrintX + 2, y, (data & 2) ? 0 : 1);
		PutPixel(PrintX + 3, y, 1);
		y++;
	}
}

void DrawString(const char* str, uint8_t x, uint8_t y)
{
	PrintX = x;
	PrintY = y;

	for (;;)
	{
		char c = pgm_read_byte(str++);
		if (!c)
			break;

		DrawChar(c);
		PrintX += FONT_WIDTH;
	}
}

#define MAX_DIGITS 5
void DrawInt(int16_t val, uint8_t x, uint8_t y)
{
	PrintX = x;
	PrintY = y;

	if (val == 0)
	{
		DrawChar('0');
		return;
	}
	else if (val < 0)
	{
		DrawChar('-');
		PrintX += FONT_WIDTH;
	}

	char buffer[MAX_DIGITS];
	int bufCount = 0;

	for (int n = 0; n < MAX_DIGITS && val != 0; n++)
	{
		unsigned char c = val % 10;
		buffer[bufCount++] = '0' + c;
		val = val / 10;
	}

	for (int n = bufCount - 1; n >= 0; n--)
	{
		DrawChar(buffer[n]);
		PrintX += FONT_WIDTH;
	}
}

uint8_t DrawCurrency(int32_t val, uint8_t x, uint8_t y)
{
	uint8_t len = 0;
	PrintX = x;
	PrintY = y;

	if (val == 0)
	{
		DrawChar('0');
		PrintX -= FONT_WIDTH;
		len++;
	}
	else
	{
		bool isNeg = val < 0;
		if (isNeg)
			val = -val;
		int nextComma = 3;

		while (val)
		{
			if (!nextComma)
			{
				DrawChar(',');
				PrintX -= FONT_WIDTH;
				nextComma = 3;
				len++;
			}
			unsigned char c = val % 10;
			DrawChar('0' + c);
			PrintX -= FONT_WIDTH;
			len++;
			val = val / 10;
			nextComma--;
		}

		if (isNeg)
		{
			DrawChar('-');
			PrintX -= FONT_WIDTH;
			len++;
		}

	}

	DrawChar('$');
	len++;
	return len;
}
