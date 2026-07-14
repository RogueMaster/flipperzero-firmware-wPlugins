#ifndef DEFINES_H_
#define DEFINES_H_

#define USE_SIMPLE_COLLISIONS

#define FXDATA_STREAMING

#if defined(_WIN32)
//#define STANDARD_FILE_STREAMING
//#define PROGMEM_MAP_STREAMING
//#define EMULATE_GAMEBUINO 1
#define EMULATE_ARDUBOY 1
//#define EMULATE_HACKVISION 1
//#define EMULATE_UZEBOX 1

#if defined(EMULATE_ARDUBOY)
// Arduboy
#define DISPLAYWIDTH  128
#define DISPLAYHEIGHT 64
#endif

#else // Arduboy
//#define PETIT_FATFS_FILE_STREAMING
//#define PROGMEM_MAP_STREAMING
#define DISPLAYWIDTH  128
#define DISPLAYHEIGHT 64
#endif

#define HALF_DISPLAYWIDTH  (DISPLAYWIDTH >> 1)
#define HALF_DISPLAYHEIGHT (DISPLAYHEIGHT >> 1)

// WIN32 specific
#ifdef _WIN32
#define ZOOM_SCALE 3

#define PROGMEM
#define PSTR
#define pgm_read_byte(x) (*((uint8_t*)x))
#define pgm_read_word(x) (*((uint16_t*)x))

#define pgm_read_ptr(x) (*((uintptr_t*)x))
#define strlen_P(x)     strlen(x)

#include <stdint.h>
typedef uint32_t __uint24;
using uint24_t = __uint24;

#include <stdio.h>
#define WARNING(msg, ...)   printf((msg), __VA_ARGS__)
#define PLATFORM_ERROR(msg) printf(msg)
#else
//#include <avr/pgmspace.h>
#define WARNING(msg, ...)

#endif
// end

#if !defined(max) && !defined(min)
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define min3(a, b, c) (min(min(a, b), c))
#define max3(a, b, c) (max(max(a, b), c))
#define sign(x)       ((x) < 0 ? -1 : 1)
#define mabs(x)       ((x) < 0 ? -(x) : (x))

#define CELL_SIZE       32
#define CELL_SIZE_SHIFT 5

#if 0
#define CELL_TO_WORLD(x) ((x) * CELL_SIZE)
#define WORLD_TO_CELL(x) ((x) / CELL_SIZE)
#else
#define CELL_TO_WORLD(x) ((x) << CELL_SIZE_SHIFT)
#define WORLD_TO_CELL(x) ((x) >> CELL_SIZE_SHIFT)
#endif

#define MAP_SIZE        64
#define MAP_BUFFER_SIZE 16
//#define MAP_SIZE 16

#define TEXTURE_SIZE   32
#define TEXTURE_STRIDE 32 //8

// ~60 degree field of view (these values in wacky 256 unit format)
#define FOV           44
#define HALF_FOV      22
#define CULLING_FOV   35
#define DRAW_DISTANCE (CELL_SIZE * 20)

//#define NEAR_PLANE 73
//#define NEAR_PLANE 104
//#define NEAR_PLANE (LCDWIDTH * (0.5/tan(PI*(FOV / 2)/180)))
#define NEAR_PLANE_MULTIPLIER 222
#define NEAR_PLANE            (DISPLAYWIDTH * NEAR_PLANE_MULTIPLIER / 256)

#define CLIP_PLANE        5
#define SPRITE_CLIP_PLANE 16
#define CAMERA_SCALE      1
//#define WALL_HEIGHT 1.0f
#define MOVEMENT          7
#define TURN              8
#define MIN_WALL_DISTANCE 8
#define MAX_DOORS         16

#define DOOR_FRAME_TEXTURE    23
#define DOOR_GENERIC_TEXTURE  22
#define DOOR_ELEVATOR_TEXTURE 12
#define DOOR_LOCKED1_TEXTURE  24
#define DOOR_LOCKED2_TEXTURE  25

#define MAX_ACTIVE_ACTORS 8
#define MAX_ACTIVE_ITEMS  6

#define EMPTY_ITEM_SLOT 0xff
#define DYNAMIC_ITEM_ID 0xfe

#define ACTOR_HITBOX_SIZE  16
#define MIN_ACTOR_DISTANCE 32

#define FIRST_FONT_GLYPH     32
#define LAST_FONT_GLYPH      95
#define FONT_WIDTH           3
#define FONT_HEIGHT          5
#define FONT_GLYPH_BYTE_SIZE 2

#define STREAM_BUFFER_SIZE 64

#ifndef TARGET_FRAMERATE
#define TARGET_FRAMERATE 24
#endif

#endif
