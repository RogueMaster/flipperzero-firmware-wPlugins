#ifndef INPUT_H
#define INPUT_H

#include "globals.h"
#include "player.h"

#define TIMER_AMOUNT 48

void checkInputs() {
    if(kid.balloons <= 0) return; // Cannot control player if dead

    cam.offset = vec2(0, 0);
    kid.isWalking = false;
    if(pressed(MYBL_DOWN)) {
        cam.offset.y = -CAMERA_OFFSET;
    } else if(pressed(MYBL_UP)) {
        cam.offset.y = CAMERA_OFFSET;
    }
    if(!kid.isSucking) {
        if(pressed(MYBL_LEFT)) {
            mapTimer = TIMER_AMOUNT;
            cam.offset.x = CAMERA_OFFSET;
            kid.direction = FACING_LEFT;
            if(!(kid.isJumping || kid.isBalloon || kid.isLanding)) {
                if(!gridGetSolid((kid.pos.x - 1) >> 4, (kid.pos.y + 8) >> 4))
                    kid.actualpos.x -= PLAYER_SPEED_WALKING;
                kid.isWalking = true;
                kid.speed.x = -1;
            } else {
                //kid.speed.x = maxOf(kid.speed.x - PLAYER_SPEED_AIR, -MAX_XSPEED);
                if(kid.speed.x > -MAX_XSPEED) kid.speed.x -= PLAYER_SPEED_AIR;
            }
        } else if(pressed(MYBL_RIGHT)) {
            //mapTimer = TIMER_AMOUNT;
            cam.offset.x = -CAMERA_OFFSET;
            kid.direction = FACING_RIGHT;
            if(!(kid.isJumping || kid.isBalloon || kid.isLanding)) {
                if(!gridGetSolid((kid.pos.x + 12) >> 4, (kid.pos.y + 8) >> 4))
                    kid.actualpos.x += PLAYER_SPEED_WALKING;
                kid.isWalking = true;
                kid.speed.x = 1;
            } else {
                //kid.speed.x = minOf(kid.speed.x + PLAYER_SPEED_AIR, MAX_XSPEED);
                if(kid.speed.x < MAX_XSPEED) kid.speed.x += PLAYER_SPEED_AIR;
            }
        }
    }
    kid.isSucking = false;
    if(pressed(MYBL_BACK)) {
        if(pressed(MYBL_DOWN))
            gameState = STATE_GAME_PAUSE;
        else //if (!kid.isBalloon)
        {
            kid.isBalloon = false;
            kid.isSucking = true;
        }
    }
    /*if (pressed(MYBL_BACK + MYBL_DOWN))  gameState = STATE_GAME_PAUSE;
  if (pressed(MYBL_BACK) && !kid.isBalloon)
  {
    kid.isSucking = true;
  }
  else
    kid.isSucking = false;*/

    // Jump Button
    if(justPressed(MYBL_OK)) {
        if(kid.speed.y == 0 && kid.isJumping == false && kid.isLanding == false) {
            playTone(200, 100);
            kid.isWalking = false;
            kid.isJumping = true;
            kid.jumpLetGo = false;
            kid.jumpTimer = PLAYER_JUMP_TIME;
            kid.speed.y = PLAYER_JUMP_VELOCITY;
            if(pressed(MYBL_RIGHT))
                kid.speed.x = MAX_XSPEED;
            else if(pressed(MYBL_LEFT))
                kid.speed.x = -MAX_XSPEED;
        } else {
            if(kid.balloons > 0) {
                kid.isBalloon = true;
                kid.balloonOffset = 16;
                kid.isJumping = false;
                kid.isLanding = true;
            }
        }
    }
    if(!pressed(MYBL_OK)) {
        kid.isBalloon = false;
        if(kid.isJumping) kid.jumpLetGo = true;
    }
}

#endif
