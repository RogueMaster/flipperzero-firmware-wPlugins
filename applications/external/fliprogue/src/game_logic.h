#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FR_MAP_W 64
#define FR_MAP_H 32
#define FR_VIEW_W 21
#define FR_VIEW_H 8
#define FR_MAX_ACTORS 24
#define FR_MAX_ITEMS 32
#define FR_MAX_TRAPS 8
#define FR_TERRAIN_FIELD_CAP 3
#define FR_INV_CAP 20
#define FR_MAX_FLOORS 18
#define FR_LOG_SIZE 64
#define FR_LABEL_SIZE 16
#define FR_EXPLORED_BYTES ((FR_MAP_W * FR_MAP_H + 7) / 8)
#define FR_FLOOR_EXPLORED_W ((FR_MAP_W + 1) / 2)
#define FR_FLOOR_EXPLORED_H ((FR_MAP_H + 1) / 2)
#define FR_FLOOR_EXPLORED_BYTES ((FR_FLOOR_EXPLORED_W * FR_FLOOR_EXPLORED_H + 7) / 8)
#define FR_MAX_TILE_DELTAS 48

#define FR_TILE_TERRAIN_MASK 0x0F
#define FR_TILE_VISIBLE 0x10
#define FR_TILE_EXPLORED 0x20
#define FR_TILE_HIDDEN_DOOR 0x40

#define FR_DEATH_NONE 0
#define FR_DEATH_KILLED 1
#define FR_DEATH_STARVED 2
#define FR_DEATH_BURNED 3
#define FR_DEATH_POISONED 4

#define FR_EVENT_NONE 0
#define FR_EVENT_DEW 1
#define FR_EVENT_SECRET 2
#define FR_EVENT_MON_PROJECTILE 3
#define FR_EVENT_SNARE 4
#define FR_EVENT_TRAP_SPOTTED 5

#define FR_ACTOR_CHASES 0x01
#define FR_ACTOR_HIDDEN 0x02
#define FR_ACTOR_ASLEEP 0x04
#define FR_ACTOR_ROAMS 0x08

#define FR_INV_EQUIPPED 0x01
#define FR_INV_CURSED 0x02
#define FR_ITEM_FLAG_OPENED 0x40
#define FR_ITEM_FLAG_MIMIC 0x80

typedef enum {
    FR_MODE_TITLE = 0,
    FR_MODE_PLAYING,
    FR_MODE_MENU,
    FR_MODE_INVENTORY,
    FR_MODE_LOOK,
    FR_MODE_TARGET,
    FR_MODE_CLASS_SELECT,
    FR_MODE_GAME_OVER,
    FR_MODE_VICTORY,
    FR_MODE_QUIT,
} FrMode;

typedef enum {
    FR_TERR_VOID = 0,
    FR_TERR_FLOOR = 1,
    FR_TERR_WALL = 2,
    FR_TERR_DOOR_CLOSED = 3,
    FR_TERR_DOOR_OPEN = 4,
    FR_TERR_GRASS = 5,
    FR_TERR_GRASS_TRAMPLED = 6,
    FR_TERR_PUDDLE = 7,
    FR_TERR_SAND = 8,
    FR_TERR_STAIRS_DOWN = 9,
    FR_TERR_STAIRS_UP = 10,
    FR_TERR_WATER = 11,
    FR_TERR_SHRINE = 12,
    FR_TERR_GRATE = 13,
    FR_TERR_BUTTON = 14,
    FR_TERR_ICE = 15,
} FrTerrain;

typedef enum {
    FR_CLASS_WARRIOR = 0,
    FR_CLASS_RANGER = 1,
    FR_CLASS_MAGE = 2,
} FrClass;

typedef enum {
    FR_MON_RAT = 1,
    FR_MON_BAT = 2,
    FR_MON_SNAKE = 3,
    FR_MON_GOBLIN = 4,
    FR_MON_ARCHER = 5,
    FR_MON_SLIME = 6,
    FR_MON_WISP = 7,
    FR_MON_KOBOLD = 8,
    FR_MON_OGRE = 9,
    FR_MON_CUBE = 10,
    FR_MON_DRAGON = 11,
    FR_MON_YONDER_WARDEN = 12,
    FR_MON_WIGHT = 13,
    FR_MON_EEL = 14,
    FR_MON_MIMIC = 15,
    FR_MON_LURKER = 16,
    FR_MON_MAX = FR_MON_LURKER,
} FrMonster;

typedef enum {
    FR_ITEM_NONE = 0,
    FR_ITEM_POTION = 1,
    FR_ITEM_SCROLL = 2,
    FR_ITEM_GOLD = 3,
    FR_ITEM_FOOD = 4,
    FR_ITEM_GEAR = 5,
    FR_ITEM_ARROWS = 6,
    FR_ITEM_WAND = 7,
    FR_ITEM_ORB = 8,
    FR_ITEM_THROWABLE = 9,
    FR_ITEM_TRINKET = 10,
    FR_ITEM_CHEST = 11,
    FR_ITEM_KEY = 12,
} FrItemType;

