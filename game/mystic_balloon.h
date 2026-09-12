/**
  @file   mystic_balloon.h
  @author apfxtech
  @brief  game core interface

  Mystic Balloon, Flipper Zero port

  apfxtech, 2026, license: MIT (see LICENSE)

  SPDX-License-Identifier: MIT
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MYBL_UP    0x01
#define MYBL_DOWN  0x02
#define MYBL_LEFT  0x04
#define MYBL_RIGHT 0x08
#define MYBL_OK    0x10
#define MYBL_BACK  0x20

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t level;
    uint8_t coins;
    uint8_t coinsHighscore;
    uint32_t score;
    uint32_t highscore;
} MyblSave;

void mybl_start(bool sound, const MyblSave* save);
void mybl_frame(uint8_t held, uint8_t pressEdges);
bool mybl_exit_requested(void);
bool mybl_take_save(MyblSave* save);

void platform_tone(uint16_t frequency, uint16_t duration_ms);

#ifdef __cplusplus
}
#endif
