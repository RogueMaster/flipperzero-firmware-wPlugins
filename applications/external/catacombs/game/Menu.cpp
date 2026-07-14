#include "game/Defines.h"
#include "game/Platform.h"
#include "game/Menu.h"
#include "game/Font.h"
#include "game/Game.h"
#include "game/Draw.h"
#include "game/Textures.h"
#include "game/Generated/SpriteTypes.h"
#include "game/Map.h"
#include "game/FixedMath.h"
#include "lib/EEPROM.h"

#include <stdio.h>
#include <string.h>

constexpr uint8_t EEPROM_BASE_ADDR = 0;

struct ObjDesc {
    const uint16_t* sprite;
    bool animated;
    bool invert;
    uint8_t varsIndex;
};

static const ObjDesc kObjects[] = {
    {chestSpriteData, false, false, 0},
    {crownSpriteData, false, false, 1},
    {scrollSpriteData, false, false, 2},
    {coinsSpriteData, false, false, 3},
    {skeletonSpriteData, true, false, 4},
    {mageSpriteData, true, false, 5},
    {batSpriteData, true, true, 6},
    {spiderSpriteData, true, false, 7},
    {exitSpriteData, false, false, 8}};

static constexpr uint8_t kObjectsCount = (uint8_t)(sizeof(kObjects) / sizeof(kObjects[0]));
static constexpr uint8_t SHIFT_MASK = 63;