typedef enum {
    FR_POTION_NONE = 0,
    FR_POTION_HEALING = 1,
    FR_POTION_STRENGTH = 2,
    FR_POTION_ANTIDOTE = 3,
    FR_POTION_FIRE_WARD = 4,
    FR_POTION_QUICKNESS = 5,
    FR_POTION_VENOM = 6,
    FR_POTION_FLAME = 7,
    FR_POTION_FROST = 8,
    FR_POTION_BLINDNESS = 9,
    FR_POTION_SMOKE = 10,
    FR_POTION_MAX = 11,
} FrPotion;

#define FR_POTION_SPEED FR_POTION_QUICKNESS
#define FR_POTION_SLOW FR_POTION_FROST
#define FR_POTION_POISON FR_POTION_VENOM
#define FR_POTION_CONFUSION FR_POTION_SMOKE

typedef enum {
    FR_SCROLL_NONE = 0,
    FR_SCROLL_IDENTIFY = 1,
    FR_SCROLL_ENCHANT_WEAPON = 2,
    FR_SCROLL_ENCHANT_ARMOR = 3,
    FR_SCROLL_FIRE = 4,
    FR_SCROLL_BLINK = 5,
    FR_SCROLL_MAPPING = 6,
    FR_SCROLL_FEAR = 7,
    FR_SCROLL_CHARGE = 8,
    FR_SCROLL_RANDOM_TELEPORT = 9,
    FR_SCROLL_REVEAL = 10,
    FR_SCROLL_DECURSE = 11,
    FR_SCROLL_CALL = 12,
    FR_SCROLL_MAX = 13,
} FrScroll;

typedef enum {
    FR_USE_QUAFF = 1,
    FR_USE_THROW = 2,
    FR_USE_READ = 3,
    FR_USE_EAT = 4,
    FR_USE_ZAP = 5,
    FR_USE_DROP = 6,
    FR_USE_EQUIP = 7,
} FrUseAction;

typedef enum {
    FR_ACTION_NONE = 0,
    FR_ACTION_MOVE,
    FR_ACTION_ATTACK,
    FR_ACTION_RANGED,
    FR_ACTION_BLOCKED,
    FR_ACTION_USE,
    FR_ACTION_DESCEND,
    FR_ACTION_REST,
    FR_ACTION_TRAP,
    FR_ACTION_ZAP,
} FrActionKind;

typedef enum {
    FR_WAND_NONE = 0,
    FR_WAND_FIRE = 1,
    FR_WAND_FROST = 2,
    FR_WAND_FORCE = 3,
    FR_WAND_BLINK = 4,
    FR_WAND_VENOM = 5,
    FR_WAND_FEAR = 6,
    FR_WAND_ARC = 7,
    FR_WAND_REVEAL = 8,
    FR_WAND_MAX = 9,
} FrWand;

#define FR_WAND_SPARK FR_WAND_FIRE
#define FR_WAND_SLOW FR_WAND_FROST
#define FR_WAND_MARK FR_WAND_VENOM

typedef enum {
    FR_THROW_NONE = 0,
    FR_THROW_STONE = 1,
    FR_THROW_DART = 2,
} FrThrowable;

typedef enum {
    FR_TRINKET_NONE = 0,
    FR_TRINKET_DEW = 1,
    FR_TRINKET_ASH = 2,
    FR_TRINKET_SCOUT = 3,
    FR_TRINKET_FANG = 4,
    FR_TRINKET_QUARTZ = 5,
    FR_TRINKET_LOCKET = 6,
    FR_TRINKET_CINDER = 7,
    FR_TRINKET_GLASS = 8,
    FR_TRINKET_HUNGRY = 9,
    FR_TRINKET_MAX = 10,
} FrTrinket;

typedef enum {
    FR_DAMAGE_MELEE = 0,
    FR_DAMAGE_PROJECTILE = 1,
    FR_DAMAGE_THROWN = 2,
    FR_DAMAGE_BURST = 3,
    FR_DAMAGE_DOT = 4,
} FrDamageKind;

typedef enum {
    FR_TRAP_NONE = 0,
    FR_TRAP_SNARE = 1,
    FR_TRAP_FIRE = 2,
    FR_TRAP_POISON = 3,
    FR_TRAP_ARROW = 4,
} FrTrapType;

typedef enum {
    FR_HUNGER_OK = 0,
    FR_HUNGER_HUNGRY = 1,
    FR_HUNGER_STARVING = 2,
} FrHungerState;

