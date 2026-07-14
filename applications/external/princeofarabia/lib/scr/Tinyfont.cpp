#include "../Tinyfont.h"
#include "TinyfontSprite.c"

#define TINYFONT_WIDTH 4
#define TINYFONT_HEIGHT 4

Tinyfont::Tinyfont(uint8_t *screenBuffer, int16_t width, int16_t height){
  sBuffer = screenBuffer;
  sWidth = width;
  sHeight = height;

  // default values
  lineHeight = TINYFONT_HEIGHT + 1;
  letterSpacing = 1;

  cursorX = cursorY = baseX = 0;
  textColor = 1;

  maskText = false;
}

size_t Tinyfont::write(uint8_t c) {
  // check \n
  if(c == '\n'){
    cursorX = baseX; // cr
    cursorY += lineHeight; // lf
  }
  // check for tab
  else if(c == '\t'){
    cursorX += TINYFONT_WIDTH + 5;
  }
  else{
    // draw char
    printChar(c, cursorX, cursorY);
    cursorX += TINYFONT_WIDTH + letterSpacing;
  }
    return 1;
}

void Tinyfont::printChar(char c, int16_t x, int16_t y)
{
  // no need to draw at all of we're offscreen
  if (x + TINYFONT_WIDTH <= 0 || x > sWidth - 1 || y + TINYFONT_HEIGHT <= 0 || y > sHeight - 1)
    return;

  // check if char is available
  if (((uint8_t) c) < 32 || ((uint8_t) c) > 127) c = (char)127;

  uint8_t cval = ((uint8_t) c) - 32;

  // layout lowercase letters
  if (cval >= 65 && cval <= 90) y++;

  // layout comma letters
  if (cval == 12 || cval == 27) y++;

  // get sprite frames
  const uint8_t *letter = TINYFONT_SPRITE + ((cval/2) * 4);

  for (uint8_t i = 0; i < 4; i++ ) {

    uint8_t colData = pgm_read_byte(letter++);
    if (c % 2 == 0) {
      // mask upper sprite
      colData &= 0x0f;
    }
    else{
      // for every odd character, shift pixels to get the correct character
      colData >>= 4;
    }

    if (maskText) {
      drawByte(x+i, y, 0x0f, (textColor == 0)?1:0);
    }

    drawByte(x+i, y, colData, textColor);
  }
}

void Tinyfont::drawByte(int16_t x, int16_t y, uint8_t pixels, uint8_t color){

  uint8_t row = (uint8_t)y / 8;

  // check if byte needs to be seperated
  if (((uint8_t)y)%8 == 0) {
    uint8_t col = (uint8_t)x % sWidth;
    if (color == 0)
      sBuffer[col + row*sWidth] &= ~pixels;
    else
      sBuffer[col + row*sWidth] |= pixels;
  }
  else{
    uint8_t d = (uint8_t)y%8;

    drawByte(x, row*8, pixels << d, color);
    drawByte(x, (row+1)*8, pixels >> (8-d), color);
  }
}

void Tinyfont::setCursor(int16_t x, int16_t y){
  cursorX = baseX = x;
  cursorY = y;
}

int16_t Tinyfont::getCursorX() const {
  return cursorX;
}

int16_t Tinyfont::getCursorY() const {
  return cursorY;
}

void Tinyfont::setTextColor(uint8_t color){
  textColor = color;
}

uint8_t Tinyfont::getTextColor() const {
  return textColor;
}
