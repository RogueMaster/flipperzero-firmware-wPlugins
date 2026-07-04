#pragma once
#include "../flipcraft.h"
#include <vector>

namespace flipcraft {

struct Vertex { float x=0, y=0, z=0, u=0, v=0; };

// Visible faces of one resident chunk, packed one face per uint32_t:
//   bits 0-2  local x        bits 3-5  local z      bits 6-9   y
//   bits 10-14 quad id       bits 15-22 texture id  bits 23-26 settings
// Rebuilt only when the chunk content changes (World::slotGen mismatch), so a
// frame never scans voxels -- it just walks these lists.
struct ChunkMesh {
    int cx = -1, cz = -1;
    uint16_t gen = 0;
    std::vector<uint32_t> faces;
};

class Renderer {
public:
    Renderer();

    float camPos[3] = {0,0,0};
    float matrix[3][3];
    int yawIndex = 0, pitchIndex = 0;
    Texture texture = TEX_EMPTY;
    struct { bool cullBackface=true, transparent=false, inverted=false, overlay=false; } settings;

    int winX0 = 0, winX1 = WORLD_SX - 1, winZ0 = 0, winZ1 = WORLD_SZ - 1;

    // Shared with Game::fb. Each byte packs depth << 1 | colour: the rasterizer
    // clamps depth to 7 bits, so one 8 KB buffer serves as both z-buffer and
    // framebuffer. 2D UI code writes plain 0/1 bytes (depth 0) on top.
    uint8_t (*zbuf)[SCREEN_WIDTH] = nullptr;

    void setCamRot(uint8_t data);
    void clearBuffer();
    // Drop all cached chunk meshes; call when a different world is opened.
    void invalidateChunkMeshes();
    float sinYaw() const, cosYaw() const;
    float camDir(int axis) const;

    void renderScene(const World& w);
    void renderFace(int x,int y,int z,uint8_t texId,int direction,bool small_);
    void renderItem(float x,float y,float z,uint8_t itemId,uint8_t inv=0);
    void renderDynamite(float x,float y,float z,uint8_t inv);
    void renderMob(float x,float y,float z,uint8_t species,uint8_t yaw,uint8_t inv,uint8_t sc16);
    void renderOverlay(const World& w,int x,int y,int z,int breakPhase);

private:
    ChunkMesh chunkMesh[WINDOW_CHUNKS][WINDOW_CHUNKS];

    void camRotToMatrix(int pitchIndex,int yawIndex);
    void renderBox(float x0,float y0,float z0,float x1,float y1,float z1,
                   const uint8_t tex[6],int texSettings,uint8_t headDir);
    Vertex worldToCam(const Vertex& v) const;
    Vertex camToScreen(const Vertex& v) const;
    void drawQuadCam(Vertex q[4]);
    void renderQuad(float x,float y,float z,int quadId,uint8_t texId,int texSettings);
    void drawBlockQuad(int x,int y,int z,int quadId,uint8_t texId,int texSettings);
    void buildChunkMesh(const World& w,int sx,int sz);
    void rasterTri(const Vertex& a,const Vertex& b,const Vertex& c);
    bool isBackfacing(const Vertex& a,const Vertex& b,const Vertex& c) const;
};

}
