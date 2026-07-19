#include "game.h"

#include "generation.h"
#include "intro.h"
#include "plate.h"
#include "random.h"
#include "wyrm.h"

static void SelectOutermostPlayerPlate(GameState* gameState) {
    if(gameState->player.length == 0) {
        gameState->selectedPlayerPlate = 0;
    } else {
        gameState->selectedPlayerPlate = gameState->player.length - 1;
    }
}

int GetMinimumAttackingPlateIndex(const GameState* gameState) {
    int minimumIndex = gameState->player.length - MAX_ATTACKING_PLATES;
    return minimumIndex < 0 ? 0 : minimumIndex;
}

static void StartEnemy(GameState* gameState) {
    gameState->enemy = MakeEnemy(gameState->round, &gameState->randomGenerator);
    gameState->enemyReward = CalculateEnemyReward(&gameState->enemy);
    SelectOutermostPlayerPlate(gameState);
    gameState->mode = GameModePlayerSelect;
}

static void StartShop(GameState* gameState) {
    FillShop(
        gameState->shopPlates,
        &gameState->shopLength,
        &gameState->randomGenerator
    );

    gameState->selectedShopItem = 0;
    gameState->pendingShopIndex = 0;
    gameState->selectedPlacement = PlatePlacementOuter;
    gameState->shopMessage = ShopMessageNone;
    gameState->mode = GameModeShopSelect;
}

static void RemoveShopPlate(GameState* gameState, int shopIndex) {
    for(int index = shopIndex; index < gameState->shopLength - 1; index++) {
        gameState->shopPlates[index] = gameState->shopPlates[index + 1];
    }

    gameState->shopLength--;

    if(gameState->shopLength == 0) {
        gameState->selectedShopItem = 0;
    } else if(gameState->selectedShopItem >= gameState->shopLength) {
        gameState->selectedShopItem = gameState->shopLength - 1;
    }
}

static void HandlePlayerSelection(GameState* gameState, GameAction action) {
    int minimumIndex = GetMinimumAttackingPlateIndex(gameState);
    int maximumIndex = gameState->player.length - 1;

    if(action == GameActionLeft && gameState->selectedPlayerPlate > minimumIndex) {
        gameState->selectedPlayerPlate--;
    } else if(
        action == GameActionRight &&
        gameState->selectedPlayerPlate < maximumIndex
    ) {
        gameState->selectedPlayerPlate++;
    } else if(action == GameActionConfirm) {
        gameState->latestAttack = ResolveAttack(
            &gameState->player,
            gameState->selectedPlayerPlate,
            &gameState->enemy
        );

        gameState->mode = GameModePlayerAttackResult;
    }
}

static void AdvanceAfterPlayerAttack(GameState* gameState) {
    if(gameState->enemy.length == 0) {
        gameState->coins += gameState->enemyReward;
        gameState->round++;
        StartShop(gameState);
        return;
    }

    int enemyAttackingPlate = gameState->enemy.length - 1;

    gameState->latestAttack = ResolveAttack(
        &gameState->enemy,
        enemyAttackingPlate,
        &gameState->player
    );

    gameState->mode = GameModeEnemyAttackResult;
}

static void AdvanceAfterEnemyAttack(GameState* gameState) {
    if(gameState->player.length == 0) {
        gameState->mode = GameModeGameOver;
        return;
    }

    SelectOutermostPlayerPlate(gameState);
    gameState->mode = GameModePlayerSelect;
}

