#include "game.h"
#include <algorithm>
#include <cstring>

namespace flipcraft {

uint8_t Game::rng() {
    rngState ^= rngState<<13; rngState ^= rngState>>17; rngState ^= rngState<<5;
    return (uint8_t)(rngState & 0xFF);
}
int Game::smul446(int a,int b){ int r=(s8(a)*s8(b))>>6; return (int8_t)std::clamp(r,-128,127); }

bool Game::setup(const GameConfig& config) {

    if(!world.openWorld(*config.files, config.worldDataPath)) return false;
    for (int i=0;i<256;i++) ram[i]=0;

    playerX=world.hdrPX; playerY=world.hdrPY; playerZ=world.hdrPZ;
    m(M_ROT)=world.hdrRot; rngState=world.hdrRng;
    m(M_ONGROUND)=0xFF; m(M_NEEDRERENDER)=0xFF; m(M_VELY)=0xFF;
    m(M_CROUCHING)=0xFF;
    m(M_HEALTH)=MAXHEALTH; m(M_LOGSINWORLD)=4;

    items.clear();
    loadStorageDirectory();   // rebuild chest/furnace headers from disk
    loadInventory();          // persisted inventory, or the starter set
    screenId=SCR_PLAY; selSlot=-1; cursor=0; score=0; gameOverPending=false;
    screen.fb=&fb;
    renderer.zcolour=fb.px;   // scene rasterises straight into the shared framebuffer

    world.updateWindow(playerX/BLOCKSIZE, playerZ/BLOCKSIZE);
    auto surfaceYAt = [&](int bx, int bz) {
        world.updateWindow(bx, bz);
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
    world.closeWorld(playerX, playerY, playerZ, m(M_ROT), rngState);
}

int Game::getBlockType(int id){
    switch(id){case BLOCK_STONE:case BLOCK_COBBLE:case BLOCK_IRONORE:case BLOCK_COALORE:case BLOCK_FURNACE:return BLOCKTYPE_STONE;
    case BLOCK_PLANK:case BLOCK_LOG:case BLOCK_TABLE:case BLOCK_CHEST:return BLOCKTYPE_WOOD;
    case BLOCK_SAND:case BLOCK_DIRT:case BLOCK_GRASS:return BLOCKTYPE_SOFT;
    case BLOCK_LEAVES:return BLOCKTYPE_LEAVES; case BLOCK_GLASS:return BLOCKTYPE_GLASS; default:return BLOCKTYPE_SAPLING;}
}
int Game::getBlockHardness(int id){
    switch(id){case BLOCK_LEAVES:case BLOCK_SAND:case BLOCK_DIRT:case BLOCK_GRASS:return 0;
    case BLOCK_GLASS:case BLOCK_PLANK:case BLOCK_LOG:case BLOCK_TABLE:case BLOCK_CHEST:return 1;
    case BLOCK_STONE:case BLOCK_COBBLE:case BLOCK_COALORE:case BLOCK_FURNACE:return 2; default:return 3;}
}

void Game::addItemToInventory(int item){
    if(item==0) return;
    int type=item&0xF0;
    if(type<0xF0){
        for(int i=0;i<15;i++){int c=m(M_INVENTORY+i); if((c&0xF0)!=type||c==0)continue;
            int tot=(c&0x0F)+(item&0x0F);
            if(tot>=16){m(M_INVENTORY+i)=(uint8_t)(type|15); item=type|(tot-15);} else {m(M_INVENTORY+i)=(uint8_t)(type+tot);return;}}
    }
    for(int i=0;i<15;i++) if(m(M_INVENTORY+i)==0){m(M_INVENTORY+i)=(uint8_t)item;return;}
}

void Game::createEntity(int x,int y,int z,int entityId){
    ItemEnt e; e.active=true; e.id=entityId;
    e.x=x*16+8; e.y=y*16+4; e.z=z*16+8; e.vy=rng()&0x07;
    if(entityId==ENTITY_FALLINGSAND) e.vy=0;
    items.push_back(e);
}
int Game::findBlockEntity(int x,int y,int z){
    for(size_t i=0;i<tiles.size();i++) if(tiles[i].active&&tiles[i].bx==x&&tiles[i].by==y&&tiles[i].bz==z) return (int)i;
    return -1;
}

// --- Storage region (persisted after the world array) -----------------------
// On-disk slot (STORAGE_SLOT_SIZE bytes):
//   [0] flags: bit0 in-use, bit1 isChest   [1] dir(0x0F)
//   [2] bx  [3] by  [4] bz                  [5..14] slot[10]
//   [15] furnace: low nibble fuelTime, high nibble timer (lit derived)
static void packStorage(const BlockEnt& t, uint8_t* b){
    memset(b,0,STORAGE_SLOT_SIZE);
    b[0]=(uint8_t)(0x01|(t.isChest?0x02:0x00));
    b[1]=(uint8_t)(t.dir&0x0F);
    b[2]=(uint8_t)t.bx; b[3]=(uint8_t)t.by; b[4]=(uint8_t)t.bz;
    for(int i=0;i<10;i++) b[5+i]=t.slot[i];
    b[15]=(uint8_t)((t.fuelTime&0x0F)|((t.timer&0x0F)<<4));
}
static void unpackContents(BlockEnt& t, const uint8_t* b){
    for(int i=0;i<10;i++) t.slot[i]=b[5+i];
    t.fuelTime=b[15]&0x0F; t.timer=(b[15]>>4)&0x0F; t.lit=t.fuelTime>0;
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
            t.dir=b[1]&0x0F; t.bx=b[2]; t.by=b[3]; t.bz=b[4];
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

void Game::loadInventory(){
    uint8_t buf[18];
    if(world.readInventory(buf,18)){
        for(int i=0;i<15;i++) m(M_INVENTORY+i)=buf[i];
        m(M_INVENTORYSLOT)=buf[15]; m(M_HEALTH)=buf[16]; m(M_LOGSINWORLD)=buf[17];
    } else {
        m(M_INVENTORY+0)=0x14; m(M_INVENTORY+1)=0x55; m(M_INVENTORY+2)=0x89;
        m(M_INVENTORY+3)=0xFE; m(M_INVENTORY+4)=0xFF;
        m(M_HEALTH)=MAXHEALTH; m(M_LOGSINWORLD)=4;
    }
}
void Game::saveInventory(){
    uint8_t buf[18];
    for(int i=0;i<15;i++) buf[i]=m(M_INVENTORY+i);
    buf[15]=m(M_INVENTORYSLOT); buf[16]=m(M_HEALTH); buf[17]=m(M_LOGSINWORLD);
    world.writeInventory(buf,18);
}

Game::RayHit Game::rayCast(){
    RayHit h{0,0,0,0,0,0,BLOCK_AIR,-1};
    float ox=playerX+PLAYERHALFWIDTH, oy=playerY+PLAYERCAMHEIGHT, oz=playerZ+PLAYERHALFWIDTH;
    float dx=renderer.camDir(0)/64.0f, dy=renderer.camDir(1)/64.0f, dz=renderer.camDir(2)/64.0f;
    float L=sqrtf(dx*dx+dy*dy+dz*dz);
    if(L<1e-6f) return h;
    dx/=L;dy/=L;dz/=L;
    int px=(int)floorf(ox/16.0f),py=(int)floorf(oy/16.0f),pz=(int)floorf(oz/16.0f);
    for(float t=0;t<=(float)RAYCASTMAXLENGTH;t+=1.0f){
        int bx=(int)floorf((ox+dx*t)/16.0f),by=(int)floorf((oy+dy*t)/16.0f),bz=(int)floorf((oz+dz*t)/16.0f);
        if(by<0){h.id=-1;h.length=(int)t;h.bx=bx;h.by=by;h.bz=bz;return h;}
        uint8_t id=world.getBlock(bx,by,bz);
        if(id!=BLOCK_AIR){h.id=id;h.length=(int)t;h.bx=bx;h.by=by;h.bz=bz;h.px=px;h.py=py;h.pz=pz;return h;}
        px=bx;py=by;pz=bz;
    }
    return h;
}

void Game::handleBreakAndPlace(const Input& in){
    RayHit hit=rayCast(); int id=hit.id;
    if(id==BLOCK_AIR||hit.length<0){
        if(in.placePressed){int slot=m(M_INVENTORYSLOT)&0x0F;int it=m(M_INVENTORY+slot);
            if((it&0xF0)==ITEM_APPLE){int hp=std::min((int)m(M_HEALTH)+APPLEHEALTH,MAXHEALTH);m(M_HEALTH)=(uint8_t)hp;
                int c=it-1; if((c&0xF0)!=ITEM_APPLE)c=0; m(M_INVENTORY+slot)=(uint8_t)c;}}
        return;
    }
    int bx=hit.bx,by=hit.by,bz=hit.bz;
    if(in.breakPressed&&id!=-1){
        if(id==BLOCK_SAPLING)return;
        if(id==BLOCK_LOG)m(M_LOGSINWORLD)=(uint8_t)(m(M_LOGSINWORLD)-1);
        int slot=m(M_INVENTORYSLOT)&0x0F;int held=m(M_INVENTORY+slot);int strength=STRENGTH_FIST;
        if(held>=ITEM_NONSTACKABLE&&held<=ITEM_SHEARS){
            int low=held&0x03,tier=(held>>2)&0x03,toolStrength=STRENGTH_WOOD+tier;
            if(held==ITEM_SHEARS) strength=(id==BLOCK_LEAVES)?STRENGTH_IRON:STRENGTH_FIST;
            else if(low==TOOL_PICKAXE&&getBlockType(id)==BLOCKTYPE_STONE)strength=toolStrength;
            else if(low==TOOL_AXE&&getBlockType(id)==BLOCKTYPE_WOOD)strength=toolStrength;
            else if(low==TOOL_SHOVEL&&getBlockType(id)==BLOCKTYPE_SOFT)strength=toolStrength;
        }
        int net=strength-getBlockHardness(id);

        int be=findBlockEntity(bx,by,bz);
        if(be>=0){
            if(tiles[be].storage>=0) freeStorageSlot(tiles[be].storage);
            if(loadedTile==be) loadedTile=-1;
            tiles[be].active=false;
            tiles[be].loaded=false;   // destroyed: never flush it back to disk
        }
        world.setBlock(bx,by,bz,BLOCK_AIR); m(M_NEEDRERENDER)=0xFF;
        if(net>=STRENGTHFORITEM&&id!=BLOCK_GLASS){
            if(id==BLOCK_GRASS)createEntity(bx,by,bz,ENTITY_DIRT);
            else if(id==BLOCK_STONE)createEntity(bx,by,bz,ENTITY_COBBLE);
            else if(id==BLOCK_LEAVES){ if(strength==STRENGTH_IRON)createEntity(bx,by,bz,ENTITY_LEAVES);
                else{int r=rng(); if(r<LEAVES_SAPLING_PROBABILITY)createEntity(bx,by,bz,ENTITY_SAPLING);
                    else if(r<LEAVES_STICK_PROBABILITY)createEntity(bx,by,bz,ENTITY_STICK);
                    else if(r<LEAVES_APPLE_PROBABILITY)createEntity(bx,by,bz,ENTITY_APPLE);}}
            else createEntity(bx,by,bz,id);
        }
        int yy=by; while(true){yy++;uint8_t a=world.getBlock(bx,yy,bz);
            if(a==BLOCK_SAND){createEntity(bx,yy,bz,ENTITY_FALLINGSAND);world.setBlock(bx,yy,bz,BLOCK_AIR);}
            else if(a==BLOCK_SAPLING){createEntity(bx,yy,bz,ENTITY_SAPLING);world.setBlock(bx,yy,bz,BLOCK_AIR);}
            else break;}
        return;
    }
    if(in.placePressed){

        if(id==BLOCK_TABLE){ screenId=SCR_CRAFTING; cursor=0; selSlot=-1; return; }
        if(id==BLOCK_FURNACE||id==BLOCK_CHEST){ int bi=findBlockEntity(bx,by,bz);
            if(bi>=0){openTileStorage(bi); loadedTile=bi; screenId=tiles[bi].isChest?SCR_CHEST:SCR_FURNACE; cursor=0; selSlot=-1;} return; }
        int slot=m(M_INVENTORYSLOT)&0x0F;int item=m(M_INVENTORY+slot);
        bool placeable=!(item==0||(item>=ITEM_IRONINGOT&&item<ITEM_TABLE)||item==ITEM_COAL);
        if(!placeable)return;
        int blockId=(item>=ITEM_TABLE)?(item&0x0F):(item>>4);
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
            b.bx=px;b.by=py;b.bz=pz;b.dir=(m(M_ROT)&0x0F);
            b.storage=allocStorage(); b.loaded=false;
            if(b.storage>=0){uint8_t sb[STORAGE_SLOT_SIZE];packStorage(b,sb);world.writeStorageSlot(b.storage,sb);}
            tiles.push_back(b);}
        if(blockId==BLOCK_LOG)m(M_LOGSINWORLD)=(uint8_t)(m(M_LOGSINWORLD)+1);
        if(item<0xF0){int c=item-1; if((c&0x0F)==0)c=0; m(M_INVENTORY+slot)=(uint8_t)c;} else m(M_INVENTORY+slot)=0;
        m(M_NEEDRERENDER)=0xFF;
    }
}

bool Game::playerCollides(int x,int y,int z){
    for(int bx=x/16;bx<=(x+PLAYERWIDTH)/16;bx++)
    for(int by=y/16;by<=(y+PLAYERHEIGHT)/16;by++)
    for(int bz=z/16;bz<=(z+PLAYERWIDTH)/16;bz++){
        uint8_t id=world.getBlock(bx,by,bz); if(id!=BLOCK_AIR&&id!=BLOCK_SAPLING)return true;}
    return false;
}
void Game::moveAndCollide(int dx,int dy,int dz){
    int x=playerX,y=playerY,z=playerZ;

    const int maxX = world.worldSX()*BLOCKSIZE - PLAYERWIDTH;
    const int maxZ = world.worldSZ()*BLOCKSIZE - PLAYERWIDTH;

    int nx=x+dx; nx=std::clamp(nx,0,maxX); if(!playerCollides(nx,y,z))x=nx;

    int nz=z+dz; nz=std::clamp(nz,0,maxZ); if(!playerCollides(x,y,nz))z=nz;

    int ny=y+dy;
    if(playerCollides(x,ny,z)){
        if(dy<0){
            int speed=-s8(m(M_VELY));
            int over=speed-MINFALLDAMAGESPEED;
            if(over>0){int dmg=smul446(over,FALLDAMAGESCALING); int hp=s8(m(M_HEALTH))-dmg;
                if(hp<=0){gameOverPending=true;} else m(M_HEALTH)=u8(hp);}
            m(M_ONGROUND)=0xFF;
        }
        m(M_VELY)=0;

        int step=(dy<0)?1:-1;
        while(playerCollides(x,ny,z)&&ny>=0&&ny<=WORLD_SY*BLOCKSIZE) ny+=step;
        y=ny;
    } else y=ny;
    if(y<0){y=0;m(M_ONGROUND)=0xFF;m(M_VELY)=0;}

    playerX=x;playerY=y;playerZ=z;
}
void Game::miscInputs(const Input& in){
    uint8_t cr=in.crouch?0xFF:0; if(cr!=m(M_CROUCHING)){m(M_CROUCHING)=cr;m(M_NEEDRERENDER)=0xFF;}
    if(in.turn||in.pitch){int rot=m(M_ROT);int yaw=(rot&0x0F),pitch=(rot>>4)&0x0F;
        yaw=(yaw+in.turn)&0x0F;

        if(in.pitch>0 && pitch!=4)  pitch=(pitch+1)&0x0F;
        if(in.pitch<0 && pitch!=12) pitch=(pitch-1)&0x0F;
        m(M_ROT)=(uint8_t)((pitch<<4)|yaw); m(M_NEEDRERENDER)=0xFF;}
    renderer.setCamRot(m(M_ROT));
    int sinY=(int)renderer.sinYaw(),cosY=(int)renderer.cosYaw();
    int fwd=smul446(in.forward,SPEEDFACTOR);

    int dx=smul446(fwd,sinY);
    int dz=smul446(fwd,cosY);
    int onG=m(M_ONGROUND); m(M_ONGROUND)=0;
    if(onG&&in.jump)m(M_VELY)=JUMPSTRENGTH; else m(M_VELY)=u8(s8(m(M_VELY))-GRAVITY);
    int dy=s8(m(M_VELY));
    moveAndCollide(dx,dy,dz);

    bool moving = in.forward!=0 && m(M_ONGROUND);
    if(moving) bobTimer += BOB_SPEED;
    float target = moving ? 1.0f : 0.0f;
    if(bobAmt<target){ bobAmt+=BOB_EASE; if(bobAmt>target)bobAmt=target; }
    else if(bobAmt>target){ bobAmt-=BOB_EASE; if(bobAmt<target)bobAmt=target; }
}

void Game::updateAllItems(){
    int pxc=playerX+PLAYERHALFWIDTH, pyc=playerY+PLAYERCAMHEIGHT, pzc=playerZ+PLAYERHALFWIDTH;
    for(auto& e:items){ if(!e.active)continue;

        e.vy-=2; if(e.vy<-8)e.vy=-8;
        int ny=e.y+e.vy;
        int bx=e.x/16, bz=e.z/16, nby=ny/16;
        if(ny<0){ny=0;e.vy=0;}
        uint8_t below=world.getBlock(bx,nby,bz);
        if(below!=BLOCK_AIR&&below!=BLOCK_SAPLING){ e.y=(nby+1)*16; e.vy=0;
            if(e.id==ENTITY_FALLINGSAND){ world.setBlock(bx,e.y/16,bz,BLOCK_SAND); e.active=false; m(M_NEEDRERENDER)=0xFF; continue; } }
        else e.y=ny;
        if(e.id==ENTITY_FALLINGSAND)continue;

        if(std::abs(e.x-pxc)<=PICKUPSIDEPOS && std::abs(e.z-pzc)<=PICKUPSIDEPOS &&
           e.y-pyc<=PICKUPUP && pyc-e.y<=PICKUPDOWN+PLAYERCAMHEIGHT){
            int item;
            switch(e.id){case ENTITY_APPLE:item=ITEM_APPLE|1;break;case ENTITY_TABLE:item=ITEM_TABLE;break;
                case ENTITY_FURNACE:item=ITEM_FURNACE;break;case ENTITY_CHEST:item=ITEM_CHEST;break;
                default:item=(e.id<<4)|1;break;}
            addItemToInventory(item); e.active=false;
        }
    }
    items.erase(std::remove_if(items.begin(),items.end(),[](const ItemEnt&e){return !e.active;}),items.end());
}

// Load furnaces that entered the active simulation window, flush+unload those
// that left it. Furnaces then smelt continuously while resident (updateAllFurnaces),
// no longer only while their GUI is open. Chests stay lazy (loaded on open) since
// they do not tick. The window is activeWindowAround() from flipcraft.h.
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
        uint16_t res=craftFurnace(f.slot[0]);
        bool valid = res!=0 && (f.slot[0]&0x0F)>0 &&
            (f.slot[2]==0 || ((f.slot[2]&0xF0)==(res&0xF0) && (f.slot[2]&0x0F)+(res&0x0F)<=15));

        if(valid && f.fuelTime==0 && f.slot[1]!=0){
            int add=0; int fuel=f.slot[1]&0xF0;
            if(fuel==ITEM_COAL)add=8; else if(fuel==ITEM_STICK||fuel==ITEM_SAPLING||(fuel>=ITEM_WOODPICKAXE&&fuel<=ITEM_WOODSWORD))add=1;
            else if(fuel==ITEM_PLANK||fuel==ITEM_LOG)add=2;
            if(add){ f.fuelTime=add; f.slot[1]=(uint8_t)((f.slot[1]&0x0F)>1?f.slot[1]-1:0); f.lit=true; }
        }
        if(valid && f.fuelTime>0){
            f.timer++;
            if(f.timer>=SMELTTIME/16){ f.timer=0;
                f.slot[0]=(uint8_t)((f.slot[0]&0x0F)>1?f.slot[0]-1:0);
                if(f.slot[2]==0)f.slot[2]=(uint8_t)res; else f.slot[2]=(uint8_t)((f.slot[2]&0xF0)|((f.slot[2]&0x0F)+(res&0x0F)));
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
        return a==BLOCK_AIR||a==BLOCK_LEAVES||a==BLOCK_GLASS||a==BLOCK_SAPLING;};

    int pbx=(playerX+PLAYERHALFWIDTH)/BLOCKSIZE, pbz=(playerZ+PLAYERHALFWIDTH)/BLOCKSIZE;
    ActiveWindow win=activeWindowAround(pbx,pbz,world.worldSX(),world.worldSZ());
    int wsx=win.x1-win.x0+1, wsz=win.z1-win.z0+1;
    for(int i=0;i<RANDOMTICKSPEED;i++){
        int x=win.x0+rng()%wsx, y=rng()%WORLD_SY, z=win.z0+rng()%wsz; uint8_t b=world.getBlock(x,y,z);
        if(b==BLOCK_DIRT){ if(!above(x,y,z))continue; bool f=false;
            for(int gx=x-1;gx<=x+1&&!f;gx++)for(int gy=y-1;gy<=y+1&&!f;gy++)for(int gz=z-1;gz<=z+1&&!f;gz++)
                if(world.getBlock(gx,gy,gz)==BLOCK_GRASS)f=true;
            if(f){world.setBlock(x,y,z,BLOCK_GRASS);m(M_NEEDRERENDER)=0xFF;}}
        else if(b==BLOCK_GRASS){ if(!above(x,y,z)){world.setBlock(x,y,z,BLOCK_DIRT);m(M_NEEDRERENDER)=0xFF;}}
        else if(b==BLOCK_LEAVES){ if(m(M_LOGSINWORLD)==0){world.setBlock(x,y,z,BLOCK_AIR);
                int r=rng(); if(r<LEAVES_SAPLING_PROBABILITY)createEntity(x,y,z,ENTITY_SAPLING);
                else if(r<LEAVES_STICK_PROBABILITY)createEntity(x,y,z,ENTITY_STICK);
                else if(r<LEAVES_APPLE_PROBABILITY)createEntity(x,y,z,ENTITY_APPLE);
                m(M_NEEDRERENDER)=0xFF;}}
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
            int logs=0;
            for(int ty=upperTop-1;ty>=y;ty--){
                uint8_t old=world.getBlock(x,ty,z);
                if(old==BLOCK_AIR||old==BLOCK_LEAVES){world.setBlock(x,ty,z,BLOCK_LOG);logs++;}
            }
            m(M_LOGSINWORLD)=(uint8_t)(m(M_LOGSINWORLD)+logs);
            m(M_NEEDRERENDER)=0xFF;
        }
    }
}

void Game::respawn(){

    int x=(playerX & ~15)+3,z=(playerZ & ~15)+3;
    int bx=x/BLOCKSIZE,bz=z/BLOCKSIZE,by=playerY/BLOCKSIZE;
    while(by<WORLD_SY){
        uint8_t feet=world.getBlock(bx,by,bz);
        uint8_t head=world.getBlock(bx,by+1,bz);
        bool feetClear=feet==BLOCK_AIR||feet==BLOCK_SAPLING;
        bool headClear=head==BLOCK_AIR||head==BLOCK_SAPLING;
        if(feetClear&&headClear)break;
        by++;
    }
    playerX=x;playerY=by*BLOCKSIZE;playerZ=z;
    m(M_VELY)=0;m(M_ONGROUND)=0;m(M_HEALTH)=MAXHEALTH;
    m(M_NEEDRERENDER)=0xFF;
    screenId=SCR_PLAY;selSlot=-1;loadedTile=-1;gameOverPending=false;
    finishRender();
}

void Game::drawHotbar(){
    screen.x1=19;screen.y1=51;screen.x2=76;screen.y2=63;screen.clearRect();
    screen.x1=20;screen.y1=52;screen.x2=75;screen.y2=63;screen.drawRect();
    int sel=m(M_INVENTORYSLOT)&0x0F;
    for(int i=0;i<5;i++){int x=21+i*11,y=53; screen.x1=x;screen.y1=y;screen.x2=x+9;screen.y2=y+9;screen.clearRect();
        if(i==sel){screen.x1=x;screen.y1=y;screen.x2=x+9;screen.y2=y+9;screen.drawRect();
            screen.x1=x+1;screen.y1=y+1;screen.x2=x+8;screen.y2=y+8;screen.clearRect();}
        int it=m(M_INVENTORY+i); if(it){screen.itemIcon(x+2,y+2,it);
            if((it&0xF0)<0xF0){int n=it&0x0F; if(n>0&&n<=9)screen.number(x+6,y+5,n);}}}
    for(int i=0;i<MAXHEALTH;i++)screen.heart(19+i*6,43,i<(int)m(M_HEALTH));
}
void Game::finishRender(){

    world.updateWindow((playerX+PLAYERHALFWIDTH)/BLOCKSIZE, (playerZ+PLAYERHALFWIDTH)/BLOCKSIZE);
    renderer.clearBuffer(); renderer.setCamRot(m(M_ROT));
    float bobV=sinf(bobTimer*2.0f)*bobAmt;
    renderer.camPos[0]=playerX+PLAYERHALFWIDTH; renderer.camPos[2]=playerZ+PLAYERHALFWIDTH;
    renderer.camPos[1]=playerY+(m(M_CROUCHING)?PLAYERCROUCHCAMHEIGHT:PLAYERCAMHEIGHT)+bobV*CAM_BOB_AMPLITUDE;
    renderer.renderScene(world);
    for(auto& f:tiles) if(f.active){
        int tex = f.isChest?TEX_CHESTFRONT:(f.lit?TEX_FURNACEFRONTON:TEX_FURNACEFRONTOFF);
        renderer.renderFace(f.bx,f.by,f.bz,(uint8_t)tex,f.dir&3,false);
    }
    for(auto& e:items) if(e.active)
        renderer.renderItem(e.x/16.0f,e.y/16.0f,e.z/16.0f,(uint8_t)e.id);
    RayHit hit=rayCast();
    if(hit.id!=BLOCK_AIR&&hit.id!=-1&&hit.length>=0) renderer.renderOverlay(world,hit.bx,hit.by,hit.bz,0);
    drawHotbar();
}

void Game::worldFrame(const Input& in){
    if(in.slotScroll){int s=(m(M_INVENTORYSLOT)&0x0F)+in.slotScroll; if(s<0)s=4; if(s>4)s=0; m(M_INVENTORYSLOT)=(uint8_t)s;}
    handleBreakAndPlace(in);
    if(screenId!=SCR_PLAY)return;
    miscInputs(in);
    updateAllItems(); doRandomTicks();
    simulateFurnaces();   // every furnace in the active window smelts, GUI open or not
    if(gameOverPending){gameOverPending=false; score=0; screenId=SCR_GAMEOVER; return;}
    finishRender();
    m(M_NEEDRERENDER)=0;
}

std::vector<Game::Slot> Game::buildSlots(ScreenId s){
    std::vector<Slot> v;
    auto add=[&](uint8_t* c,int gx,int gy,int sx,int sy,bool grid,bool out){v.push_back({c,gx,gy,sx,sy,grid,out});};

    for(int r=0;r<3;r++)for(int c=0;c<5;c++) add(&m(M_INVENTORY+r*5+c),c,r,21+c*11,53-r*11,false,false);
    if(s==SCR_INVENTORY){
        int map[4]={0,1,3,4};
        for(int i=0;i<4;i++){int gx=i%2,gy=i/2; add(&m(M_CRAFTINGGRID+map[i]),5+gx,4+gy,26+gx*9,9+gy*9,true,false);}
        add(&m(M_CRAFTINGOUTPUT),6,5,60,14,false,true);
    } else if(s==SCR_CRAFTING){
        for(int i=0;i<9;i++){int gx=i%3,gy=i/3; add(&m(M_CRAFTINGGRID+i),5+gx,4+gy,21+gx*9,1+gy*9,true,false);}
        add(&m(M_CRAFTINGOUTPUT),8,5,65,10,false,true);
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
    uint8_t grid[9]={0}; for(int i=0;i<9;i++)grid[i]=m(M_CRAFTINGGRID+i);
    uint16_t out=craftTable(grid); if(!out)return;

    for(int i=0;i<9;i++){int c=m(M_CRAFTINGGRID+i); if(c){int n=(c&0x0F)-1; m(M_CRAFTINGGRID+i)=(uint8_t)(n>0?(c&0xF0)|n:0);}}
    addItemToInventory(out&0xFF);
}
void Game::guiFrame(const Input& in){
    auto slots=buildSlots(screenId);
    if(in.openInventory){
        if(loadedTile>=0 && (size_t)loadedTile<tiles.size()) flushTileStorage(loadedTile);
        screenId=SCR_PLAY; selSlot=-1; m(M_NEEDRERENDER)=0xFF; loadedTile=-1; return;}

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
            int sv=*src.cell, stype=sv&0xF0, scnt=sv&0x0F;
            if(stype<0xF0 && scnt>0 && !dst.output){
                int dv=*dst.cell;
                bool moved=false;
                if(dv==0){ *dst.cell=(uint8_t)(stype|1); moved=true; }
                else if((dv&0xF0)==stype && (dv&0x0F)<15){ *dst.cell=(uint8_t)(dv+1); moved=true; }
                if(moved){ scnt--; if(scnt<=0){*src.cell=0; selSlot=-1;} else *src.cell=(uint8_t)(stype|scnt); }
            }
            if(screenId==SCR_INVENTORY||screenId==SCR_CRAFTING){
                uint8_t g[9]={0}; for(int i=0;i<9;i++)g[i]=m(M_CRAFTINGGRID+i);
                m(M_CRAFTINGOUTPUT)=(uint8_t)(craftTable(g)&0xFF);
            }
        }
    }
    if(in.menuSelect&&!slots.empty()){
        Slot& cur=slots[cursor];
        if(cur.output){
            if(screenId==SCR_FURNACE){
                if(*cur.cell){addItemToInventory(*cur.cell);*cur.cell=0;}
            } else if(*cur.cell){
                tryCraft();
            }
        } else if(selSlot<0){ if(*cur.cell)selSlot=cursor; }
        else { std::swap(*slots[selSlot].cell,*cur.cell); selSlot=-1; }

        if(screenId==SCR_INVENTORY||screenId==SCR_CRAFTING){
            uint8_t g[9]={0}; for(int i=0;i<9;i++)g[i]=m(M_CRAFTINGGRID+i);
            m(M_CRAFTINGOUTPUT)=(uint8_t)(craftTable(g)&0xFF);
        }
    }
    if(screenId==SCR_FURNACE)updateAllFurnaces();
    drawGui();
}
void Game::drawGui(){
    if(screenId==SCR_GAMEOVER){ screen.clearScreen();
        screen.x1=12;screen.y1=10;screen.x2=83;screen.y2=46;screen.drawRect();
        screen.x1=13;screen.y1=11;screen.x2=82;screen.y2=45;screen.clearRect();

        screen.x1=44;screen.y1=16;screen.x2=51;screen.y2=23;screen.drawRect();
        screen.x1=46;screen.y1=18;screen.x2=47;screen.y2=19;screen.clearRect();
        screen.x1=49;screen.y1=18;screen.x2=50;screen.y2=19;screen.clearRect();
        int sc=score; int x=42; for(int p=100;p>=1;p/=10){int d=(sc/p)%10; if(p<100&&sc<p&&p>1){} screen.number(x,28,d); x+=5;}
        screen.x1=34;screen.y1=36;screen.x2=61;screen.y2=43;screen.drawRect();
        screen.x1=35;screen.y1=37;screen.x2=60;screen.y2=42;screen.clearRect();
        return; }
    screen.clearScreen();
    auto slots=buildSlots(screenId);
    for(size_t i=0;i<slots.size();i++){Slot& s=slots[i]; int it=*s.cell;
        int box=s.grid?8:10; screen.x1=s.sx;screen.y1=s.sy;screen.x2=s.sx+box-1;screen.y2=s.sy+box-1;screen.drawRect();
        screen.x1=s.sx+1;screen.y1=s.sy+1;screen.x2=s.sx+box-2;screen.y2=s.sy+box-2;screen.clearRect();
        if(it){screen.itemIcon(s.sx+2,s.sy+2,it); if((it&0xF0)<0xF0){int n=it&0x0F;if(n>0&&n<=9)screen.number(s.sx+6,s.sy+5,n);}}
        if((int)i==cursor)
            screen.invertRect(s.sx+1,s.sy+1,s.sx+box-2,s.sy+box-2);
    }
    if(selSlot>=0&&selSlot<(int)slots.size()){Slot& s=slots[selSlot];
        screen.x1=s.sx+3;screen.y1=s.sy+3;screen.x2=s.sx+4;screen.y2=s.sy+4;screen.drawRect();}
}

void Game::frame(const Input& in){
    if(screenId==SCR_GAMEOVER){ if(in.menuSelect){respawn();} else drawGui(); return; }
    if(screenId==SCR_PLAY){
        if(in.openInventory){screenId=SCR_INVENTORY;cursor=0;selSlot=-1;drawGui();return;}
        worldFrame(in);
        if(screenId!=SCR_PLAY)drawGui();
    } else {
        guiFrame(in);
    }
}

}
