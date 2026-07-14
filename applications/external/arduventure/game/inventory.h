#ifndef INVENTORY_H
#define INVENTORY_H

#include "globals.h"

void showInventory() {
    fillWithSentence(4);
    drawTextBox(96, 2, WHITE);
}

byte foundLastPiece() {
    //if (bitCount(player.bossActiveAlive & 0xF0) == 4)
    if((player.bossActiveAlive & 0B11110000) == 0B11110000) {
        // Have all blade pieces
        // Put blade in inventory.
        bitSet(player.hasStuff[2], 7);
        //fillWithSentence(26, TEXT_ROLL);
        return 26;
    }
    return 36;
}

//byte returnObject

void investigateObjects(byte object) {
    if(object == TILE_ROCK) //31
    {
        if(bitRead(player.hasStuff[7], player.currentRegion)) {
            bitSet(player.gameTriggers[3], player.currentRegion);
            fillWithSentence(55, TEXT_ROLL); // "a lightbeam emerges from your amulet..."
            flashWhite = true;
        } else {
            fillWithSentence(54, TEXT_ROLL); // "the rock has symbols resembling ______"
            fillWithWord(33, 121 + player.currentRegion); // water,leaf,fire
        }
        foundSomething = true;
    }

    if(object > TILE_WALL && object != TILE_BLACK) {
        //byte regionComingFrom = (player.lastDoor - 28);
        switch(object) {
        /*case TILE_BED_LOW:            // 48
      case TILE_BED_UP:             // 49
        break;*/
        case TILE_SHELVE_LOW: // 50
            if(bitRead(player.gameTriggers[3], 3) ==
               0) /*&& (player.x / 16 == 63) && (player.lastDoor == 24))*/
            {
                bitSet(player.gameTriggers[3], 3);
                fillWithSentence(49, TEXT_ROLL);
                fillWithWord(23, 144); // map
            } else
                fillWithSentence(56, TEXT_ROLL);
            break;
        /*case TILE_TABLE:              // 52
        switch (player.lastDoor)
        {

          case 28:                  // CAVE FIELDS
          case 29:                  // CAVE SWAMP
          case 30:                  // CAVE FOREST
          case 31:                  // CAVE CANYON
            if (bitRead(player.bossActiveAlive, player.lastDoor - 28))
            {
              gameState = STATE_GAME_BOSS;
            }
            else fillWithSentence(44, TEXT_ROLL); // "nothing there to take"
            break;
          default:
            fillWithSentence(44, TEXT_ROLL); // "nothing there to take"
            break;
        }
        break;*/
        case TILE_SIGN: // 56
            if((player.currentRegion) == REGION_YOUR_GARDEN) {
                fillWithSentence(1, TEXT_ROLL);
                fillWithName(34);
            } else if(playerReducedY > 30) {
                fillWithSentence(23, TEXT_ROLL);
            } else {
                fillWithSentence(53, TEXT_ROLL);
                fillWithWord(24, 169 + player.currentRegion);
            }
            break;
            //      ----CHESTS----
        case TILE_CLOSED_BOX: //57
        {
            // Blade shards
            if(player.currentRegion >= REGION_FIELDS && player.currentRegion <= REGION_CANYONS) {
                bitSet(player.bossActiveAlive, player.currentRegion - 5);
                bitSet(player.gameTriggers[3], player.currentRegion - 5);
                fillWithSentence(foundLastPiece(), TEXT_ROLL);
            }
            // Game bosses
            else if(player.lastDoor >= 28 && player.lastDoor < 32) {
                if(player.bossActiveAlive == 0B11110000 ||
                   bitRead(player.bossActiveAlive, player.lastDoor - 28) ||
                   !bitRead(player.hasStuff[6], player.lastDoor - 28)) {
                    gameState = STATE_GAME_BOSS;
                } else {
                    fillWithSentence(44, TEXT_ROLL); // "nothing there to take"
                }
            }
            // Armor in players house
            else if((player.lastDoor) == 43) // Players house
            {
                bitSet(player.hasStuff[4], 0);
                bitSet(player.gameTriggers[2], 4);
                fillWithSentence(51, TEXT_ROLL); // Found armor
                fillWithWord(23, 113); // Wool
            } else {
                bitSet(player.gameTriggers[player.lastDoor / 8], player.lastDoor % 8);
                byte amount = ((player.lastDoor / 5) * 2) + 1;
                byte door = player.lastDoor % 5;
                if(door == 0) // 1, 3, 5, 7 gold  lastDoor 0, 5, 10, 15
                {
                    fillWithSentence(48, TEXT_ROLL); // Found <item> amount <amount>
                    fillWithWord(11, 125); // Gold
                    fillWithNumber(28, amount * 10);
                    player.gold += amount * 10;
                } else if(door == 1) // 1, 3, 5, 7 Apples lastDoor 1, 6, 11, 16
                {
                    fillWithSentence(48, TEXT_ROLL); // Found <item> amount <amount>
                    bitSet(player.hasStuff[0], 0);
                    fillWithNumber(28, amount);
                    player.itemsAmount[0] += amount;
                    fillWithWord(11, 97); // Apple
                }

                else if(door == 2) // 1, 3, 5, 7 Anise lastDoor 2, 7, 12, 17
                {
                    fillWithSentence(48, TEXT_ROLL);
                    bitSet(player.hasStuff[0], 2);
                    fillWithNumber(28, amount);
                    player.itemsAmount[2] += amount;
                    fillWithWord(11, 99);
                }

                else if(door == 3) // Weapons 1, 2, 3, 4 lastDoor 3, 8, 13, 18
                {
                    fillWithSentence(50, TEXT_ROLL);
                    bitSet(player.hasStuff[2], (amount + 1) / 2);
                    fillWithWord(23, 105 + (amount + 1) / 2);
                }

                else if(door == 4) // Armor 1, 3, 5, 7 lastDoor 4, 9, 14, 19
                {
                    fillWithSentence(51, TEXT_ROLL);
                    bitSet(player.hasStuff[4], amount);
                    fillWithWord(23, 113 + amount);
                }
            }
        } break;
        default:
            fillWithSentence(44, TEXT_ROLL);
            break;
        }
        foundSomething = true;
    }
    if(!foundSomething) openMiniMap();

    investigating = false;
}

#endif
