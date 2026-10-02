// NW Crawl: a tiny roguelike set on the alphabet streets of NW Portland.
// Head north from Burnside, one street per floor, to the Witch's Castle in Forest Park.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_W 40
#define MAP_H 26
#define TILE 8
#define VIEW_W 16
#define VIEW_H 7
#define HUD_Y (VIEW_H * TILE)

#define MAX_ROOMS 12
#define MIN_ROOMS 5
#define EXIT_ROOM_W 5
#define EXIT_ROOM_H 3
#define MAX_MONSTERS 16
#define MAX_ITEMS 6
#define FOV_RADIUS 4
#define MAX_COFFEE 9
#define COFFEE_HEAL 5

typedef enum {
    TileWall,
    TileFloor,
    TileExit,
    TileBartender,
} Tile;

typedef enum {
    MonsterRat,
    MonsterCrow,
    MonsterRaccoon,
    MonsterScooter,
    MonsterCoyote,
    MonsterWitch,
    // After the Witch so it stays out of the random spawn pool
    MonsterSasquatch,
    MonsterTypeCount,
} MonsterType;

typedef enum {
    ItemCoffee,
    ItemDonut,
    ItemIpa,
    ItemBook,
    ItemTypeCount,
} ItemType;

typedef enum {
    StateTitle,
    StatePlaying,
    StateDead,
    StateWon,
} GameState;

typedef struct {
    const char* name;
    const char* verb;
    uint8_t hp;
    uint8_t atk;
    uint8_t xp;
} MonsterInfo;

static const MonsterInfo monster_info[MonsterTypeCount] = {
    [MonsterRat] = {"Rat", "bites", 2, 1, 1},
    [MonsterCrow] = {"Crow", "pecks", 3, 1, 2},
    [MonsterRaccoon] = {"Raccoon", "claws", 5, 2, 3},
    [MonsterScooter] = {"Scooter", "rams", 4, 3, 4},
    [MonsterCoyote] = {"Coyote", "bites", 8, 3, 6},
    [MonsterWitch] = {"Witch", "smites", 50, 6, 0},
    [MonsterSasquatch] = {"Sasquatch", "clobbers", 24, 4, 12},
};

// One floor per street, heading north. The last entry is the boss floor.
static const char* const level_names[] = {
    "Burnside",
    "Couch",
    "Davis",
    "Everett",
    "Flanders",
    "Glisan",
    "Hoyt",
    "Irving",
    "Johnson",
    "Kearney",
    "Lovejoy",
    "Marshall",
    "Northrup",
    "Overton",
    "Pettygrove",
    "Quimby",
    "Raleigh",
    "Savier",
    "Thurman",
    "Witch's Castle",
};
#define LEVEL_COUNT ((int)COUNT_OF(level_names))

// Sasquatch wanders down from Forest Park on the northern half of the walk,
// and is too elusive to be seen from more than a couple of tiles away
#define SASQUATCH_MIN_TIER 5
#define SASQUATCH_ODDS 2
#define SASQUATCH_SIGHT 2

// Joe's Cellar is a safe room with a bartender on this street
#define BAR_STREET "Pettygrove"
// Difficulty is scaled as if the walk were this many floors long
#define DIFFICULTY_TIERS 10

// Sprites are written MSB-left for readability; XBM wants LSB-left.
#define B(b)                                                                       \
    (uint8_t)((((b) & 0x80) >> 7) | (((b) & 0x40) >> 5) | (((b) & 0x20) >> 3) |    \
              (((b) & 0x10) >> 1) | (((b) & 0x08) << 1) | (((b) & 0x04) << 3) |    \
              (((b) & 0x02) << 5) | (((b) & 0x01) << 7))

static const uint8_t spr_player[8] = {
    B(0b00111100),
    B(0b00111100),
    B(0b00011000),
    B(0b01111110),
    B(0b00011000),
    B(0b00011000),
    B(0b00100100),
    B(0b01100110),
};

static const uint8_t spr_wall[8] = {
    B(0b11111111),
    B(0b10000000),
    B(0b10000000),
    B(0b10000000),
    B(0b11111111),
    B(0b00001000),
    B(0b00001000),
    B(0b00001000),
};

static const uint8_t spr_exit[8] = {
    B(0b00011000),
    B(0b00111100),
    B(0b01111110),
    B(0b11011011),
    B(0b00011000),
    B(0b00011000),
    B(0b00011000),
    B(0b00000000),
};

static const uint8_t spr_bartender[8] = {
    B(0b00111000),
    B(0b00111000),
    B(0b00010011),
    B(0b01111111),
    B(0b00010011),
    B(0b11111111),
    B(0b11111111),
    B(0b10101010),
};

