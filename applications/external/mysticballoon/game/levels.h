#ifndef LEVELS_H
#define LEVELS_H

#include "../lib/Arduino.h"
#include "globals.h"
#include "enemies.h"
//#include "Point.h"
#include "player.h"

#define LSTART  0
#define LFINISH 1 << 5
#define LWALKER 2 << 5
#define LFAN    3 << 5
#define LSPIKES 4 << 5
#define LCOIN   5 << 5
#define LKEY    6 << 5

//char gameGrid[LEVEL_ARRAY_SIZE]; // grid with cell information
// upper byte tile xxxx ____
// LSB solid ____ ___x

// Cell based grid checking
bool gridGetSolid(int8_t x, int8_t y) {
    if(x < 0 || x >= LEVEL_WIDTH_CELLS) return 1;

    if(y < 0 || y >= LEVEL_HEIGHT_CELLS) return 0;

    const uint8_t* lvl = levels[level];
    byte b = pgm_read_byte(lvl + (x >> 3) + (y * (LEVEL_WIDTH_CELLS >> 3)));
    return ((b >> (x % 8)) & 0x01);
}

byte gridGetTile(int8_t x, int8_t y) {
    //if (!gridGetSolid(x, y)) return 0;
    if(!gridGetSolid(x, y)) return 16;
    //if (x < 0 || x >= LEVEL_WIDTH || y < 0 || y >= LEVEL_HEIGHT || !gridGetSolid(x, y))
    //return 0;
    //return gameGrid[x + (y * LEVEL_WIDTH_CELLS)] >> 4;
    byte l, r, t, b, f;
    l = gridGetSolid(x - 1, y);
    t = gridGetSolid(x, y - 1);
    r = gridGetSolid(x + 1, y);
    b = gridGetSolid(x, y + 1);

    f = 0;
    f = r | (t << 1) | (l << 2) | (b << 3);

    return f;

    /*f = 0;
  f |= t << 3;
  f |= l << 2;
  f |= r << 1;
  f |= b;

  switch (f) {
    case 3: i = 1; break;
    case 7: i = 2; break;
    case 5: i = 3; break;
    case 11: i = 4; break;
    case 15: i = 5; break; // solid all around
    case 13: i = 6; break;
    case 10: i = 7; break;
    case 14: i = 8; break;
    case 12: i = 9; break;
    default: i = 10;
  }

  return i;*/
}

void levelLoad(const uint8_t* lvl) {
    byte i = 0;
    lvl += LEVEL_ARRAY_SIZE >> 3;

    byte b = pgm_read_byte(lvl);
    while(b != 0xFF) {
        byte id, x, y;
        id = pgm_read_byte(lvl + i) & 0xE0;
        y = (pgm_read_byte(lvl + i++) & 0x1F);
        x = pgm_read_byte(lvl + i++) & 0x1F;
        switch(id) {
        case LSTART: {
            // Start
            startPos.x = (int)x << (FIXED_POINT + 4);
            startPos.y = (int)y << (FIXED_POINT + 4);
            //kid.actualpos.x = (int)x << (FIXED_POINT + 4);
            //kid.actualpos.y = (int)y << (FIXED_POINT + 4);
            kid.actualpos = startPos;
        } break;
        case LFINISH: {
            // Finish
            levelExit.x = x << 4;
            levelExit.y = y << 4;
        } break;
        case LWALKER: {
            // Walker
            walkersCreate(vec2(x, y));
        } break;
        case LFAN: {
            // Fan
            byte t = pgm_read_byte(lvl + i++);
            if(t < 64)
                fansCreate(vec2(x, y), t);
            else if(t < 192)
                fansCreate(vec2(x, y), t & 0x3F, FAN_RIGHT);
            else
                fansCreate(vec2(x, y), t & 0x3F, FAN_LEFT);
        } break;
        case LSPIKES: {
            // Spikes
            spikesCreate(vec2(x, y), pgm_read_byte(lvl + (i - 1)) >> 5);
        } break;
        case LCOIN: {
            // Coins
            coinsCreate(vec2(x, y));
        } break;
        default: //case LKEY:
        {
            // Key
            keyCreate(vec2(x, y));
        } break;
            //default:
            //break;
        }

        b = pgm_read_byte(lvl + i);
    }
}

