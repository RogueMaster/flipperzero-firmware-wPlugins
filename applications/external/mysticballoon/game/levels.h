#ifndef LEVELS_H
#define LEVELS_H

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
// upper uint8_t tile xxxx ____
// LSB solid ____ ___x

// Cell based grid checking
bool gridGetSolid(int8_t x, int8_t y) {
    if(x < 0 || x >= LEVEL_WIDTH_CELLS) return 1;

    if(y < 0 || y >= LEVEL_HEIGHT_CELLS) return 0;

    const uint8_t* lvl = levels[level];
    uint8_t b = *(lvl + (x >> 3) + (y * (LEVEL_WIDTH_CELLS >> 3)));
    return ((b >> (x % 8)) & 0x01);
}

uint8_t gridGetTile(int8_t x, int8_t y) {
    //if (!gridGetSolid(x, y)) return 0;
    if(!gridGetSolid(x, y)) return 16;
    //if (x < 0 || x >= LEVEL_WIDTH || y < 0 || y >= LEVEL_HEIGHT || !gridGetSolid(x, y))
    //return 0;
    //return gameGrid[x + (y * LEVEL_WIDTH_CELLS)] >> 4;
    uint8_t l, r, t, b, f;
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
    uint8_t i = 0;
    lvl += LEVEL_ARRAY_SIZE >> 3;

    uint8_t b = *(lvl);
    while(b != 0xFF) {
        uint8_t id, x, y;
        id = *(lvl + i) & 0xE0;
        y = (*(lvl + i++) & 0x1F);
        x = *(lvl + i++) & 0x1F;
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
            uint8_t t = *(lvl + i++);
            if(t < 64)
                fansCreate(vec2(x, y), t);
            else if(t < 192)
                fansCreate(vec2(x, y), t & 0x3F, FAN_RIGHT);
            else
                fansCreate(vec2(x, y), t & 0x3F, FAN_LEFT);
        } break;
        case LSPIKES: {
            // Spikes
            spikesCreate(vec2(x, y), *(lvl + (i - 1)) >> 5);
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

        b = *(lvl + i);
    }
}

void drawGrid() {
    //Serial.println("Start of tile drawing");
    int spacing = 16;
    //for (int x = 0; x < 9; ++x) {
    //for (int y = 0; y < 5; ++y) {
    for(uint8_t x = 8; x < 9; --x) {
        for(uint8_t y = 5; y < 6; --y) {
            //gfx_sprite_self_masked(x * spacing - (cam.pos.x >> 2) % spacing, (int)y * spacing - (cam.pos.y >> 2) % spacing, tileSetTwo, 16);
            gfx_sprite_self_masked(
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
                uint8_t tile = gridGetTile(x, y);
                if(tile != 16)
                    gfx_sprite_overwrite(
                        (x << 4) - cam.pos.x, (y << 4) - cam.pos.y, tileSetTwo, tile);
            }
        }
    }
    //gfx_sprite_plus_mask(levelExit.x - cam.pos.x, levelExit.y - cam.pos.y, sprDoor, walkerFrame);
    //uint8_t frame = 0;
    //if (key.haveKey) frame = walkerFrame + 1;
    //gfx_sprite_plus_mask(levelExit.x - cam.pos.x, levelExit.y - cam.pos.y, sprDoor, (walkerFrame + 1) * (key.haveKey));
    int commonx = levelExit.x - cam.pos.x;
    int commony = levelExit.y - cam.pos.y;
    //gfx_sprite_self_masked(commonx, commony, largeMask, 0);
    //gfx_sprite_erase(commonx, commony, sprDoor, (walkerFrame + 1) * (key.haveKey));
    gfx_sprite_overwrite(commonx, commony, door, (key.haveKey));
    //Serial.println("End of tile drawing");
}

void windNoise() {
    if(everyXFrames(2)) playTone(320 + randomOf(20), 30);
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
    playTone(420, 100);
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
        playTone(420, 200);
    }

    // Level exit
    HighRect exitRect = {.x = levelExit.x + 4, .y = levelExit.y, .width = 8, .height = 16};
    if(collide(exitRect, playerRect) && justPressed(MYBL_UP) && key.haveKey) {
        balloonsLeft = kid.balloons;
        scoreIsVisible = true;
        canPressButton = false;
        level++;
        gameState = STATE_GAME_NEXT_LEVEL;
    }

    // Enemies and objects
    //for (uint8_t i = 0; i < MAX_PER_TYPE; ++i)
    for(uint8_t i = MAX_PER_TYPE - 1; i < MAX_PER_TYPE; --i) {
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
                playTone(400, 200);
                if(coinsActive == 0) {
#ifndef HARD_MODE
                    scorePlayer += 500;
#else
                    scorePlayer += 1000;
#endif
                    //playTone(400, 200);
                } else {
#ifndef HARD_MODE
                    scorePlayer += 200;
#else
                    scorePlayer += 400;
#endif
                    //playTone(370, 200);
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
                            playTone(200, 100);
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
                            playTone(200, 100);
                        }
                    }
                }
            } else
                walkers[i].hurt = false;

            // Hurt player
            if(collide(playerRect, walkerrect) && walkers[i].HP > 0 && !kid.isImune) {
                kidHurt();
                kid.speed.y = PLAYER_JUMP_VELOCITY;
                kid.speed.x = maxOf(minOf((kid.pos.x - walkers[i].pos.x - 2), 3), -3)
                              << FIXED_POINT;
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
                    kid.speed.y = minOf(kid.speed.y + FAN_POWER, MAX_YSPEED);
                    break;
                case FAN_RIGHT:
                    kid.speed.x = minOf(kid.speed.x + FAN_POWER, MAX_XSPEED_FAN);
                    break;
                default:
                    kid.speed.x = maxOf(kid.speed.x - FAN_POWER, -MAX_XSPEED_FAN);
                }
                //kid.speed.y = minOf(kid.speed.y + FAN_POWER, MAX_YSPEED);
                //if (everyXFrames(3)) playTone(330 + randomOf(20), 30);
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
    //for (uint8_t i = 0; i < 16; i++)
    for(uint8_t i = 15; i < 16; --i) {
        gfx_sprite_self_masked(i * 8, 0, smallMask, 0);
    }
    drawBalloonLives();
    drawNumbers(91, 0, FONT_SMALL, DATA_SCORE);
    //if (coinsCollected < 6 || walkerFrame == 0)
    drawCoinHUD();
    if(key.haveKey) gfx_sprite_overwrite(28, 0, elementsHUD, 13);
}
#endif
