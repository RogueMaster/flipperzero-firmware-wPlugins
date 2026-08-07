#ifndef yo3gnd_txanim_3fa
#define yo3gnd_txanim_3fa

#include <stdbool.h>
#include <stdint.h>
#include <gui/canvas.h>

bool txdraw(Canvas* c, uint32_t ms);
void txpic(Canvas* c, uint8_t f);
void txrand(void);
void txcancel(uint32_t ms);

#endif