namespace {
constexpr uint8_t MENU_ITEMS_COUNT = 4;
constexpr uint8_t VISIBLE_ROWS = 2;

constexpr uint8_t MENU_FIRST_ROW = 4;
constexpr uint8_t TEXT_X = 18;
constexpr uint8_t CURSOR_X = 10;

constexpr uint8_t SPLASH_TIME_TICKS = 45;
static uint8_t splashTimer = 0;
static bool splashActive = true;

static uint8_t Wrap(int v, int n) {
    v %= n;
    if(v < 0) v += n;
    return (uint8_t)v;
}

static uint8_t MaxTop() {
    return (MENU_ITEMS_COUNT > VISIBLE_ROWS) ? (uint8_t)(MENU_ITEMS_COUNT - VISIBLE_ROWS) : 0;
}

void DrawMenuRoom() {
    const int16_t leftWall = 1 * CELL_SIZE;
    const int16_t rightWall = 4 * CELL_SIZE;
    const int16_t topWall = 1 * CELL_SIZE;
    const int16_t bottomWall = 4 * CELL_SIZE;

    Renderer::camera.x = (int16_t)((leftWall + rightWall) / 2);
    Renderer::camera.y = (int16_t)((topWall + bottomWall) / 2);

    static uint16_t angleFP = 0;
    constexpr uint16_t SPEED_FP = 64;
    angleFP = (uint16_t)(angleFP + SPEED_FP);
    Renderer::camera.angle = (uint8_t)(angleFP >> 8);

    Renderer::camera.tilt = 0;
    Renderer::camera.bob = 0;

    Renderer::globalRenderFrame++;
    Renderer::DrawBackground();

    Renderer::numBufferSlicesFilled = 0;
    Renderer::numQueuedDrawables = 0;

    for(uint8_t n = 0; n < DISPLAY_WIDTH; n++) {
        Renderer::wBuffer[n] = 0;
        Renderer::horizonBuffer[n] = HORIZON +
                                     (((DISPLAY_WIDTH / 2 - n) * Renderer::camera.tilt) >> 8) +
                                     Renderer::camera.bob;
    }

    Renderer::camera.cellX = Renderer::camera.x / CELL_SIZE;
    Renderer::camera.cellY = Renderer::camera.y / CELL_SIZE;

    {
        uint16_t rotPhase = (uint16_t)(0 - angleFP);
        uint8_t a0 = (uint8_t)(rotPhase >> 8);
        uint8_t f = (uint8_t)(rotPhase & 0xff);
        uint8_t a1 = (uint8_t)(a0 + 1);

        int16_t c0 = FixedCos(a0);
        int16_t c1 = FixedCos(a1);
        int16_t s0 = FixedSin(a0);
        int16_t s1 = FixedSin(a1);

        Renderer::camera.rotCos = (int16_t)(c0 + (((int32_t)(c1 - c0) * f) >> 8));
        Renderer::camera.rotSin = (int16_t)(s0 + (((int32_t)(s1 - s0) * f) >> 8));
    }

    {
        uint16_t clipPhase = (uint16_t)(((uint16_t)CLIP_ANGLE << 8) - angleFP);
        uint8_t a0 = (uint8_t)(clipPhase >> 8);
        uint8_t f = (uint8_t)(clipPhase & 0xff);
        uint8_t a1 = (uint8_t)(a0 + 1);

        int16_t c0 = FixedCos(a0);
        int16_t c1 = FixedCos(a1);
        int16_t s0 = FixedSin(a0);
        int16_t s1 = FixedSin(a1);

        Renderer::camera.clipCos = (int16_t)(c0 + (((int32_t)(c1 - c0) * f) >> 8));
        Renderer::camera.clipSin = (int16_t)(s0 + (((int32_t)(s1 - s0) * f) >> 8));
    }

#if WITH_IMAGE_TEXTURES
    const uint16_t* texture = wallTextureData;
#elif WITH_VECTOR_TEXTURES
    const uint8_t* texture = vectorTexture0;
#endif

    constexpr int8_t MIN_CELL = 0;
    constexpr int8_t MAX_CELL = 4;

#define MENU_SOLID(cx, cy) (((cx) == 0) || ((cx) == MAX_CELL) || ((cy) == 0) || ((cy) == MAX_CELL))
#define MENU_SOLID_SAFE(cx, cy)                                                   \
    (((cx) < MIN_CELL || (cx) > MAX_CELL || (cy) < MIN_CELL || (cy) > MAX_CELL) ? \
         true :                                                                   \
         MENU_SOLID((cx), (cy)))

    int8_t xd, yd;
    int8_t x1, y1, x2, y2;

    if(Renderer::camera.rotCos > 0) {
        x1 = MIN_CELL;
        x2 = MAX_CELL + 1;
        xd = 1;
    } else {
        x2 = MIN_CELL - 1;
        x1 = MAX_CELL;
        xd = -1;
    }

    if(Renderer::camera.rotSin < 0) {
        y1 = MIN_CELL;
        y2 = MAX_CELL + 1;
        yd = 1;
    } else {
        y2 = MIN_CELL - 1;
        y1 = MAX_CELL;
        yd = -1;
    }

    auto drawMenuCell = [&](int8_t x, int8_t y) {
        if(!MENU_SOLID(x, y)) return;
        if(Renderer::isFrustrumClipped(x, y)) return;
        if(Renderer::numBufferSlicesFilled >= DISPLAY_WIDTH) return;

        const bool blockedLeft = MENU_SOLID_SAFE(x - 1, y);
        const bool blockedRight = MENU_SOLID_SAFE(x + 1, y);
        const bool blockedUp = MENU_SOLID_SAFE(x, y - 1);
        const bool blockedDown = MENU_SOLID_SAFE(x, y + 1);

        int16_t wx1 = (int16_t)(x * CELL_SIZE);
        int16_t wy1 = (int16_t)(y * CELL_SIZE);
        int16_t wx2 = (int16_t)(wx1 + CELL_SIZE);
        int16_t wy2 = (int16_t)(wy1 + CELL_SIZE);

        if(!blockedLeft && Renderer::camera.x < wx1) {
#if WITH_TEXTURES
            Renderer::DrawWall(
                texture,
                wx1,
                wy1,
                wx1,
                wy2,
                !blockedUp && Renderer::camera.y > wy1,
                !blockedDown && Renderer::camera.y < wy2,
                false);
#else
            Renderer::DrawWall(
                wx1,
                wy1,
                wx1,
                wy2,
                !blockedUp && Renderer::camera.y > wy1,
                !blockedDown && Renderer::camera.y < wy2,
                false);
#endif
        }

        if(!blockedDown && Renderer::camera.y > wy2) {
#if WITH_TEXTURES
            Renderer::DrawWall(
                texture,
                wx1,
                wy2,
                wx2,
                wy2,
                !blockedLeft && Renderer::camera.x > wx1,
                !blockedRight && Renderer::camera.x < wx2,
                false);
#else
            Renderer::DrawWall(
                wx1,
                wy2,
                wx2,
                wy2,
                !blockedLeft && Renderer::camera.x > wx1,
                !blockedRight && Renderer::camera.x < wx2,
                false);
#endif
        }

        if(!blockedRight && Renderer::camera.x > wx2) {
#if WITH_TEXTURES
            Renderer::DrawWall(
                texture,
                wx2,
                wy2,
                wx2,
                wy1,
                !blockedDown && Renderer::camera.y < wy2,
                !blockedUp && Renderer::camera.y > wy1,
                false);
#else
            Renderer::DrawWall(
                wx2,
                wy2,
                wx2,
                wy1,
                !blockedDown && Renderer::camera.y < wy2,
                !blockedUp && Renderer::camera.y > wy1,
                false);
#endif
        }

        if(!blockedUp && Renderer::camera.y < wy1) {
#if WITH_TEXTURES
            Renderer::DrawWall(
                texture,
                wx2,
                wy1,
                wx1,
                wy1,
                !blockedRight && Renderer::camera.x < wx2,
                !blockedLeft && Renderer::camera.x > wx1,
                false);
#else
            Renderer::DrawWall(
                wx2,
                wy1,
                wx1,
                wy1,
                !blockedRight && Renderer::camera.x < wx2,
                !blockedLeft && Renderer::camera.x > wx1,
                false);
#endif
        }
    };

    if(ABS(Renderer::camera.rotCos) < ABS(Renderer::camera.rotSin)) {
        for(int8_t y = y1; y != y2; y += yd) {
            for(int8_t x = x1; x != x2; x += xd) {
                drawMenuCell(x, y);
            }
        }
    } else {
        for(int8_t x = x1; x != x2; x += xd) {
            for(int8_t y = y1; y != y2; y += yd) {
                drawMenuCell(x, y);
            }
        }
    }

#undef MENU_SOLID_SAFE
#undef MENU_SOLID
}
}

