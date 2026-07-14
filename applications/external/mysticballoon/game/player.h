#ifndef PLAYER_H
#define PLAYER_H

#include "../lib/Arduino.h"
#include "globals.h"
//#include "levels.h"
#include "vec2.h"

#define FIXED_POINT          5
#define PLAYER_SPEED_WALKING 1 << FIXED_POINT
#define PLAYER_SPEED_AIR     2
#define PLAYER_PARTICLES     3
#define PLAYER_JUMP_VELOCITY (1 << FIXED_POINT) + 8
#define GRAVITY              3
#define FRICTION             1 // for horizontal speed
#define MAX_XSPEED           PLAYER_SPEED_WALKING
#define MAX_XSPEED_FAN       54
#define MAX_YSPEED           3 * (1 << FIXED_POINT)
#define CAMERA_OFFSET        16

extern bool gridGetSolid(int8_t x, int8_t y);
extern void kidHurt();
extern void windNoise();

struct Camera {
    // x and y are 9.6 signed fixed vec2 values
    vec2 pos;
    vec2 offset;
};

struct Players {
public:
    // x and y are 9.6 signed fixed vec2 values
    vec2 pos;
    vec2 actualpos;
    vec2 speed;
    boolean isActive;
    boolean isImune;
    boolean direction;
    boolean isWalking;
    boolean isJumping;
    boolean isLanding;
    boolean isBalloon;
    boolean jumpLetGo;
    boolean isSucking;
    byte imuneTimer;
    byte jumpTimer;
    byte frame;
    byte balloons;
    byte balloonOffset;
    vec2 particles[PLAYER_PARTICLES];
};

Players kid;
Camera cam;

void setKid() {
    kid.pos.x = 0;
    kid.pos.y = 0;
    kid.actualpos.x = 128;
    kid.actualpos.y = 0;
    kid.speed.x = 0;
    kid.speed.y = 0;
    kid.isActive = true;
    kid.isImune = true;
    kid.imuneTimer = 0;
    kid.jumpTimer = 0;
    kid.direction = FACING_RIGHT;
    kid.isWalking = false;
    kid.isJumping = false;
    kid.isLanding = false;
    kid.isBalloon = false;
    kid.jumpLetGo = true;
    kid.isSucking = false;
#ifndef HARD_MODE
    kid.balloons = 3;
#endif
    kid.balloonOffset = 0;
    for(byte i = 0; i < PLAYER_PARTICLES; ++i)
        kid.particles[i] = vec2(random(16), random(16));
}

