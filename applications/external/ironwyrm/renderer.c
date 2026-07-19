#include "renderer.h"

#include <stdio.h>
#include <string.h>

#include "core/data.h"
#include "core/intro.h"
#include "font3x5.h"
#include "font5x8.h"
#include "ironwyrm_icons.h"

enum {
    ScreenWidth = 128,

    CoreWidth = 8,
    PlateWidth = 5,
    BoxWidth = 4,

    PlayerCoreX = 0,
    PlayerLabelY = 0,
    PlayerIntegrityY = 10,
    PlayerIconY = 17,
    PlayerPowerY = 27,
    PlayerBoxY = 34,

    EnemyCoreX = 120,
    EnemyLabelY = 56,
    EnemyIntegrityY = 32,
    EnemyIconY = 39,
    EnemyPowerY = 49,
};

typedef enum {
    TextFont3x5,
    TextFont5x8,
    TextFontPrimary,
    TextFontSecondary,
} TextFont;

static void DrawChar3x5(Canvas* canvas, int x, int y, char character) {
    int glyphIndex = Font3x5GlyphIndex(character);

    if(glyphIndex < 0) {
        return;
    }

    for(int row = 0; row < FONT3X5_HEIGHT; row++) {
        uint8_t rowBits = font3x5[glyphIndex][row];

        for(int column = 0; column < FONT3X5_WIDTH; column++) {
            if(rowBits & (1U << (2 - column))) {
                canvas_draw_dot(canvas, x + column, y + row);
            }
        }
    }
}

static int TextWidth3x5(const char* text) {
    int characterCount = (int)strlen(text);

    if(characterCount == 0) {
        return 0;
    }

    return characterCount * (FONT3X5_WIDTH + 1) - 1;
}

static int TextWidth5x8(const char* text) {
    int characterCount = (int)strlen(text);

    if(characterCount == 0) {
        return 0;
    }

    return characterCount * (FONT5X8_WIDTH + 1) - 1;
}

static int TextWidth(Canvas* canvas, const char* text, TextFont font) {
    if(font == TextFont3x5) {
        return TextWidth3x5(text);
    }

    if(font == TextFont5x8) {
        return TextWidth5x8(text);
    }

    canvas_set_font(
        canvas,
        font == TextFontPrimary ? FontPrimary : FontSecondary
    );

    return canvas_string_width(canvas, text);
}

static void DrawText(
    Canvas* canvas,
    int x,
    int y,
    const char* text,
    TextFont font
) {
    if(font == TextFont3x5) {
        while(*text != '\0') {
            if(*text != ' ') {
                DrawChar3x5(canvas, x, y, *text);
            }

            x += FONT3X5_WIDTH + 1;
            text++;
        }

        return;
    }

    if(font == TextFont5x8) {
        while(*text != '\0') {
            int glyphIndex = Font5x8GlyphIndex(*text);

            if(glyphIndex >= 0) {
                for(int row = 0; row < FONT5X8_HEIGHT; row++) {
                    uint8_t rowBits = font5x8[glyphIndex][row];

                    for(int column = 0; column < FONT5X8_WIDTH; column++) {
                        if(rowBits & (1U << (4 - column))) {
                            canvas_draw_dot(canvas, x + column, y + row);
                        }
                    }
                }
            }

            x += FONT5X8_WIDTH + 1;
            text++;
        }

        return;
    }

    canvas_set_font(
        canvas,
        font == TextFontPrimary ? FontPrimary : FontSecondary
    );
    canvas_draw_str(canvas, x, y, text);
}

static void DrawTextCentered(
    Canvas* canvas,
    int centreX,
    int y,
    const char* text,
    TextFont font
) {
    DrawText(
        canvas,
        centreX - TextWidth(canvas, text, font) / 2,
        y,
        text,
        font
    );
}