static const uint8_t spr_monsters[MonsterTypeCount][8] = {
    [MonsterRat] =
        {
            B(0b00000000),
            B(0b00100000),
            B(0b01110000),
            B(0b11111100),
            B(0b11111110),
            B(0b01111101),
            B(0b01001001),
            B(0b00000110),
        },
    [MonsterCrow] =
        {
            B(0b00000000),
            B(0b00110000),
            B(0b01111000),
            B(0b11011100),
            B(0b00111110),
            B(0b00111111),
            B(0b00010100),
            B(0b00110110),
        },
    [MonsterRaccoon] =
        {
            B(0b10000001),
            B(0b11000011),
            B(0b01111110),
            B(0b11011011),
            B(0b11111111),
            B(0b01100110),
            B(0b00111100),
            B(0b00011000),
        },
    [MonsterScooter] =
        {
            B(0b00000011),
            B(0b00000010),
            B(0b00000100),
            B(0b00000100),
            B(0b00001000),
            B(0b01111000),
            B(0b11001100),
            B(0b11001100),
        },
    [MonsterCoyote] =
        {
            B(0b00000101),
            B(0b10000111),
            B(0b01001111),
            B(0b01111110),
            B(0b01111100),
            B(0b01000100),
            B(0b01000100),
            B(0b00000000),
        },
    [MonsterWitch] =
        {
            B(0b00010000),
            B(0b00111000),
            B(0b00111000),
            B(0b11111110),
            B(0b00111000),
            B(0b01111100),
            B(0b11111110),
            B(0b11111110),
        },
};

static const uint8_t spr_sasquatch[8] = {
    B(0b00111000),
    B(0b00111000),
    B(0b01111100),
    B(0b11111110),
    B(0b10111010),
    B(0b00111000),
    B(0b00101100),
    B(0b01100100),
};

static const uint8_t spr_items[ItemTypeCount][8] = {
    [ItemCoffee] =
        {
            B(0b00000000),
            B(0b01010000),
            B(0b00101000),
            B(0b11111000),
            B(0b11111110),
            B(0b11111010),
            B(0b11111100),
            B(0b01110000),
        },
    [ItemDonut] =
        {
            B(0b00000000),
            B(0b00111100),
            B(0b01111110),
            B(0b11100111),
            B(0b11100111),
            B(0b01111110),
            B(0b00111100),
            B(0b00000000),
        },
    [ItemIpa] =
        {
            B(0b00000000),
            B(0b01111110),
            B(0b01111110),
            B(0b01000010),
            B(0b01000010),
            B(0b00100100),
            B(0b00100100),
            B(0b00111100),
        },
    [ItemBook] =
        {
            B(0b01111110),
            B(0b01000011),
            B(0b01011011),
            B(0b01000011),
            B(0b01011011),
            B(0b01000011),
            B(0b01111111),
            B(0b00111111),
        },
};

// Square-wave blips through the notification service, so they follow the
// Flipper's volume and stealth-mode settings and never block the game loop.
#define NOTE(n, d) &message_note_##n, &message_delay_##d
#define SFX_END &message_sound_off, NULL

static const NotificationSequence sfx_hit = {NOTE(a4, 25), NOTE(e4, 25), SFX_END};
static const NotificationSequence sfx_kill = {NOTE(e5, 25), NOTE(g5, 25), NOTE(c6, 50), SFX_END};
static const NotificationSequence sfx_hurt = {
    &message_vibro_on,
    NOTE(a2, 50),
    NOTE(f2, 50),
    &message_vibro_off,
    SFX_END,
};
static const NotificationSequence sfx_pickup = {NOTE(c6, 25), NOTE(e6, 25), NOTE(g6, 50), SFX_END};
static const NotificationSequence sfx_drink = {NOTE(g4, 50), NOTE(c5, 50), NOTE(e5, 50), SFX_END};
static const NotificationSequence sfx_stairs = {
    NOTE(c5, 50),
    NOTE(e5, 50),
    NOTE(g5, 50),
    NOTE(c6, 100),
    SFX_END,
};
static const NotificationSequence sfx_level_up = {
    NOTE(c5, 50),
    NOTE(g5, 50),
    NOTE(c6, 50),
    NOTE(e6, 50),
    NOTE(g6, 100),
    SFX_END,
};
// A little blues lick for the bar
static const NotificationSequence sfx_bar = {
    NOTE(c5, 100),
    NOTE(ds5, 50),
    NOTE(f5, 100),
    NOTE(fs5, 50),
    NOTE(g5, 100),
    NOTE(as5, 50),
    NOTE(c6, 250),
    SFX_END,
};
static const NotificationSequence sfx_magic = {
    NOTE(b6, 25),
    NOTE(f6, 25),
    NOTE(b6, 25),
    NOTE(f6, 25),
    NOTE(cs6, 50),
    SFX_END,
};
static const NotificationSequence sfx_death = {
    &message_vibro_on,
    NOTE(g4, 100),
    &message_vibro_off,
    NOTE(e4, 100),
    NOTE(c4, 100),
    NOTE(a3, 100),
    NOTE(f3, 250),
    SFX_END,
};
static const NotificationSequence sfx_win = {
    NOTE(c5, 100),
    NOTE(c5, 50),
    NOTE(c5, 50),
    NOTE(g5, 100),
    NOTE(e5, 100),
    NOTE(g5, 100),
    NOTE(c6, 500),
    SFX_END,
};

// When several things happen in one turn only the most important one is heard
typedef enum {
    SfxPrioNone,
    SfxPrioHit,
    SfxPrioPickup,
    SfxPrioKill,
    SfxPrioHurt,
    SfxPrioMagic,
    SfxPrioLevel,
    SfxPrioFinal,
} SfxPrio;

typedef struct {
    int8_t x, y, w, h;
} Room;

typedef struct {
    int8_t x, y;
    int8_t hp;
    uint8_t type;
    bool alive;
} Monster;

