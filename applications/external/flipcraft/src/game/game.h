#pragma once
#include "../flipcraft.h"
#include "render.h"
#include "gui.h"
#include <vector>

namespace flipcraft {

enum ScreenId {
    SCR_PLAY,
    SCR_INVENTORY,
    SCR_CRAFTING,
    SCR_FURNACE,
    SCR_CHEST,
    SCR_GAMEOVER
};

struct Input {
    int forward = 0;
    int turn = 0, pitch = 0;
    bool jump = false, crouch = false;
    bool breakPressed = false, placePressed = false;
    int slotScroll = 0;
    bool openInventory = false;

    int navX = 0, navY = 0;
    bool menuSelect = false;
    bool distribute = false;
};

// All persistent player-visible state. Tools occupy the 0xF0..0xFF type range
// and never stack (count is always 1).
struct PlayerState {
    ItemCell inventory[15];
    uint8_t invSlot = 0; // selected hotbar slot, 0..4
    ItemCell craftGrid[9];
    ItemCell craftOutput;
    uint8_t rot = 0x08; // camera: pitch << 4 | yaw, 16 steps per turn
    uint8_t health = MAXHEALTH;
    bool onGround = false;
    bool crouching = false;
};

struct ItemEnt {
    int id = 0;
    int x = 0, y = 0, z = 0;
    int vy = 0;
    int fuse = 0;
    bool active = false;
};

// One creature. Behaviour is a 2-bit mode; everything species-specific comes
// from the MobSpec byte table, everything situational from `timer`/`target`.
struct Mob {
    bool active = false;
    uint8_t species = 0;
    uint8_t hp = 0;
    uint8_t yaw = 0; // 16-step heading, camera convention: fwd=(-sin,cos)
    uint8_t mode = 0; // MOB_IDLE / MOB_WANDER / MOB_CHASE / MOB_FLEE
    uint8_t timer = 0; // ticks until the next decision / aggro left
    uint8_t hurt = 0; // damage-flash ticks left, colour inverts on odd ticks
    uint8_t cool = 0; // touch-attack cooldown / exploder fuse
    uint8_t target = 0xFF; // chase/flee subject: 0xFF player, else mob index
    uint8_t tamed = 0; // guards the player, never bites him
    uint8_t sated = 0; // full: skips food prey, exploders still hunted
    uint8_t alt = 0; // flyer: wanted hover height above ground, sub-px
    uint8_t seek = 0; // ticks until the chase goal may be re-aimed
    uint8_t fx = 0, fz = 0; // walk remainders, 7 fractional bits (see updateAllMobs)
    int16_t gx = 0, gz = 0; // chase/flee goal point, sub-pixels
    int x = 0, y = 0, z = 0; // world sub-pixels, min corner (body is MOBWIDTH wide)
    int vy = 0;
};
struct BlockEnt {
    bool active = false;
    bool isChest = false;
    int bx = 0, by = 0, bz = 0;
    int dir = 0;
    ItemCell slot[10];
    int timer = 0;
    int fuelTime = 0;
    bool lit = false;
    int storage = -1; // index into the on-disk storage table (explicit handle)
    bool loaded = false; // slot[]/furnace state resident? (lazy: only while open)
};

class Game {
public:
    World world;
    Renderer renderer;
    Framebuffer fb;
    Screen2D screen;
    PlayerState pl;
    uint32_t rngState = 0x1234;
    uint32_t spawnRngState = 0x1234; // separate stream so spawn draws never
        // correlate with world-tick consumers

    int playerX = 0, playerY = 0, playerZ = 0;
    int velYsub = 0, posYsub = 0;

    ScreenId screenId = SCR_PLAY;
    std::vector<ItemEnt> items;
    Mob mobs[MAX_MOBS];
    std::vector<BlockEnt> tiles;
    int loadedTile = -1;
    int score = 0;
    uint8_t hudItemTicks = 0; // ticks left showing the switched-item tooltip

    float bobTimer = 0.0f;
    float bobAmt = 0.0f;

    uint8_t storageUsed[STORAGE_CAPACITY / 8] = {0}; // free-slot bitmap for the table

    bool setup(const GameConfig& config);
    void shutdown();
    void simulate(const Input& in);
    bool render();
    ItemCell guiCursorItem(int* sx, int* sy);

private:
    uint8_t rng();
    uint8_t spawnRng();
    int smul446(int a, int b);
    uint8_t lastSpawn = 0xFF; // species of the previous spawn (anti-streak)

    void worldFrame(const Input& in);
    void handleBreakAndPlace(const Input& in);
    void miscInputs(const Input& in);
    void moveAndCollide(int dx, int dy, int dz);
    bool playerCollides(int x, int y, int z);
    bool boxCollides(int x, int y, int z, int w, int h);
    struct RayHit {
        int bx, by, bz, px, py, pz, id, length, mob;
    };
    RayHit rayCast();
    void createEntity(int x, int y, int z, int entityId);
    void addItemToInventory(uint8_t type, uint8_t count);
    void updateAllItems();
    void updateAllMobs();
    void trySpawnMob();
    void hurtMobFrom(int index, int dmg, int srcX, int srcZ, uint8_t attacker);
    void explodeMob(Mob& m);
    void explodeAt(int cx, int cy, int cz);
    void igniteDynamite(int bx, int by, int bz, int fuse);
    bool mobBlocksPlayer(int ox, int oz, int nx, int ny, int nz);
    void updateAllFurnaces();
    void simulateFurnaces(); // load/tick/flush furnaces inside the active window
    void doRandomTicks();
    void respawn();
    void renderWorld();
    void finishRender();
    void drawHotbar();
    int findBlockEntity(int x, int y, int z);

    void loadStorageDirectory(); // rebuild `tiles` headers from the table at open
    void loadInventory(); // read inventory region or seed starter set
    void saveInventory(); // write inventory region
    int allocStorage(); // reserve a free table index, or -1 if full
    void freeStorageSlot(int index);
    void openTileStorage(int tileIndex); // lazy: read contents into slot[]
    void flushTileStorage(int tileIndex); // write contents back, mark unloaded

    // w: cell size; mark: craft-target cell, empty ones get corner ticks
    struct Slot {
        ItemCell* cell;
        int sx, sy;
        uint8_t w;
        bool mark;
        bool output;
    };
    // The GUI never shows more than 25 slots (15 inventory + 9 craft grid + 1
    // output); a fixed list on the caller's stack avoids per-frame heap churn.
    struct SlotList {
        static constexpr int MAX = 25;
        Slot s[MAX];
        int n = 0;
        size_t size() const {
            return (size_t)n;
        }
        bool empty() const {
            return n == 0;
        }
        Slot& operator[](size_t i) {
            return s[i];
        }
    };
    SlotList buildSlots(ScreenId s);
    void guiFrame(const Input& in);
    void drawGui();
    void tryCraft();
    int cursor = 0;
    int selSlot = -1;
    bool gameOverPending = false;

    uint32_t visualSignature() const;
    uint32_t lastSig = 0;
    bool forceRedraw = true;
};

}