static void DrawTextRight(
    Canvas* canvas,
    int rightX,
    int y,
    const char* text,
    TextFont font
) {
    DrawText(
        canvas,
        rightX - TextWidth(canvas, text, font),
        y,
        text,
        font
    );
}

static void DrawTextClipped(
    Canvas* canvas,
    int x,
    int y,
    const char* text,
    int maximumX,
    TextFont font
) {
    if(font == TextFont3x5) {
        while(*text != '\0' && x + FONT3X5_WIDTH - 1 <= maximumX) {
            if(*text != ' ') {
                DrawChar3x5(canvas, x, y, *text);
            }

            x += FONT3X5_WIDTH + 1;
            text++;
        }

        return;
    }

    if(x + TextWidth(canvas, text, font) <= maximumX) {
        DrawText(canvas, x, y, text, font);
    }
}

static void DrawIntegerCentered(
    Canvas* canvas,
    int centreX,
    int y,
    int value
) {
    char buffer[12];

    snprintf(buffer, sizeof(buffer), "%d", value);

    DrawTextCentered(
        canvas,
        centreX,
        y,
        buffer,
        TextFont3x5
    );
}

static void DrawIntroParagraph(Canvas* canvas, const char* paragraph) {
    char line[64] = {0};
    char word[64];
    int lineLength = 0;
    int y = 0;
    const char* cursor = paragraph;

    while(*cursor != '\0') {
        while(*cursor == ' ') {
            cursor++;
        }

        if(*cursor == '\n') {
            if(lineLength > 0) {
                DrawText(canvas, 0, y, line, TextFont3x5);

                lineLength = 0;
                line[0] = '\0';
            }

            /*
             * Every newline advances one line.
             * Therefore "\n\n" leaves one blank line between paragraphs.
             */
            y += FONT3X5_HEIGHT + 1;

            cursor++;
            continue;
        }

        const char* wordStart = cursor;

        while(*cursor != '\0' && *cursor != ' ' && *cursor != '\n') {
            cursor++;
        }

        int wordLength = (int)(cursor - wordStart);

        if(wordLength >= (int)sizeof(word)) {
            wordLength = sizeof(word) - 1;
        }

        memcpy(word, wordStart, (size_t)wordLength);
        word[wordLength] = '\0';

        int wordWidth = TextWidth3x5(word);
        int separatorWidth =
            lineLength > 0 ? FONT3X5_WIDTH + 1 : 0;

        if(
            lineLength > 0 &&
            TextWidth3x5(line) + separatorWidth + wordWidth > ScreenWidth
        ) {
            DrawText(canvas, 0, y, line, TextFont3x5);

            y += FONT3X5_HEIGHT + 1;
            lineLength = 0;
            line[0] = '\0';
            separatorWidth = 0;
        }

        if(separatorWidth > 0) {
            line[lineLength++] = ' ';
        }

        memcpy(
            &line[lineLength],
            word,
            (size_t)wordLength + 1
        );

        lineLength += wordLength;
    }

    if(lineLength > 0) {
        DrawText(canvas, 0, y, line, TextFont3x5);
    }
}

static void DrawIntroScreen(
    Canvas* canvas,
    const GameState* gameState
) {
    DrawIntroParagraph(
        canvas,
        introParagraphs[gameState->introParagraph]
    );

    DrawTextRight(
        canvas,
        ScreenWidth,
        59,
        "PRESS OK TO CONTINUE",
        TextFont3x5
    );
}

static void GetPlateName(
    const Plate* plate,
    char* buffer,
    size_t bufferSize,
    bool includePlateWord
) {
    const Modifier* modifier = &modifiers[plate->modifierIndex];
    const Material* material = &materials[plate->materialIndex];

    snprintf(
        buffer,
        bufferSize,
        includePlateWord ? "%s %s Plate" : "%s %s",
        modifier->name,
        material->name
    );
}

