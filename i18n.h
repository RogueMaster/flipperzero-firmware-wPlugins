#pragma once
#include <stdint.h>

// English text strings (used when g.lang == 1)
// 中文模式直接用 zh_chars.h 中的 XBM 位图; 英文模式用 canvas_draw_str
#define LANG_ZH 0
#define LANG_EN 1

// Title
#define EN_TITLE       "3D MAZE"
// Menu items
#define EN_M1          "1. Campaign"
#define EN_M2          "2. Endless"
#define EN_M3          "3. Visitor"
#define EN_HINT_MENU   "Up/Dn Select  OK Start  Back Exit"
#define EN_LANG_TAG    "EN"
#define EN_ZH_TAG      "CN"

// HUD labels
#define EN_HUD_LV      "LV"
#define EN_HUD_FLOOR   "FL"
#define EN_HUD_HP      "HP"
#define EN_HUD_KEY     "K"
#define EN_HUD_TORCH   "T"

// Overlay titles
#define EN_OV_CLEAR    "CLEAR!"
#define EN_OV_OVER     "DEAD"
#define EN_OV_PAUSED   "PAUSE"
// Overlay buttons
#define EN_OV_BTNS     "OK:Next    Back:Menu"
#define EN_OV_BTNS2    "OK:Retry   Back:Menu"
#define EN_OV_BTNS3    "OK:Resume  Back:Menu"

// In-game messages (one line)
#define EN_MSG_KEY     "Got Key!"
#define EN_MSG_TORCH   "Torch +1"
#define EN_MSG_TRAP    "Trap! -1HP"
#define EN_MSG_DOOR    "Door Open"
#define EN_MSG_NEEDKEY "Need Key"
#define EN_MSG_FINDEXIT "Find Exit!"
#define EN_MSG_CARE    "Watch Out!"
#define EN_MSG_PUZZLE  "Key->Door"
#define EN_MSG_VISITOR "Visitor Mode"
#define EN_MSG_RUN     "Endless Run"
#define EN_MSG_HIT     "Hit! -1HP"
#define EN_MSG_EXIT    "Exit Ahead"

// Map msg_id -> English string
static inline const char* en_msg_str(int id) {
    switch(id) {
        case 0:  return EN_MSG_KEY;
        case 1:  return EN_MSG_TORCH;
        case 2:  return EN_MSG_TRAP;
        case 3:  return EN_MSG_DOOR;
        case 4:  return EN_MSG_NEEDKEY;
        case 5:  return EN_MSG_FINDEXIT;
        case 6:  return EN_MSG_CARE;
        case 7:  return EN_MSG_PUZZLE;
        case 8:  return EN_MSG_VISITOR;
        case 9:  return EN_MSG_RUN;
        case 10: return EN_MSG_HIT;
        case 11: return EN_MSG_EXIT;
        default: return "";
    }
}
