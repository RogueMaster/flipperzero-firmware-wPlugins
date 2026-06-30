#pragma once
#include "flip_world_icons.h"
#include "game.h"

typedef enum
{
    ICON_ID_HOUSE,
    ICON_ID_MAN,
    ICON_ID_PLANT,
    ICON_ID_TREE,
    ICON_ID_WOMAN,
    ICON_ID_FENCE,
    ICON_ID_FENCE_END,
    ICON_ID_FENCE_VERTICAL_END,
    ICON_ID_FENCE_VERTICAL_START,
    ICON_ID_FLOWER,
    ICON_ID_LAKE_BOTTOM,
    ICON_ID_LAKE_BOTTOM_LEFT,
    ICON_ID_LAKE_BOTTOM_RIGHT,
    ICON_ID_LAKE_LEFT,
    ICON_ID_LAKE_RIGHT,
    ICON_ID_LAKE_TOP,
    ICON_ID_LAKE_TOP_LEFT,
    ICON_ID_LAKE_TOP_RIGHT,
    ICON_ID_ROCK_LARGE,
    ICON_ID_ROCK_MEDIUM,
    ICON_ID_ROCK_SMALL,
} IconID;

typedef struct
{
    IconID id;
    const Icon *icon;
    Vector pos;  // position at which to draw the icon
    Vector size; // dimensions for centering
} IconSpec;

typedef struct
{
    int count;       // number of icons in this group
    IconSpec *icons; // pointer to an array of icon specs
} IconGroupContext;

extern IconGroupContext *g_current_icon_group;

extern const EntityDescription icon_desc;