static int GetPlateX(int coreX, int direction, int plateIndex) {
    if(direction > 0) {
        return coreX + CoreWidth + plateIndex * PlateWidth;
    }

    return coreX - (plateIndex + 1) * PlateWidth;
}

static void DrawWyrm(
    Canvas* canvas,
    const Wyrm* wyrm,
    const Icon* plateIcon,
    const Icon* destroyedPlateIcon,
    int destroyedPlateCount,
    int coreX,
    int iconY,
    int integrityY,
    int powerY,
    int direction,
    int statLabelOffsetX,
    bool showAttackSelectors,
    int selectedPlate,
    int minimumSelectablePlate,
    int boxY
) {
    canvas_draw_icon(canvas, coreX, iconY, &I_core);

    DrawText(
        canvas,
        coreX + (CoreWidth - FONT3X5_WIDTH) / 2 + statLabelOffsetX,
        integrityY,
        "I",
        TextFont3x5
    );

    DrawText(
        canvas,
        coreX + (CoreWidth - FONT3X5_WIDTH) / 2 + statLabelOffsetX,
        powerY,
        "P",
        TextFont3x5
    );

    for(
        int plateIndex = 0;
        plateIndex < wyrm->length + destroyedPlateCount;
        plateIndex++
    ) {
        int plateX = GetPlateX(coreX, direction, plateIndex);

        if(plateIndex >= wyrm->length) {
            canvas_draw_icon(canvas, plateX, iconY, destroyedPlateIcon);
            continue;
        }

        const Plate* plate = &wyrm->plates[plateIndex];

        canvas_draw_icon(canvas, plateX, iconY, plateIcon);

        DrawIntegerCentered(
            canvas,
            plateX + PlateWidth / 2,
            integrityY,
            plate->integrity
        );

        DrawIntegerCentered(
            canvas,
            plateX + PlateWidth / 2,
            powerY,
            plate->power
        );

        if(showAttackSelectors && plateIndex >= minimumSelectablePlate) {
            int boxX = plateX + (PlateWidth - BoxWidth) / 2;

            canvas_draw_icon(canvas, boxX, boxY, &I_box);

            if(plateIndex == selectedPlate) {
                canvas_draw_icon(
                    canvas,
                    boxX + 1,
                    boxY + 1,
                    &I_selector
                );
            }
        }
    }
}

static void DrawCombatStatus(
    Canvas* canvas,
    const GameState* gameState
) {
    char status[32];

    switch(gameState->mode) {
    case GameModePlayerSelect:
        DrawText(canvas, 0, 56, "CHOOSE PLATE", TextFont5x8);
        break;

    case GameModePlayerAttackResult:
        if(gameState->latestAttack.defenderDestroyed) {
            DrawText(
                canvas,
                0,
                56,
                "ENEMY DESTROYED",
                TextFont5x8
            );
        } else {
            snprintf(
                status,
                sizeof(status),
                "DEALT %u DMG",
                gameState->latestAttack.initialDamage
            );

            DrawText(canvas, 0, 56, status, TextFont5x8);
        }
        break;

    case GameModeEnemyAttackResult:
        if(gameState->latestAttack.defenderDestroyed) {
            DrawText(
                canvas,
                0,
                56,
                "WYRM DESTROYED",
                TextFont5x8
            );
        } else {
            snprintf(
                status,
                sizeof(status),
                "TOOK %u DMG",
                gameState->latestAttack.initialDamage
            );

            DrawText(canvas, 0, 56, status, TextFont5x8);
        }
        break;

    default:
        break;
    }
}

