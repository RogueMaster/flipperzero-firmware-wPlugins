#ifndef PEOPLE_H
#define PEOPLE_H

#include "globals.h"

#define NPC_TYPE_INFO     0
#define NPC_TYPE_QUESTION 1
#define NPC_TYPE_SHOP     2

#define NPC_FIELD_ONE   0
#define NPC_FIELD_TWO   1
#define NPC_FIELD_THREE 2
#define NPC_FIELD_FOUR  3

#define NPC_SWAMP_ONE   4
#define NPC_SWAMP_TWO   5
#define NPC_SWAMP_THREE 6
#define NPC_SWAMP_FOUR  7

#define NPC_FOREST_ONE   8
#define NPC_FOREST_TWO   9
#define NPC_FOREST_THREE 10
#define NPC_FOREST_FOUR  11

#define NPC_CANYON_ONE   12
#define NPC_CANYON_TWO   13
#define NPC_CANYON_THREE 14
#define NPC_CANYON_FOUR  15

#define NPC_TOTAL 5
#define NPC_INN   0
#define NPC_SHOP  1
#define NPC_HOUSE 2
#define NPC_TREE  3

struct People {
    int x, y;
    byte type;
};

People npc;

const unsigned char PROGMEM npcData[] = {
    // x,y, tile coördinate for each npc

    // interior people
    // ===============
    // 100, 100, 0B00000000
    //             ||||||||
    //             |||||||└->  0 -
    //             ||||||└-->  1   | type of body, 8 possibilities
    //             |||||└--->  2 -/
    //             ||||└---->  3 reserved
    //             |||└----->  4 reserved
    //             ||└------>  5 -
    //             |└------->  6   | type of head, 8 possibilities
    //             └-------->  7 -/

    118,
    187,
    0B01100011, // interior Inn
    70,
    187,
    0B00100001, // interior Shop
    20,
    188,
    0B01000010, // interior House

    // exterior people
    // ===============
    13,
    153,
    0B00000010, // exterior House

};

byte getArea(uint16_t area_x, uint16_t area_y) {
    byte testArea = ((area_x / 384) + (area_y / 384) * 8);
    switch(testArea) {
    case 48: // 2
        return 3;
        break;
    case 56:
        return 2;
        break;
    case 58:
        return 1; // Shop keeper
        break;
    default:
        return 0;
    }
}

void updatePeople() {
    byte area = 3 * getArea(player.x, player.y);
    npc.x = pgm_read_byte(&npcData[area]) * 16;
    npc.y = pgm_read_byte(&npcData[area + 1]) * 16;
    npc.type = pgm_read_byte(&npcData[area + 2]);
}

void drawPeople() {
    //byte breath = (((globalFrame % 20) < 10) ? 1 : 0);
    //byte handMove = ((((globalFrame + 5) % 20) < 10) ? 1 : 0);
    //byte breath = ((globalFrame % 16) >> 4);
    byte breath = (globalFrame >> 3) % 2;
    //byte handMove = (((globalFrame + 5) % 16) >> 4);
    byte handMove = ((globalFrame + 5) >> 3) % 2;
    int people_x = npc.x - camX;
    int people_y = npc.y - camY;
    //sprites.drawPlusMask(people_x + 3, people_y + 9 , npcBody_plus_mask, (npc.type << 5) >> 5); //npc.type & 0b00000111);
    sprites.drawPlusMask(
        people_x + 3, people_y + 9, npcBody_plus_mask, npc.type & 0x07); //npc.type & 0b00000111);
    sprites.drawPlusMask(people_x + 2, people_y + 11 - handMove, npcHands_plus_mask, 0);
    sprites.drawPlusMask(people_x, people_y + breath, npcHead_plus_mask, npc.type >> 5);
    blinkingEyes(people_x + 6, people_y + 7 + breath);
}

/* Interact with a person.
 *  Skips investigating objects if a person was found.
 */
void investigatePeople(int ix, int iy) {
    Point playerpoint = {ix, iy};
    Rect personrect = {npc.x - 8, npc.y, 32, 32};

    if(arduboy.collide(playerpoint, personrect)) {
        foundSomething = true;
        investigating = false; // don't check tiles if an npc was present
        byte testArea = ((player.x / 384) + (player.y / 384) * 8);
        switch(testArea) {
        case 48: // neighbor npc outside
            fillWithSentence(63, TEXT_ROLL); // "you'll need a map"
            break;
        case 58: // shops
            foundSomething = false;
            gameState = STATE_GAME_SHOP;
            //cursorY = 0;
            break;
        case 60: // INN
            foundSomething = false;
            gameState = STATE_GAME_INN;
            fadeCounter = 0;
            return;
            break;
        default: //case 61: // houses
        {
            fillWithSentence(60, TEXT_ROLL); // "lots of monsters!"
            /*switch (player.lastDoor)
          {
            case 34: // NPC in door 34 Fields
              fillWithSentence(60, TEXT_ROLL);
              break;
          }*/
        } break;
        }
    }
    talkingWithNPC = false;
}

boolean getCollisionPeople(int fx, int fy) {
    Point playerpoint = {fx, fy};
    Rect personrect = {npc.x - 1, npc.y, 15, 16};

    return (arduboy.collide(playerpoint, personrect));
}

#endif
