#ifndef GAME_H
#define GAME_H

#include "globals.h"
#include "inputs.h"
#include "inventory.h"
#include "items.h"
#include "worlddata.h"
#include "battles.h"
#include "people.h"

void drawMap() {
    if(bitRead(player.gameTriggers[3], 3)) {
        if(player.currentRegion > 12) {
            fillWithSentence(42, TEXT_ROLL);
            drawTextBox(7, 29, BLACK);
        } else {
            //for (byte miniMapOnScreenX = 0; miniMapOnScreenX <= 128; miniMapOnScreenX += 8)
            for(byte miniMapOnScreenX = 128; miniMapOnScreenX <= 128; miniMapOnScreenX -= 8) {
                //for (byte miniMapOnScreenY = 0; miniMapOnScreenY <= 64; miniMapOnScreenY += 8)
                for(byte miniMapOnScreenY = 64; miniMapOnScreenY <= 64; miniMapOnScreenY -= 8) {
                    byte miniMapTile = 0;
                    byte chunk_pos_x = (miniCamX + miniMapOnScreenX) >> 3;
                    byte chunk_pos_y = (miniCamY + miniMapOnScreenY) >> 3;
                    if(getMapFog(chunk_pos_x, chunk_pos_y)) {
                        byte testRegion;
                        miniMapTile = 8;
                        // Regional chunk
                        if(getChunk(chunk_pos_x, chunk_pos_y) < 128) {
                            if(getChunkBit(chunk_pos_x, chunk_pos_y)) {
                                testRegion = getRegion(chunk_pos_x, chunk_pos_y);
                                /*if (testRegion < 4) miniMapTile = 2;
                else if (testRegion < 6) miniMapTile = 3;
                else if (testRegion < 10) miniMapTile = 4;
                else if (testRegion == 10) miniMapTile = 3;
                else if (testRegion == 11) miniMapTile = 2;
                else if (testRegion == 12) miniMapTile = 1;*/
                                switch(testRegion) {
                                case 4:
                                case 5:
                                case 10:
                                    miniMapTile = 3;
                                    break;
                                case 6:
                                case 7:
                                case 8:
                                case 9:
                                    miniMapTile = 4;
                                    break;
                                case 12:
                                    miniMapTile = 1;
                                    break;
                                default:
                                    miniMapTile = 2;
                                }
                            }
                            //else miniMapTile = 8;
                        }
                        // Specific chunk
                        else {
                            testRegion = getChunk(chunk_pos_x, chunk_pos_y);
                            if(testRegion == 170)
                                miniMapTile = 5;
                            else if(testRegion == 172)
                                miniMapTile = 6;
                            else if(testRegion == 146)
                                miniMapTile = 8;
                            else
                                miniMapTile = 7;
                        }
                    }
                    sprites.drawOverwrite(
                        (((miniCamX >> 3) << 3) + miniMapOnScreenX) - miniCamX,
                        (((miniCamY >> 3) << 3) + miniMapOnScreenY) - miniCamY,
                        miniMapSheet,
                        miniMapTile);
                }
            }
            // Player
            sprites.drawOverwrite(
                (playerReducedX * 8) - miniCamX,
                (playerReducedY * 8) - miniCamY,
                miniMapPlayer,
                globalFrame % 4);
        }
    } else {
        fillWithSentence(5, TEXT_ROLL);
        drawTextBox(16, 29, BLACK);
    }
}

void stateGameNew() {
    arduboy.fillScreen(1);
    byte alphabetX = 12;
    byte alphabetY = 17;
    for(byte i = 0; i < 36; i++) {
        sprites.drawErase(alphabetX, alphabetY, font, i + 1);
        alphabetX += 12;
        if(alphabetX > 119) {
            alphabetY += 10;
            alphabetX = 12;
        }
    }
    fillWithSentence(3);
    fillWithName(13);
    drawTextBox(12, 3, BLACK);

    sprites.drawErase(6 + (cursorX * 12), 17 + (cursorY * 10), font, 44);
}

void stateGameIntro() {
    fillWithSentence(37 + fadeCounter, TEXT_ROLL);
    if(fadeCounter == 2) fillWithName(23);

    if(fadeCounter < 4) {
        arduboy.fillScreen(0);
        drawRectangle(0, 48, 130, 64, BLACK);
        drawTextBox(4, 50, WHITE);
    } else {
        showFadeOutIn();
        if(arduboy.everyXFrames(6)) fadeCounter++;
        if(fadeCounter > 7) {
            fadeCounter = 0;
            gameState = STATE_GAME_PLAYING;
        }
    }
    drawPlayer();
    checkCam();
}

