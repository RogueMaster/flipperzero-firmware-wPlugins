#ifndef INPUTS_H
#define INPUTS_H

#include "globals.h"
#include "player.h"
#include "worlddata.h"
#include "items.h"
#include "battles.h"
#include "people.h"

extern bool textReset;

bool checkPlayerCollision(byte orientation) {
    uint16_t pointOne = player.x + collisionPoints[2 * orientation][0];
    uint16_t pointTwo = player.y + collisionPoints[2 * orientation][1];
    uint16_t pointThree = player.x + collisionPoints[(2 * orientation) + 1][0];
    uint16_t pointFour = player.y + collisionPoints[(2 * orientation) + 1][1];
    if(!getSolid(pointOne, pointTwo) && !getSolid(pointThree, pointFour) &&
       !getCollisionPeople(pointOne, pointTwo) && !getCollisionPeople(pointThree, pointFour)) {
        playerWalking = true;
        if(player.currentRegion < REGION_HOUSE_INTERIOR) playerSteps++;
        return true;
    }
    return false;
}

void buttonsUpDownA() {
    if(arduboy.justPressed(UP_BUTTON))
        cursorYesNoY = true;
    else if(arduboy.justPressed(DOWN_BUTTON | A_BUTTON))
        cursorYesNoY = false;
}