void checkKid() {
    if(kid.isImune) {
        if(arduboy.everyXFrames(2)) kid.isActive = !kid.isActive;
        kid.imuneTimer++;
        if(kid.imuneTimer > 60) {
            kid.imuneTimer = 0;
            kid.isImune = false;
            kid.isActive = true;
        }
    }

    if(kid.isWalking || kid.isSucking) {
        if(arduboy.everyXFrames(8)) {
            kid.frame = (kid.frame + 1) % 4;
            if(kid.frame % 2 == 0) sound.tone(150, 20);
        }
    } else
        kid.frame = 0;

    // Kid is moving up
    if(!kid.isBalloon && kid.speed.y > 0) {
        kid.isJumping = true;
        kid.isLanding = false;
        if(!kid.jumpLetGo && kid.jumpTimer > 0) {
            kid.speed.y += GRAVITY + 2;
            kid.jumpTimer--;
        }
    } else if(kid.speed.y < 0) {
        // Kid is moving down
        kid.isJumping = false;
        kid.isLanding = true;
    }

    // Update position---
    // -Solid checking
    int tx = (kid.pos.x + 6) >> 4;
    int ty = (kid.pos.y + 8) >> 4;
    boolean solidbelow = gridGetSolid(tx, (kid.pos.y + 16) >> 4);
    //boolean solidabove = gridGetSolid(tx, (kid.pos.y - 1) >> 4);
    //boolean solidleft = gridGetSolid((kid.pos.x - 1) >> 4, ty);
    //boolean solidright = gridGetSolid((kid.pos.x + 13) >> 4, ty);
    int tx2 = (((kid.actualpos.x + kid.speed.x) >> FIXED_POINT) - 1 + (kid.speed.x > 0) * 14) >> 4;
    boolean solidH = gridGetSolid(tx2, (kid.pos.y + 2) >> 4) ||
                     gridGetSolid(tx2, (kid.pos.y + 13) >> 4);
    int ty2 = (((kid.actualpos.y - kid.speed.y) >> FIXED_POINT) + (kid.speed.y < 0) * 17) >> 4;
    boolean solidV = gridGetSolid((kid.pos.x + 2) >> 4, ty2) ||
                     gridGetSolid((kid.pos.x + 10) >> 4, ty2);

    // Gravity
    if(kid.balloons == 0 || kid.speed.y > 0 || !solidbelow) {
        //kid.speed.y += GRAVITY;
        kid.speed.y = (kid.speed.y > -MAX_YSPEED) ? kid.speed.y - GRAVITY : -MAX_YSPEED;
        if(kid.isBalloon) {
            //cam.offset.y += 3;
            if(kid.balloonOffset > 0)
                kid.balloonOffset -= 2;
            else {
                //kid.speed.y = max(-((8 / kid.balloons) >> 1), kid.speed.y);
                kid.speed.y = max(kid.balloons - 5, kid.speed.y);
            }
        }
    }

    // Kid on ground
    if(kid.balloons > 0 && kid.speed.y <= 0 && (solidV || solidbelow)) {
        if(kid.isLanding) sound.tone(80, 30);
        kid.speed.y = 0;
        kid.speed.x = 0;
        kid.isLanding = false;
        kid.isJumping = false;
        kid.isBalloon = false;
        int8_t ysnap = (((kid.actualpos.y >> FIXED_POINT) + 12) >> 4);
        //if (!gridGetSolid(ysnap, kid.pos.x >> 4))
        kid.actualpos.y = ysnap << (FIXED_POINT + 4);

        // Fall off edge
        //if (abs(((kid.pos.x + 6) % 16) - 8) >= 4)
        //if (!arduboy.pressed(RIGHT_BUTTON) && !arduboy.pressed(LEFT_BUTTON))
        if(!arduboy.pressed(RIGHT_BUTTON | LEFT_BUTTON)) {
            int yy = (kid.pos.y + 16) >> 4;
            bool sl = gridGetSolid((kid.pos.x + 4) >> 4, yy);
            bool sr = gridGetSolid((kid.pos.x + 8) >> 4, yy);
            if(!sl & gridGetSolid((kid.pos.x + 11) >> 4, yy))
                kid.actualpos.x -= FIXED_POINT << 2;
            else if(!sr && gridGetSolid((kid.pos.x) >> 4, yy))
                kid.actualpos.x += FIXED_POINT << 2;
        }
    } else {
        // Friction in air
        if(abs(kid.speed.x) > FRICTION) {
            if(arduboy.everyXFrames(4)) {
                if(kid.speed.x > 0)
                    kid.speed.x -= FRICTION;
                else if(kid.speed.x < 0)
                    kid.speed.x += FRICTION;
            }
        } else {
            kid.speed.x = 0;
        }
    }

    // Move out of walls
    if(!gridGetSolid(tx, ty)) {
        if(gridGetSolid((kid.pos.x) >> 4, ty))
            kid.actualpos.x += 8;
        else if(gridGetSolid((kid.pos.x + 11) >> 4, ty))
            kid.actualpos.x -= 8;
    }

    if(kid.balloons == 0 || (!solidV && kid.speed.y != 0)) {
        kid.actualpos.y -= kid.speed.y;
    } else {
        if(solidV && kid.speed.y > 0) {
            kid.actualpos.y = ((kid.pos.y + 8) >> 4) << (FIXED_POINT + 4);
            kid.speed.y = 0;
            sound.tone(80, 30);
        }
    }

    // -X Position
    //if ((kid.speed.x < 0 && !solidH) || (kid.speed.x > 0 && !solidH))
    if(kid.speed.x != 0) {
        if(!solidH) {
            kid.actualpos.x += kid.speed.x;
        } else {
            kid.speed.x = 0;
            kid.actualpos.x = ((((kid.pos.x + 6) >> 4) << 4) + ((!kid.direction) * 4))
                              << (FIXED_POINT);
        }
    }

    kid.pos = (kid.actualpos >> FIXED_POINT);

    if(kid.isSucking) windNoise(); //sound.tone(300 + random(10), 20);
}

