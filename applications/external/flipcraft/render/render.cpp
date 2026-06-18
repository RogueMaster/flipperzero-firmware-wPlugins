

#include "render.h"
#include <algorithm>
#include <cstring>

namespace flipcraft {

static constexpr float PI = 3.14159265358979323846f;

static float kFsin[16], kFcos[16];
static float kSinYaw[16], kCosYaw[16];
static bool  gTrigReady = false;

static void initTrigLUT() {
    if(gTrigReady) return;
    auto fsinf = [](float a){ return FixedPoint(fabsf(sinf(a)),16,14) * (sinf(a) > 0 ? 1.0f : -1.0f); };
    auto fcosf = [](float a){ return FixedPoint(fabsf(cosf(a)),16,14) * (cosf(a) > 0 ? 1.0f : -1.0f); };
    for(int i = 0; i < 16; i++) {
        float a = PI * 2.0f * (i / 16.0f);
        kFsin[i] = fsinf(a);
        kFcos[i] = fcosf(a);
        kSinYaw[i] = floorf(-sinf(a) * 64.0f);
        kCosYaw[i] = floorf( cosf(a) * 64.0f);
    }
    gTrigReady = true;
}

Renderer::Renderer() {
    initTrigLUT();
    camRotToMatrix(0, 0);
    clearBuffer();
}

void Renderer::camRotToMatrix(int pitch, int yaw) {
    yawIndex = yaw; pitchIndex = pitch;
    float sc = kFsin[yaw & 0xF],   cc = kFcos[yaw & 0xF];
    float sb = kFsin[pitch & 0xF], cb = kFcos[pitch & 0xF];
    matrix[0][0]=cc;       matrix[0][1]=0;  matrix[0][2]=sc;
    matrix[1][0]=sb*sc;    matrix[1][1]=cb; matrix[1][2]=-sb*cc;
    matrix[2][0]=-cb*sc;   matrix[2][1]=sb; matrix[2][2]=cb*cc;
}
void Renderer::setCamRot(uint8_t data) { camRotToMatrix(data >> 4, data & 0xF); }

float Renderer::sinYaw() const { return kSinYaw[yawIndex & 0xF]; }
float Renderer::cosYaw() const { return kCosYaw[yawIndex & 0xF]; }
float Renderer::camDir(int axis) const { return floorf(matrix[2][axis]*64.0f); }

Vertex Renderer::worldToCam(const Vertex& v) const {
    float ox = v.x - camPos[0], oy = v.y - camPos[1], oz = v.z - camPos[2];
    Vertex r;
    r.x = FixedPoint(matrix[0][0]*ox + matrix[0][1]*oy + matrix[0][2]*oz, 16, 7, true);
    r.y = FixedPoint(matrix[1][0]*ox + matrix[1][1]*oy + matrix[1][2]*oz, 16, 7, true);
    r.z = FixedPoint(matrix[2][0]*ox + matrix[2][1]*oy + matrix[2][2]*oz, 16, 7, true);
    r.u = v.u; r.v = v.v;
    return r;
}

Vertex Renderer::camToScreen(const Vertex& v) const {
    float invZ = FixedPoint(1.0f / v.z, 17, 16);
    float persp = FixedPoint((float)LENS * invZ, 16, 10);
    float vx = FixedPoint(fabsf(v.x) * persp, 16, 0) * (v.x >= 0 ? 1.0f : -1.0f);
    float vy = FixedPoint(fabsf(v.y) * persp, 16, 0) * (v.y >= 0 ? 1.0f : -1.0f);
    constexpr float HALF_W = SCREEN_WIDTH / 2;                       // 64
    constexpr float CENTER_Y = (SCREEN_HEIGHT - 1) - SCREEN_HEIGHT / 2; // 31
    float nvx = FixedPoint(vx + HALF_W, 16, 0, true);
    float nvy = FixedPoint(CENTER_Y - vy, 16, 0, true);
    nvx = std::clamp(nvx, -255.0f, 255.0f);
    nvy = std::clamp(nvy, -255.0f, 255.0f);
    Vertex r;
    r.x = nvx; r.y = nvy; r.z = invZ;
    r.u = FixedPoint(v.u * invZ, 16, 15);
    r.v = FixedPoint(v.v * invZ, 16, 15);
    return r;
}

bool Renderer::isBackfacing(const Vertex& v1,const Vertex& v2,const Vertex& v3) const {
    float cross = (v3.x - v1.x)*(v1.y - v2.y) - (v1.y - v3.y)*(v2.x - v1.x);
    return cross < 0.0f;
}

void Renderer::clearBuffer() {
    memset(zdepth, 0, sizeof(zdepth));
    if(zcolour) memset(zcolour, 0, (size_t)SCREEN_HEIGHT * SCREEN_WIDTH);
}

// Pixels between perspective-correct samples. Texture coords are interpolated
// affinely inside a run of this many pixels, so one reciprocal serves PERSP_STEP
// pixels instead of one per pixel. For voxel-sized faces the drift is invisible.
static constexpr int PERSP_STEP = 8;

void Renderer::rasterTri(const Vertex& A,const Vertex& B,const Vertex& C) {
    const float area = (B.x-A.x)*(C.y-A.y) - (B.y-A.y)*(C.x-A.x);
    if (fabsf(area) < 1e-9f) return;

    int minY = ifloor(std::min({A.y,B.y,C.y}));
    int maxY = -ifloor(-std::max({A.y,B.y,C.y}));   // == ceil
    int minX = ifloor(std::min({A.x,B.x,C.x}));
    int maxX = -ifloor(-std::max({A.x,B.x,C.x}));
    minY=std::max(minY,0); maxY=std::min(maxY,SCREEN_HEIGHT-1);
    minX=std::max(minX,0); maxX=std::min(maxX,SCREEN_WIDTH-1);
    if (minX>maxX || minY>maxY) return;

    const uint8_t* tex = textureBitmap(texture);

    const float e0dx = B.y-C.y, e0dy = C.x-B.x, e0c = B.x*C.y - B.y*C.x;
    const float e1dx = C.y-A.y, e1dy = A.x-C.x, e1c = C.x*A.y - C.y*A.x;

    const float ia = 1.0f/area;
    const float zA=A.z*ia, zB=B.z*ia, zC=C.z*ia;     // vertex 1/z, weighted
    const float uA=A.u*ia, uB=B.u*ia, uC=C.u*ia;     // vertex u/z, weighted
    const float vA=A.v*ia, vB=B.v*ia, vC=C.v*ia;     // vertex v/z, weighted
    const bool posArea = area > 0.0f;

    // Per-pixel deltas of the screen-linear quantities (constant for the tri):
    // invZ, S=u/z, T=v/z all step by these as x advances by one.
    const float de2dx = -(e0dx + e1dx);
    const float dInvZ = e0dx*zA + e1dx*zB + de2dx*zC;
    const float dS    = e0dx*uA + e1dx*uB + de2dx*uC;
    const float dT    = e0dx*vA + e1dx*vB + de2dx*vC;
    const float dDepth = 512.0f * dInvZ;

    for (int y=minY;y<=maxY;y++) {
        const float py = y+0.5f, px0 = minX+0.5f;
        float e0 = e0c + e0dx*px0 + e0dy*py;
        float e1 = e1c + e1dx*px0 + e1dy*py;
        // Seed the linear accumulators at the row's first pixel.
        float e2    = area - e0 - e1;
        float invZ  = e0*zA + e1*zB + e2*zC;
        float S     = e0*uA + e1*uB + e2*uC;
        float T     = e0*vA + e1*vB + e2*vC;
        float depthAcc = 512.0f * invZ;
        uint8_t* drow = zdepth[y];
        uint8_t* crow = zcolour[y];

        int sub = 0;            // pixels left in the current affine run
        float fu=0, fv=0, dfu=0, dfv=0;   // 8*u, 8*v and their per-pixel steps

        for (int x=minX;x<=maxX;x++,
             e0+=e0dx, e1+=e1dx, invZ+=dInvZ, S+=dS, T+=dT, depthAcc+=dDepth) {
            e2 = area - e0 - e1;
            if (posArea ? (e0<0||e1<0||e2<0) : (e0>0||e1>0||e2>0)) { sub = 0; continue; }
            if (invZ <= 0) { sub = 0; continue; }   // safety; never true inside tri

            int depth = (int)depthAcc;              // invZ>0 -> trunc == floor
            if (depth > 127) depth = 127;
            if (drow[x] > depth) {                  // occluded
                if (sub > 0) { fu += dfu; fv += dfv; sub--; }
                continue;
            }

            if (sub == 0) {                         // perspective-correct sample
                const float rz = 1.0f/invZ;
                fu = 8.0f * S * rz;
                fv = 8.0f * T * rz;
                // local affine gradient: d(u)/dx = rz*(dS - u*dInvZ)
                dfu = 8.0f * rz * (dS - (S*rz)*dInvZ);
                dfv = 8.0f * rz * (dT - (T*rz)*dInvZ);
                sub = PERSP_STEP;
            }

            int a = (int)fu; if (a<0) a=0; else if (a>7) a=7;
            int b = (int)fv; if (b<0) b=0; else if (b>7) b=7;
            uint8_t color = tex[8*(7-b) + a];

            fu += dfu; fv += dfv; sub--;

            if (settings.transparent && color==0) continue;
            if (settings.inverted) color ^= 1;
            if (settings.overlay)  color ^= crow[x];
            drow[x] = (uint8_t)depth;
            crow[x] = color;
        }
    }
}

static int clipNear(const Vertex* in, int n, Vertex* out) {
    int m = 0;
    for (int i=0;i<n;i++) {
        const Vertex& cur = in[i];
        const Vertex& nxt = in[(i+1)%n];
        bool curIn = cur.z >= CLIP, nxtIn = nxt.z >= CLIP;
        if (curIn) out[m++] = cur;
        if (curIn != nxtIn) {
            float t = ((float)CLIP - cur.z) / (nxt.z - cur.z);
            Vertex& e = out[m++];
            e.x = cur.x + t*(nxt.x-cur.x);
            e.y = cur.y + t*(nxt.y-cur.y);
            e.z = CLIP;
            e.u = cur.u + t*(nxt.u-cur.u);
            e.v = cur.v + t*(nxt.v-cur.v);
        }
    }
    return m;
}

void Renderer::drawQuadCam(Vertex q[4]) {
    if (q[0].z < CLIP && q[1].z < CLIP && q[2].z < CLIP && q[3].z < CLIP) return;
    Vertex clipped[8];
    int n = clipNear(q, 4, clipped);
    if (n < 3) return;

    Vertex scr[8];
    for (int i=0;i<n;i++) scr[i] = camToScreen(clipped[i]);

    if (isBackfacing(scr[0], scr[1], scr[2])) {
        if (settings.cullBackface) return;
        for (int i=0, j=n-1; i<j; i++, j--) std::swap(scr[i], scr[j]);
    }

    for (int i=1;i+1<n;i++) rasterTri(scr[0], scr[i], scr[i+1]);
}

void Renderer::drawQuadWorld(const Vertex quad[4]) {
    Vertex cam[4];
    for (int i=0;i<4;i++) cam[i] = worldToCam(quad[i]);
    drawQuadCam(cam);
}

void Renderer::renderQuad(float x,float y,float z,int quadId,uint8_t texId,int texSettings) {

    int bx = (int)floorf(x), bz = (int)floorf(z);
    if (bx < winX0 || bx > winX1 || bz < winZ0 || bz > winZ1) return;
    static const float Uvs[4][2] = {{0,0},{0,1},{1,1},{1,0}};
    const int (*tmpl)[3] = quadTemplate(quadId);
    Vertex cam[4];
    for (int i=0;i<4;i++) {
        Vertex world;
        world.x = x*16.0f + tmpl[i][0];
        world.y = y*16.0f + tmpl[i][1];
        world.z = z*16.0f + tmpl[i][2];
        world.u = Uvs[i][0]; world.v = Uvs[i][1];
        cam[i] = worldToCam(world);
    }
    texture = (Texture)texId;
    settings.cullBackface = (texSettings & 0b1000) != 0;
    settings.transparent  = (texSettings & 0b0100) != 0;
    settings.inverted     = (texSettings & 0b0010) != 0;
    settings.overlay      = (texSettings & 0b0001) != 0;
    drawQuadCam(cam);
}

void Renderer::renderFace(int x,int y,int z,uint8_t texId,int direction,bool small_) {
    int quadId = direction + (small_ ? 8 : 0);
    renderQuad(x, y, z, quadId, texId, 0b1000);
}

void Renderer::renderOverlay(const World& w,int x,int y,int z,int breakPhase) {
    int texId = TEX_BREAK0 + breakPhase;
    static const int Faces[6][3] = {{-1,0,0},{1,0,0},{0,0,-1},{0,0,1},{0,-1,0},{0,1,0}};
    static const int BlockQuads[6] = {QUAD_FULL_NEGX,QUAD_FULL_POSX,QUAD_FULL_NEGZ,QUAD_FULL_POSZ,QUAD_FULL_NEGY,QUAD_FULL_POSY};
    for (int i=0;i<6;i++) {
        uint8_t adj = w.getBlock(x+Faces[i][0], y+Faces[i][1], z+Faces[i][2]);
        if (blockIsTransparent(adj))
            renderQuad(x, y, z, BlockQuads[i], texId, 0b1101);
    }
}

void Renderer::renderItem(float x,float y,float z,uint8_t itemId) {
    const MeshEntry& it = meshItem(itemId);
    if (it.exists && itemIsBlockItem(itemId)) {
        static const int ItemQuads[6] = {QUAD_BLOCKITEM_NEGY,QUAD_BLOCKITEM_POSY,QUAD_BLOCKITEM_NEGX,
                                         QUAD_BLOCKITEM_POSX,QUAD_BLOCKITEM_NEGZ,QUAD_BLOCKITEM_POSZ};
        static const int TexIndices[6] = {1,0,2,2,3,2};
        for (int i=0;i<6;i++) {
            if (TexIndices[i] >= it.texCount) continue;
            const MeshTex& t = it.textures[TexIndices[i]];
            renderQuad(x, y, z, ItemQuads[i], t.id, t.settings);
        }
    } else if (it.exists) {
        for (int qi=0; qi<it.quadCount; qi++) {
            const MeshQuadRef& q = it.quads[qi];
            if (q.texIndex >= it.texCount) continue;
            const MeshTex& t = it.textures[q.texIndex];
            renderQuad(x, y, z, q.quadId, t.id, t.settings);
        }
    }
    renderQuad(x, y, z, QUAD_ITEMSHADOW, TEX_SHADOW, 0b1110);
}

void Renderer::renderScene(const World& w) {

    int camBX = (int)floorf(camPos[0] / (float)BLOCKSIZE);
    int camBZ = (int)floorf(camPos[2] / (float)BLOCKSIZE);
    ActiveWindow win = activeWindowAround(camBX, camBZ, w.worldSX(), w.worldSZ());
    winX0 = win.x0; winX1 = win.x1; winZ0 = win.z0; winZ1 = win.z1;
    int renderYHi = w.activeMaxY(win);

    for (int axis=0; axis<3; axis++) {
        int currentFace, previousFace, currentTexIndex, previousTexIndex;
        if (axis==0) { currentFace=QUAD_FULL_NEGX; previousFace=QUAD_FULL_POSX; currentTexIndex=2; previousTexIndex=2; }
        else if (axis==1) { currentFace=QUAD_FULL_NEGZ; previousFace=QUAD_FULL_POSZ; currentTexIndex=2; previousTexIndex=2; }
        else { currentFace=QUAD_FULL_NEGY; previousFace=QUAD_FULL_POSY; currentTexIndex=1; previousTexIndex=0; }

        int a1lo,a1hi,a2lo,a2hi,a3lo,a3hi;
        if (axis==0)      { a1lo=0;     a1hi=renderYHi; a2lo=winZ0; a2hi=winZ1;    a3lo=winX0; a3hi=winX1; }
        else if (axis==1) { a1lo=winX0; a1hi=winX1;     a2lo=0;     a2hi=renderYHi; a3lo=winZ0; a3hi=winZ1; }
        else              { a1lo=winZ0; a1hi=winZ1;     a2lo=winX0; a2hi=winX1;    a3lo=0;     a3hi=renderYHi; }
        for (int a1=a1lo;a1<=a1hi;a1++) for (int a2=a2lo;a2<=a2hi;a2++) {
            uint8_t previous = BLOCK_AIR; bool previousFull=false, previousTransparent=false;
            for (int a3=a3lo;a3<=a3hi;a3++) {
                int pos[3];
                if (axis==0) { pos[0]=a3; pos[1]=a1; pos[2]=a2; }
                else if (axis==1) { pos[0]=a1; pos[1]=a2; pos[2]=a3; }
                else { pos[0]=a2; pos[1]=a3; pos[2]=a1; }
                uint8_t current = w.getBlock(pos[0],pos[1],pos[2]);
                bool currentFull = blockIsFull(current);
                bool currentTransparent = blockIsTransparent(current);
                if (axis==2) {
                    if (pos[1]==0 && currentTransparent)
                        renderQuad(pos[0],pos[1],pos[2],QUAD_BEDROCK,TEX_STONE,0b1010);
                    else if (pos[1]==renderYHi && currentFull) {
                        const MeshEntry& m = meshBlock(current);
                        if (m.exists && m.texCount>0)
                            renderQuad(pos[0],pos[1],pos[2],QUAD_FULL_POSY,m.textures[0].id,m.textures[0].settings);
                    }
                    if (!currentFull && current != BLOCK_AIR) {
                        const MeshEntry& m = meshBlock(current);
                        if (m.exists) for (int qi=0; qi<m.quadCount; qi++) {
                            const MeshQuadRef& q = m.quads[qi];
                            if (q.texIndex >= m.texCount) continue;
                            const MeshTex& t = m.textures[q.texIndex];
                            renderQuad(pos[0],pos[1],pos[2],q.quadId,t.id,t.settings);
                        }
                    }
                }
                if (current == previous) { previous=current; previousFull=currentFull; previousTransparent=currentTransparent; continue; }
                if (currentTransparent && previousFull) {
                    const MeshEntry& m = meshBlock(previous);
                    if (m.exists && previousTexIndex < m.texCount) {
                        const MeshTex& t = m.textures[previousTexIndex];
                        renderQuad(pos[0]-(axis==0?1:0), pos[1]-(axis==2?1:0), pos[2]-(axis==1?1:0), previousFace, t.id, t.settings);
                    }
                }
                if (previousTransparent && currentFull) {
                    const MeshEntry& m = meshBlock(current);
                    if (m.exists && currentTexIndex < m.texCount) {
                        const MeshTex& t = m.textures[currentTexIndex];
                        renderQuad(pos[0],pos[1],pos[2],currentFace,t.id,t.settings);
                    }
                }
                previous=current; previousFull=currentFull; previousTransparent=currentTransparent;
            }
        }
    }
}

}