void checkInputs() {
    switch(gameState) {
    case STATE_MENU_INTRO:
        if(arduboy.justPressed(A_BUTTON | B_BUTTON)) {
            globalCounter = 0;
            gameState = STATE_MENU_MAIN;
            if(EEPROM.read(EEPROM_START) == GAME_ID) firstGame = false;
            cursorY = STATE_MENU_CONTINUE + firstGame;
            //textspeed = TEXT_ROLL_DELAY;
        }
        break;

    case STATE_MENU_MAIN:
        if(arduboy.justPressed(UP_BUTTON) && (cursorY > 2 + firstGame))
            cursorY--;
        else if(arduboy.justPressed(DOWN_BUTTON) && (cursorY < 6))
            cursorY++;
        else if(arduboy.justPressed(A_BUTTON)) {
            // Back button exits the main loop from the menu.
            arduboy.exitToBootloader();
        } else if(arduboy.justPressed(B_BUTTON)) {
            if(cursorY == 6)
                arduboy.exitToBootloader();
            else
                gameState = cursorY;
            clearCursor();
        }
        break;

    case STATE_MENU_CREDITS:
        if(arduboy.justPressed(B_BUTTON)) {
            gameState = STATE_MENU_MAIN;
            cursorY = STATE_MENU_CONTINUE + firstGame;
        }
        break;

    case STATE_GAME_NEW:
        if(arduboy.justPressed(UP_BUTTON))
            cursorY += (cursorY > 0) ? -1 : 4;
        else if(arduboy.justPressed(DOWN_BUTTON))
            cursorY += (cursorY < 4) ? 1 : -4;
        else if(arduboy.justPressed(LEFT_BUTTON))
            cursorX += (cursorX > 0) ? -1 : 8;
        else if(arduboy.justPressed(RIGHT_BUTTON))
            cursorX += (cursorX < 8) ? 1 : -8;
        if(currentLetter == 5) cursorY = 4;
        if(cursorY == 4) cursorX = 7;
        if(arduboy.justPressed(B_BUTTON)) {
            byte selectedLetter = cursorX + (cursorY * 9) + 1;
            if(selectedLetter > 36) {
                player.name[0] = currentLetter;
                currentLetter = 0;
                clearCursor();
                gameState = STATE_GAME_INTRO;
                ATM.play(badNews);
                return;
            } else {
                player.name[currentLetter + 1] = selectedLetter;
                currentLetter = min(currentLetter + 1, 5);
            }
        }

        else if(arduboy.justPressed(A_BUTTON)) {
            if(currentLetter > 0) {
                player.name[currentLetter] = 50;
                currentLetter--;
            } else {
                gameState = STATE_MENU_INTRO;
                globalCounter = 255;
            }
        }
        break;

    case STATE_GAME_INTRO:
        if(arduboy.justPressed(B_BUTTON)) {
            if(fadeCounter < 4 && textReset) {
                fadeCounter++;
                playerDirection = 0;
            }
        }
        break;

    case STATE_GAME_PLAYING:
        playerWalking = false;
        if(!investigating && !foundSomething && !talkingWithNPC) {
            if(arduboy.pressed(DOWN_BUTTON) &&
               checkPlayerCollision(playerDirection = FACING_SOUTH))
                player.y++;
            else if(
                arduboy.pressed(LEFT_BUTTON) &&
                checkPlayerCollision(playerDirection = FACING_WEST))
                player.x--;
            else if(arduboy.pressed(UP_BUTTON) && checkPlayerCollision(playerDirection = FACING_NORTH))
                player.y--;
            else if(
                arduboy.pressed(RIGHT_BUTTON) &&
                checkPlayerCollision(playerDirection = FACING_EAST))
                player.x++;
            if(arduboy.justPressed(A_BUTTON))
                gameState = STATE_GAME_INVENTORY;
            else if(arduboy.justPressed(B_BUTTON)) {
                investigating = true;
                talkingWithNPC = true;
            }
        } else if(arduboy.justPressed(A_BUTTON | B_BUTTON) && textReset)
            foundSomething = false;
        break;

    case STATE_GAME_INVENTORY:
        if(arduboy.justPressed(UP_BUTTON) && (cursorY > 0))
            cursorY--;
        else if(arduboy.justPressed(DOWN_BUTTON) && (cursorY < 4))
            cursorY++;
        else if(arduboy.justPressed(A_BUTTON)) {
            gameState = STATE_GAME_PLAYING;
            textBox[0] = 0;
        } else if(arduboy.justPressed(B_BUTTON)) {
            switch(cursorY) {
            case 0:
                gameState = STATE_GAME_ITEMS;
                break;
            case 3:
                openMiniMap();
                break;
            case 4:
                gameState = STATE_GAME_SAVE;
                break;
            default:
                gameState = STATE_GAME_ITEMS - 5 + cursorY;
                //miniCamX = (playerReducedX * 8) - 64;
                //miniCamY = (playerReducedY * 8) - 32;

                /*miniCamX = ((int(playerReducedX * 8) - 64) < 0) ? 0 : byte(int(playerReducedX * 8) - 64);
            if (miniCamX > 128) miniCamX = 128;
            miniCamY = ((int(playerReducedY * 8) - 32) < 0) ? 0 : byte(int(playerReducedY * 8) - 32);
            if (miniCamY > 176) miniCamY = 176;*/
                //miniCamX = (playerReducedX * 8) - 64;
                //miniCamY = (playerReducedY * 8) - 32;
                //miniCamX %= 129;
                //miniCamY %= 178;
                break;
            }
            clearCursor();
        }
        break;
    case STATE_GAME_EQUIP:
        if(arduboy.justPressed(UP_BUTTON) && (cursorY > 0))
            cursorY--;
        else if(arduboy.justPressed(DOWN_BUTTON) && (cursorY < 2))
            cursorY++;
        else if(arduboy.justPressed(A_BUTTON)) {
            gameState = STATE_GAME_INVENTORY;
            cursorY = 1;
        } else if(arduboy.justPressed(B_BUTTON)) {
            gameState = STATE_GAME_WEAPON + cursorY;
            clearCursor();
        }
        break;
    case STATE_GAME_STATS:
        if(arduboy.justPressed(A_BUTTON | B_BUTTON)) gameState = STATE_GAME_OBJECTS;
        break;
    case STATE_GAME_OBJECTS:
        if(arduboy.justPressed(A_BUTTON | B_BUTTON)) {
            gameState = STATE_GAME_INVENTORY;
            cursorY = 2;
        }
        break;
    case STATE_GAME_SHOP:
        if(!yesNo) {
            if(arduboy.justPressed(A_BUTTON)) {
                gameState = STATE_GAME_PLAYING;
                question = false;
            } else if(arduboy.justPressed(UP_BUTTON) && (cursorY > 0))
                cursorY--;
            else if(arduboy.justPressed(DOWN_BUTTON) && (cursorY < TOTAL_SHOP_ITEMS - 1))
                cursorY++;
            else if(arduboy.justPressed(B_BUTTON)) {
                if(!question) {
                    question = true;
                    yesNo = true;
                } else {
                    question = false;
                    needMoreMoney = false;
                }
            }
        } else {
            buttonsUpDownA();
            if(arduboy.justPressed(B_BUTTON)) {
                if(cursorYesNoY) // Did player select yes?
                {
                    // yes to buying item
                    buyItem();
                } else
                    question = false;
                yesNo = false;
                cursorYesNoY = true;
            }
        }
        break;
    case STATE_GAME_ITEMS:
    case STATE_GAME_WEAPON:
    case STATE_GAME_ARMOR:
    case STATE_GAME_AMULET:
        if(!yesNo) {
            if(arduboy.justPressed(UP_BUTTON) && (cursorY > 0))
                cursorY--;
            else if(
                (cursorY < bitCount(player.hasStuff[2 * (gameState - STATE_GAME_ITEMS)]) - 1) &&
                arduboy.justPressed(DOWN_BUTTON))
                cursorY++;
            else if(arduboy.justPressed(A_BUTTON)) {
                cursorY = (gameState == STATE_GAME_ITEMS) ? 0 : (gameState - 13);
                gameState = (gameState == STATE_GAME_ITEMS) ? STATE_GAME_INVENTORY :
                                                              STATE_GAME_EQUIP;
                if(previousGameState == STATE_GAME_BATTLE) gameState = STATE_GAME_BATTLE;
            } else if(
                arduboy.justPressed(B_BUTTON) &&
                player.hasStuff[(2 * (gameState - STATE_GAME_ITEMS))]) {
                question = true;
                yesNo = true;
            }
        } else {
            buttonsUpDownA();
            if(arduboy.justPressed(B_BUTTON)) {
                if(cursorYesNoY) {
                    selectItemsEquipment();
                    clearCursor();
                    battleProgress = BATTLE_ENEMY_TURN;
                    playerFirst = true;
                }
                if(previousGameState == STATE_GAME_BATTLE) {
                    gameState = STATE_GAME_BATTLE;
                }
                question = false;
                yesNo = false;
                cursorYesNoY = true;
            }
        }
        break;
    case STATE_GAME_MAP:
        if(arduboy.justPressed(B_BUTTON | A_BUTTON))
            gameState = STATE_GAME_PLAYING;
        else if(arduboy.pressed(LEFT_BUTTON) && miniCamX > 0)
            miniCamX -= 2;
        else if(arduboy.pressed(UP_BUTTON) && miniCamY > 0)
            miniCamY -= 2;
        else if(arduboy.pressed(RIGHT_BUTTON) && miniCamX < 128)
            miniCamX += 2;
        else if(arduboy.pressed(DOWN_BUTTON) && miniCamY < 176)
            miniCamY += 2;
        break;

    case STATE_GAME_SAVE:
        buttonsUpDownA();
        if(arduboy.justPressed(B_BUTTON)) {
            if(cursorYesNoY) saveGame();
            gameState = STATE_GAME_SOUND;
            yesNo = false;
            cursorYesNoY = false;
        }
        break;

    case STATE_GAME_SOUND:
        buttonsUpDownA();
        if(arduboy.justPressed(B_BUTTON)) {
            if(cursorYesNoY) toggleSound();
            gameState = STATE_GAME_END;
            yesNo = false;
            cursorYesNoY = false;
        }
        break;

    case STATE_GAME_END:
        buttonsUpDownA();
        if(arduboy.justPressed(B_BUTTON)) {
            if(cursorYesNoY) {
                gameState = STATE_MENU_MAIN;
                ATM.play(titleSong);
                cursorY = 3;
                //gameState = STATE_GAME_OVER;
                //ATM.play(youDied);
            } else {
                gameState = STATE_GAME_INVENTORY;
                //cursorY = 4;
            }
            yesNo = false;
            cursorYesNoY = true;
        }
        break;

    case STATE_GAME_OVER:
        if(arduboy.justPressed(B_BUTTON)) {
            gameState = STATE_MENU_MAIN;
            ATM.play(titleSong);
            cursorY = STATE_MENU_CONTINUE + firstGame;
        }
        break;
    case STATE_GAME_BATTLE:
        switch(battleProgress) {
        case BATTLE_START:
            if(arduboy.justPressed(LEFT_BUTTON) && cursorX > 0)
                cursorX--;
            else if(arduboy.justPressed(RIGHT_BUTTON) && cursorX < 1)
                cursorX++;
            else if(arduboy.justPressed(UP_BUTTON) && cursorY > 0)
                cursorY--;
            else if(arduboy.justPressed(DOWN_BUTTON) && cursorY < 1)
                cursorY++;
            else if(arduboy.justPressed(B_BUTTON | A_BUTTON)) {
                byte state = cursorX + (2 * cursorY);
                switch(state) {
                case BATTLE_MAGIC:
                    if(player.magic < magiccost) {
                        battleBlink = 0;
                        battleProgress = BATTLE_NOMANA;
                        break;
                    }
                    [[fallthrough]];
                case BATTLE_ATTACK:
                    attackType = state;
                    if(playerFirst)
                        battleProgress = state;
                    else
                        battleProgress = BATTLE_ENEMY_TURN;
                    break;
                default:
                    battleProgress = state;
                    break;
                }
            }
            break;
        case BATTLE_ESCAPE:
            //if (arduboy.justPressed(B_BUTTON | A_BUTTON)) fadeCounter = 7;
            if(textReset) {
                player.gold -= battleRewardNumb[0];
                gameState = STATE_GAME_PLAYING;
            }
            break;
        case BATTLE_ITEMS:
            if(arduboy.justPressed(B_BUTTON | A_BUTTON)) battleProgress = BATTLE_START;
            //fadeCounter++;
            break;
        case BATTLE_NO_ESCAPE:
            //if (arduboy.justPressed(B_BUTTON | A_BUTTON) && textReset) battleProgress = BATTLE_ENEMY_TURN;
            if(textReset) battleProgress = BATTLE_ENEMY_TURN;
            break;
        }
        [[fallthrough]];
    case STATE_GAME_INN:
        if(arduboy.justPressed(B_BUTTON)) {
            talkingWithNPC = false;
        }
        break;
    case STATE_BATTLE_REWARDS:
        if(textReset) ++fadeCounter;
        break;
        //case STATE_GAME_BOSS:
        //break;
    }
}

#endif