typedef struct {
    int8_t x, y;
    uint8_t type;
    bool active;
} Item;

typedef struct {
    FuriMutex* mutex;
    NotificationApp* notifications;

    GameState state;
    int depth;

    uint8_t tiles[MAP_H][MAP_W];
    bool seen[MAP_H][MAP_W];
    bool visible[MAP_H][MAP_W];

    Monster monsters[MAX_MONSTERS];
    Item items[MAX_ITEMS];

    Room bar;
    bool has_bar;
    bool bar_served;

    int px, py;
    int hp, max_hp;
    int atk;
    int xp, level;
    int coffee;

    const NotificationSequence* sfx;
    SfxPrio sfx_prio;

    // Dev mode: toggled with Up on the title screen, lets you pick the starting street
    bool dev_mode;
    int dev_start;

    const char* killer;
    char msg[48];
} Game;

static void play(Game* g, const NotificationSequence* sequence, SfxPrio prio) {
    if(prio < g->sfx_prio) return;
    g->sfx = sequence;
    g->sfx_prio = prio;
}

static int rnd(int n) {
    return (int)(furi_hal_random_get() % (uint32_t)n);
}

static void say(Game* g, const char* text) {
    strlcpy(g->msg, text, sizeof(g->msg));
}

static void say_more(Game* g, const char* text) {
    if(g->msg[0]) strlcat(g->msg, " ", sizeof(g->msg));
    strlcat(g->msg, text, sizeof(g->msg));
}

static bool in_map(int x, int y) {
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H;
}

static bool walkable(Game* g, int x, int y) {
    return in_map(x, y) && (g->tiles[y][x] == TileFloor || g->tiles[y][x] == TileExit);
}

static bool in_bar(Game* g, int x, int y) {
    const Room* b = &g->bar;
    return g->has_bar && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
}

static Monster* monster_at(Game* g, int x, int y) {
    for(int i = 0; i < MAX_MONSTERS; i++) {
        Monster* m = &g->monsters[i];
        if(m->alive && m->x == x && m->y == y) return m;
    }
    return NULL;
}

static Item* item_at(Game* g, int x, int y) {
    for(int i = 0; i < MAX_ITEMS; i++) {
        Item* it = &g->items[i];
        if(it->active && it->x == x && it->y == y) return it;
    }
    return NULL;
}