static void DrawCombatScreen(
    Canvas* canvas,
    const GameState* gameState
) {
    DrawText(canvas, 0, PlayerLabelY, "PLAYER", TextFont5x8);

    if(gameState->player.length > 0) {
        const Plate* selectedPlate =
            &gameState->player.plates[
                gameState->selectedPlayerPlate
            ];

        DrawTextRight(
            canvas,
            ScreenWidth,
            PlayerLabelY,
            modifiers[selectedPlate->modifierIndex].name,
            TextFont5x8
        );

        DrawTextRight(
            canvas,
            ScreenWidth,
            PlayerLabelY + FONT5X8_HEIGHT + 1,
            materials[selectedPlate->materialIndex].name,
            TextFont5x8
        );

        DrawTextRight(
            canvas,
            ScreenWidth,
            PlayerLabelY + 2 * (FONT5X8_HEIGHT + 1),
            "Plate",
            TextFont5x8
        );
    }

    DrawWyrm(
        canvas,
        &gameState->player,
        &I_plate,
        &I_destroyed,
        gameState->mode == GameModeEnemyAttackResult ?
            gameState->latestAttack.platesDestroyed : 0,
        PlayerCoreX,
        PlayerIconY,
        PlayerIntegrityY,
        PlayerPowerY,
        1,
        1,
        true,
        gameState->selectedPlayerPlate,
        GetMinimumAttackingPlateIndex(gameState),
        PlayerBoxY
    );

    DrawTextRight(
        canvas,
        ScreenWidth,
        EnemyLabelY,
        "ENEMY",
        TextFont5x8
    );

    DrawWyrm(
        canvas,
        &gameState->enemy,
        &I_plate2,
        &I_destroyed2,
        gameState->mode == GameModePlayerAttackResult ?
            gameState->latestAttack.platesDestroyed : 0,
        EnemyCoreX,
        EnemyIconY,
        EnemyPowerY,
        EnemyIntegrityY,
        -1,
        0,
        false,
        0,
        0,
        0
    );

    DrawCombatStatus(canvas, gameState);
}

static const char* GetShopMessageText(ShopMessage message) {
    switch(message) {
    case ShopMessageCannotAfford:
        return "CANT AFFORD";

    case ShopMessageWyrmFull:
        return "WYRM FULL";

    case ShopMessageBought:
        return "BOUGHT";

    case ShopMessageNone:
    default:
        return "PLATE";
    }
}

static void DrawShopScreen(
    Canvas* canvas,
    const GameState* gameState
) {
    char coinsText[32];

    snprintf(
        coinsText,
        sizeof(coinsText),
        "SHOP - %d\xA2",
        gameState->coins
    );

    DrawText(canvas, 40, 1, coinsText, TextFont5x8);

    DrawText(
        canvas,
        8,
        10,
        GetShopMessageText(gameState->shopMessage),
        TextFont3x5
    );

    DrawTextRight(
        canvas,
        ScreenWidth - 2,
        10,
        "I P \xA2",
        TextFont3x5
    );

    int totalItems = gameState->shopLength + 1;

    for(int itemIndex = 0; itemIndex < totalItems; itemIndex++) {
        int rowY = 17 + itemIndex * 7;

        canvas_draw_icon(canvas, 2, rowY, &I_box);

        if(itemIndex == gameState->selectedShopItem) {
            canvas_draw_icon(
                canvas,
                3,
                rowY + 1,
                &I_selector
            );
        }

        if(itemIndex == gameState->shopLength) {
            DrawText(
                canvas,
                8,
                rowY,
                "LEAVE SHOP",
                TextFont3x5
            );

            continue;
        }

        const Plate* plate =
            &gameState->shopPlates[itemIndex];

        char plateName[40];
        char stats[24];

        GetPlateName(
            plate,
            plateName,
            sizeof(plateName),
            false
        );

        snprintf(
            stats,
            sizeof(stats),
            "%u %u %u",
            plate->integrity,
            plate->power,
            plate->price
        );

        DrawTextClipped(
            canvas,
            8,
            rowY,
            plateName,
            92,
            TextFont3x5
        );

        DrawTextRight(
            canvas,
            ScreenWidth - 2,
            rowY,
            stats,
            TextFont3x5
        );
    }
}

