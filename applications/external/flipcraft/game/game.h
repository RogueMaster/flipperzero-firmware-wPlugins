#pragma once
#include "../flipcraft.h"
#include "../render/render.h"
#include "../render/gui.h"
#include <vector>

namespace flipcraft {

enum Mem {
    M_X=0, M_Y=1, M_Z=2, M_ROT=3, M_ONGROUND=4, M_VELY=5, M_NEEDRERENDER=6,
    M_CROUCHING=7,
    M_PREVX=8,  // legacy not used (prev player X)
    M_PREVY=9,  // legacy not used (prev player Y)
    M_PREVZ=10, // legacy not used (prev player Z)
    M_INVENTORY=11, M_INVENTORYSLOT=26, M_CRAFTINGGRID=27, M_CRAFTINGOUTPUT=36,
    M_PERMSELSLOT=37, // legacy not used (old: m(M_PERMSELSLOT)=0xFF on crafting/inventory open)
    M_HEALTH=41, M_LOGSINWORLD=44,
};

enum ScreenId { SCR_PLAY, SCR_INVENTORY, SCR_CRAFTING, SCR_FURNACE, SCR_CHEST, SCR_GAMEOVER };

struct Input {

    int  forward=0;
    int  turn=0, pitch=0;
    bool jump=false, crouch=false;
    bool breakPressed=false, placePressed=false;
    int  slotScroll=0;
    bool openInventory=false;

    int  navX=0, navY=0;
    bool menuSelect=false;
    bool distribute=false;
};

struct ItemEnt { int id=0; int x=0,y=0,z=0; int vy=0; bool active=false; };
struct BlockEnt {
    bool active=false; bool isChest=false; int bx=0,by=0,bz=0; int dir=0;
    uint8_t slot[10]={0};
    int timer=0;
    int fuelTime=0;
    bool lit=false;
    int  storage=-1;     // index into the on-disk storage table (explicit handle)
    bool loaded=false;   // slot[]/furnace state resident? (lazy: only while open)
};

class Game {
public:
    World world;
    Renderer renderer;
    Framebuffer fb;
    Screen2D screen;
    uint8_t ram[256] = {0};
    uint32_t rngState = 0x1234;

    int playerX = 0, playerY = 0, playerZ = 0;

    ScreenId screenId = SCR_PLAY;
    std::vector<ItemEnt> items;
    std::vector<BlockEnt> tiles;
    int loadedTile = -1;
    int score = 0;

    float bobTimer = 0.0f;
    float bobAmt   = 0.0f;

    uint8_t storageUsed[STORAGE_CAPACITY / 8] = {0}; // free-slot bitmap for the table

    bool setup(const GameConfig& config);
    void shutdown();
    void frame(const Input& in);
    uint8_t& m(int addr) { return ram[addr]; }

private:
    uint8_t rng();
    int smul446(int a,int b);

    void worldFrame(const Input& in);
    void handleBreakAndPlace(const Input& in);
    void miscInputs(const Input& in);
    void moveAndCollide(int dx,int dy,int dz);
    bool playerCollides(int x,int y,int z);
    struct RayHit { int bx,by,bz, px,py,pz, id, length; };
    RayHit rayCast();
    int getBlockType(int id);
    int getBlockHardness(int id);
    void createEntity(int x,int y,int z,int entityId);
    void addItemToInventory(int item);
    void updateAllItems();
    void updateAllFurnaces();
    void simulateFurnaces();   // load/tick/flush furnaces inside the active window
    void doRandomTicks();
    void respawn();
    void finishRender();
    void drawHotbar();
    int  findBlockEntity(int x,int y,int z);

    // Storage persistence (see world.cpp for the on-disk region).
    void loadStorageDirectory();   // rebuild `tiles` headers from the table at open
    void loadInventory();          // read inventory region or seed starter set
    void saveInventory();          // write inventory region
    int  allocStorage();           // reserve a free table index, or -1 if full
    void freeStorageSlot(int index);
    void openTileStorage(int tileIndex);   // lazy: read contents into slot[]
    void flushTileStorage(int tileIndex);  // write contents back, mark unloaded

    struct Slot { uint8_t* cell; int gx, gy, sx, sy; bool grid; bool output; };
    std::vector<Slot> buildSlots(ScreenId s);
    void guiFrame(const Input& in);
    void drawGui();
    void tryCraft();
    int cursor = 0;
    int selSlot = -1;
    bool gameOverPending=false;
};

}