void Menu::Draw() {
    DrawMenuRoom();

    if(splashActive) {
        Font::PrintString(PSTR("FLIPPER GAME"), 2, 42, COLOUR_WHITE);
        Font::PrintString(PSTR("JHHOWARD & APFXTECH"), 4, 26, COLOUR_WHITE);
        Font::PrintString(PSTR("PRESENT"), 6, 52, COLOUR_WHITE);
        return;
    }

    Font::PrintString(PSTR("CATACOMBS OF THE DAMNED"), 2, 18, COLOUR_WHITE);

    for(uint8_t row = 0; row < VISIBLE_ROWS; ++row) {
        uint8_t idx = (uint8_t)(m_topIndex + row);
        if(idx >= MENU_ITEMS_COUNT) break;
        PrintItem(idx, (uint8_t)(MENU_FIRST_ROW + row));
    }

    static uint8_t bubble = 0;
    static uint16_t lastFrameSeen = 0xFFFF;

    const uint16_t frame = (uint16_t)Game::globalTickFrame;

    if(frame != lastFrameSeen) {
        if((frame & SHIFT_MASK) == 0) {
            bubble = (uint8_t)(bubble + 2);
            if(bubble >= kObjectsCount) bubble = (uint8_t)(bubble - kObjectsCount);
            if(bubble >= kObjectsCount) bubble = (uint8_t)(bubble - kObjectsCount);
        }
        lastFrameSeen = frame;
    }

    const uint8_t num1 = bubble;
    uint8_t num2 = (uint8_t)(bubble + 1);
    if(num2 >= kObjectsCount) num2 = 0;

    const ObjDesc& sprite1 = kObjects[num1];
    const ObjDesc& sprite2 = kObjects[num2];

    const int animOffset = ((Game::globalTickFrame & 8) == 0) ? 32 : 0;
    const int off1 = sprite1.animated ? animOffset : 0;
    const int off2 = sprite2.animated ? animOffset : 0;

    const uint16_t* torchSprite = (Game::globalTickFrame & 4) ? torchSpriteData1 :
                                                                torchSpriteData2;

    if(sprite1.invert) {
        Renderer::DrawScaled(sprite1.sprite + off1, 66, 29, 9, 255, true, COLOUR_BLACK);
    } else {
        Renderer::DrawScaled(sprite1.sprite + off1, 66, 29, 9, 255);
    }

    if(sprite2.invert) {
        Renderer::DrawScaled(sprite2.sprite + off2, 96, 30, 9, 255, true, COLOUR_BLACK);
    } else {
        Renderer::DrawScaled(sprite2.sprite + off2, 96, 30, 9, 255);
    }

    Renderer::DrawScaled(torchSprite, 0, 10, 9, 255);
    Renderer::DrawScaled(torchSprite, DISPLAY_WIDTH - 18, 10, 9, 255);

    Font::PrintInt(m_save[sprite1.varsIndex], MENU_FIRST_ROW + 1, 86, COLOUR_WHITE);
    Font::PrintInt(m_save[sprite2.varsIndex], MENU_FIRST_ROW + 1, 116, COLOUR_WHITE);

    Font::PrintString(PSTR(">"), (uint8_t)(MENU_FIRST_ROW + m_cursorPos), CURSOR_X, COLOUR_WHITE);
}