enum {
    FR_FX_BURNING_INDEX = 0,
    FR_FX_POISONED_INDEX = 1,
    FR_FX_STUNNED_INDEX = 2,
    FR_FX_SLOWED_INDEX = 3,
    FR_FX_AFRAID_INDEX = 4,
    FR_FX_BLIND_INDEX = 5,
    FR_FX_CONFUSED_INDEX = 6,
    FR_FX_MARKED_INDEX = 7,
};

#define FR_FX_BURNING (1u << FR_FX_BURNING_INDEX)
#define FR_FX_POISONED (1u << FR_FX_POISONED_INDEX)
#define FR_FX_STUNNED (1u << FR_FX_STUNNED_INDEX)
#define FR_FX_SLOWED (1u << FR_FX_SLOWED_INDEX)
#define FR_FX_AFRAID (1u << FR_FX_AFRAID_INDEX)
#define FR_FX_BLIND (1u << FR_FX_BLIND_INDEX)
#define FR_FX_CONFUSED (1u << FR_FX_CONFUSED_INDEX)
#define FR_FX_MARKED (1u << FR_FX_MARKED_INDEX)

#define FR_SKILL_SLOT1 0x01
#define FR_SKILL_SLOT2 0x02

#define FR_PERK_1 0x01
#define FR_PERK_2 0x02
#define FR_PERK_3 0x04
#define FR_PERK_4 0x08
#define FR_PERK_5 0x10

typedef struct {
    uint8_t kind;
} FrActionResult;

typedef struct {
    uint8_t type;
    uint8_t subtype;
    uint8_t amount;
    uint8_t flags;
} FrInvSlot;

typedef struct {
    uint8_t class_id;
    uint8_t x;
    uint8_t y;
    uint8_t hp;
    uint8_t max_hp;
    uint8_t level;
    uint8_t xp;
    uint8_t str;
    uint8_t dex;
    uint8_t wil;
    uint8_t sword_lvl;
    uint8_t dagger_lvl;
    uint8_t bow_lvl;
    uint8_t staff_lvl;
    uint8_t shield_lvl;
    uint8_t head_lvl;
    uint8_t body_lvl;
    uint8_t feet_lvl;
    uint8_t food;
    uint16_t gold;
    uint8_t hunger;
    uint8_t arrows;
    uint8_t charges;
    uint16_t known_potions;
    uint16_t known_scrolls;
    uint16_t known_trinkets;
    uint8_t skills;
    uint8_t perks;
    uint8_t pending_perks;
    uint8_t effects;
    uint8_t fx_timer[8];
    uint8_t fire_ward_timer;
    uint8_t inv_count;
    uint8_t has_orb;
    uint8_t cube_hp;
    uint8_t cube_max_hp;
    FrInvSlot inv[FR_INV_CAP];
} FrPlayer;

typedef struct {
    bool active;
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint8_t hp;
    uint8_t max_hp;
    uint8_t dmg;
    uint8_t effects;
    uint8_t fx_timer[8];
    uint8_t cooldown;
    uint8_t target_x;
    uint8_t target_y;
    int8_t target_dx;
    int8_t target_dy;
    uint8_t memory;
    uint8_t flags;
    uint8_t pack_id;
} FrActor;

typedef struct {
    bool active;
    uint8_t type;
    uint8_t subtype;
    uint8_t x;
    uint8_t y;
    uint8_t amount;
    uint8_t flags;
} FrItem;

typedef struct {
    uint8_t type;
    uint8_t subtype;
    uint8_t x;
    uint8_t y;
    uint8_t amount;
    uint8_t flags;
} FrSavedItem;

typedef struct {
    bool active;
    uint8_t x;
    uint8_t y;
    uint8_t type;
    uint8_t hidden;
    uint8_t source_x;
    uint8_t source_y;
    int8_t dir_x;
    int8_t dir_y;
} FrTrap;

typedef enum {
    FR_TERRAIN_FIELD_NONE = 0,
    FR_TERRAIN_FIELD_FIRE = 1,
    FR_TERRAIN_FIELD_ICE = 2,
} FrTerrainFieldType;

typedef struct {
    bool active;
    uint8_t type;
    uint8_t floor;
    uint8_t age;
    uint8_t ttl;
    uint8_t cells[FR_EXPLORED_BYTES];
} FrTerrainField;

typedef struct {
    bool active;
    uint8_t type;
    uint8_t x;
    uint8_t y;
    uint8_t hp;
    uint8_t effects;
    uint8_t fx_timer[8];
    uint8_t flags;
    uint8_t pack_id;
    uint8_t cooldown;
} FrSavedActor;

