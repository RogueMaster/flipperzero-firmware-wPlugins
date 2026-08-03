#pragma once
#include <stdint.h>

// English text strings (used when g.lang == 1)
// 中文模式直接用 zh_chars.h 中的 XBM 位图; 英文模式用 canvas_draw_str
#define LANG_EN 0
#define LANG_ZH 1

// Title
#define EN_TITLE     "3D MAZE"
// Menu items (M1 = story/campaign mode, M2 = endless, M3 = visitor)
#define EN_M1        "1. Story"
#define EN_M2        "2. Endless"
#define EN_M3        "3. Visitor"
#define EN_HINT_MENU "Up/Dn Select  OK Start  Back Exit"
#define EN_LANG_TAG  "EN"
#define EN_ZH_TAG    "CN"

// Level select
#define EN_LS_TITLE_STORY   "STORY - SELECT LEVEL"
#define EN_LS_TITLE_ENDLESS "ENDLESS - SELECT LEVEL"
#define EN_LS_LOCKED        "Locked"
#define EN_LS_CLEARED       "Cleared"
#define EN_LS_HINT          "Up/Dn:Level  OK:Enter  Back"

// Inventory
#define EN_INV_TITLE    "INVENTORY"
#define EN_INV_KEY      "Key"
#define EN_INV_TORCH    "Torch"
#define EN_INV_POTION   "Potion"
#define EN_INV_AMULET   "Amulet"
#define EN_INV_EMPTY    "(empty)"
#define EN_INV_HINT     "Up/Dn:Select  OK:Use  Back"
#define EN_INV_USED     "Used!"
#define EN_INV_NOITEM   "No item"
#define EN_INV_FULLHP   "Full HP"
#define EN_INV_TELEPORT "Warp!"

// Story
#define EN_STORY_HINT   "OK:Next  Back:Skip"
#define EN_STORY_CHOOSE "OK:A  Right:B"
#define EN_STORY_PAGE   "p"

// HUD labels
#define EN_HUD_LV    "LV"
#define EN_HUD_FLOOR "FL"
#define EN_HUD_HP    "HP"
#define EN_HUD_KEY   "K"
#define EN_HUD_TORCH "T"

// Overlay titles
#define EN_OV_CLEAR  "CLEAR!"
#define EN_OV_OVER   "DEAD"
#define EN_OV_PAUSED "PAUSE"
// Overlay buttons
#define EN_OV_BTNS   "OK:Next    Back:Menu"
#define EN_OV_BTNS2  "OK:Retry   Back:Menu"
#define EN_OV_BTNS3  "OK:Resume  Back:Menu"

// In-game messages (one line)
#define EN_MSG_KEY       "Got Key!"
#define EN_MSG_TORCH     "Torch +1"
#define EN_MSG_TRAP      "Trap! -1HP"
#define EN_MSG_DOOR      "Door Open"
#define EN_MSG_NEEDKEY   "Need Key"
#define EN_MSG_FINDEXIT  "Find Exit!"
#define EN_MSG_CARE      "Watch Out!"
#define EN_MSG_PUZZLE    "Key->Door"
#define EN_MSG_VISITOR   "Visitor Mode"
#define EN_MSG_RUN       "Endless Run"
#define EN_MSG_HIT       "Hit! -1HP"
#define EN_MSG_EXIT      "Exit Ahead"
#define EN_MSG_QUESTDONE "Quest Done!"
#define EN_MSG_POTION    "Potion +1"
#define EN_MSG_AMULET    "Amulet +1"
#define EN_MSG_MINE      "Mined"
#define EN_MSG_PLACE     "Placed"
#define EN_MSG_LOCKED    "Locked! Do Task"
#define EN_MSG_NOAMMO    "No Ammo!"
#define EN_MSG_AMMO      "Ammo +3"
#define EN_MSG_ACHIEVE   "Achievement!"
#define EN_MSG_TASKPROG  "Task Progress"
#define EN_HUD_ENEMY     "E"

// Map msg_id -> English string
static inline const char* en_msg_str(int id) {
    switch(id) {
    case 0:
        return EN_MSG_KEY;
    case 1:
        return EN_MSG_TORCH;
    case 2:
        return EN_MSG_TRAP;
    case 3:
        return EN_MSG_DOOR;
    case 4:
        return EN_MSG_NEEDKEY;
    case 5:
        return EN_MSG_FINDEXIT;
    case 6:
        return EN_MSG_CARE;
    case 7:
        return EN_MSG_PUZZLE;
    case 8:
        return EN_MSG_VISITOR;
    case 9:
        return EN_MSG_RUN;
    case 10:
        return EN_MSG_HIT;
    case 11:
        return EN_MSG_EXIT;
    case 12:
        return EN_MSG_QUESTDONE;
    case 13:
        return EN_MSG_POTION;
    case 14:
        return EN_MSG_AMULET;
    case 15:
        return EN_MSG_MINE;
    case 16:
        return EN_MSG_PLACE;
    case 17:
        return EN_MSG_LOCKED;
    case 18:
        return EN_MSG_NOAMMO;
    case 19:
        return EN_MSG_AMMO;
    case 20:
        return EN_MSG_ACHIEVE;
    case 21:
        return EN_MSG_TASKPROG;
    default:
        return "";
    }
}