void Menu::PrintItem(uint8_t idx, uint8_t row) {
    switch(idx) {
    case 0:
        Font::PrintString(PSTR("Play"), row, TEXT_X, COLOUR_WHITE);
        break;
    case 1:
        Font::PrintString(PSTR("Sound:"), row, TEXT_X, COLOUR_WHITE);
        Font::PrintString(
            Platform::IsAudioEnabled() ? PSTR("on") : PSTR("off"), row, TEXT_X + 28, COLOUR_WHITE);
        break;
    case 2:
        Font::PrintString(PSTR("Score:"), row, TEXT_X, COLOUR_WHITE);
        Font::PrintInt(m_score, row, TEXT_X + 28, COLOUR_WHITE);
        break;
    case 3:
        Font::PrintString(PSTR("High:"), row, TEXT_X, COLOUR_WHITE);
        Font::PrintInt(m_high, row, TEXT_X + 28, COLOUR_WHITE);
        break;
    }
}

void Menu::Init() {
    m_selection = 0;
    m_topIndex = 0;
    m_cursorPos = 0;

    splashTimer = 0;
    splashActive = true;
}

void Menu::DrawEnteringLevel() {
    DrawMenuRoom();
    Font::PrintString(PSTR("Entering floor"), 3, 30, COLOUR_BLACK);
    Font::PrintInt(Game::floor, 3, 90, COLOUR_BLACK);
}

static int CountCharsInt(int v) {
    int n = 1;
    if(v < 0) {
        n++;
        v = -v;
    } // минус тоже символ
    while(v >= 10) {
        v /= 10;
        n++;
    }
    return n;
}

void PrintScoreCentered(int finalScore) {
    const int screenW = 128;

    int n = CountCharsInt(finalScore);
    int textW = 4 * n - 1;

    int x = (screenW - textW) / 2;

    Font::PrintInt(finalScore, 2, x, COLOUR_BLACK);
}

