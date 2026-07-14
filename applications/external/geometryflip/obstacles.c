#include "obstacles.h"
#include "constants.h"

// Include the function definitions that were missing
ObstacleType char_to_obstacle_type(char c) {
    switch(c) {
    case '0':
        return ObstacleTypeNone;
    case '1':
        return ObstacleTypeBlock;
    case '2':
        return ObstacleTypeSpikeUp;
    case '3':
        return ObstacleTypeSpikeDown;
    case '4':
        return ObstacleTypeSpikeLeft;
    case '5':
        return ObstacleTypeSpikeRight;
    case '6':
        return ObstacleTypePortalShip;
    case '7':
        return ObstacleTypePortalCube;
    case '8':
        return ObstacleTypePortalBall;
    case '9':
        return ObstacleTypePortalUfo;
    case 'a':
        return ObstacleTypeGravityUp;
    case 'b':
        return ObstacleTypeGravityDown;
    default:
        return ObstacleTypeNone;
    }
}

int safe_atoi(const char* str) {
    if(!str) return 0;
    while(*str == ' ')
        str++;
    if(*str == '\0' || (!isdigit((unsigned char)*str) && *str != '-' && *str != '+')) {
        return 0;
    }
    return atoi(str);
}
