#pragma once

#if defined(_WIN32)
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR
#define pgm_read_byte(x) (*((uint8_t*)x))
#define pgm_read_word(x) (*((uint16_t*)x))
#define pgm_read_ptr(x) (*((uintptr_t*)x))
#define strlen_P(x) strlen(x)
#else
#include <lib/pgmspace.h>
//#define pgm_read_ptr pgm_read_word
#endif

#define TILE_SIZE 8
#define TILE_SIZE_SHIFT 3

#ifdef _WIN32
//#define DISPLAY_WIDTH 192
//#define DISPLAY_HEIGHT 192
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define MAP_WIDTH 48
#define MAP_HEIGHT 48
#else
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define MAP_WIDTH 48
#define MAP_HEIGHT 48
#endif


#define MAX_SCROLL_X (MAP_WIDTH * TILE_SIZE - DISPLAY_WIDTH)
#define MAX_SCROLL_Y (MAP_HEIGHT * TILE_SIZE - DISPLAY_HEIGHT)

#define VISIBLE_TILES_X ((DISPLAY_WIDTH / TILE_SIZE) + 1)
#define VISIBLE_TILES_Y ((DISPLAY_HEIGHT / TILE_SIZE) + 1)

#define MAX_BUILDINGS 130

// How long a button has to be held before the first event repeats
#define INPUT_REPEAT_TIME 10

// When repeating, how long between each event is fired
#define INPUT_REPEAT_FREQUENCY 2

#define BULLDOZER_COST 1
#define ROAD_COST 10
#define POWERLINE_COST 5

#define FIRST_TERRAIN_TILE 1
#define FIRST_WATER_TILE 17
#define LAST_WATER_TILE (FIRST_WATER_TILE + 3)

#define FIRST_FIRE_TILE 232
#define LAST_FIRE_TILE (FIRST_FIRE_TILE + 3)

#define FIRST_ROAD_TILE 5
#define FIRST_ROAD_TRAFFIC_TILE (FIRST_ROAD_TILE + 16)
#define LAST_ROAD_TRAFFIC_TILE (FIRST_ROAD_TRAFFIC_TILE + 10)
#define FIRST_POWERLINE_TILE 53
#define FIRST_POWERLINE_ROAD_TILE 49

#define FIRST_ROAD_BRIDGE_TILE 32
#define FIRST_POWERLINE_BRIDGE_TILE 34

#define FIRST_BUILDING_TILE 224

#define FIRST_EDGE_TILE 79
#define NORTH_WEST_EDGE_TILE FIRST_EDGE_TILE
#define NORTH_EAST_EDGE_TILE (FIRST_EDGE_TILE + 16)
#define SOUTH_WEST_EDGE_TILE (FIRST_EDGE_TILE + 32)
#define SOUTH_EAST_EDGE_TILE (FIRST_EDGE_TILE + 48)

#define POWERCUT_TILE 48
#define RUBBLE_TILE 51

#define FIRST_BRUSH_TILE 240

#define NUM_TOOLBAR_BUTTONS 13

#define MAX_POPULATION_DENSITY 15

#define NUM_TERRAIN_TYPES 3

#define STARTING_TAX_RATE 7
#define STARTING_FUNDS 10000

#define FIRE_AND_POLICE_MAINTENANCE_COST 100
#define ROAD_MAINTENANCE_COST 10

#define POPULATION_MULTIPLIER 17

#define MIN_BUDGET_DISPLAY_TIME 16

#define BUILDING_MAX_FIRE_COUNTER 3

#define MIN_FRAMES_BETWEEN_DISASTER 2500
#define FRAMES_PER_YEAR (MAX_BUILDINGS * 12)
#define MIN_TIME_BETWEEN_DISASTERS (FRAMES_PER_YEAR * 2)
#define MAX_TIME_BETWEEN_DISASTERS (FRAMES_PER_YEAR * 6)

#define DISASTER_MESSAGE_DISPLAY_TIME 60
