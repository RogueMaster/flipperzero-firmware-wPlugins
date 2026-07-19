#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    MAX_PLATES = 9,
    SHOP_CAPACITY = 6,
    MAX_ATTACKING_PLATES = 3,
};

typedef enum {
    MaterialCardboard,
    MaterialWood,
    MaterialBone,
    MaterialCopper,
    MaterialAluminium,
    MaterialCastIron,
    MaterialBrass,
    MaterialBronze,
    MaterialSteel,
    MaterialCeramic,
    MaterialTitanium,
    MaterialTungsten,
    MaterialIdCount,
} MaterialId;

typedef enum {
    ModifierCracked,
    ModifierBlunt,
    ModifierCorroded,
    ModifierFlawed,
    ModifierWorn,
    ModifierStandard,
    ModifierRefined,
    ModifierTempered,
    ModifierHardened,
    ModifierReinforced,
    ModifierSerrated,
    ModifierMasterwork,
    ModifierIdCount,
} ModifierId;

typedef struct {
    const char* name;
    int8_t integrity;
    int8_t power;
    int8_t price;
} Material;

typedef struct {
    const char* name;
    int8_t integrity;
    int8_t power;
    int8_t price;
} Modifier;

typedef struct {
    uint8_t materialIndex;
    uint8_t modifierIndex;
    uint8_t integrity;
    uint8_t power;
    uint8_t price;
} Plate;

typedef struct {
    Plate plates[MAX_PLATES];
    uint8_t length;
} Wyrm;

typedef struct {
    uint32_t state;
} RandomGenerator;

typedef enum {
    GameModeTitle,
    GameModeIntro,
    GameModePlayerSelect,
    GameModePlayerAttackResult,
    GameModeEnemyAttackResult,
    GameModeShopSelect,
    GameModeShopPlacement,
    GameModeGameOver,
} GameMode;

typedef enum {
    GameActionNone,
    GameActionUp,
    GameActionDown,
    GameActionLeft,
    GameActionRight,
    GameActionConfirm,
    GameActionCancel,
} GameAction;

typedef enum {
    PlatePlacementInner,
    PlatePlacementOuter,
} PlatePlacement;

typedef enum {
    ShopMessageNone,
    ShopMessageCannotAfford,
    ShopMessageWyrmFull,
    ShopMessageBought,
} ShopMessage;