void Menu::DrawGameOver() {
    uint16_t finalScore = 0;
    constexpr int finishBonus = 500;
    constexpr int levelBonus = 20;
    constexpr int chestBonus = 15;
    constexpr int crownBonus = 10;
    constexpr int scrollBonus = 8;
    constexpr int coinsBonus = 4;
    constexpr int skeletonKillBonus = 10;
    constexpr int mageKillBonus = 10;
    constexpr int batKillBonus = 5;
    constexpr int spiderKillBonus = 4;

    finalScore += (Game::floor - 1) * levelBonus;
    if(Game::stats.killedBy == EnemyType::None) finalScore += finishBonus;
    finalScore += Game::stats.chestsOpened * chestBonus;
    finalScore += Game::stats.crownsCollected * crownBonus;
    finalScore += Game::stats.scrollsCollected * scrollBonus;
    finalScore += Game::stats.coinsCollected * coinsBonus;
    finalScore += Game::stats.enemyKills[(int)EnemyType::Skeleton] * skeletonKillBonus;
    finalScore += Game::stats.enemyKills[(int)EnemyType::Mage] * mageKillBonus;
    finalScore += Game::stats.enemyKills[(int)EnemyType::Bat] * batKillBonus;
    finalScore += Game::stats.enemyKills[(int)EnemyType::Spider] * spiderKillBonus;

    DrawMenuRoom();
    PrintScoreCentered(finalScore);

    switch(Game::stats.killedBy) {
    case EnemyType::Exit:
        Font::PrintString(PSTR("You have left the game."), 1, 18, COLOUR_BLACK);
        break;
    case EnemyType::None:
        Font::PrintString(PSTR("You escaped the catacombs!"), 1, 12, COLOUR_BLACK);
        break;
    case EnemyType::Mage:
        Font::PrintString(PSTR("Killed by a mage on level"), 1, 8, COLOUR_BLACK);
        Font::PrintInt(Game::floor, 1, 112, COLOUR_BLACK);
        break;
    case EnemyType::Skeleton:
        Font::PrintString(PSTR("Killed by a knight on level"), 1, 4, COLOUR_BLACK);
        Font::PrintInt(Game::floor, 1, 116, COLOUR_BLACK);
        break;
    case EnemyType::Bat:
        Font::PrintString(PSTR("Killed by a bat on level"), 1, 10, COLOUR_BLACK);
        Font::PrintInt(Game::floor, 1, 110, COLOUR_BLACK);
        break;
    case EnemyType::Spider:
        Font::PrintString(PSTR("Killed by a spider on level"), 1, 4, COLOUR_BLACK);
        Font::PrintInt(Game::floor, 1, 116, COLOUR_BLACK);
        break;
    }

    constexpr uint8_t firstRow = 21;
    constexpr uint8_t secondRow = 38;

    int offset = (Game::globalTickFrame & 8) == 0 ? 32 : 0;

    Renderer::DrawScaled(chestSpriteData, 6, firstRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.chestsOpened, 4, 24, COLOUR_BLACK);

    Renderer::DrawScaled(crownSpriteData, 6, secondRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.crownsCollected, 6, 24, COLOUR_BLACK);

    Renderer::DrawScaled(scrollSpriteData, 36, firstRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.scrollsCollected, 4, 54, COLOUR_BLACK);

    Renderer::DrawScaled(coinsSpriteData, 36, secondRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.coinsCollected, 6, 54, COLOUR_BLACK);

    Renderer::DrawScaled(skeletonSpriteData + offset, 72, firstRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.enemyKills[(int)EnemyType::Skeleton], 4, 90, COLOUR_BLACK);

    Renderer::DrawScaled(mageSpriteData + offset, 72, secondRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.enemyKills[(int)EnemyType::Mage], 6, 90, COLOUR_BLACK);

    Renderer::DrawScaled(batSpriteData + offset, 102, firstRow, 9, 255, true, COLOUR_BLACK);
    Font::PrintInt(Game::stats.enemyKills[(int)EnemyType::Bat], 4, 120, COLOUR_BLACK);

    Renderer::DrawScaled(spiderSpriteData + offset, 102, secondRow, 9, 255, false, COLOUR_WHITE);
    Font::PrintInt(Game::stats.enemyKills[(int)EnemyType::Spider], 6, 120, COLOUR_BLACK);

    m_save[0] = Game::stats.chestsOpened;
    m_save[1] = Game::stats.crownsCollected;
    m_save[2] = Game::stats.scrollsCollected;
    m_save[3] = Game::stats.coinsCollected;
    m_save[4] = Game::stats.enemyKills[(int)EnemyType::Skeleton];
    m_save[5] = Game::stats.enemyKills[(int)EnemyType::Mage];
    m_save[6] = Game::stats.enemyKills[(int)EnemyType::Bat];
    m_save[7] = Game::stats.enemyKills[(int)EnemyType::Spider];
    m_save[8] = 0;

    if(Game::floor > 0) {
        m_save[8] = Game::floor - 1;
    }

    SetScore(finalScore);
}

void Menu::Tick() {
    static uint8_t lastInput = 0;
    uint8_t input = Platform::GetInput();

    if(splashActive) {
        if(splashTimer < SPLASH_TIME_TICKS) splashTimer++;
        if(splashTimer >= SPLASH_TIME_TICKS) splashActive = false;
        lastInput = input;
        return;
    }

    auto syncWindow = [&]() {
        uint8_t maxTop = MaxTop();

        if(m_selection < m_topIndex) m_topIndex = m_selection;

        uint8_t end = (uint8_t)(m_topIndex + (VISIBLE_ROWS - 1));
        if(m_selection > end) {
            int t = (int)m_selection - (VISIBLE_ROWS - 1);
            if(t < 0) t = 0;
            if(t > maxTop) t = maxTop;
            m_topIndex = (uint8_t)t;
        }

        m_cursorPos = (uint8_t)(m_selection - m_topIndex);
        if(m_cursorPos >= VISIBLE_ROWS) m_cursorPos = (VISIBLE_ROWS - 1);
    };

    if((input & INPUT_DOWN) && !(lastInput & INPUT_DOWN)) {
        uint8_t next = Wrap((int)m_selection + 1, MENU_ITEMS_COUNT);

        if(m_cursorPos < (VISIBLE_ROWS - 1)) {
            m_selection = next;
            m_cursorPos++;
        } else {
            m_selection = next;

            if(m_topIndex < MaxTop()) {
                m_topIndex++;
            } else {
                m_topIndex = 0;
                m_cursorPos = 0;
            }
        }

        syncWindow();
    }

    if((input & INPUT_UP) && !(lastInput & INPUT_UP)) {
        uint8_t prev = Wrap((int)m_selection - 1, MENU_ITEMS_COUNT);

        if(m_cursorPos > 0) {
            m_selection = prev;
            m_cursorPos--;
        } else {
            m_selection = prev;

            if(m_topIndex > 0) {
                m_topIndex--;
            } else {
                m_topIndex = MaxTop();
                m_cursorPos = (VISIBLE_ROWS - 1);
            }
        }

        syncWindow();
    }

    if((input & (INPUT_A | INPUT_B)) && !(lastInput & (INPUT_A | INPUT_B))) {
        switch(m_selection) {
        case 0:
            Game::StartGame();
            break;
        case 1:
            Platform::SetAudioEnabled(!Platform::IsAudioEnabled());
            break;
        default:
            break;
        }
    }

    lastInput = input;
}

