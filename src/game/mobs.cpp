#include "game.h"
#include <algorithm>

namespace flipcraft {

// forward = (-sin, cos) * 64, camera yaw convention
static constexpr int8_t kDirX[16] = {0,-24,-45,-59,-64,-59,-45,-24,0,24,45,59,64,59,45,24};
static constexpr int8_t kDirZ[16] = {64,59,45,24,0,-24,-45,-59,-64,-59,-45,-24,0,24,45,59};
// yaw sector along (dx,dz), index (dx>0)<<2 | (dz>0)<<1 | (|dx|>|dz|)
static constexpr uint8_t kSeekYaw[8] = {7,5,1,3,9,11,15,13};
// indexed by temperament
static constexpr uint8_t kHurtMode[3] = {MOB_FLEE, MOB_CHASE, MOB_CHASE};
static constexpr uint8_t kHurtTime[3] = {30, 90, 90};
static constexpr uint8_t kAggro[3] = {0, 0, 96};
static constexpr uint8_t kMobDrop[3] = {ENTITY_APPLE /* мясо */, ENTITY_APPLE /* мясо */, ENTITY_GUNPOWDER};
// 16 nibbles of species id: 8 sheep, 6 wolves, 2 creepers
static constexpr uint64_t SPAWN_ROLL = UINT64_C(0x2211111100000000);

static inline int mobHeight(const MobSpec& s) { return (s.geom >> 4) << 1; }

void Game::trySpawnMob(){
    if(rng() & 0x1F) return;
    int slot=-1;
    for(int i=0;i<MAX_MOBS;i++) if(!mobs[i].active){ slot=i; break; }
    if(slot<0) return;

    uint8_t p=rng(), q=rng();
    int bx=(playerX+PLAYERHALFWIDTH)/BLOCKSIZE + (3+(int)(((p&31)*6)>>5))*(1-(int)((p>>5)&2));
    int bz=(playerZ+PLAYERHALFWIDTH)/BLOCKSIZE + (3+(int)(((q&31)*6)>>5))*(1-(int)((q>>5)&2));
    if((unsigned)bx>=(unsigned)world.worldSX() || (unsigned)bz>=(unsigned)world.worldSZ()) return;

    int by=WORLD_SY-1;
    while(by>0 && world.getBlock(bx,by,bz)==BLOCK_AIR) by--;
    if(!blockIsSolid(world.getBlock(bx,by,bz))) return;

    uint8_t sp=(uint8_t)((SPAWN_ROLL>>((rng()&15)<<2))&0xF);
    const MobSpec& s=mobSpec(sp);
    int x=bx*BLOCKSIZE+1, y=(by+1)*BLOCKSIZE, z=bz*BLOCKSIZE+1;
    if(boxCollides(x,y,z,MOBWIDTH,mobHeight(s))) return;

    Mob m{}; m.active=true; m.species=sp;
    m.x=x; m.y=y; m.z=z;
    m.hp=s.hpDmg>>4; m.yaw=rng()&15; m.mode=MOB_WANDER; m.timer=20+(rng()&31);
    mobs[slot]=m;
}

void Game::hurtMobFrom(int index,int dmg,int srcX,int srcZ,uint8_t attacker){
    Mob& m=mobs[index];
    const MobSpec& s=mobSpec(m.species);
    if((int)m.hp<=dmg){
        createEntity((m.x+7)>>4,(m.y+8)>>4,(m.z+7)>>4,kMobDrop[m.species]);
        score++;
        m.active=false;
        return;
    }
    m.hp=(uint8_t)(m.hp-dmg);
    m.hurt=MOB_HURT_TICKS;

    int temp=(s.info>>1)&3;
    m.mode=kHurtMode[temp]; m.timer=kHurtTime[temp];
    m.target=attacker;
    int dx=srcX-(m.x+7), dz=srcZ-(m.z+7);
    int key=((dx>0)<<2)|((dz>0)<<1)|(std::abs(dx)>std::abs(dz));
    m.yaw=kSeekYaw[key]^((m.mode&1)<<3);

    int away=kSeekYaw[key]^8;
    int nx=std::clamp(m.x+((kDirX[away]*10)>>6), 0, world.worldSX()*BLOCKSIZE-MOBWIDTH-1);
    int nz=std::clamp(m.z+((kDirZ[away]*10)>>6), 0, world.worldSZ()*BLOCKSIZE-MOBWIDTH-1);
    if(!boxCollides(nx,m.y,nz,MOBWIDTH,mobHeight(s))){ m.x=nx; m.z=nz; }
    m.vy=5;
}

void Game::explodeAt(int cx,int cy,int cz){
    for(int by=cy-1;by<=cy+1;by++)
    for(int bz=cz-1;bz<=cz+1;bz++)
    for(int bx=cx-1;bx<=cx+1;bx++){
        if(world.getBlock(bx,by,bz)==BLOCK_DYNAMITE){
            igniteDynamite(bx,by,bz,3+(rng()&3));
            continue;
        }
        int be=findBlockEntity(bx,by,bz);
        if(be>=0){
            if(tiles[be].storage>=0) freeStorageSlot(tiles[be].storage);
            if(loadedTile==be) loadedTile=-1;
            tiles[be].active=false; tiles[be].loaded=false;
        }
        world.setBlock(bx,by,bz,BLOCK_AIR);
    }
    for(int bz=cz-1;bz<=cz+1;bz++)
    for(int bx=cx-1;bx<=cx+1;bx++){
        int yy=cy+1;
        while(true){ yy++; uint8_t a=world.getBlock(bx,yy,bz);
            if(a==BLOCK_SAND){createEntity(bx,yy,bz,ENTITY_FALLINGSAND);world.setBlock(bx,yy,bz,BLOCK_AIR);}
            else if(a==BLOCK_SAPLING){createEntity(bx,yy,bz,ENTITY_SAPLING);world.setBlock(bx,yy,bz,BLOCK_AIR);}
            else break; }
    }
    int ex=cx*16+8, ey=cy*16+8, ez=cz*16+8;
    if(std::abs(playerX+PLAYERHALFWIDTH-ex)<MOB_BLAST_RANGE &&
       std::abs(playerZ+PLAYERHALFWIDTH-ez)<MOB_BLAST_RANGE &&
       std::abs(playerY+PLAYERHEIGHT/2-ey)<MOB_BLAST_RANGE){
        int hp=(int)pl.health-MOB_BLAST_DMG;
        if(hp<=0) gameOverPending=true; else pl.health=u8(hp);
    }
    for(int i=0;i<MAX_MOBS;i++){
        Mob& o=mobs[i];
        if(!o.active) continue;
        if(std::abs(o.x+7-ex)<MOB_BLAST_RANGE && std::abs(o.z+7-ez)<MOB_BLAST_RANGE &&
           std::abs(o.y-ey)<MOB_BLAST_RANGE)
            hurtMobFrom(i,MOB_BLAST_DMG,ex,ez,0xFF);
    }
}

void Game::explodeMob(Mob& m){
    m.active=false;
    createEntity((m.x+7)>>4,(m.y+8)>>4,(m.z+7)>>4,ENTITY_GUNPOWDER);
    explodeAt((m.x+7)>>4,(m.y+8)>>4,(m.z+7)>>4);
}

void Game::updateAllMobs(){
    trySpawnMob();

    const int pxc=playerX+PLAYERHALFWIDTH, pzc=playerZ+PLAYERHALFWIDTH;
    ActiveWindow win=activeWindowAround(pxc/BLOCKSIZE, pzc/BLOCKSIZE,
                                        world.worldSX(), world.worldSZ());
    // blocks beyond the resident ring read as air, so movement is clamped to it
    const int wx0=win.x0*BLOCKSIZE, wx1=(win.x1+1)*BLOCKSIZE-MOBWIDTH-1;
    const int wz0=win.z0*BLOCKSIZE, wz1=(win.z1+1)*BLOCKSIZE-MOBWIDTH-1;

    for(int mi=0;mi<MAX_MOBS;mi++){
        Mob& m=mobs[mi];
        if(!m.active) continue;
        const MobSpec& s=mobSpec(m.species);
        const int hgt=mobHeight(s), temp=(s.info>>1)&3;

        int bx=(m.x+7)>>4, bz=(m.z+7)>>4;
        if(bx<win.x0-4||bx>win.x1+4||bz<win.z0-4||bz>win.z1+4){ m.active=false; continue; }
        if(bx<win.x0||bx>win.x1||bz<win.z0||bz>win.z1) continue;

        if(m.hurt) m.hurt--;
        if(m.cool) m.cool--;

        if(m.mode<MOB_CHASE){
            int pdx=std::abs(pxc-(m.x+7)), pdz=std::abs(pzc-(m.z+7));
            if((pdx|pdz)<kAggro[temp] && std::abs(playerY-m.y)<48){
                m.mode=MOB_CHASE; m.target=0xFF; m.timer=kHurtTime[temp];
            } else if(s.prey && !(rng()&7)){
                for(int oi=0;oi<MAX_MOBS;oi++){
                    const Mob& o=mobs[oi];
                    if(!o.active || !((s.prey>>o.species)&1)) continue;
                    if((std::abs(o.x-m.x)|std::abs(o.z-m.z))<96 && std::abs(o.y-m.y)<48){
                        m.mode=MOB_CHASE; m.target=(uint8_t)oi; m.timer=90;
                        break;
                    }
                }
            }
        }

        int tx=pxc, ty=playerY, tz=pzc;
        if(m.target!=0xFF){
            const Mob& o=mobs[m.target];
            if(!o.active){ m.target=0xFF; if(m.mode>=MOB_CHASE){m.mode=MOB_WANDER; m.timer=20;} }
            else { tx=o.x+7; ty=o.y; tz=o.z+7; }
        }
        int dx=tx-(m.x+7), dz=tz-(m.z+7);
        int adx=std::abs(dx), adz=std::abs(dz);

        if(m.timer) m.timer--;
        else { uint8_t r=rng(); m.mode=r&1; m.yaw=(r>>1)&15; m.timer=20+(r>>4); m.target=0xFF; }

        // wander at half speed, chase/flee at full
        int v = m.mode ? (int)(s.geom&0x0F)<<(m.mode>>1)>>1 : 0;
        int mx,mz;
        if(m.mode>=MOB_CHASE){
            int key=((dx>0)<<2)|((dz>0)<<1)|(adx>adz);
            m.yaw=kSeekYaw[key]^((m.mode&1)<<3);
            int L=adx>adz?adx:adz; if(!L)L=1;
            int sgn=1-((m.mode&1)<<1);
            mx=v*dx*sgn/L; mz=v*dz*sgn/L;
        } else {
            mx=(v*kDirX[m.yaw])>>6; mz=(v*kDirZ[m.yaw])>>6;
        }

        if(s.info&1){
            if(m.cool){
                m.hurt=m.cool;
                if(m.cool==1){ explodeMob(m); continue; }
                if((adx|adz)>64){ m.cool=0; m.hurt=0; }
                mx=0; mz=0;
            } else if(m.mode==MOB_CHASE && (adx|adz)<30 && std::abs(ty-m.y)<32){
                m.cool=MOB_FUSE_TICKS;
            }
        }

        if(boxCollides(m.x,m.y,m.z,MOBWIDTH,hgt)) m.y+=16;

        auto overPlayer=[&](int x,int y,int z){
            return x<playerX+PLAYERWIDTH && x+MOBWIDTH>playerX &&
                   z<playerZ+PLAYERWIDTH && z+MOBWIDTH>playerZ &&
                   y<playerY+PLAYERHEIGHT && y+hgt>playerY;
        };
        const bool wasP=overPlayer(m.x,m.y,m.z);
        bool grounded=boxCollides(m.x,m.y-1,m.z,MOBWIDTH,hgt);
        int nx=std::clamp(m.x+mx,wx0,wx1), nz=std::clamp(m.z+mz,wz0,wz1);
        if(m.mode==MOB_WANDER && (nx!=m.x+mx || nz!=m.z+mz)) m.yaw^=8;
        int stepY=((m.y>>4)+1)<<4;
        if(overPlayer(nx,m.y,m.z) && !wasP) nx=m.x;
        if(!boxCollides(nx,m.y,m.z,MOBWIDTH,hgt)) m.x=nx;
        else if(grounded && m.vy==0 && !boxCollides(nx,stepY,m.z,MOBWIDTH,hgt)) m.vy=9;
        if(overPlayer(m.x,m.y,nz) && !wasP) nz=m.z;
        if(!boxCollides(m.x,m.y,nz,MOBWIDTH,hgt)) m.z=nz;
        else if(grounded && m.vy==0 && !boxCollides(m.x,stepY,nz,MOBWIDTH,hgt)) m.vy=9;

        m.vy-=2; if(m.vy<-8)m.vy=-8;
        int ny=m.y+m.vy; if(ny<0){ny=0;m.vy=0;}
        if(boxCollides(m.x,ny,m.z,MOBWIDTH,hgt)){
            if(m.vy<0) m.y=((ny>>4)+1)<<4;
            m.vy=0;
        } else m.y=ny;

        int dmgN=s.hpDmg&0x0F;
        if(dmgN && m.mode==MOB_CHASE && !m.cool &&
           adx<18 && adz<18 && m.y<ty+24 && m.y+hgt>ty){
            m.cool=MOB_ATTACK_COOL;
            if(m.target==0xFF){
                int hp=(int)pl.health-dmgN;
                if(hp<=0) gameOverPending=true; else pl.health=u8(hp);
            } else {
                bool boomPrey=(mobSpec(mobs[m.target].species).info&1)!=0;
                hurtMobFrom(m.target,dmgN,m.x+7,m.z+7,(uint8_t)mi);
                if(boomPrey){ m.mode=MOB_FLEE; m.timer=25; m.cool=38; }   // bite and run, ~3 s
            }
        }
    }
}

}
