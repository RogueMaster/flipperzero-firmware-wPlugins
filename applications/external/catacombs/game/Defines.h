#pragma once
#include <cstdint>
#include <cstring>

static inline const void* pgm_read_ptr_safe(const void* p) {
    const void* out;
    std::memcpy(&out, p, sizeof(out));
    return out;
}

// Platform detection
#if defined(_WIN32)
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR(s)               (s)
#define pgm_read_byte(x)      (*((uint8_t*)(x)))
#define pgm_read_word(x)      (*((uint16_t*)(x)))
#define pgm_read_ptr(x)       (*((uintptr_t*)(x)))
#define strlen_P(x)           strlen(x)
#define strcpy_P(dst, src)    strcpy(dst, src)
#define memcpy_P(dst, src, n) memcpy(dst, src, n)
#elif defined(__AVR__)
// Arduino/Arduboy platform
#include <avr/pgmspace.h>
#else
// Flipper Zero and other ARM platforms
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR(s)               (s)
#define pgm_read_byte(x)      (*((const uint8_t*)(x)))
#define pgm_read_word(x)      (*((const uint16_t*)(x)))
#define pgm_read_dword(x)     (*((const uint32_t*)(x)))
#define strlen_P(x)           strlen(x)
#define strcpy_P(dst, src)    strcpy(dst, src)
#define strcmp_P(s1, s2)      strcmp(s1, s2)
#define strncmp_P(s1, s2, n)  strncmp(s1, s2, n)
#define memcpy_P(dst, src, n) memcpy(dst, src, n)
#define sprintf_P             sprintf
#define snprintf_P            snprintf
#endif

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