void stateGamePlaying() {
    //drawTiles();
    previousGameState = STATE_GAME_PLAYING;
    updatePeople();
    updatePlayer();
    checkRegion();
    checkBattle();
    //if (arduboy.everyXFrames(15)) frameBoolean = !frameBoolean;
    frameBoolean = (arduboy.frameCount() % 32 < 16);
    drawPeople();
    drawPlayer();
    // Reveal map area around player
    discoverMap(player.x, player.y);
    checkDoors();
    checkCam();
    //int investigate_x = player.x - 3 + pgm_read_byte(&collisionPoints[8 + playerDirection][0]);
    //int investigate_y = player.y - 1 + pgm_read_byte(&collisionPoints[8 + playerDirection][1]);
    int investigate_x = player.x - 3 + collisionPoints[8 + playerDirection][0];
    int investigate_y = player.y - 1 + collisionPoints[8 + playerDirection][1];
    if(talkingWithNPC) investigatePeople(investigate_x, investigate_y);
    if(investigating) investigateObjects(getTileID(investigate_x, investigate_y));

    if(foundSomething) {
        drawRectangle(0, 48, 130, 64, BLACK);
        drawTextBox(4, 50, WHITE);
    }
}

void stateGameInventory() {
    drawPlayer();
    drawRectangle(83, 0, 130, 64, BLACK);
    fillWithSentence(4); // ITEMS EQUIP STATS MAP EXTRA
    drawTextBox(93, 5, WHITE);
    sprites.drawSelfMasked(86, 5 + (cursorY * 12), font, 44);
}

void stateGameEquip() {
    drawRectangle(83, 0, 130, 64, BLACK);
    fillWithSentence(10); // WEAPON ARMOR AMULET
    drawTextBox(93, 5, WHITE);
    sprites.drawSelfMasked(86, 5 + (cursorY * 12), font, 44);
}

void stateGameStats() {
    arduboy.fillScreen(0);
    drawPlayerStats();
}

void stateGameObjects() {
    arduboy.fillScreen(0);
    drawPlayerObjects();
}

void stateGameMap() {
    arduboy.fillScreen(1);
    drawMap();
}

void stateGameSaveSoundEnd() {
    drawRectangle(0, 48, 130, 64, BLACK);
    fillWithSentence(gameState + 36, TEXT_ROLL);
    drawTextBox(4, 50, WHITE);
    yesNo = true;
}

void showSubMenuStuff() {
    arduboy.fillScreen(1);
    drawRectangle(0, 0, 130, 8, BLACK);
    drawRectangle(0, 47, 130, 64, BLACK);

    drawList();

    if(player.hasStuff[(2 * (gameState - STATE_GAME_ITEMS))])
        sprites.drawErase(5, 9 + (6 * cursorY), font, 44);

    fillWithWord(0, 93 + (gameState - STATE_GAME_ITEMS));
    drawTextBox(4, 0, WHITE);
}

void walkingThroughDoor() {
    showFadeOutIn();
    //if (arduboy.everyXFrames(6)) fadeCounter++;
    if(arduboy.frameCount() % 8 == 0) ++fadeCounter;
    if(counterDown == false && fadeCounter == 4) {
        counterDown = true;
        if(playerDirection == FACING_SOUTH) {
            player.x = (pgm_read_byte(&doors[player.lastDoor * 2]) * 16);
            player.y = (pgm_read_byte(&doors[(player.lastDoor * 2)] + 1) * 16) + 12;
        } else {
            if((getChunk(playerReducedX, playerReducedY) & 0x7F) == 34)
                player.x = 336;
            else
                player.x =
                    (720 + (384 * ((getChunk(playerReducedX, playerReducedY) & 0x7F) - 42)));
            player.y = 3048;
        }
        checkCam();
    } else if(fadeCounter > 6) {
        fadeCounter = 0;
        playerSteps = 0;
        gameState = STATE_GAME_PLAYING;
        counterDown = false;
    }
}

/*
 * Talking to shop keeper.
 */
void stateGameShop() {
    arduboy.fillScreen(1);
    drawRectangle(0, 0, 130, 8, BLACK);
    drawRectangle(0, 57, 130, 64, BLACK);

    drawShop();

    sprites.drawErase(5, 9 + (6 * cursorY), font, 44);

    fillWithWord(0, 194);
    drawTextBox(4, 0, WHITE);
}

/*
 * Battle End function leads here when player health is 0
 */
void stateGameOver() {
    arduboy.fillScreen(1);
    sprites.drawErase(56, 30, playerDead, 0);
    fillWithSentence(43, TEXT_ROLL);
    drawTextBox(37, 16, BLACK);
}

/*
 * Talk to inn keeper
 */
void stateGameInn() {
    player.health = player.healthTotal;
    player.magic = player.magicTotal;
    if(talkingWithNPC) {
        drawRectangle(0, 48, 130, 64, BLACK);
        fillWithSentence(82, TEXT_ROLL); // you need rest
        drawTextBox(4, 50, WHITE);
        foundSomething = false;
    } else {
        showFadeOutIn();
        //if (arduboy.everyXFrames(12)) fadeCounter++;
        if(arduboy.frameCount() % 16 == 0) ++fadeCounter;
    }
    if(fadeCounter > 6) gameState = STATE_GAME_PLAYING;
}

#endif
