#include "terrain_view.h"

#include "map_state.h"
#include "perception.h"

char fr_tile_glyph(const FrGame* game, uint8_t x, uint8_t y) {
    if(fr_player_detects_trap(game, x, y)) return '^';
    switch(fr_get_terrain(game, x, y)) {
    case FR_TERR_FLOOR:
        return '.';
    case FR_TERR_WALL:
        return '#';
    case FR_TERR_DOOR_CLOSED:
        return '+';
    case FR_TERR_DOOR_OPEN:
        return '-';
    case FR_TERR_GRASS:
        return '"';
    case FR_TERR_GRASS_TRAMPLED:
        return '\'';
    case FR_TERR_PUDDLE:
        return '~';
    case FR_TERR_SAND:
        return ',';
    case FR_TERR_WATER:
        return '~';
    case FR_TERR_SHRINE:
        return 'T';
    case FR_TERR_GRATE:
        return '#';
    case FR_TERR_BUTTON:
        return '.';
    case FR_TERR_ICE:
        return '_';
    case FR_TERR_STAIRS_DOWN:
        return '>';
    case FR_TERR_STAIRS_UP:
        return '<';
    default:
        return ' ';
    }
}

const char* fr_terrain_name(uint8_t terrain) {
    switch(terrain) {
    case FR_TERR_FLOOR:
        return "Floor";
    case FR_TERR_WALL:
        return "Wall";
    case FR_TERR_DOOR_CLOSED:
        return "Closed door";
    case FR_TERR_DOOR_OPEN:
        return "Open door";
    case FR_TERR_GRASS:
        return "Tall grass";
    case FR_TERR_GRASS_TRAMPLED:
        return "Trampled grass";
    case FR_TERR_PUDDLE:
        return "Puddle";
    case FR_TERR_SAND:
        return "Dust";
    case FR_TERR_WATER:
        return "Deep water";
    case FR_TERR_SHRINE:
        return "Old god statue";
    case FR_TERR_GRATE:
        return "Grate";
    case FR_TERR_BUTTON:
        return "Floor button";
    case FR_TERR_ICE:
        return "Ice";
    case FR_TERR_STAIRS_DOWN:
        return "Stairs down";
    case FR_TERR_STAIRS_UP:
        return "Stairs up";
    default:
        return "Void";
    }
}
