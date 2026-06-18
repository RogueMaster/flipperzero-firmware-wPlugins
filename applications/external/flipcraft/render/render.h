
#pragma once
#include "../flipcraft.h"

namespace flipcraft {
struct Vertex { float x=0, y=0, z=0, u=0, v=0; };

class Renderer {
public:
    Renderer();

    float camPos[3] = {0,0,0};
    float matrix[3][3];
    int yawIndex = 0, pitchIndex = 0;
    Texture texture = TEX_EMPTY;
    struct { bool cullBackface=true, transparent=false, inverted=false, overlay=false; } settings;

    int winX0 = 0, winX1 = WORLD_SX - 1, winZ0 = 0, winZ1 = WORLD_SZ - 1;

    uint8_t zdepth[SCREEN_HEIGHT][SCREEN_WIDTH];
    uint8_t (*zcolour)[SCREEN_WIDTH] = nullptr;

    void setCamRot(uint8_t data);
    void clearBuffer();
    float sinYaw() const, cosYaw() const;
    float camDir(int axis) const;

    Vertex worldToCam(const Vertex& v) const;
    void drawQuadWorld(const Vertex quad[4]);

    void renderScene(const World& w);
    void renderFace(int x,int y,int z,uint8_t texId,int direction,bool small_);
    void renderItem(float x,float y,float z,uint8_t itemId);
    void renderOverlay(const World& w,int x,int y,int z,int breakPhase);

private:
    void camRotToMatrix(int pitchIndex,int yawIndex);
    Vertex camToScreen(const Vertex& v) const;
    void drawQuadCam(Vertex q[4]);
    void renderQuad(float x,float y,float z,int quadId,uint8_t texId,int texSettings);
    void rasterTri(const Vertex& a,const Vertex& b,const Vertex& c);
    bool isBackfacing(const Vertex& a,const Vertex& b,const Vertex& c) const;
};

}
