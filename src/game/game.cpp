#include "game.h"
#include <algorithm>
#include <cstring>

namespace flipcraft {

// Per-block material type (low nibble) and hardness (high nibble).
static constexpr uint8_t kBlockTypeHard[32] = {
    /* AIR     */ 0x30 | BLOCKTYPE_SAPLING,
    /* GRASS   */ 0x00 | BLOCKTYPE_SOFT,
    /* DIRT    */ 0x00 | BLOCKTYPE_SOFT,
    /* STONE   */ 0x20 | BLOCKTYPE_STONE,
    /* COBBLE  */ 0x20 | BLOCKTYPE_STONE,
    /* LOG     */ 0x10 | BLOCKTYPE_WOOD,
    /* LEAVES  */ 0x00 | BLOCKTYPE_LEAVES,
    /* PLANK   */ 0x10 | BLOCKTYPE_WOOD,
    /* COALORE */ 0x20 | BLOCKTYPE_STONE,
    /* IRONORE */ 0x30 | BLOCKTYPE_STONE,
    /* SAND    */ 0x00 | BLOCKTYPE_SOFT,
    /* GLASS   */ 0x10 | BLOCKTYPE_GLASS,
    /* SAPLING */ 0x30 | BLOCKTYPE_SAPLING,
    /* TABLE   */ 0x10 | BLOCKTYPE_WOOD,
    /* FURNACE */ 0x20 | BLOCKTYPE_STONE,
    /* CHEST   */ 0x10 | BLOCKTYPE_WOOD,
    /* DYNAMITE*/ 0x00 | BLOCKTYPE_SOFT,
};
static inline int blockType(uint8_t id)     { return kBlockTypeHard[id & 0x1F] & 0x0F; }
static inline int blockHardness(uint8_t id) { return kBlockTypeHard[id & 0x1F] >> 4; }

// Entity dropped when a block breaks; 0xFF marks the leaves special case
// (random sapling/stick/apple), 0 drops nothing.
static constexpr uint8_t kBlockDrop[32] = {
    0, ENTITY_DIRT, ENTITY_DIRT, ENTITY_COBBLE, ENTITY_COBBLE, ENTITY_LOG,
    0xFF, ENTITY_PLANK, ENTITY_COAL, ENTITY_IRONORE, ENTITY_SAND, 0,
    ENTITY_SAPLING, ENTITY_TABLE, ENTITY_FURNACE, ENTITY_CHEST,
    ENTITY_DYNAMITE,
};

// Blocks grass can "see the sky" through (leaf decay / grass spread checks).
static constexpr uint32_t BLOCKS_SEETHROUGH_LIGHT =
    (1u << BLOCK_AIR) | (1u << BLOCK_LEAVES) | (1u << BLOCK_GLASS) | (1u << BLOCK_SAPLING);

uint8_t Game::rng() {
    rngState ^= rngState<<13; rngState ^= rngState>>17; rngState ^= rngState<<5;
    return (uint8_t)(rngState & 0xFF);
}
int Game::smul446(int a,int b){ int r=(s8(a)*s8(b))>>6; return (int8_t)std::clamp(r,-128,127); }

bool Game::setup(const GameConfig& config) {
    if(!world.openWorld(config.worldDataPath)) return false;

    pl = PlayerState{};
    renderer.invalidateChunkMeshes(); // the Game object survives across worlds

    playerX=world.hdrPX; playerY=world.hdrPY; playerZ=world.hdrPZ;
    pl.rot=world.hdrRot; rngState=world.hdrRng;
    pl.onGround=true; velYsub=0; posYsub=0;
    forceRedraw=true; lastSig=0;

    items.clear();
    for(auto& m:mobs) m=Mob{};
    loadStorageDirectory();
    loadInventory();
    screenId=SCR_PLAY; selSlot=-1; cursor=0; score=0; gameOverPending=false; loadedTile=-1;
    screen.fb=&fb;
    renderer.zbuf=fb.px;   // one shared buffer: depth in bits 1-7, colour in bit 0

    world.updateWindow(playerX/BLOCKSIZE, playerZ/BLOCKSIZE, true);
    auto surfaceYAt = [&](int bx, int bz) {
        world.updateWindow(bx, bz, true);
        for (int by = WORLD_SY - 1; by >= 0; by--)
            if (world.getBlock(bx, by, bz) != BLOCK_AIR) return by;
        return -1;
    };
    int bx=playerX/BLOCKSIZE, bz=playerZ/BLOCKSIZE;
    int surfaceY=surfaceYAt(bx, bz);
    if (surfaceY < 0) {
        bx = std::min(4, world.worldSX() - 1);
        bz = std::min(4, world.worldSZ() - 1);
        surfaceY = surfaceYAt(bx, bz);
        playerX = bx * BLOCKSIZE; playerZ = bz * BLOCKSIZE;
        if (surfaceY >= 0) playerY = (surfaceY + 1) * BLOCKSIZE;
    }
    if (!world.existed && surfaceY >= 0 && world.getBlock(bx, playerY/BLOCKSIZE, bz)==BLOCK_AIR)
        playerY=(surfaceY + 1)*BLOCKSIZE;
    return true;
}

void Game::shutdown() {
    // Flush every furnace/chest still resident (sim-loaded near the player or
    // the one whose GUI is open), not just the open one.
    for(size_t i=0;i<tiles.size();i++) if(tiles[i].active && tiles[i].loaded) flushTileStorage((int)i);
    saveInventory();
    world.closeWorld(playerX, playerY, playerZ, pl.rot, rngState);
}

void Game::addItemToInventory(uint8_t type,uint8_t count){
    if(!type||!count) return;
    if(type<ITEM_NONSTACKABLE){
        for(int i=0;i<15;i++){ItemCell& c=pl.inventory[i]; if(c.type!=type)continue;
            int tot=c.count+count;
            if(tot>MAX_STACK){c.count=MAX_STACK; count=(uint8_t)(tot-MAX_STACK);} else {c.count=(uint8_t)tot;return;}}
    }
    for(int i=0;i<15;i++) if(pl.inventory[i].empty()){pl.inventory[i]={type,count};return;}
}

void Game::createEntity(int x,int y,int z,int entityId){
    ItemEnt e; e.active=true; e.id=entityId;
    e.x=x*16+8; e.y=y*16+4; e.z=z*16+8; e.vy=rng()&0x07;
    if(entityId==ENTITY_FALLINGSAND||entityId==ENTITY_LITDYNAMITE) e.vy=0;
    items.push_back(e);
}
void Game::igniteDynamite(int bx,int by,int bz,int fuse){
    world.setBlock(bx,by,bz,BLOCK_AIR);
    createEntity(bx,by,bz,ENTITY_LITDYNAMITE);
    items.back().fuse=fuse;
}
int Game::findBlockEntity(int x,int y,int z){
    for(size_t i=0;i<tiles.size();i++) if(tiles[i].active&&tiles[i].bx==x&&tiles[i].by==y&&tiles[i].bz==z) return (int)i;
    return -1;
}

// On-disk slot (STORAGE_SLOT_SIZE bytes):
//   [0] flags: bit0 in-use, bit1 isChest
//   [1] dir(0x0F) | bits8-9 of bx << 4 | bits8-9 of bz << 6 (10-bit coords)
//   [2] bx low byte  [3] by  [4] bz low byte
//   [5] furnace: low nibble fuelTime, high nibble timer (lit derived)
//   [6..25] slot[10] as {type,count} pairs
static void packStorage(const BlockEnt& t, uint8_t* b){
    memset(b,0,STORAGE_SLOT_SIZE);
    b[0]=(uint8_t)(0x01|(t.isChest?0x02:0x00));
    b[1]=(uint8_t)((t.dir&0x0F)|(((t.bx>>8)&0x3)<<4)|(((t.bz>>8)&0x3)<<6));
    b[2]=(uint8_t)t.bx; b[3]=(uint8_t)t.by; b[4]=(uint8_t)t.bz;
    b[5]=(uint8_t)((t.fuelTime&0x0F)|((t.timer&0x0F)<<4));
    for(int i=0;i<10;i++){ b[6+2*i]=t.slot[i].type; b[7+2*i]=t.slot[i].count; }
}
static void unpackContents(BlockEnt& t, const uint8_t* b){
    for(int i=0;i<10;i++) t.slot[i]={b[6+2*i],b[7+2*i]};
    t.fuelTime=b[5]&0x0F; t.timer=(b[5]>>4)&0x0F; t.lit=t.fuelTime>0;
}

void Game::loadStorageDirectory(){
    tiles.clear();
    memset(storageUsed,0,sizeof(storageUsed));
    uint8_t buf[16*STORAGE_SLOT_SIZE];
    for(int first=0; first<STORAGE_CAPACITY; first+=16){
        int cnt=std::min(16,STORAGE_CAPACITY-first);
        if(!world.readStorageBatch(first,cnt,buf)) break;
        for(int i=0;i<cnt;i++){
            const uint8_t* b=buf+i*STORAGE_SLOT_SIZE;
            if(!(b[0]&0x01)) continue;            // free slot
            int idx=first+i;
            BlockEnt t; t.active=true; t.isChest=(b[0]&0x02)!=0;
            t.dir=b[1]&0x0F;
            t.bx=b[2]|(((b[1]>>4)&0x3)<<8); t.by=b[3]; t.bz=b[4]|(((b[1]>>6)&0x3)<<8);
            t.storage=idx; t.loaded=false;        // contents stay lazy
            storageUsed[idx>>3]|=(uint8_t)(1<<(idx&7));
            tiles.push_back(t);
        }
    }
}

int Game::allocStorage(){
    for(int i=0;i<STORAGE_CAPACITY;i++)
        if(!(storageUsed[i>>3]&(1<<(i&7)))){ storageUsed[i>>3]|=(uint8_t)(1<<(i&7)); return i; }
    return -1;
}
void Game::freeStorageSlot(int index){
    if((unsigned)index>=STORAGE_CAPACITY) return;
    storageUsed[index>>3]&=(uint8_t)~(1<<(index&7));
    uint8_t b[STORAGE_SLOT_SIZE]; memset(b,0,sizeof(b)); // clear in-use on disk
    world.writeStorageSlot(index,b);
}
void Game::openTileStorage(int ti){
    BlockEnt& t=tiles[ti];
    if(t.loaded||t.storage<0) return;
    uint8_t b[STORAGE_SLOT_SIZE];
    if(world.readStorageSlot(t.storage,b)) unpackContents(t,b);
    t.loaded=true;
}
void Game::flushTileStorage(int ti){
    BlockEnt& t=tiles[ti];
    if(!t.loaded||t.storage<0){ t.loaded=false; return; }
    uint8_t b[STORAGE_SLOT_SIZE]; packStorage(t,b);
    world.writeStorageSlot(t.storage,b);
    t.loaded=false; t.lit=false;   // smelting pauses while closed
}

// On-disk inventory record: 15 {type,count} cells, then health<<4 | invSlot.
void Game::loadInventory(){
    uint8_t buf[31];
    if(world.readInventory(buf,31)){
        for(int i=0;i<15;i++) pl.inventory[i]={buf[2*i],buf[2*i+1]};
        pl.invSlot=(uint8_t)(buf[30]&0x0F); if(pl.invSlot>4) pl.invSlot=0;
        pl.health=(uint8_t)(buf[30]>>4);
        if(pl.health==0||pl.health>MAXHEALTH) pl.health=MAXHEALTH;
    } else {
        pl.inventory[0]={ITEM_STICK,4}; pl.inventory[1]={ITEM_LOG,5}; pl.inventory[2]={ITEM_COAL,9};
        pl.inventory[3]={ITEM_FURNACE,1}; pl.inventory[4]={ITEM_CHEST,1};
        pl.health=MAXHEALTH;
    }
}
void Game::saveInventory(){
    uint8_t buf[31];
    for(int i=0;i<15;i++){ buf[2*i]=pl.inventory[i].type; buf[2*i+1]=pl.inventory[i].count; }
    buf[30]=(uint8_t)(((pl.health&0x0F)<<4)|(pl.invSlot&0x0F));
    world.writeInventory(buf,31);
}

// Voxel DDA: steps one block boundary at a time, visiting exactly the blocks
// the ray crosses. Creatures are slab-tested afterwards; a body closer than
// the first solid block wins the hit (h.mob >= 0).
Game::RayHit Game::rayCast(){
    RayHit h{0,0,0,0,0,0,BLOCK_AIR,-1,-1};
    const float ox=playerX+PLAYERHALFWIDTH, oy=playerY+PLAYERCAMHEIGHT, oz=playerZ+PLAYERHALFWIDTH;
    const float dx=renderer.matrix[2][0], dy=renderer.matrix[2][1], dz=renderer.matrix[2][2];

    int bx=ifloor(ox*(1.0f/16.0f)), by=ifloor(oy*(1.0f/16.0f)), bz=ifloor(oz*(1.0f/16.0f));
    int px=bx, py=by, pz=bz;

    constexpr float FAR = 1e9f;
    const int sx = dx>0 ? 1:-1, sy = dy>0 ? 1:-1, sz = dz>0 ? 1:-1;
    const float tdx = dx!=0 ? fabsf(16.0f/dx) : FAR;
    const float tdy = dy!=0 ? fabsf(16.0f/dy) : FAR;
    const float tdz = dz!=0 ? fabsf(16.0f/dz) : FAR;
    float tmx = dx!=0 ? ((dx>0 ? (bx+1)*16.0f-ox : ox-bx*16.0f)*tdx*(1.0f/16.0f)) : FAR;
    float tmy = dy!=0 ? ((dy>0 ? (by+1)*16.0f-oy : oy-by*16.0f)*tdy*(1.0f/16.0f)) : FAR;
    float tmz = dz!=0 ? ((dz>0 ? (bz+1)*16.0f-oz : oz-bz*16.0f)*tdz*(1.0f/16.0f)) : FAR;

    float t=0, tBlock=(float)RAYCASTMAXLENGTH;
    while(t<=(float)RAYCASTMAXLENGTH){
        if(by<0){h.id=-1;h.length=(int)t;h.bx=bx;h.by=by;h.bz=bz;tBlock=t;break;}
        uint8_t id=world.getBlock(bx,by,bz);
        if(id!=BLOCK_AIR){h.id=id;h.length=(int)t;h.bx=bx;h.by=by;h.bz=bz;h.px=px;h.py=py;h.pz=pz;tBlock=t;break;}
        px=bx;py=by;pz=bz;
        if(tmx<=tmy && tmx<=tmz){ t=tmx; tmx+=tdx; bx+=sx; }
        else if(tmy<=tmz)       { t=tmy; tmy+=tdy; by+=sy; }
        else                    { t=tmz; tmz+=tdz; bz+=sz; }
    }

    // Generous pick: a creature counts as aimed at when its centre projects
    // into the middle half of the screen (|sx-64|<=32 -> 7|camX| <= 4*camZ)
    // and no wall is closer along the view axis. Nearest such body wins.
    float bestW=tBlock*16.0f;
    ActiveWindow win=activeWindowAround((playerX+PLAYERHALFWIDTH)/BLOCKSIZE,
                                        (playerZ+PLAYERHALFWIDTH)/BLOCKSIZE,
                                        world.worldSX(), world.worldSZ());
    const float (*M)[3]=renderer.matrix;
    for(int i=0;i<MAX_MOBS;i++){
        const Mob& m=mobs[i]; if(!m.active)continue;
        int mbx=(m.x+7)>>4, mbz=(m.z+7)>>4;   // dormant off-ring bodies are not hittable
        if(mbx<win.x0||mbx>win.x1||mbz<win.z0||mbz>win.z1)continue;
        const float hgt=(float)((mobSpec(m.species).geom>>4)<<1);
        float rx=m.x+7-ox, ry=m.y+hgt*0.5f-oy, rz=m.z+7-oz;
        float cz=M[2][0]*rx+M[2][1]*ry+M[2][2]*rz;
        if(cz<(float)CLIP || cz>=bestW) continue;
        float cx=M[0][0]*rx+M[0][2]*rz;
        float cy=M[1][0]*rx+M[1][1]*ry+M[1][2]*rz;
        if(7.0f*fabsf(cx)>4.0f*cz || 7.0f*fabsf(cy)>4.0f*cz) continue;
        h.mob=i; bestW=cz;
    }
    return h;
}

// melee damage per tool sub-id (held & 0x0F); swords 2/3/4 by tier
static constexpr uint8_t kToolDmg[16] = {1,1,1,2, 1,1,1,3, 1,1,1,4, 1,1,1,1};

void Game::handleBreakAndPlace(const Input& in){
    if(!in.breakPressed && !in.placePressed) return;   // no raycast on idle ticks

    RayHit hit=rayCast(); int id=hit.id;
    if(hit.mob>=0){
        Mob& tm=mobs[hit.mob];
        if(in.placePressed){   // short: attack
            uint8_t held=pl.inventory[pl.invSlot].type;
            hurtMobFrom(hit.mob, held>=ITEM_NONSTACKABLE ? kToolDmg[held&0x0F] : 1,
                        playerX+PLAYERHALFWIDTH, playerZ+PLAYERHALFWIDTH, 0xFF);
        } else if(tm.species==MOB_WOLF && !tm.tamed){   // long: tame for two of мясо
            int have=0;
            for(int i=0;i<15;i++) if(pl.inventory[i].type==ITEM_APPLE) have+=pl.inventory[i].count;
            if(have>=2){
                int need=2;
                for(int i=0;i<15&&need;i++){ItemCell& c=pl.inventory[i];
                    if(c.type!=ITEM_APPLE)continue;
                    int t=std::min(need,(int)c.count); need-=t; c.count=(uint8_t)(c.count-t);
                    if(!c.count)c={};}
                tm.tamed=1; tm.mode=MOB_IDLE; tm.target=0xFF; tm.timer=10;
            }
        }
        return;
    }
    if(id==BLOCK_AIR||hit.length<0){
        if(in.placePressed){ItemCell& it=pl.inventory[pl.invSlot];
            if(it.type==ITEM_APPLE){int hp=std::min((int)pl.health+APPLEHEALTH,MAXHEALTH);pl.health=(uint8_t)hp;
                if(--it.count==0)it={};}}
        return;
    }
    int bx=hit.bx,by=hit.by,bz=hit.bz;
    if(in.breakPressed&&id!=-1){
        if(id==BLOCK_SAPLING)return;
        int held=pl.inventory[pl.invSlot].type;
        int strength=STRENGTH_FIST;
        if(held>=ITEM_NONSTACKABLE&&held<=ITEM_SHEARS){
            if(held==ITEM_SHEARS) strength=(id==BLOCK_LEAVES)?STRENGTH_IRON:STRENGTH_FIST;
            // pickaxe/axe/shovel index equals the block type they break fast
            else if((held&3)<3 && (held&3)==blockType(id)) strength=STRENGTH_WOOD+((held>>2)&3);
        }
        int net=strength-blockHardness(id);

        int be=findBlockEntity(bx,by,bz);
        if(be>=0){
            if(tiles[be].storage>=0) freeStorageSlot(tiles[be].storage);
            if(loadedTile==be) loadedTile=-1;
            tiles[be].active=false;
            tiles[be].loaded=false;   // destroyed: never flush it back to disk
        }
        world.setBlock(bx,by,bz,BLOCK_AIR);
        if(net>=STRENGTHFORITEM){
            uint8_t drop=kBlockDrop[id&0x1F];
            if(drop==0xFF){ // leaves
                if(strength==STRENGTH_IRON)createEntity(bx,by,bz,ENTITY_LEAVES);
                else{int r=rng(); if(r<LEAVES_SAPLING_PROBABILITY)createEntity(bx,by,bz,ENTITY_SAPLING);
                    else if(r<LEAVES_STICK_PROBABILITY)createEntity(bx,by,bz,ENTITY_STICK);
                    else if(r<LEAVES_APPLE_PROBABILITY)createEntity(bx,by,bz,ENTITY_APPLE);}}
            else if(drop) createEntity(bx,by,bz,drop);
        }
        int yy=by; while(true){yy++;uint8_t a=world.getBlock(bx,yy,bz);
            if(a==BLOCK_SAND){createEntity(bx,yy,bz,ENTITY_FALLINGSAND);world.setBlock(bx,yy,bz,BLOCK_AIR);}
            else if(a==BLOCK_SAPLING){createEntity(bx,yy,bz,ENTITY_SAPLING);world.setBlock(bx,yy,bz,BLOCK_AIR);}
            else break;}
        return;
    }
    if(in.placePressed){
        if(id==BLOCK_TABLE){ screenId=SCR_CRAFTING; cursor=0; selSlot=-1; return; }
        if(id==BLOCK_DYNAMITE){ igniteDynamite(bx,by,bz,DYNAMITE_FUSE_TICKS); return; }
        if(id==BLOCK_FURNACE||id==BLOCK_CHEST){ int bi=findBlockEntity(bx,by,bz);
            if(bi<0){ // block without a directory entry (storage table was full): adopt it
                BlockEnt b;b.active=true;b.isChest=(id==BLOCK_CHEST);
                b.bx=bx;b.by=by;b.bz=bz;
                b.dir=(hit.pz<bz)?2:(hit.pz>bz)?3:(hit.px<bx)?0:(hit.px>bx)?1:(pl.rot&0x0F);
                b.storage=allocStorage();
                if(b.storage>=0){uint8_t sb[STORAGE_SLOT_SIZE];packStorage(b,sb);world.writeStorageSlot(b.storage,sb);}
                tiles.push_back(b); bi=(int)tiles.size()-1;}
            openTileStorage(bi); loadedTile=bi; screenId=tiles[bi].isChest?SCR_CHEST:SCR_FURNACE; cursor=0; selSlot=-1; return; }
        ItemCell& cell=pl.inventory[pl.invSlot];
        uint8_t item=cell.type;
        bool placeable=!(item==0||cell.count==0||(item>=ITEM_IRONINGOT&&item<ITEM_TABLE)||
                         item==ITEM_COAL||item==ITEM_GUNPOWDER);
        if(!placeable)return;
        int blockId=(item==ITEM_DYNAMITE)?BLOCK_DYNAMITE:((item>=ITEM_TABLE)?(item&0x0F):(item>>4));
        int px=hit.px,py=hit.py,pz=hit.pz;
        if(blockId==BLOCK_SAPLING){uint8_t below=world.getBlock(px,py-1,pz); if(below!=BLOCK_DIRT&&below!=BLOCK_GRASS)return;}
        if(blockId!=BLOCK_SAPLING){
            int blockX=px*BLOCKSIZE,blockY=py*BLOCKSIZE,blockZ=pz*BLOCKSIZE;
            bool overlapsPlayer =
                playerX<blockX+BLOCKSIZE && playerX+PLAYERWIDTH>blockX &&
                playerY<blockY+BLOCKSIZE && playerY+PLAYERWIDTH>blockY &&
                playerZ<blockZ+BLOCKSIZE && playerZ+PLAYERWIDTH>blockZ;
            if(overlapsPlayer)return;
        }
        if(blockId==BLOCK_SAND){uint8_t below=world.getBlock(px,py-1,pz);
            if(below==BLOCK_AIR){createEntity(px,py,pz,ENTITY_FALLINGSAND);} else world.setBlock(px,py,pz,BLOCK_SAND);}
        else world.setBlock(px,py,pz,(uint8_t)blockId);
        if(blockId==BLOCK_CHEST||blockId==BLOCK_FURNACE){BlockEnt b;b.active=true;b.isChest=(blockId==BLOCK_CHEST);
            b.bx=px;b.by=py;b.bz=pz;b.dir=(pl.rot&0x0F);
            b.storage=allocStorage(); b.loaded=false;
            if(b.storage>=0){uint8_t sb[STORAGE_SLOT_SIZE];packStorage(b,sb);world.writeStorageSlot(b.storage,sb);}
            tiles.push_back(b);}
        if(--cell.count==0)cell={};
    }
}

bool Game::boxCollides(int x,int y,int z,int w,int h){
    for(int bx=x/16;bx<=(x+w)/16;bx++)
    for(int by=y/16;by<=(y+h)/16;by++)
    for(int bz=z/16;bz<=(z+w)/16;bz++)
        if(blockIsSolid(world.getBlock(bx,by,bz)))return true;
    return false;
}
bool Game::playerCollides(int x,int y,int z){
    return boxCollides(x,y,z,PLAYERWIDTH,PLAYERHEIGHT);
}
// blocks only newly created overlaps, so an overlapped pair can separate
bool Game::mobBlocksPlayer(int ox,int oz,int nx,int ny,int nz){
    for(const auto& m:mobs){
        if(!m.active)continue;
        int h=(mobSpec(m.species).geom>>4)<<1;
        if(ny>=m.y+h || ny+PLAYERHEIGHT<=m.y) continue;
        bool now = nx<m.x+MOBWIDTH && nx+PLAYERWIDTH>m.x &&
                   nz<m.z+MOBWIDTH && nz+PLAYERWIDTH>m.z;
        bool was = ox<m.x+MOBWIDTH && ox+PLAYERWIDTH>m.x &&
                   oz<m.z+MOBWIDTH && oz+PLAYERWIDTH>m.z;
        if(now && !was) return true;
    }
    return false;
}
void Game::moveAndCollide(int dx,int dy,int dz){
    int x=playerX,y=playerY,z=playerZ;
    if(playerCollides(x,y,z)){
        int bx0=x>>4, bx1=(x+PLAYERWIDTH)>>4, bz0=z>>4, bz1=(z+PLAYERWIDTH)>>4;
        int top=-1;
        for(int by=WORLD_SY-1;by>=0&&top<0;by--)
            for(int cx=bx0;cx<=bx1&&top<0;cx++)
                for(int cz=bz0;cz<=bz1;cz++)
                    if(blockIsSolid(world.getBlock(cx,by,cz))){top=by;break;}
        y=(top+1)*BLOCKSIZE;
        velYsub=0; posYsub=0; pl.onGround=true;
    }

    const int maxX = world.worldSX()*BLOCKSIZE - PLAYERWIDTH;
    const int maxZ = world.worldSZ()*BLOCKSIZE - PLAYERWIDTH;

    int nx=x+dx; nx=std::clamp(nx,0,maxX);
    if(!playerCollides(nx,y,z) && !mobBlocksPlayer(x,z,nx,y,z))x=nx;
    int nz=z+dz; nz=std::clamp(nz,0,maxZ);
    if(!playerCollides(x,y,nz) && !mobBlocksPlayer(x,z,x,y,nz))z=nz;

    int ny=y+dy;
    if(playerCollides(x,ny,z)){
        if(dy<0){
            int speed=-velYsub*JUMP_AIRTIME/VERT_SUBPIXEL;
            int over=speed-MINFALLDAMAGESPEED;
            if(over>0){int dmg=smul446(over,FALLDAMAGESCALING); int hp=(int)pl.health-dmg;
                if(hp<=0){gameOverPending=true;} else pl.health=u8(hp);}
            pl.onGround=true;
        }
        velYsub=0; posYsub=0;

        int step=(dy<0)?1:-1;
        while(playerCollides(x,ny,z)&&ny>=0&&ny<=WORLD_SY*BLOCKSIZE) ny+=step;
        y=ny;
    } else y=ny;
    if(y<0){y=0;pl.onGround=true;velYsub=0;posYsub=0;}

    playerX=x;playerY=y;playerZ=z;
}
void Game::miscInputs(const Input& in){
    pl.crouching=in.crouch;
    if(in.turn||in.pitch){int yaw=(pl.rot&0x0F),pitch=(pl.rot>>4)&0x0F;
        yaw=(yaw+in.turn)&0x0F;
        if(in.pitch>0 && pitch!=4)  pitch=(pitch+1)&0x0F;
        if(in.pitch<0 && pitch!=12) pitch=(pitch-1)&0x0F;
        pl.rot=(uint8_t)((pitch<<4)|yaw);}
    renderer.setCamRot(pl.rot);
    int sinY=(int)renderer.sinYaw(),cosY=(int)renderer.cosYaw();
    int fwd=smul446(in.forward,SPEEDFACTOR);

    int dx=smul446(fwd,sinY);
    int dz=smul446(fwd,cosY);
    bool grounded = playerCollides(playerX, playerY-1, playerZ);
    pl.onGround=false;
    if(grounded && in.jump){
        velYsub = JUMPSTRENGTH*VERT_SUBPIXEL/JUMP_AIRTIME; posYsub=0;
    } else if(grounded){
        velYsub=0; posYsub=0; pl.onGround=true;
    } else {
        velYsub -= GRAVITY*VERT_SUBPIXEL/(JUMP_AIRTIME*JUMP_AIRTIME);
    }
    posYsub += velYsub;
    int dy = posYsub/VERT_SUBPIXEL; posYsub -= dy*VERT_SUBPIXEL;
    moveAndCollide(dx,dy,dz);

    bool moving = in.forward!=0 && pl.onGround;
    if(moving) bobTimer += BOB_SPEED;
    float target = moving ? 1.0f : 0.0f;
    if(bobAmt<target){ bobAmt+=BOB_EASE; if(bobAmt>target)bobAmt=target; }
    else if(bobAmt>target){ bobAmt-=BOB_EASE; if(bobAmt<target)bobAmt=target; }
}

void Game::updateAllItems(){
    int pxc=playerX+PLAYERHALFWIDTH, pyc=playerY+PLAYERCAMHEIGHT, pzc=playerZ+PLAYERHALFWIDTH;
    int boom[4][3]; int booms=0;   // deferred: explodeAt() mutates `items`
    for(auto& e:items){ if(!e.active)continue;

        e.vy-=2; if(e.vy<-8)e.vy=-8;
        int ny=e.y+e.vy;
        int bx=e.x/16, bz=e.z/16, nby=ny/16;
        if(ny<0){ny=0;e.vy=0;}
        if(blockIsSolid(world.getBlock(bx,nby,bz))){ e.y=(nby+1)*16; e.vy=0;
            if(e.id==ENTITY_FALLINGSAND){ world.setBlock(bx,e.y/16,bz,BLOCK_SAND); e.active=false; continue; } }
        else e.y=ny;
        if(e.id==ENTITY_FALLINGSAND)continue;
        if(e.id==ENTITY_LITDYNAMITE){
            if(--e.fuse<=0){
                if(booms<4){ boom[booms][0]=e.x/16; boom[booms][1]=e.y/16; boom[booms][2]=e.z/16; booms++; e.active=false; }
                else e.fuse=1;
            }
            continue;
        }

        if(std::abs(e.x-pxc)<=PICKUPSIDEPOS && std::abs(e.z-pzc)<=PICKUPSIDEPOS &&
           e.y-pyc<=PICKUPUP && pyc-e.y<=PICKUPDOWN+PLAYERCAMHEIGHT){
            uint8_t type;
            switch(e.id){case ENTITY_APPLE:type=ITEM_APPLE;break;case ENTITY_TABLE:type=ITEM_TABLE;break;
                case ENTITY_FURNACE:type=ITEM_FURNACE;break;case ENTITY_CHEST:type=ITEM_CHEST;break;
                case ENTITY_GUNPOWDER:type=ITEM_GUNPOWDER;break;
                case ENTITY_DYNAMITE:type=ITEM_DYNAMITE;break;
                default:type=(uint8_t)(e.id<<4);break;}
            addItemToInventory(type,1); e.active=false;
        }
    }
    items.erase(std::remove_if(items.begin(),items.end(),[](const ItemEnt&e){return !e.active;}),items.end());
    for(int i=0;i<booms;i++) explodeAt(boom[i][0],boom[i][1],boom[i][2]);
}

// Load furnaces entering the active window, flush+unload those leaving it, then
// tick every resident furnace. Chests stay lazy (loaded only when opened).
void Game::simulateFurnaces(){
    int pbx=(playerX+PLAYERHALFWIDTH)/BLOCKSIZE, pbz=(playerZ+PLAYERHALFWIDTH)/BLOCKSIZE;
    ActiveWindow win=activeWindowAround(pbx,pbz,world.worldSX(),world.worldSZ());
    for(size_t i=0;i<tiles.size();i++){
        BlockEnt& t=tiles[i];
        if(!t.active||t.isChest)continue;
        bool inWin = t.bx>=win.x0 && t.bx<=win.x1 && t.bz>=win.z0 && t.bz<=win.z1;
        if(inWin){ if(!t.loaded) openTileStorage((int)i); }
        else if(t.loaded && (int)i!=loadedTile){ flushTileStorage((int)i); }
    }
    updateAllFurnaces();
}

void Game::updateAllFurnaces(){
    for(auto& f:tiles){ if(!f.active||f.isChest||!f.loaded)continue;
        uint16_t res=craftFurnace(f.slot[0].type);
        uint8_t rt=(uint8_t)(res>>8), rc=(uint8_t)res;
        bool valid = res!=0 && f.slot[0].count>0 &&
            (f.slot[2].empty() || (f.slot[2].type==rt && f.slot[2].count+rc<=MAX_STACK));

        if(valid && f.fuelTime==0 && !f.slot[1].empty()){
            int add=0; uint8_t fuel=f.slot[1].type;
            if(fuel==ITEM_COAL)add=8; else if(fuel==ITEM_STICK||fuel==ITEM_SAPLING||(fuel>=ITEM_WOODPICKAXE&&fuel<=ITEM_WOODSWORD))add=1;
            else if(fuel==ITEM_PLANK||fuel==ITEM_LOG)add=2;
            if(add){ f.fuelTime=add; if(--f.slot[1].count==0)f.slot[1]={}; f.lit=true; }
        }
        if(valid && f.fuelTime>0){
            f.timer++;
            if(f.timer>=SMELTTIME/16){ f.timer=0;
                if(--f.slot[0].count==0)f.slot[0]={};
                if(f.slot[2].empty())f.slot[2]={rt,rc}; else f.slot[2].count=(uint8_t)(f.slot[2].count+rc);
                f.fuelTime--;
                if(f.fuelTime==0)f.lit=false;
            }
        } else if(f.fuelTime==0) {
            f.lit=false;
        }
    }
}

void Game::doRandomTicks(){
    auto above=[&](int x,int y,int z){uint8_t a=world.getBlock(x,y+1,z);
        return ((BLOCKS_SEETHROUGH_LIGHT>>(a&0x1F))&1u)!=0;};
    auto nearLog=[&](int x,int y,int z){
        for(int r=0;r<=LEAF_LOG_RADIUS;r++)
            for(int dx=-r;dx<=r;dx++)for(int dz=-r;dz<=r;dz++){
                if(dx>-r&&dx<r&&dz>-r&&dz<r)continue; // interior covered by smaller r
                for(int dy=-1;dy<=1;dy++)
                    if(world.getBlock(x+dx,y+dy,z+dz)==BLOCK_LOG)return true;
            }
        return false;};

    int pbx=(playerX+PLAYERHALFWIDTH)/BLOCKSIZE, pbz=(playerZ+PLAYERHALFWIDTH)/BLOCKSIZE;
    ActiveWindow win=activeWindowAround(pbx,pbz,world.worldSX(),world.worldSZ());
    int wsx=win.x1-win.x0+1, wsz=win.z1-win.z0+1;
    for(int i=0;i<RANDOMTICKSPEED;i++){
        int x=win.x0+rng()%wsx, y=rng()%WORLD_SY, z=win.z0+rng()%wsz; uint8_t b=world.getBlock(x,y,z);
        if(b==BLOCK_DIRT){ if(!above(x,y,z))continue; bool f=false;
            for(int gx=x-1;gx<=x+1&&!f;gx++)for(int gy=y-1;gy<=y+1&&!f;gy++)for(int gz=z-1;gz<=z+1&&!f;gz++)
                if(world.getBlock(gx,gy,gz)==BLOCK_GRASS)f=true;
            if(f){world.setBlock(x,y,z,BLOCK_GRASS);}}
        else if(b==BLOCK_GRASS){ if(!above(x,y,z)){world.setBlock(x,y,z,BLOCK_DIRT);}}
        else if(b==BLOCK_LEAVES){ if(!nearLog(x,y,z)){world.setBlock(x,y,z,BLOCK_AIR);
                int r=rng(); if(r<LEAVES_SAPLING_PROBABILITY)createEntity(x,y,z,ENTITY_SAPLING);
                else if(r<LEAVES_STICK_PROBABILITY)createEntity(x,y,z,ENTITY_STICK);
                else if(r<LEAVES_APPLE_PROBABILITY)createEntity(x,y,z,ENTITY_APPLE);}}
        else if(b==BLOCK_SAPLING){
            int extraHeight=rng()&1;
            int lowerTop=y+2+extraHeight;
            for(int lx=x-2;lx<=x+2;lx++)for(int ly=lowerTop-1;ly<=lowerTop;ly++)
                for(int lz=z-2;lz<=z+2;lz++)
                    if(world.getBlock(lx,ly,lz)==BLOCK_AIR)world.setBlock(lx,ly,lz,BLOCK_LEAVES);
            int upperTop=lowerTop+2;
            for(int lx=x-1;lx<=x+1;lx++)for(int ly=upperTop-1;ly<=upperTop;ly++)
                for(int lz=z-1;lz<=z+1;lz++)
                    if(world.getBlock(lx,ly,lz)==BLOCK_AIR)world.setBlock(lx,ly,lz,BLOCK_LEAVES);
            for(int ty=upperTop-1;ty>=y;ty--){
                uint8_t old=world.getBlock(x,ty,z);
                if(old==BLOCK_AIR||old==BLOCK_LEAVES||old==BLOCK_SAPLING)world.setBlock(x,ty,z,BLOCK_LOG);
            }
        }
    }
}

void Game::respawn(){
    int x=(playerX & ~15)+3,z=(playerZ & ~15)+3;
    int bx=x/BLOCKSIZE,bz=z/BLOCKSIZE,by=playerY/BLOCKSIZE;
    while(by<WORLD_SY){
        if(!blockIsSolid(world.getBlock(bx,by,bz)) && !blockIsSolid(world.getBlock(bx,by+1,bz)))break;
        by++;
    }
    playerX=x;playerY=by*BLOCKSIZE;playerZ=z;
    velYsub=0;posYsub=0;pl.onGround=false;pl.health=MAXHEALTH;
    screenId=SCR_PLAY;selSlot=-1;loadedTile=-1;gameOverPending=false;
    world.updateWindow((playerX+PLAYERHALFWIDTH)/BLOCKSIZE,(playerZ+PLAYERHALFWIDTH)/BLOCKSIZE,true);
}

void Game::drawHotbar(){
    screen.x1=19;screen.y1=51;screen.x2=76;screen.y2=63;screen.clearRect();
    screen.x1=20;screen.y1=52;screen.x2=75;screen.y2=63;screen.drawRect();
    int sel=pl.invSlot;
    for(int i=0;i<5;i++){int x=21+i*11,y=53; screen.x1=x;screen.y1=y;screen.x2=x+9;screen.y2=y+9;screen.clearRect();
        if(i==sel){screen.x1=x;screen.y1=y;screen.x2=x+9;screen.y2=y+9;screen.drawRect();
            screen.x1=x+1;screen.y1=y+1;screen.x2=x+8;screen.y2=y+8;screen.clearRect();}
        ItemCell it=pl.inventory[i]; if(it.type){screen.itemIcon(x+2,y+2,it.type);
            if(it.type<ITEM_NONSTACKABLE){int n=it.count;
                if(n>9)screen.number(x+2,y+5,n/10);
                screen.number(x+6,y+5,n%10);}}}
    if(!hudItemTicks)   // tooltip in device.cpp takes the hearts' place
        for(int i=0;i<MAXHEALTH;i++)screen.heart(19+i*6,43,i<(int)pl.health);
}
void Game::finishRender(){
    renderer.clearBuffer(); renderer.setCamRot(pl.rot);
    float bobV=sinf(bobTimer*2.0f)*bobAmt;
    renderer.camPos[0]=playerX+PLAYERHALFWIDTH; renderer.camPos[2]=playerZ+PLAYERHALFWIDTH;
    renderer.camPos[1]=playerY+(pl.crouching?PLAYERCROUCHCAMHEIGHT:PLAYERCAMHEIGHT)+bobV*CAM_BOB_AMPLITUDE;
    renderer.renderScene(world);
    for(auto& f:tiles) if(f.active){
        int tex = f.isChest?TEX_CHESTFRONT:(f.lit?TEX_FURNACEFRONTON:TEX_FURNACEFRONTOFF);
        renderer.renderFace(f.bx,f.by,f.bz,(uint8_t)tex,f.dir&3,false);
    }
    for(auto& e:items) if(e.active){
        if(e.id==ENTITY_LITDYNAMITE)
            renderer.renderDynamite((float)e.x,(float)e.y,(float)e.z,(uint8_t)((e.fuse&1)<<1));
        else
            renderer.renderItem(e.x/16.0f,e.y/16.0f,e.z/16.0f,(uint8_t)e.id);
    }
    for(const auto& m:mobs) if(m.active){
        int sc16=16;   // fusing exploder swells to ~1.4x at detonation
        if((mobSpec(m.species).info&1) && m.cool)
            sc16=16+((MOB_FUSE_TICKS-m.cool)*7)/MOB_FUSE_TICKS;
        renderer.renderMob((float)(m.x+7), (float)m.y, (float)(m.z+7), m.species,
                           (uint8_t)(m.yaw&15),
                           (uint8_t)((m.hurt&1)<<1), (uint8_t)sc16);
    }
    RayHit hit=rayCast();
    if(hit.mob<0&&hit.id!=BLOCK_AIR&&hit.id!=-1&&hit.length>=0) renderer.renderOverlay(world,hit.bx,hit.by,hit.bz,0);
    drawHotbar();
}

void Game::worldFrame(const Input& in){
    if(in.slotScroll){int s=pl.invSlot+in.slotScroll; if(s<0)s=4; if(s>4)s=0; pl.invSlot=(uint8_t)s; hudItemTicks=HUD_LABEL_TICKS;}
    else if(hudItemTicks) hudItemTicks--;
    handleBreakAndPlace(in);
    if(screenId!=SCR_PLAY)return;
    miscInputs(in);
    updateAllItems(); updateAllMobs(); doRandomTicks();
    simulateFurnaces();   // every furnace in the active window smelts, GUI open or not
    if(gameOverPending){gameOverPending=false; score=0; screenId=SCR_GAMEOVER; return;}
    world.updateWindow((playerX+PLAYERHALFWIDTH)/BLOCKSIZE,(playerZ+PLAYERHALFWIDTH)/BLOCKSIZE);
}

Game::SlotList Game::buildSlots(ScreenId s){
    SlotList v;
    auto add=[&](ItemCell* c,int gx,int gy,int sx,int sy,bool grid,bool out){
        if(v.n<SlotList::MAX) v.s[v.n++]={c,gx,gy,sx,sy,grid,out};};

    for(int r=0;r<3;r++)for(int c=0;c<5;c++) add(&pl.inventory[r*5+c],c,r,21+c*11,53-r*11,false,false);
    if(s==SCR_INVENTORY){
        int map[4]={0,1,3,4};
        for(int i=0;i<4;i++){int gx=i%2,gy=i/2; add(&pl.craftGrid[map[i]],5+gx,4+gy,26+gx*9,9+gy*9,true,false);}
        add(&pl.craftOutput,6,5,60,14,false,true);
    } else if(s==SCR_CRAFTING){
        for(int i=0;i<9;i++){int gx=i%3,gy=i/3; add(&pl.craftGrid[i],5+gx,4+gy,21+gx*9,1+gy*9,true,false);}
        add(&pl.craftOutput,8,5,65,10,false,true);
    } else if(s==SCR_FURNACE&&loadedTile>=0){ BlockEnt& f=tiles[loadedTile];
        add(&f.slot[0],6,4,30,1,false,false);
        add(&f.slot[1],6,5,30,19,false,false);
        add(&f.slot[2],8,4,56,10,false,true);
    } else if(s==SCR_CHEST&&loadedTile>=0){ BlockEnt& ch=tiles[loadedTile];
        for(int i=0;i<10;i++){int gx=i%5,gy=i/5; add(&ch.slot[i],gx,4+gy,21+gx*11,18-gy*11,false,false);}
    }
    return v;
}
void Game::tryCraft(){
    uint16_t out=craftTable(pl.craftGrid); if(!out)return;

    for(int i=0;i<9;i++){ItemCell& c=pl.craftGrid[i]; if(c.type){if(--c.count==0)c={};}}
    addItemToInventory((uint8_t)(out>>8),(uint8_t)out);
}
void Game::guiFrame(const Input& in){
    auto slots=buildSlots(screenId);
    if(in.openInventory){
        if(loadedTile>=0 && (size_t)loadedTile<tiles.size()) flushTileStorage(loadedTile);
        screenId=SCR_PLAY; selSlot=-1; loadedTile=-1; return;}

    if((in.navX||in.navY)&&!slots.empty()){

        int csx=slots[cursor].sx,csy=slots[cursor].sy; int best=cursor,bestd=1<<30;
        for(size_t i=0;i<slots.size();i++){int dsx=slots[i].sx-csx,dsy=slots[i].sy-csy;
            if(in.navX>0&&dsx<=0) continue;
            if(in.navX<0&&dsx>=0) continue;
            if(in.navY>0&&dsy>=0) continue;
            if(in.navY<0&&dsy<=0) continue;
            int d=dsx*dsx+dsy*dsy; if(d<bestd){bestd=d;best=(int)i;}}
        cursor=best;

        if(in.distribute && selSlot>=0 && selSlot<(int)slots.size() && cursor!=selSlot){
            Slot& src=slots[selSlot]; Slot& dst=slots[cursor];
            ItemCell& sv=*src.cell;
            if(sv.type && sv.type<ITEM_NONSTACKABLE && sv.count>0 && !dst.output){
                ItemCell& dv=*dst.cell;
                bool moved=false;
                if(dv.empty()){ dv={sv.type,1}; moved=true; }
                else if(dv.type==sv.type && dv.count<MAX_STACK){ dv.count++; moved=true; }
                if(moved && --sv.count==0){ sv={}; selSlot=-1; }
            }
            if(screenId==SCR_INVENTORY||screenId==SCR_CRAFTING){
                uint16_t o=craftTable(pl.craftGrid);
                pl.craftOutput={(uint8_t)(o>>8),(uint8_t)o};
            }
        }
    }
    if(in.menuSelect&&!slots.empty()){
        Slot& cur=slots[cursor];
        if(cur.output){
            if(screenId==SCR_FURNACE){
                if(cur.cell->type){addItemToInventory(cur.cell->type,cur.cell->count);*cur.cell={};}
            } else if(cur.cell->type){
                tryCraft();
            }
        } else if(selSlot<0){ if(cur.cell->type)selSlot=cursor; }
        else {
            ItemCell& sv=*slots[selSlot].cell; ItemCell& dv=*cur.cell;
            if(sv.type && sv.type==dv.type && sv.type<ITEM_NONSTACKABLE){
                int tot=(int)dv.count+(int)sv.count;
                if(tot<=MAX_STACK){ dv.count=(uint8_t)tot; sv={}; }
                else { dv.count=MAX_STACK; sv.count=(uint8_t)(tot-MAX_STACK); }
            } else std::swap(sv,dv);
            selSlot=-1;
        }

        if(screenId==SCR_INVENTORY||screenId==SCR_CRAFTING){
            uint16_t o=craftTable(pl.craftGrid);
            pl.craftOutput={(uint8_t)(o>>8),(uint8_t)o};
        }
    }
    if(screenId==SCR_FURNACE)updateAllFurnaces();
}
void Game::drawGui(){
    if(screenId==SCR_GAMEOVER){ screen.clearScreen(); return; }   // text drawn in device.cpp
    screen.clearScreen();
    auto slots=buildSlots(screenId);
    for(size_t i=0;i<slots.size();i++){Slot& s=slots[i]; ItemCell it=*s.cell;
        int box=s.grid?8:10; screen.x1=s.sx;screen.y1=s.sy;screen.x2=s.sx+box-1;screen.y2=s.sy+box-1;screen.drawRect();
        screen.x1=s.sx+1;screen.y1=s.sy+1;screen.x2=s.sx+box-2;screen.y2=s.sy+box-2;screen.clearRect();
        if(it.type){screen.itemIcon(s.sx+2,s.sy+2,it.type);
            if(it.type<ITEM_NONSTACKABLE){int n=it.count;
                if(n>9)screen.number(s.sx+2,s.sy+5,n/10);
                screen.number(s.sx+6,s.sy+5,n%10);}}
        if((int)i==cursor)
            screen.invertRect(s.sx+1,s.sy+1,s.sx+box-2,s.sy+box-2);
    }
    if(selSlot>=0&&selSlot<(int)slots.size()){Slot& s=slots[selSlot];
        screen.x1=s.sx+3;screen.y1=s.sy+3;screen.x2=s.sx+4;screen.y2=s.sy+4;screen.drawRect();}
}

void Game::simulate(const Input& in){
    if(screenId==SCR_GAMEOVER){ if(in.menuSelect)respawn(); return; }
    if(screenId==SCR_PLAY){
        if(in.openInventory){screenId=SCR_INVENTORY;cursor=0;selSlot=-1;return;}
        worldFrame(in);
    } else {
        guiFrame(in);
    }
}

// FNV-1a hash of all visible state; render() redraws only when it changes.
uint32_t Game::visualSignature() const {
    uint32_t h=2166136261u;
    auto mix=[&](uint32_t v){ h^=v; h*=16777619u; };
    auto mixf=[&](float f){ uint32_t u; memcpy(&u,&f,4); mix(u); };
    mix((uint32_t)screenId); mix((uint32_t)(cursor+1)); mix((uint32_t)(selSlot+1));
    mix((uint32_t)(loadedTile+1)); mix((uint32_t)gameOverPending); mix((uint32_t)score);
    mix((uint32_t)playerX); mix((uint32_t)playerY); mix((uint32_t)playerZ);
    mixf(bobTimer); mixf(bobAmt);
    mix(world.revision);
    auto mixCell=[&](const ItemCell& c){ mix((uint32_t)(c.type|(c.count<<8))); };
    for(int i=0;i<15;i++) mixCell(pl.inventory[i]);
    for(int i=0;i<9;i++) mixCell(pl.craftGrid[i]);
    mixCell(pl.craftOutput); mix(pl.invSlot); mix(pl.rot); mix(pl.health);
    mix(hudItemTicks);
    mix((uint32_t)pl.crouching);
    for(const auto& e:items) if(e.active){ mix((uint32_t)e.id); mix((uint32_t)e.x); mix((uint32_t)e.y); mix((uint32_t)e.z); mix((uint32_t)e.fuse); }
    for(const auto& m:mobs) if(m.active){
        mix((uint32_t)m.x); mix((uint32_t)((m.y<<12)^m.z));
        mix((uint32_t)((m.species<<24)|(m.mode<<16)|(m.yaw<<8)|m.hurt)); }
    for(const auto& t:tiles) if(t.active){
        mix((uint32_t)((t.bx<<16)|(t.by<<8)|t.bz));
        mix((uint32_t)((t.dir<<2)|(t.isChest?2:0)|(t.lit?1:0))); }
    if(loadedTile>=0 && (size_t)loadedTile<tiles.size()){
        const BlockEnt& t=tiles[loadedTile];
        for(int i=0;i<10;i++) mixCell(t.slot[i]);
        mix((uint32_t)((t.timer<<8)|t.fuelTime)); }
    return h;
}

bool Game::render(){
    uint32_t sig=visualSignature();
    if(!forceRedraw && sig==lastSig) return false;
    lastSig=sig; forceRedraw=false;
    if(screenId==SCR_PLAY) finishRender();
    else drawGui();
    return true;
}

}