static bool line_of_sight(Game* g, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while(true) {
        if(x0 == x1 && y0 == y1) return true;
        if(g->tiles[y0][x0] == TileWall) return false;
        int e2 = 2 * err;
        if(e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if(e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void update_fov(Game* g) {
    memset(g->visible, 0, sizeof(g->visible));
    for(int y = g->py - FOV_RADIUS; y <= g->py + FOV_RADIUS; y++) {
        for(int x = g->px - FOV_RADIUS; x <= g->px + FOV_RADIUS; x++) {
            if(!in_map(x, y)) continue;
            if(line_of_sight(g, g->px, g->py, x, y)) {
                g->visible[y][x] = true;
                g->seen[y][x] = true;
            }
        }
    }
}

static void reveal_map(Game* g) {
    for(int y = 0; y < MAP_H; y++) {
        for(int x = 0; x < MAP_W; x++) {
            if(g->tiles[y][x] != TileWall) {
                g->seen[y][x] = true;
                continue;
            }
            // Only walls that border something walkable, so the map keeps its shape
            for(int ny = y - 1; ny <= y + 1; ny++) {
                for(int nx = x - 1; nx <= x + 1; nx++) {
                    if(in_map(nx, ny) && g->tiles[ny][nx] != TileWall) g->seen[y][x] = true;
                }
            }
        }
    }
}

static void carve_h(Game* g, int x0, int x1, int y) {
    if(x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for(int x = x0; x <= x1; x++)
        g->tiles[y][x] = TileFloor;
}

static void carve_v(Game* g, int y0, int y1, int x) {
    if(y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for(int y = y0; y <= y1; y++)
        g->tiles[y][x] = TileFloor;
}

static bool find_free_spot(Game* g, const Room* room, int* out_x, int* out_y) {
    for(int tries = 0; tries < 20; tries++) {
        int x = room->x + rnd(room->w);
        int y = room->y + rnd(room->h);
        if(g->tiles[y][x] != TileFloor) continue;
        if(x == g->px && y == g->py) continue;
        if(monster_at(g, x, y) || item_at(g, x, y)) continue;
        *out_x = x;
        *out_y = y;
        return true;
    }
    return false;
}

static void spawn_monster(Game* g, const Room* room, MonsterType type) {
    for(int i = 0; i < MAX_MONSTERS; i++) {
        Monster* m = &g->monsters[i];
        if(m->alive) continue;
        int x, y;
        if(!find_free_spot(g, room, &x, &y)) return;
        m->x = x;
        m->y = y;
        m->type = type;
        m->hp = monster_info[type].hp;
        m->alive = true;
        return;
    }
}

static void spawn_item(Game* g, const Room* room, ItemType type) {
    for(int i = 0; i < MAX_ITEMS; i++) {
        Item* it = &g->items[i];
        if(it->active) continue;
        int x, y;
        if(!find_free_spot(g, room, &x, &y)) return;
        it->x = x;
        it->y = y;
        it->type = type;
        it->active = true;
        return;
    }
}

// The bartender stands in the wall behind the bar, so they can never block a corridor
static void place_bartender(Game* g) {
    const Room* b = &g->bar;
    const int rows[2] = {b->y - 1, b->y + b->h};
    for(int i = 0; i < 2; i++) {
        for(int x = b->x + b->w / 2; x < b->x + b->w; x++) {
            if(g->tiles[rows[i]][x] == TileWall) {
                g->tiles[rows[i]][x] = TileBartender;
                return;
            }
        }
    }
}

// Carves a room at a random spot and joins it to the previous one with a corridor.
// Fails without touching the map if it would overlap or touch an existing room.
static bool try_place_room(Game* g, Room* rooms, int* count, int w, int h) {
    Room r;
    r.w = w;
    r.h = h;
    r.x = 1 + rnd(MAP_W - r.w - 1);
    r.y = 1 + rnd(MAP_H - r.h - 1);

    for(int i = 0; i < *count; i++) {
        const Room* o = &rooms[i];
        if(r.x <= o->x + o->w && r.x + r.w >= o->x && r.y <= o->y + o->h && r.y + r.h >= o->y) {
            return false;
        }
    }

    for(int y = r.y; y < r.y + r.h; y++)
        for(int x = r.x; x < r.x + r.w; x++)
            g->tiles[y][x] = TileFloor;

    if(*count > 0) {
        const Room* p = &rooms[*count - 1];
        int ax = p->x + p->w / 2, ay = p->y + p->h / 2;
        int bx = r.x + r.w / 2, by = r.y + r.h / 2;
        if(rnd(2)) {
            carve_h(g, ax, bx, ay);
            carve_v(g, ay, by, bx);
        } else {
            carve_v(g, ay, by, ax);
            carve_h(g, ax, bx, by);
        }
    }
    rooms[(*count)++] = r;
    return true;
}

static void generate_level(Game* g) {
    Room rooms[MAX_ROOMS];
    int count;
    bool bar_level = strcmp(level_names[g->depth], BAR_STREET) == 0;

    bool placed_exit;

    do {
        memset(g->tiles, TileWall, sizeof(g->tiles));
        count = 0;
        for(int attempt = 0; attempt < 150 && count < MAX_ROOMS - 1; attempt++) {
            try_place_room(g, rooms, &count, 3 + rnd(4), 2 + rnd(3));
        }
        // The exit always gets a proper room of its own, placed last
        placed_exit = false;
        for(int attempt = 0; attempt < 60 && !placed_exit; attempt++) {
            placed_exit = try_place_room(g, rooms, &count, EXIT_ROOM_W + rnd(2), EXIT_ROOM_H + rnd(2));
        }
    } while(count < MIN_ROOMS || !placed_exit);

    // Side alleys off each block: some dead-end, some loop back into other streets
    // (never starting from the exit room, so it keeps its shape)
    for(int i = 0; i < count * 2; i++) {
        const Room* r = &rooms[rnd(count - 1)];
        int x = r->x + rnd(r->w), y = r->y + rnd(r->h);
        bool horizontal = rnd(2);
        for(int leg = 0; leg < 3; leg++) {
            int step = rnd(2) ? 1 : -1;
            int len = 3 + rnd(7);
            for(int n = 0; n < len; n++) {
                int tx = x + (horizontal ? step : 0);
                int ty = y + (horizontal ? 0 : step);
                if(tx < 1 || ty < 1 || tx > MAP_W - 2 || ty > MAP_H - 2) break;
                x = tx;
                y = ty;
                g->tiles[y][x] = TileFloor;
            }
            horizontal = !horizontal;
        }
    }

    // A middle room, so it is never the start room or the exit room
    int bar_index = count / 2;
    g->has_bar = bar_level;
    g->bar_served = false;
    if(bar_level) {
        g->bar = rooms[bar_index];
        place_bartender(g);
    }

    memset(g->seen, 0, sizeof(g->seen));
    memset(g->monsters, 0, sizeof(g->monsters));
    memset(g->items, 0, sizeof(g->items));

    g->px = rooms[0].x + rooms[0].w / 2;
    g->py = rooms[0].y + rooms[0].h / 2;

    const Room* last = &rooms[count - 1];
    bool boss_level = g->depth == LEVEL_COUNT - 1;
    if(boss_level) {
        spawn_monster(g, last, MonsterWitch);
    } else {
        g->tiles[last->y + last->h / 2][last->x + last->w / 2] = TileExit;
    }

    // Tougher locals show up the further north you get
    int tier = g->depth * DIFFICULTY_TIERS / (LEVEL_COUNT - 1);
    int variety = MIN(1 + tier / 2, (int)MonsterWitch);
    // Leave spare slots on the boss floor for the Witch's summoned crows
    int monster_count = MIN(3 + tier, MAX_MONSTERS - 4);
    for(int i = 0; i < monster_count; i++) {
        int room;
        do {
            room = 1 + rnd(count - 1);
        } while(bar_level && room == bar_index);
        spawn_monster(g, &rooms[room], (MonsterType)rnd(variety));
    }
    if(!boss_level && tier >= SASQUATCH_MIN_TIER && rnd(SASQUATCH_ODDS) == 0) {
        spawn_monster(g, last, MonsterSasquatch);
    }

    spawn_item(g, &rooms[rnd(count)], ItemCoffee);
    if(rnd(2)) spawn_item(g, &rooms[rnd(count)], ItemCoffee);
    if(rnd(2)) spawn_item(g, &rooms[rnd(count)], ItemDonut);
    if(rnd(3) == 0) spawn_item(g, &rooms[rnd(count)], ItemIpa);
    if(rnd(2)) spawn_item(g, &rooms[rnd(count)], ItemBook);

    if(boss_level) {
        say(g, "The Witch's Castle...");
    } else {
        snprintf(g->msg, sizeof(g->msg), "NW %s St", level_names[g->depth]);
    }
    play(g, &sfx_stairs, SfxPrioLevel);
    update_fov(g);
}

static void new_game(Game* g) {
    g->state = StatePlaying;
    g->depth = g->dev_mode ? g->dev_start : 0;
    // A dev start gets roughly the character a real run would have by that street
    g->level = 1 + g->depth / 2;
    g->max_hp = 12 + 3 * (g->level - 1);
    g->hp = g->max_hp;
    g->atk = 2 + (g->level - 1);
    g->xp = 0;
    g->coffee = g->depth ? 3 : 1;
    g->killer = NULL;
    generate_level(g);
}

static void gain_xp(Game* g, int amount) {
    g->xp += amount;
    while(g->xp >= g->level * 6) {
        g->xp -= g->level * 6;
        g->level++;
        g->max_hp += 3;
        g->hp = MIN(g->hp + 3, g->max_hp);
        g->atk++;
        say_more(g, "Level up!");
        play(g, &sfx_level_up, SfxPrioLevel);
    }
}

// The Witch won't stand still and trade blows: she slips away mid-fight
static void witch_blink(Game* g, Monster* m) {
    for(int tries = 0; tries < 30; tries++) {
        int x = m->x + rnd(9) - 4;
        int y = m->y + rnd(9) - 4;
        if(!walkable(g, x, y) || monster_at(g, x, y)) continue;
        if(abs(x - g->px) + abs(y - g->py) < 3) continue;
        m->x = x;
        m->y = y;
        say_more(g, "She vanishes!");
        play(g, &sfx_magic, SfxPrioMagic);
        return;
    }
}

static void drop_item(Game* g, int x, int y, ItemType type) {
    for(int i = 0; i < MAX_ITEMS; i++) {
        Item* it = &g->items[i];
        if(it->active) continue;
        it->x = x;
        it->y = y;
        it->type = type;
        it->active = true;
        return;
    }
}

static void attack_monster(Game* g, Monster* m) {
    const MonsterInfo* info = &monster_info[m->type];
    int dmg = 1 + rnd(g->atk);
    m->hp -= dmg;
    if(m->hp > 0) {
        snprintf(g->msg, sizeof(g->msg), "Hit %s.", info->name);
        play(g, &sfx_hit, SfxPrioHit);
        if(m->type == MonsterWitch && rnd(3) == 0) witch_blink(g, m);
        return;
    }
    m->alive = false;
    if(m->type == MonsterWitch) {
        g->state = StateWon;
        play(g, &sfx_win, SfxPrioFinal);
        return;
    }
    snprintf(g->msg, sizeof(g->msg), "%s down!", info->name);
    play(g, &sfx_kill, SfxPrioKill);
    if(m->type == MonsterSasquatch) {
        drop_item(g, m->x, m->y, rnd(2) ? ItemDonut : ItemIpa);
        say_more(g, "He dropped something.");
    }
    gain_xp(g, info->xp);
}

static void pick_up(Game* g, Item* it) {
    switch(it->type) {
    case ItemCoffee:
        if(g->coffee >= MAX_COFFEE) {
            say(g, "Too jittery for more.");
            return;
        }
        g->coffee++;
        say(g, "Coffee to go.");
        break;
    case ItemDonut:
        g->max_hp += 2;
        g->hp = MIN(g->hp + 4, g->max_hp);
        say(g, "Pink-box donut!");
        break;
    case ItemIpa:
        g->atk++;
        say(g, "Hazy IPA. Bold!");
        break;
    case ItemBook:
        reveal_map(g);
        say(g, "Used book: a map!");
        break;
    default:
        break;
    }
    it->active = false;
    play(g, &sfx_pickup, SfxPrioPickup);
}

static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

// Returns true if the blow finished the player off
static bool hurt_player(Game* g, const MonsterInfo* info, const char* verb, int dmg) {
    char buf[32];
    g->hp -= dmg;
    snprintf(buf, sizeof(buf), "%s %s -%d", info->name, verb, dmg);
    say_more(g, buf);
    play(g, &sfx_hurt, SfxPrioHurt);
    if(g->hp > 0) return false;
    g->hp = 0;
    g->killer = info->name;
    g->state = StateDead;
    play(g, &sfx_death, SfxPrioFinal);
    return true;
}

static void witch_summon(Game* g, const Monster* witch) {
    for(int i = 0; i < MAX_MONSTERS; i++) {
        Monster* m = &g->monsters[i];
        if(m->alive) continue;
        for(int d = 0; d < 4; d++) {
            int x = witch->x + dirs[d][0], y = witch->y + dirs[d][1];
            if(!walkable(g, x, y) || monster_at(g, x, y) || (x == g->px && y == g->py)) continue;
            m->x = x;
            m->y = y;
            m->type = MonsterCrow;
            m->hp = monster_info[MonsterCrow].hp;
            m->alive = true;
            say_more(g, "She calls a crow!");
            play(g, &sfx_magic, SfxPrioMagic);
            return;
        }
        return;
    }
}

static void monsters_act(Game* g) {

    for(int i = 0; i < MAX_MONSTERS; i++) {
        Monster* m = &g->monsters[i];
        if(!m->alive) continue;
        const MonsterInfo* info = &monster_info[m->type];

        int dx = g->px - m->x;
        int dy = g->py - m->y;

        if(abs(dx) + abs(dy) == 1) {
            if(hurt_player(g, info, info->verb, 1 + rnd(info->atk))) return;
            continue;
        }

        // At range the Witch hexes you or calls in crows instead of just walking up
        if(m->type == MonsterWitch && g->visible[m->y][m->x]) {
            int roll = rnd(4);
            if(roll == 0) {
                witch_summon(g, m);
                continue;
            } else if(roll == 1) {
                if(hurt_player(g, info, "hexes", 1 + rnd(3))) return;
                continue;
            }
        }

        int nx = m->x, ny = m->y;
        bool chasing = g->visible[m->y][m->x];
        // Crows never fly straight
        if(m->type == MonsterCrow && rnd(2)) chasing = false;

        if(chasing) {
            bool horizontal_first = abs(dx) > abs(dy) || (abs(dx) == abs(dy) && rnd(2));
            int step_x = (dx > 0) - (dx < 0);
            int step_y = (dy > 0) - (dy < 0);
            for(int attempt = 0; attempt < 2; attempt++) {
                int tx = m->x, ty = m->y;
                if(horizontal_first == (attempt == 0)) {
                    tx += step_x;
                } else {
                    ty += step_y;
                }
                if((tx != m->x || ty != m->y) && walkable(g, tx, ty) && !monster_at(g, tx, ty)) {
                    nx = tx;
                    ny = ty;
                    break;
                }
            }
        } else if(rnd(3) == 0) {
            const int8_t* d = dirs[rnd(4)];
            int tx = m->x + d[0], ty = m->y + d[1];
            if(walkable(g, tx, ty) && !monster_at(g, tx, ty) && !(tx == g->px && ty == g->py)) {
                nx = tx;
                ny = ty;
            }
        }
        m->x = nx;
        m->y = ny;
    }
}

static void end_turn(Game* g) {
    if(g->state != StatePlaying) return;
    monsters_act(g);
    update_fov(g);
}

static void player_move(Game* g, int dx, int dy) {
    int nx = g->px + dx;
    int ny = g->py + dy;
    if(!in_map(nx, ny) || g->tiles[ny][nx] == TileWall) return;

    g->msg[0] = '\0';

    if(g->tiles[ny][nx] == TileBartender) {
        if(g->bar_served) {
            say(g, "\"You're cut off, hon.\"");
        } else {
            g->bar_served = true;
            g->hp = g->max_hp;
            g->atk++;
            say(g, "Stiff pour! Full HP, +ATK");
            play(g, &sfx_bar, SfxPrioLevel);
        }
        end_turn(g);
        return;
    }

    Monster* m = monster_at(g, nx, ny);
    if(m) {
        attack_monster(g, m);
        end_turn(g);
        return;
    }

    bool was_in_bar = in_bar(g, g->px, g->py);
    g->px = nx;
    g->py = ny;
    if(!was_in_bar && in_bar(g, nx, ny)) say(g, "Joe's Cellar. Pull up a stool.");

    if(g->tiles[ny][nx] == TileExit) {
        g->depth++;
        generate_level(g);
        return;
    }

    Item* it = item_at(g, nx, ny);
    if(it) {
        // Keep the bar greeting if we walked in onto an item
        char greeting[sizeof(g->msg)];
        strlcpy(greeting, g->msg, sizeof(greeting));
        pick_up(g, it);
        if(greeting[0]) say(g, greeting);
    }
    end_turn(g);
}

static void player_drink_or_wait(Game* g) {
    g->msg[0] = '\0';
    if(g->coffee > 0 && g->hp < g->max_hp) {
        g->coffee--;
        g->hp = MIN(g->hp + COFFEE_HEAL, g->max_hp);
        say(g, "Ahh, coffee.");
        play(g, &sfx_drink, SfxPrioPickup);
    } else if(g->coffee == 0 && g->hp < g->max_hp) {
        say(g, "Out of coffee!");
    }
    end_turn(g);
}

static void handle_key(Game* g, InputKey key) {
    if(g->state == StateTitle) {
        if(key == InputKeyOk) {
            new_game(g);
        } else if(key == InputKeyUp) {
            g->dev_mode = !g->dev_mode;
        } else if(g->dev_mode && key == InputKeyRight) {
            g->dev_start = (g->dev_start + 1) % LEVEL_COUNT;
        } else if(g->dev_mode && key == InputKeyLeft) {
            g->dev_start = (g->dev_start + LEVEL_COUNT - 1) % LEVEL_COUNT;
        }
        return;
    }
    if(g->state != StatePlaying) {
        // In dev mode go back to the title so another street can be picked
        if(key == InputKeyOk) {
            if(g->dev_mode) {
                g->state = StateTitle;
            } else {
                new_game(g);
            }
        }
        return;
    }

    switch(key) {
    case InputKeyUp:
        player_move(g, 0, -1);
        break;
    case InputKeyDown:
        player_move(g, 0, 1);
        break;
    case InputKeyLeft:
        player_move(g, -1, 0);
        break;
    case InputKeyRight:
        player_move(g, 1, 0);
        break;
    case InputKeyOk:
        player_drink_or_wait(g);
        break;
    default:
        break;
    }
}

static void draw_title(Canvas* canvas, Game* g) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "NW CRAWL");
    canvas_set_font(canvas, FontSecondary);
    if(g->dev_mode) {
        char buf[40];
        bool castle = g->dev_start == LEVEL_COUNT - 1;
        snprintf(
            buf, sizeof(buf), "DEV < %s%s >", castle ? "" : "NW ", level_names[g->dev_start]);
        canvas_draw_str_aligned(canvas, 64, 19, AlignCenter, AlignTop, buf);
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 19, AlignCenter, AlignTop, "Burnside to Witch's Castle");
    }
    canvas_draw_xbm(canvas, 44, 31, 8, 8, spr_player);
    canvas_draw_xbm(canvas, 60, 31, 8, 8, spr_monsters[MonsterRaccoon]);
    canvas_draw_xbm(canvas, 76, 31, 8, 8, spr_items[ItemCoffee]);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "OK: start / drink coffee");
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Hold Back: quit");
}

static void draw_end(Canvas* canvas, Game* g) {
    char buf[40];
    canvas_set_font(canvas, FontPrimary);
    if(g->state == StateWon) {
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "The Witch is gone!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Forest Park is quiet again.");
        snprintf(buf, sizeof(buf), "Level %d, %d coffees left", g->level, g->coffee);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, buf);
    } else {
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "You got Portlanded");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Bested by a %s", g->killer ? g->killer : "mystery");
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, buf);
        if(g->depth == LEVEL_COUNT - 1) {
            snprintf(buf, sizeof(buf), "at the %s", level_names[g->depth]);
        } else {
            snprintf(buf, sizeof(buf), "on NW %s St", level_names[g->depth]);
        }
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, buf);
    }
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "OK: again   Hold Back: quit");
}