static void HandleShopSelection(GameState* gameState, GameAction action) {
    int leaveShopIndex = gameState->shopLength;

    if(action == GameActionUp && gameState->selectedShopItem > 0) {
        gameState->selectedShopItem--;
        gameState->shopMessage = ShopMessageNone;
    } else if(
        action == GameActionDown &&
        gameState->selectedShopItem < leaveShopIndex
    ) {
        gameState->selectedShopItem++;
        gameState->shopMessage = ShopMessageNone;
    } else if(action == GameActionConfirm) {
        if(gameState->selectedShopItem == leaveShopIndex) {
            StartEnemy(gameState);
            return;
        }

        const Plate* selectedPlate =
            &gameState->shopPlates[gameState->selectedShopItem];

        if(gameState->player.length >= MAX_PLATES) {
            gameState->shopMessage = ShopMessageWyrmFull;
        } else if(gameState->coins < selectedPlate->price) {
            gameState->shopMessage = ShopMessageCannotAfford;
        } else {
            gameState->pendingShopIndex = gameState->selectedShopItem;
            gameState->selectedPlacement = PlatePlacementOuter;
            gameState->shopMessage = ShopMessageNone;
            gameState->mode = GameModeShopPlacement;
        }
    }
}

static void HandleShopPlacement(GameState* gameState, GameAction action) {
    if(action == GameActionLeft || action == GameActionUp) {
        gameState->selectedPlacement = PlatePlacementInner;
    } else if(action == GameActionRight || action == GameActionDown) {
        gameState->selectedPlacement = PlatePlacementOuter;
    } else if(action == GameActionCancel) {
        gameState->mode = GameModeShopSelect;
    } else if(action == GameActionConfirm) {
        Plate selectedPlate = gameState->shopPlates[gameState->pendingShopIndex];
        bool plateAdded = false;

        if(gameState->selectedPlacement == PlatePlacementInner) {
            plateAdded = AddInnerPlate(&gameState->player, selectedPlate);
        } else {
            plateAdded = AddOuterPlate(&gameState->player, selectedPlate);
        }

        if(!plateAdded) {
            gameState->shopMessage = ShopMessageWyrmFull;
            gameState->mode = GameModeShopSelect;
            return;
        }

        gameState->coins -= selectedPlate.price;
        RemoveShopPlate(gameState, gameState->pendingShopIndex);
        gameState->shopMessage = ShopMessageBought;
        gameState->mode = GameModeShopSelect;
    }
}

void InitializeGame(GameState* gameState, uint32_t randomSeed) {
    *gameState = (GameState){0};

    RandomInitialize(&gameState->randomGenerator, randomSeed);

    Plate startingPlate = MakePlate(MaterialSteel, ModifierStandard);

    for(int plateIndex = 0; plateIndex < 4; plateIndex++) {
        AddOuterPlate(&gameState->player, startingPlate);
    }

    StartEnemy(gameState);
    gameState->introParagraph = 0;
    gameState->mode = GameModeTitle;
}

void HandleGameAction(GameState* gameState, GameAction action) {
    switch(gameState->mode) {
    case GameModeTitle:
        if(action == GameActionConfirm) {
            gameState->mode = GameModeIntro;
        }
        break;

    case GameModeIntro:
        if(action == GameActionConfirm) {
            if(
                gameState->introParagraph <
                GetIntroParagraphCount() - 1
            ) {
                gameState->introParagraph++;
            } else {
                gameState->mode = GameModePlayerSelect;
            }
        }
        if (action == GameActionCancel) {
            gameState->mode = GameModePlayerSelect;
        }
        break;

    case GameModePlayerSelect:
        HandlePlayerSelection(gameState, action);
        break;

    case GameModePlayerAttackResult:
        if(action == GameActionConfirm) {
            AdvanceAfterPlayerAttack(gameState);
        }
        break;

    case GameModeEnemyAttackResult:
        if(action == GameActionConfirm) {
            AdvanceAfterEnemyAttack(gameState);
        }
        break;

    case GameModeShopSelect:
        HandleShopSelection(gameState, action);
        break;

    case GameModeShopPlacement:
        HandleShopPlacement(gameState, action);
        break;

    case GameModeGameOver:
        if(action == GameActionConfirm) {
            uint32_t nextSeed = RandomNext(&gameState->randomGenerator);
            InitializeGame(gameState, nextSeed);
        }
        break;
    }
}