void drawGrid() {
    //Serial.println("Start of tile drawing");
    int spacing = 16;
    //for (int x = 0; x < 9; ++x) {
    //for (int y = 0; y < 5; ++y) {
    for(byte x = 8; x < 9; --x) {
        for(byte y = 5; y < 6; --y) {
            //sprites.drawSelfMasked(x * spacing - (cam.pos.x >> 2) % spacing, (int)y * spacing - (cam.pos.y >> 2) % spacing, tileSetTwo, 16);
            sprites.drawSelfMasked(
                x * spacing - (cam.pos.x >> 2) % spacing,
                (int)y * spacing - ((cam.pos.y + 64) >> 2) % spacing,
                tileSetTwo,
                16);
        }
    }
    for(int x = (cam.pos.x >> 4); x <= (cam.pos.x >> 4) + 8; ++x) {
        for(int y = (cam.pos.y >> 4); y <= (cam.pos.y >> 4) + 4; ++y) {
            //if (x >= 0 && x < LEVEL_WIDTH
            //&& y >= 0 && y < LEVEL_HEIGHT)
            {
                /*int index = (i.y * LEVEL_WIDTH_CELLS) + i.x;
          i = i << 4;
          i -= cam.pos >> FIXED_POINT;*/
                //        Serial.print("Pos: ");
                //        Serial.print(x);
                //        Serial.print(", ");
                //        Serial.print(y);
                //        Serial.print(" Tile: ");
                //        Serial.print(gridGetTile(x, y));
                //        Serial.print("\n");
                byte tile = gridGetTile(x, y);
                if(tile != 16)
                    sprites.drawOverwrite(
                        (x << 4) - cam.pos.x, (y << 4) - cam.pos.y, tileSetTwo, tile);
            }
        }
    }
    //sprites.drawPlusMask(levelExit.x - cam.pos.x, levelExit.y - cam.pos.y, sprDoor, walkerFrame);
    //byte frame = 0;
    //if (key.haveKey) frame = walkerFrame + 1;
    //sprites.drawPlusMask(levelExit.x - cam.pos.x, levelExit.y - cam.pos.y, sprDoor, (walkerFrame + 1) * (key.haveKey));
    int commonx = levelExit.x - cam.pos.x;
    int commony = levelExit.y - cam.pos.y;
    //sprites.drawSelfMasked(commonx, commony, largeMask, 0);
    //sprites.drawErase(commonx, commony, sprDoor, (walkerFrame + 1) * (key.haveKey));
    sprites.drawOverwrite(commonx, commony, door, (key.haveKey));
    //Serial.println("End of tile drawing");
}

void windNoise() {
    if(arduboy.everyXFrames(2)) sound.tone(320 + random(20), 30);
}

void kidHurt() {
    if(kid.balloons == 0) return;
    /*if (kid.balloons == 1)
  {
    // dead
    gameState = STATE_GAME_OVER;
  }
  else
  {*/
    kid.isBalloon = false;
    kid.balloons--;
    sound.tone(420, 100);
    kid.isImune = true;
    kid.imuneTimer = 0;
    //}
}