static void draw_game(Canvas* canvas, Game* g) {
    int cam_x = CLAMP(g->px - VIEW_W / 2, MAP_W - VIEW_W, 0);
    int cam_y = CLAMP(g->py - VIEW_H / 2, MAP_H - VIEW_H, 0);

    for(int vy = 0; vy < VIEW_H; vy++) {
        for(int vx = 0; vx < VIEW_W; vx++) {
            int mx = cam_x + vx, my = cam_y + vy;
            if(!g->seen[my][mx]) continue;
            int sx = vx * TILE, sy = vy * TILE;
            switch(g->tiles[my][mx]) {
            case TileWall:
                canvas_draw_xbm(canvas, sx, sy, TILE, TILE, spr_wall);
                break;
            case TileExit:
                canvas_draw_xbm(canvas, sx, sy, TILE, TILE, spr_exit);
                break;
            case TileBartender:
                canvas_draw_xbm(canvas, sx, sy, TILE, TILE, spr_bartender);
                break;
            default:
                // Lit floor gets a dot; remembered floor stays blank
                if(!g->visible[my][mx]) break;
                if(in_bar(g, mx, my)) {
                    // Checkered bar floor
                    canvas_draw_box(canvas, sx, sy, 2, 2);
                    canvas_draw_box(canvas, sx + 4, sy + 4, 2, 2);
                } else {
                    canvas_draw_dot(canvas, sx + 3, sy + 4);
                }
                break;
            }
        }
    }

    for(int i = 0; i < MAX_ITEMS; i++) {
        const Item* it = &g->items[i];
        if(!it->active || !g->seen[it->y][it->x]) continue;
        int vx = it->x - cam_x, vy = it->y - cam_y;
        if(vx < 0 || vy < 0 || vx >= VIEW_W || vy >= VIEW_H) continue;
        canvas_draw_xbm(canvas, vx * TILE, vy * TILE, TILE, TILE, spr_items[it->type]);
    }

    for(int i = 0; i < MAX_MONSTERS; i++) {
        const Monster* m = &g->monsters[i];
        if(!m->alive || !g->visible[m->y][m->x]) continue;
        if(m->type == MonsterSasquatch &&
           MAX(abs(m->x - g->px), abs(m->y - g->py)) > SASQUATCH_SIGHT)
            continue;
        int vx = m->x - cam_x, vy = m->y - cam_y;
        if(vx < 0 || vy < 0 || vx >= VIEW_W || vy >= VIEW_H) continue;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, vx * TILE, vy * TILE, TILE, TILE);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_xbm(
            canvas,
            vx * TILE,
            vy * TILE,
            TILE,
            TILE,
            m->type == MonsterSasquatch ? spr_sasquatch : spr_monsters[m->type]);
    }

    int player_vy = g->py - cam_y;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, (g->px - cam_x) * TILE, player_vy * TILE, TILE, TILE);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, (g->px - cam_x) * TILE, player_vy * TILE, TILE, TILE, spr_player);

    canvas_set_font(canvas, FontSecondary);

    if(g->msg[0]) {
        // Keep the message bar off the player's row
        int bar_y = player_vy < 2 ? HUD_Y - 10 : 0;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, bar_y, 128, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 0, bar_y == 0 ? 9 : bar_y, 127, bar_y == 0 ? 9 : bar_y);
        canvas_draw_str(canvas, 1, bar_y == 0 ? 8 : bar_y + 9, g->msg);
    }

    char hud[40];
    snprintf(
        hud,
        sizeof(hud),
        "HP%d/%d  c%d  %s",
        g->hp,
        g->max_hp,
        g->coffee,
        g->depth == LEVEL_COUNT - 1 ? "Castle" : level_names[g->depth]);
    canvas_draw_box(canvas, 0, HUD_Y, 128, 64 - HUD_Y);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 1, 63, hud);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    Game* g = ctx;
    furi_mutex_acquire(g->mutex, FuriWaitForever);
    canvas_clear(canvas);
    switch(g->state) {
    case StateTitle:
        draw_title(canvas, g);
        break;
    case StatePlaying:
        draw_game(canvas, g);
        break;
    default:
        draw_end(canvas, g);
        break;
    }
    furi_mutex_release(g->mutex);
}

