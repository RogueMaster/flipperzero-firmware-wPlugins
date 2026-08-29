#pragma once

#include <stdint.h>
#include <string.h>

// Display configuration
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

// Game settings
#define DEV_MODE 0

// Input definitions
#define INPUT_LEFT  1
#define INPUT_RIGHT 2
#define INPUT_UP    4
#define INPUT_DOWN  8
#define INPUT_A     16
#define INPUT_B     32

#ifndef COLOUR_WHITE
#define COLOUR_WHITE 1
#endif

#ifndef COLOUR_BLACK
#define COLOUR_BLACK 0
#endif

// Angle system (256 = 360 degrees)
#define FIXED_ANGLE_MAX 256

// 3D rendering settings
#define CAMERA_SCALE          1
#define CLIP_PLANE            32
#define CLIP_ANGLE            32
#define NEAR_PLANE_MULTIPLIER 130
#define NEAR_PLANE            (DISPLAY_WIDTH * NEAR_PLANE_MULTIPLIER / 256)
#define HORIZON               (DISPLAY_HEIGHT / 2)

// World settings
#define CELL_SIZE            256
#define PARTICLES_PER_SYSTEM 8
#define BASE_SPRITE_SIZE     16
#define MAX_SPRITE_SIZE      (DISPLAY_HEIGHT / 2)
#define MIN_TEXTURE_DISTANCE 4
#define MAX_QUEUED_DRAWABLES 12

// Player settings
#define TURN_SPEED 3
