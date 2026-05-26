#include "room_templates.h"

#include "game_core.h"
#include "special_floors.h"

static uint32_t fr_mix_template_seed(uint32_t run_seed, uint8_t floor) {
    uint32_t x = run_seed ? run_seed : 1u;
    x ^= (uint32_t)floor * 0x9E3779B9u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

uint8_t fr_room_template_id(uint32_t run_seed, uint8_t floor) {
    if(fr_special_floor_type(run_seed, floor) == FR_SPECIAL_FLOOR_MAZE) return FR_ROOM_TEMPLATE_MAZE;
    return (uint8_t)(fr_mix_template_seed(run_seed, floor) % FR_ROOM_TEMPLATE_COUNT);
}

static FrRoom fr_room_jitter(
    FrGame* game,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    uint8_t x_jitter,
    uint8_t y_jitter,
    uint8_t w_jitter,
    uint8_t h_jitter) {
    return (FrRoom){
        (uint8_t)(x + (x_jitter ? fr_rand_u8(game, x_jitter) : 0)),
        (uint8_t)(y + (y_jitter ? fr_rand_u8(game, y_jitter) : 0)),
        (uint8_t)(w + (w_jitter ? fr_rand_u8(game, w_jitter) : 0)),
        (uint8_t)(h + (h_jitter ? fr_rand_u8(game, h_jitter) : 0)),
    };
}

static void fr_build_template_classic(FrGame* game, FrRoom rooms[FR_ROOM_MAX], uint8_t* room_count) {
    *room_count = (uint8_t)(6 + fr_rand_u8(game, 2));
    rooms[0] = fr_room_jitter(game, 2, 3, 7, 5, 4, 19, 4, 4);
    rooms[1] = fr_room_jitter(game, 15, 2, 8, 5, 7, 7, 5, 4);
    rooms[2] = fr_room_jitter(game, 15, 18, 8, 5, 7, 5, 5, 4);
    rooms[3] = fr_room_jitter(game, 31, 4, 8, 6, 8, 15, 5, 5);
    rooms[4] = fr_room_jitter(game, 48, 2, 8, 5, 6, 8, 5, 4);
    rooms[5] = fr_room_jitter(game, 48, 18, 8, 5, 6, 5, 5, 4);
    rooms[6] = fr_room_jitter(game, 28, 12, 8, 5, 14, 7, 5, 4);
}

static void fr_build_template_hub(FrGame* game, FrRoom rooms[FR_ROOM_MAX], uint8_t* room_count) {
    *room_count = (uint8_t)(6 + fr_rand_u8(game, 2));
    rooms[0] = fr_room_jitter(game, 3, 11, 8, 6, 3, 6, 4, 3);
    rooms[1] = fr_room_jitter(game, 20, 2, 8, 5, 5, 4, 4, 3);
    rooms[2] = fr_room_jitter(game, 18, 22, 9, 5, 6, 3, 4, 3);
    rooms[3] = fr_room_jitter(game, 31, 11, 9, 7, 4, 5, 4, 3);
    rooms[4] = fr_room_jitter(game, 49, 3, 8, 5, 4, 5, 4, 3);
    rooms[5] = fr_room_jitter(game, 48, 21, 8, 5, 5, 4, 4, 3);
    rooms[6] = fr_room_jitter(game, 34, 2, 8, 5, 6, 5, 4, 3);
}

static void fr_build_template_split(FrGame* game, FrRoom rooms[FR_ROOM_MAX], uint8_t* room_count) {
    *room_count = (uint8_t)(6 + fr_rand_u8(game, 2));
    rooms[0] = fr_room_jitter(game, 3, 12, 8, 6, 4, 5, 4, 3);
    rooms[1] = fr_room_jitter(game, 17, 3, 8, 5, 5, 5, 4, 3);
    rooms[2] = fr_room_jitter(game, 17, 20, 8, 5, 5, 4, 4, 3);
    rooms[3] = fr_room_jitter(game, 31, 11, 8, 7, 5, 5, 4, 3);
    rooms[4] = fr_room_jitter(game, 49, 5, 8, 5, 4, 5, 4, 3);
    rooms[5] = fr_room_jitter(game, 49, 20, 8, 5, 4, 4, 4, 3);
    rooms[6] = fr_room_jitter(game, 31, 22, 8, 5, 7, 3, 4, 3);
}

static void fr_build_template_long(FrGame* game, FrRoom rooms[FR_ROOM_MAX], uint8_t* room_count) {
    *room_count = (uint8_t)(6 + fr_rand_u8(game, 2));
    rooms[0] = fr_room_jitter(game, 2, 10, 8, 6, 3, 8, 4, 3);
    rooms[1] = fr_room_jitter(game, 13, 4, 8, 5, 4, 6, 4, 3);
    rooms[2] = fr_room_jitter(game, 14, 20, 8, 5, 4, 4, 4, 3);
    rooms[3] = fr_room_jitter(game, 27, 10, 9, 7, 5, 5, 4, 3);
    rooms[4] = fr_room_jitter(game, 42, 4, 8, 5, 5, 6, 4, 3);
    rooms[5] = fr_room_jitter(game, 52, 17, 8, 5, 2, 6, 3, 3);
    rooms[6] = fr_room_jitter(game, 39, 20, 8, 5, 6, 4, 4, 3);
}

static void fr_build_template_maze(FrGame* game, FrRoom rooms[FR_ROOM_MAX], uint8_t* room_count) {
    *room_count = 7;
    rooms[0] = fr_room_jitter(game, 3, 3, 6, 4, 3, 2, 2, 2);
    rooms[1] = fr_room_jitter(game, 14, 5, 6, 4, 3, 2, 2, 2);
    rooms[2] = fr_room_jitter(game, 24, 15, 6, 4, 3, 2, 2, 2);
    rooms[3] = fr_room_jitter(game, 33, 6, 6, 4, 3, 3, 2, 2);
    rooms[4] = fr_room_jitter(game, 43, 17, 6, 4, 3, 2, 2, 2);
    rooms[5] = fr_room_jitter(game, 53, 8, 6, 4, 2, 3, 2, 2);
    rooms[6] = fr_room_jitter(game, 32, 24, 6, 4, 5, 1, 2, 1);
}

void fr_room_template_build(
    FrGame* game,
    uint8_t floor,
    FrRoom rooms[FR_ROOM_MAX],
    uint8_t* room_count,
    uint8_t* template_id) {
    uint32_t run_seed = game->run_seed ? game->run_seed : game->seed;
    uint8_t id = fr_room_template_id(run_seed, floor);
    if(template_id) *template_id = id;

    switch(id) {
    case 1:
        fr_build_template_hub(game, rooms, room_count);
        break;
    case 2:
        fr_build_template_split(game, rooms, room_count);
        break;
    case 3:
        fr_build_template_long(game, rooms, room_count);
        break;
    case FR_ROOM_TEMPLATE_MAZE:
        fr_build_template_maze(game, rooms, room_count);
        break;
    case 0:
    default:
        fr_build_template_classic(game, rooms, room_count);
        break;
    }
}