static void DrawPlacementScreen(
    Canvas* canvas,
    const GameState* gameState
) {
    const Plate* pendingPlate =
        &gameState->shopPlates[
            gameState->pendingShopIndex
        ];

    char plateName[48];
    char details[32];

    GetPlateName(
        pendingPlate,
        plateName,
        sizeof(plateName),
        true
    );

    snprintf(
        details,
        sizeof(details),
        "I%u P%u \xA2%u",
        pendingPlate->integrity,
        pendingPlate->power,
        pendingPlate->price
    );

    DrawTextClipped(
        canvas,
        0,
        0,
        plateName,
        ScreenWidth,
        TextFont3x5
    );

    DrawText(canvas, 0, 7, details, TextFont3x5);

    DrawText(canvas, 0, 16, "WHERE WOULD YOU LIKE", TextFont3x5);
    DrawText(canvas, 0, 22, "TO PUT THE PLATE?", TextFont3x5);

    canvas_draw_icon(canvas, 0, 31, &I_box);

    if(gameState->selectedPlacement == PlatePlacementInner) {
        canvas_draw_icon(canvas, 1, 32, &I_selector);
    }

    DrawText(canvas, 8, 31, "INNER", TextFont3x5);

    canvas_draw_icon(canvas, 35, 31, &I_box);

    if(gameState->selectedPlacement == PlatePlacementOuter) {
        canvas_draw_icon(canvas, 36, 32, &I_selector);
    }

    DrawText(canvas, 43, 31, "OUTER", TextFont3x5);

    DrawWyrm(
        canvas,
        &gameState->player,
        &I_plate,
        &I_destroyed,
        0,
        PlayerCoreX + 8,
        49,
        42,
        59,
        1,
        1,
        false,
        0,
        0,
        0
    );
}

static void DrawTitleScreen(
    Canvas* canvas
) {
    DrawTextCentered(
        canvas,
        ScreenWidth / 2,
        20,
        "IRONWYRM",
        TextFont5x8
    );

    DrawTextCentered(
        canvas,
        ScreenWidth / 2,
        35,
        "PRESS OK TO START",
        TextFont3x5
    );

    DrawTextCentered(
        canvas,
        ScreenWidth / 2,
        59,
        "(c) Henry Gurney 2026",
        TextFont3x5
    );
}

static void DrawGameOverScreen(
    Canvas* canvas,
    const GameState* gameState
) {
    char scoreText[24];

    snprintf(
        scoreText,
        sizeof(scoreText),
        "SCORE: %d",
        gameState->round
    );

    DrawTextCentered(
        canvas,
        ScreenWidth / 2,
        24,
        "GAME OVER",
        TextFont5x8
    );

    DrawTextCentered(
        canvas,
        ScreenWidth / 2,
        37,
        scoreText,
        TextFont3x5
    );

    /*DrawText(
        canvas,
        0,
        0,
        "(c) Henry Gurney 2026",
        TextFont3x5
    );*/

    DrawTextCentered(
        canvas,
        ScreenWidth / 2,
        59,
        "PRESS OK TO RESTART",
        TextFont3x5
    );
}

void DrawGame(Canvas* canvas, const GameState* gameState) {
    canvas_clear(canvas);

    switch(gameState->mode) {
    case GameModeTitle:
        DrawTitleScreen(canvas);
        break;

    case GameModeIntro:
        DrawIntroScreen(canvas, gameState);
        break;

    case GameModePlayerSelect:
    case GameModePlayerAttackResult:
    case GameModeEnemyAttackResult:
        DrawCombatScreen(canvas, gameState);
        break;

    case GameModeShopSelect:
        DrawShopScreen(canvas, gameState);
        break;

    case GameModeShopPlacement:
        DrawPlacementScreen(canvas, gameState);
        break;

    case GameModeGameOver:
        DrawGameOverScreen(canvas, gameState);
        break;
    }
}