// Title-screen house beat. The speaker has one voice, so each 20 ms tick picks
// the most important sound: kick > clap > hi-hat > bass.
#define MUSIC_TICK_MS 20
#define MUSIC_TICKS_PER_STEP 6 // one 16th note = 120 ms, about 125 BPM
#define MUSIC_STEPS 32 // two bars
#define MUSIC_VOLUME 0.5f

#define N_E3 164.8f
#define N_F3 174.6f
#define N_G3 196.0f
#define N_A3 220.0f
#define N_B3 246.9f
#define N_C4 261.6f

static float title_music_freq(uint32_t tick) {
    // Offbeat bass, two notes after each kick
    static const float bass[MUSIC_STEPS] = {
        0, 0, N_A3, N_A3, 0, 0, N_A3, N_C4, 0, 0, N_A3, N_A3, 0, 0, N_G3, N_E3,
        0, 0, N_F3, N_F3, 0, 0, N_F3, N_A3, 0, 0, N_G3, N_G3, 0, 0, N_G3, N_B3,
    };
    static const float kick[3] = {180.0f, 120.0f, 80.0f};

    uint32_t step = (tick / MUSIC_TICKS_PER_STEP) % MUSIC_STEPS;
    uint32_t sub = tick % MUSIC_TICKS_PER_STEP;

    if(step % 4 == 0) {
        // Four on the floor, with a clap behind beats 2 and 4
        if(sub < 3) return kick[sub];
        if(step % 8 == 4 && sub < 5) return sub == 3 ? 2400.0f : 1700.0f;
        return 0;
    }
    if(step % 4 == 2 && sub == 0) return 7000.0f; // offbeat hi-hat
    if(sub == MUSIC_TICKS_PER_STEP - 1) return 0; // gap so repeated notes re-trigger
    return bass[step];
}