/*  updateCamera()
 * Positions camera to show kid
 */
void updateCamera() {
    if(kid.balloons == 0) return;
    // Camera offset
    if(cam.offset.x > 0)
        cam.offset.x--;
    else if(cam.offset.x < 0)
        cam.offset.x++;
    if(cam.offset.y > 0)
        cam.offset.y--;
    else if(cam.offset.y < 0)
        cam.offset.y++;

    vec2 cp;
    //kp = kid.pos;
    cp = (cam.pos + cam.offset);

    vec2 V;
    //vec2 V = (kid.pos - cam.pos + cam.offset) >> 3; // more bytes
    V.x = kid.pos.x - cp.x - 58;
    V.y = kid.pos.y - cp.y - 24;
    V = V >> 2;

    cam.pos += V;
    cam.pos.y = min(320, cam.pos.y);
}

void drawKid() {
    if(kid.isActive) {
        vec2 kidcam;
        kidcam.x = kid.pos.x - cam.pos.x;
        kidcam.y = kid.pos.y - cam.pos.y;
        // Fall off earth
        if(kidcam.y > 64 + (CAMERA_OFFSET * 3)) {
            kid.actualpos = startPos;
            kidHurt();
            if(kid.balloons == 0) {
                // dead
                gameState = STATE_GAME_OVER;
            }
            //--kid.balloons;
        }

        if(kid.isBalloon) {
            int commonx = kidcam.x - (6 * kid.direction);
            int commony = kidcam.y + kid.balloonOffset;
            if(kid.balloons > 1) {
                sprites.drawPlusMask(commonx + 1, commony - 11, balloon_plus_mask, 0);
                if(kid.balloons > 2)
                    sprites.drawPlusMask(commonx + 7, commony - 12, balloon_plus_mask, 0);
            }
            sprites.drawPlusMask(commonx + 4, commony - 9, balloon_plus_mask, 0);
        }
        if(!kid.isSucking) {
            sprites.drawSelfMasked(kidcam.x, kidcam.y, kidSprite, 12 + kid.direction);
            sprites.drawErase(
                kidcam.x,
                kidcam.y,
                kidSprite,
                kid.frame + 6 * kid.direction +
                    ((kid.isJumping << 2) + 5 * (kid.isLanding || kid.isBalloon)) *
                        !kid.isSucking);
        }

        else {
            sprites.drawPlusMask(
                kidcam.x - 2,
                kidcam.y,
                kidSpriteSuck_plus_mask,
                walkerFrame + 2 * kid.direction); //kidSpriteSuck
            for(byte i = 0; i < PLAYER_PARTICLES; ++i) {
                // Update
                if(kid.particles[i].y > 2)
                    --kid.particles[i].y;
                else if(kid.particles[i].y < -2)
                    ++kid.particles[i].y;
                kid.particles[i].x -= 2;
                if(kid.particles[i].x < 0) {
                    kid.particles[i].x = 16;
                    kid.particles[i].y = -4 + random(13);
                }

                // Draw
                if(kid.direction)
                    sprites.drawErase(
                        kidcam.x - kid.particles[i].x,
                        kidcam.y + 10 + kid.particles[i].y,
                        particle,
                        0);
                else
                    sprites.drawErase(
                        kidcam.x + 15 + kid.particles[i].x,
                        kidcam.y + 10 + kid.particles[i].y,
                        particle,
                        0);
            }
        }
    }
}

#endif