void checkCollisions() {
    if(kid.balloons == 0) return;

    HighRect playerRect = {.x = kid.pos.x + 2, .y = kid.pos.y + 2, .width = 8, .height = 12};
    HighRect playerSuckRect = {
        .x = kid.pos.x + ((kid.direction ^ 1) * 16) - (kid.direction * 16),
        .y = kid.pos.y + 2,
        .width = 16,
        .height = 14};

    // Key
    HighRect keyRect = {.x = key.pos.x, .y = key.pos.y, .width = 8, .height = 16};
    if(collide(keyRect, playerRect) && key.active) {
        key.active = false;
        key.haveKey = true;
        sound.tone(420, 200);
    }

    // Level exit
    HighRect exitRect = {.x = levelExit.x + 4, .y = levelExit.y, .width = 8, .height = 16};
    if(collide(exitRect, playerRect) && arduboy.justPressed(UP_BUTTON) && key.haveKey) {
        balloonsLeft = kid.balloons;
        scoreIsVisible = true;
        canPressButton = false;
        level++;
        gameState = STATE_GAME_NEXT_LEVEL;
    }

    // Enemies and objects
    //for (byte i = 0; i < MAX_PER_TYPE; ++i)
    for(byte i = MAX_PER_TYPE - 1; i < MAX_PER_TYPE; --i) {
        // Coins
        if(coins[i].active) {
            HighRect coinrect = {
                .x = coins[i].pos.x, .y = coins[i].pos.y, .width = 10, .height = 12};
            if(kid.isSucking && collide(playerSuckRect, coinrect)) {
                // Suck coin closer
                if(kid.direction)
                    ++coins[i].pos.x;
                else
                    --coins[i].pos.x;
            } else if(collide(playerRect, coinrect)) {
                // Collect coin
                coins[i].active = false;
                --coinsActive;
                ++coinsCollected;
                ++totalCoins;
                sound.tone(400, 200);
                if(coinsActive == 0) {
#ifndef HARD_MODE
                    scorePlayer += 500;
#else
                    scorePlayer += 1000;
#endif
                    //sound.tone(400, 200);
                } else {
#ifndef HARD_MODE
                    scorePlayer += 200;
#else
                    scorePlayer += 400;
#endif
                    //sound.tone(370, 200);
                }
            }
        }
        // Walkers
        // Getting Sucked In
        if(walkers[i].active) {
            HighRect walkerrect = {
                .x = walkers[i].pos.x, .y = walkers[i].pos.y, .width = 8, .height = 8};
            if(collide(playerSuckRect, walkerrect) && kid.isSucking) {
                --walkers[i].HP;
                walkers[i].hurt = true;
                if(walkers[i].HP <= 0) {
                    if(kid.direction) {
                        ++walkers[i].pos.x;
                        if(walkers[i].pos.x > kid.pos.x - 8) {
                            walkers[i].active = false;
                            if(kid.balloons < 3)
                                ++kid.balloons;
                            else
                                scorePlayer += 100;
                            scorePlayer += 50;
                            sound.tone(200, 100);
                        }
                    } else {
                        --walkers[i].pos.x;
                        if(walkers[i].pos.x < kid.pos.x + 16) {
                            walkers[i].active = false;
                            if(kid.balloons < 3)
                                ++kid.balloons;
                            else
                                scorePlayer += 100;
                            scorePlayer += 50;
                            sound.tone(200, 100);
                        }
                    }
                }
            } else
                walkers[i].hurt = false;

            // Hurt player
            if(collide(playerRect, walkerrect) && walkers[i].HP > 0 && !kid.isImune) {
                kidHurt();
                kid.speed.y = PLAYER_JUMP_VELOCITY;
                kid.speed.x = max(min((kid.pos.x - walkers[i].pos.x - 2), 3), -3) << FIXED_POINT;
            }
        }
        // Fans
        if(fans[i].active) {
            HighRect fanrect;
            switch(fans[i].dir) {
            case FAN_UP:
                fanrect.x = fans[i].pos.x;
                fanrect.y = fans[i].pos.y - fans[i].height;
                fanrect.width = 16;
                fanrect.height = fans[i].height;
                break;
            case FAN_RIGHT:
                fanrect.x = fans[i].pos.x + 16;
                fanrect.y = fans[i].pos.y;
                fanrect.width = fans[i].height;
                fanrect.height = 16;
                break;
            default:
                fanrect.x = fans[i].pos.x - fans[i].height;
                fanrect.y = fans[i].pos.y;
                fanrect.width = fans[i].height;
                fanrect.height = 16;
            }
            /*HighRect fanrect = {.x = fans[i].pos.x, .y = fans[i].pos.y - (fans[i].height),
                      .width = 16, .height = fans[i].height
                     };*/
            if(collide(playerRect, fanrect) && kid.isBalloon) {
                switch(fans[i].dir) {
                case FAN_UP:
                    kid.speed.y = min(kid.speed.y + FAN_POWER, MAX_YSPEED);
                    break;
                case FAN_RIGHT:
                    kid.speed.x = min(kid.speed.x + FAN_POWER, MAX_XSPEED_FAN);
                    break;
                default:
                    kid.speed.x = max(kid.speed.x - FAN_POWER, -MAX_XSPEED_FAN);
                }
                //kid.speed.y = min(kid.speed.y + FAN_POWER, MAX_YSPEED);
                //if (arduboy.everyXFrames(3)) sound.tone(330 + random(20), 30);
                windNoise();
                //kid.actualpos.y -= FAN_POWER;
            }
        }

        // Spikes
        if(!kid.isImune && bitRead(spikes[i].characteristics, 2) &&
           collide(playerRect, spikes[i].pos)) {
            kidHurt();
            if(kid.pos.y < spikes[i].pos.y) kid.speed.y = PLAYER_JUMP_VELOCITY;
        }
    }
}

void drawHUD() {
    //for (byte i = 0; i < 16; i++)
    for(byte i = 15; i < 16; --i) {
        sprites.drawSelfMasked(i * 8, 0, smallMask, 0);
    }
    drawBalloonLives();
    drawNumbers(91, 0, FONT_SMALL, DATA_SCORE);
    //if (coinsCollected < 6 || walkerFrame == 0)
    drawCoinHUD();
    if(key.haveKey) sprites.drawOverwrite(28, 0, elementsHUD, 13);
}
#endif