void Menu::TickEnteringLevel() {
    constexpr uint8_t showTime = 45;
    if(timer < showTime) timer++;
    if(timer == showTime && Platform::GetInput() == 0) {
        Game::StartLevel();
    }
}

void Menu::TickGameOver() {
    constexpr uint8_t minShowTime = 30;

    if(timer < minShowTime) timer++;

    if(timer == minShowTime && (Platform::GetInput() & (INPUT_A | INPUT_B))) {
        timer++;
    } else if(timer == minShowTime + 1 && Platform::GetInput() == 0) {
        Game::SwitchState(Game::State::Menu);
    }
}

static inline void DrawEraseTile8x8(int16_t x, int16_t y, const uint8_t* frame8bytes) {
    for(uint8_t row = 0; row < 8; row++) {
        uint8_t rowMask = pgm_read_byte(frame8bytes + row);
        while(rowMask) {
            uint8_t b = (uint8_t)__builtin_ctz((unsigned)rowMask);
            uint8_t col = 7 - b;
            Platform::PutPixel(x + col, y + row, COLOUR_WHITE);
            rowMask &= (uint8_t)(rowMask - 1);
        }
    }
}

void Menu::DrawTransitionFrame(uint8_t frameIndex) {
    const uint8_t w = pgm_read_byte(transitionSet + 0);
    const uint8_t h = pgm_read_byte(transitionSet + 1);
    (void)w;
    (void)h;

    const uint8_t* framePtr = transitionSet + 2 + (uint16_t)frameIndex * 8;
    int16_t tileX = 120;
    int16_t tileY = 56;
    while(true) {
        DrawEraseTile8x8(tileX, tileY, framePtr);

        tileX -= 8;
        if(tileX < 0) {
            tileX = 120;
            tileY -= 8;
            if(tileY < 0) break;
        }
    }
}

void Menu::ResetTimer() {
    timer = 0;
}

static constexpr uint8_t TOTAL_TIME = 40;
static constexpr uint8_t TOTAL_FRAMES = 8;

void Menu::RunTransition(Menu* menu, uint8_t& t, TransitionNextFn next) {
    uint8_t frameIndex = (uint16_t)t * TOTAL_FRAMES / TOTAL_TIME;
    if(frameIndex >= TOTAL_FRAMES) frameIndex = TOTAL_FRAMES - 1;
    menu->DrawTransitionFrame(frameIndex);
    if(frameIndex >= TOTAL_FRAMES - 2) {
        t = 0;
        next();
        return;
    }
    ++t;
}

void Menu::FadeOut() {
    static uint8_t t = 0;
    RunTransition(this, t, +[]() { Game::SwitchState(Game::State::GameOver); });
}

void Menu::ReadSave() {
    uint8_t addr = EEPROM_BASE_ADDR;
    m_score = (uint16_t)EEPROM.read(addr) | ((uint16_t)EEPROM.read(addr + 1) << 8);
    addr += 2;
    m_high = (uint16_t)EEPROM.read(addr) | ((uint16_t)EEPROM.read(addr + 1) << 8);
    addr += 2;
    m_storedHigh = m_high;

    for(int i = 0; i < 9; i++) {
        m_save[i] = EEPROM.read(addr++);
    }
}

void Menu::SetScore(uint16_t score) {
    if(score == 0) return;
    m_high = (score > m_storedHigh) ? score : m_storedHigh;
    m_score = score;
}

void Menu::WriteSave() {
    uint8_t addr = EEPROM_BASE_ADDR;
    EEPROM.update(addr++, (uint8_t)(m_score & 0xFF));
    EEPROM.update(addr++, (uint8_t)(m_score >> 8));
    EEPROM.update(addr++, (uint8_t)(m_high & 0xFF));
    EEPROM.update(addr++, (uint8_t)(m_high >> 8));

    for(int i = 0; i < 9; i++) {
        EEPROM.update(addr++, m_save[i]);
    }
    EEPROM.commit();
}