typedef struct {
    bool generated;
    uint8_t up_x;
    uint8_t up_y;
    uint8_t down_x;
    uint8_t down_y;
    uint8_t tile_delta_count;
    uint8_t explored[FR_FLOOR_EXPLORED_BYTES];
    uint16_t tile_delta_pos[FR_MAX_TILE_DELTAS];
    uint8_t tile_delta_tile[FR_MAX_TILE_DELTAS];
    FrSavedActor actors[FR_MAX_ACTORS];
    FrSavedItem items[FR_MAX_ITEMS];
    FrTrap traps[FR_MAX_TRAPS];
} FrFloorState;

typedef struct {
    uint8_t tiles[FR_MAP_H][FR_MAP_W];
    FrPlayer player;
    FrActor actors[FR_MAX_ACTORS];
    FrItem items[FR_MAX_ITEMS];
    FrTrap traps[FR_MAX_TRAPS];
    FrTerrainField terrain_fields[FR_TERRAIN_FIELD_CAP];
    FrFloorState floors[FR_MAX_FLOORS];
    uint8_t floor;
    uint32_t seed;
    uint32_t run_seed;
    uint32_t turn;
    uint8_t mode;
    uint8_t death_cause;
    uint8_t last_event;
    uint8_t event_x;
    uint8_t event_y;
    uint8_t event_tx;
    uint8_t event_ty;
    char event_glyph;
    uint8_t suppress_deltas;
    uint8_t suppress_floor_state_save;
    char log[FR_LOG_SIZE];
    char death_log[FR_LOG_SIZE];
    char potion_labels[FR_POTION_MAX][FR_LABEL_SIZE];
    char scroll_labels[FR_SCROLL_MAX][FR_LABEL_SIZE];
    char trinket_labels[FR_TRINKET_MAX][FR_LABEL_SIZE];
} FrGame;

void fr_game_init(FrGame* game, uint32_t seed);
void fr_game_init_class(FrGame* game, uint32_t seed, uint8_t class_id);
void fr_generate_floor(FrGame* game, uint8_t floor);

uint8_t fr_get_terrain(const FrGame* game, uint8_t x, uint8_t y);
void fr_set_terrain(FrGame* game, uint8_t x, uint8_t y, uint8_t terrain);
bool fr_find_first_tile(const FrGame* game, uint8_t terrain, uint8_t* out_x, uint8_t* out_y);

uint8_t fr_count_active_actors(const FrGame* game);
uint8_t fr_count_active_items(const FrGame* game);
FrActor* fr_spawn_actor(FrGame* game, uint8_t type, uint8_t x, uint8_t y);
FrActor* fr_actor_at(FrGame* game, uint8_t x, uint8_t y);

bool fr_place_trap(FrGame* game, uint8_t x, uint8_t y, uint8_t type);
FrTrap* fr_trap_at(FrGame* game, uint8_t x, uint8_t y);
bool fr_player_sees_traps(const FrGame* game);
bool fr_player_knows_scrolls(const FrGame* game);
bool fr_player_can_ranged(const FrGame* game);
bool fr_actor_visible_to_player(const FrGame* game, const FrActor* actor);
bool fr_player_detects_trap(const FrGame* game, uint8_t x, uint8_t y);
uint8_t fr_hidden_trap_detection_chance(const FrGame* game);
bool fr_search_nearby(FrGame* game);
bool fr_path_exists(const FrGame* game, uint8_t start_x, uint8_t start_y, uint8_t goal_x, uint8_t goal_y);

bool fr_add_inventory(FrGame* game, uint8_t type, uint8_t subtype, uint8_t amount);

#include "item_use_actions.h"
#include "player_actions.h"

FrActionResult fr_descend(FrGame* game);
FrActionResult fr_ascend(FrGame* game);
void fr_auto_close_doors(FrGame* game);
void fr_update_fov(FrGame* game);
uint8_t fr_hunger_state(const FrGame* game);

char fr_tile_glyph(const FrGame* game, uint8_t x, uint8_t y);
char fr_actor_glyph(uint8_t type);
char fr_item_glyph(uint8_t type);
const char* fr_actor_name(uint8_t type);
const char* fr_actor_log_name(uint8_t type);
const char* fr_actor_flavor(uint8_t type);
const char* fr_item_name(const FrInvSlot* slot);
const char* fr_inventory_label(const FrGame* game, const FrInvSlot* slot);
const char* fr_inventory_hint(const FrGame* game, const FrInvSlot* slot);
const char* fr_potion_label(const FrGame* game, uint8_t potion);
const char* fr_scroll_label(const FrGame* game, uint8_t scroll);
const char* fr_trinket_label(const FrGame* game, uint8_t trinket);
const char* fr_wand_label(uint8_t wand, uint8_t charges, uint8_t max_charges);
const char* fr_terrain_name(uint8_t terrain);
const char* fr_player_effects_label(const FrGame* game);

#include "progression.h"
#include "run_report.h"