static void title_music_tick(uint32_t tick) {
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) return;
    if(!furi_hal_speaker_is_mine() && !furi_hal_speaker_acquire(0)) return;
    float freq = title_music_freq(tick);
    if(freq > 0) {
        furi_hal_speaker_start(freq, MUSIC_VOLUME);
    } else {
        furi_hal_speaker_stop();
    }
}

// Must be called before any sound effect, which needs the speaker for itself
static void title_music_stop(void) {
    if(!furi_hal_speaker_is_mine()) return;
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 0);
}

int32_t nw_crawl_app(void* p) {
    UNUSED(p);

    Game* g = malloc(sizeof(Game));
    memset(g, 0, sizeof(Game));
    g->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g->state = StateTitle;

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, g);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    g->notifications = furi_record_open(RECORD_NOTIFICATION);

    InputEvent event;
    uint32_t music_tick = 0;
    uint32_t next_tick = furi_get_tick();
    while(true) {
        // Only this thread changes the state, so it is safe to read without the mutex
        uint32_t timeout = FuriWaitForever;
        if(g->state == StateTitle) {
            uint32_t now = furi_get_tick();
            // Resync after time away from the title screen rather than fast-forwarding
            if(now - next_tick > furi_ms_to_ticks(200) && now > next_tick) next_tick = now;
            if(now >= next_tick) {
                title_music_tick(music_tick++);
                next_tick += furi_ms_to_ticks(MUSIC_TICK_MS);
            }
            now = furi_get_tick();
            timeout = next_tick > now ? next_tick - now : 1;
        }
        if(furi_message_queue_get(queue, &event, timeout) != FuriStatusOk) continue;

        if(event.key == InputKeyBack) {
            if(event.type == InputTypeLong) break;
            continue;
        }
        if(event.type != InputTypeShort && event.type != InputTypeRepeat) continue;

        furi_mutex_acquire(g->mutex, FuriWaitForever);
        g->sfx = NULL;
        g->sfx_prio = SfxPrioNone;
        handle_key(g, event.key);
        const NotificationSequence* sfx = g->sfx;
        furi_mutex_release(g->mutex);
        view_port_update(view_port);
        if(g->state != StateTitle) title_music_stop();
        if(sfx) notification_message(g->notifications, sfx);
    }
    title_music_stop();

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(g->mutex);
    free(g);

    return 0;
}
