
#pragma GCC optimize("O3")

#include "render.h"
#include <algorithm>
#include <cstring>

namespace flipcraft {

static constexpr float PI = 3.14159265358979323846f;

static float kFsin[16], kFcos[16];
static float kSinYaw[16], kCosYaw[16];

// Per-block face textures for full cubes: [block][0 top, 1 bottom, 2 side].
struct FaceTex { uint8_t tex, set; bool valid; };
static FaceTex gFaceTex[32][3];

// Pre-flattened mesh quads for the non-full blocks (sapling cross, chest box).
struct NonFullQuad { uint8_t quad, tex, set; };
struct NonFullMesh { uint8_t count; NonFullQuad q[8]; };
static NonFullMesh gNonFull[32];

// Axis-aligned faces are invisible unless the camera is on their front side;
// this rejects roughly half of all cached faces with one compare, before any
// vertex transform. axis<0 marks quads with no single facing plane.
struct FaceCull { int8_t axis; int8_t neg; uint8_t off; };
static const FaceCull kCull[QUAD_COUNT] = {
    {0,1,0}, {0,0,16}, {2,1,0}, {2,0,16}, {1,1,0}, {1,0,16},   // full cube
    {-1,0,0}, {-1,0,0},                                        // cross
    {0,1,1}, {0,0,15}, {2,1,1}, {2,0,15}, {1,1,0}, {1,0,14},   // small cube
    {-1,0,0},                                                  // item shadow
    {-1,0,0}, {-1,0,0}, {-1,0,0}, {-1,0,0}, {-1,0,0}, {-1,0,0},// block item
    {-1,0,0}, {-1,0,0}, {-1,0,0}, {-1,0,0},                    // cross item
    {1,0,0},                                                   // bedrock
};

static bool gTablesReady = false;

static void initTables() {
    if (gTablesReady) return;

    for (int i = 0; i < 16; i++) {
        float a = PI * 2.0f * (i / 16.0f);
        kFsin[i] = sinf(a);
        kFcos[i] = cosf(a);
        kSinYaw[i] = floorf(-sinf(a) * 64.0f);
        kCosYaw[i] = floorf( cosf(a) * 64.0f);
    }

    for (int id = 0; id < 32; id++) {
        const MeshEntry& m = meshBlock((uint8_t)id);
        for (int f = 0; f < 3; f++) {
            bool valid = m.exists && f < m.texCount;
            gFaceTex[id][f] = {valid ? m.textures[f].id : (uint8_t)0,
                               valid ? m.textures[f].settings : (uint8_t)0, valid};
        }
        NonFullMesh& nf = gNonFull[id];
        nf.count = 0;
        if (m.exists && !blockIsFull((uint8_t)id)) {
            for (int qi = 0; qi < m.quadCount && nf.count < 8; qi++) {
                const MeshQuadRef& q = m.quads[qi];
                if (q.texIndex >= m.texCount) continue;
                nf.q[nf.count++] = {q.quadId, m.textures[q.texIndex].id,
                                    m.textures[q.texIndex].settings};
            }
        }
    }
    gTablesReady = true;
}

Renderer::Renderer() {
    initTables();
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

void Renderer::invalidateChunkMeshes() {
    for (auto& col : chunkMesh)
        for (auto& cm : col) {
            cm.cx = cm.cz = -1;
            cm.faces.clear();
            cm.faces.shrink_to_fit();   // release, don't keep the old world's capacity
        }
}

// matrix[0][1] is 0 by construction (no roll), so row 0 skips the oy term.
Vertex Renderer::worldToCam(const Vertex& v) const {
    float ox = v.x - camPos[0], oy = v.y - camPos[1], oz = v.z - camPos[2];
    Vertex r;
    r.x = matrix[0][0]*ox + matrix[0][2]*oz;
    r.y = matrix[1][0]*ox + matrix[1][1]*oy + matrix[1][2]*oz;
    r.z = matrix[2][0]*ox + matrix[2][1]*oy + matrix[2][2]*oz;
    r.u = v.u; r.v = v.v;
    return r;
}

Vertex Renderer::camToScreen(const Vertex& v) const {
    const float invZ = 1.0f / v.z;
    const float persp = (float)LENS * invZ;
    constexpr float HALF_W   = SCREEN_WIDTH / 2;                        // 64
    constexpr float CENTER_Y = (SCREEN_HEIGHT - 1) - SCREEN_HEIGHT / 2; // 31
    Vertex r;
    r.x = std::clamp(v.x * persp + HALF_W, -255.0f, 255.0f);
    r.y = std::clamp(CENTER_Y - v.y * persp, -255.0f, 255.0f);
    r.z = invZ;
    r.u = v.u * invZ;
    r.v = v.v * invZ;
    return r;
}

bool Renderer::isBackfacing(const Vertex& v1,const Vertex& v2,const Vertex& v3) const {
    float cross = (v3.x - v1.x)*(v1.y - v2.y) - (v1.y - v3.y)*(v2.x - v1.x);
    return cross < 0.0f;
}

void Renderer::clearBuffer() {
    if(zbuf) memset(zbuf, 0, (size_t)SCREEN_HEIGHT * SCREEN_WIDTH);
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

    const uint8_t* trow = texturePacked(texture);
    const bool skipZero   = settings.transparent;
    const uint8_t invMask = settings.inverted ? 1 : 0;
    const bool useOverlay = settings.overlay;

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
        uint8_t* row = zbuf[y];

        bool wasIn = false;     // the row's span is contiguous: leave -> done
        int sub = 0;            // pixels left in the current affine run
        float fu=0, fv=0, dfu=0, dfv=0;   // 8*u, 8*v and their per-pixel steps

        for (int x=minX;x<=maxX;x++,
             e0+=e0dx, e1+=e1dx, e2+=de2dx, invZ+=dInvZ, S+=dS, T+=dT, depthAcc+=dDepth) {
            if ((posArea ? (e0<0||e1<0||e2<0) : (e0>0||e1>0||e2>0)) || invZ <= 0) {
                if (wasIn) break;
                sub = 0;
                continue;
            }
            wasIn = true;

            int depth = (int)depthAcc;              // invZ>0 -> trunc == floor
            if (depth > 127) depth = 127;
            if ((row[x] >> 1) > depth) {            // occluded
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
            uint8_t color = (uint8_t)((trow[b] >> a) & 1);

            fu += dfu; fv += dfv; sub--;

            if (skipZero && color==0) continue;
            color ^= invMask;
            if (useOverlay) color ^= row[x] & 1;
            row[x] = (uint8_t)((depth << 1) | color);
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

static const float kQuadUvs[4][2] = {{0,0},{0,1},{1,1},{1,0}};

// Float-position path used for item entities and overlays; block faces from
// the chunk meshes go through drawBlockQuad instead.
void Renderer::renderQuad(float x,float y,float z,int quadId,uint8_t texId,int texSettings) {
    int bx = ifloor(x), bz = ifloor(z);
    if (bx < winX0 || bx > winX1 || bz < winZ0 || bz > winZ1) return;
    const int (*tmpl)[3] = quadTemplate(quadId);
    Vertex cam[4];
    for (int i=0;i<4;i++) {
        Vertex world;
        world.x = x*16.0f + tmpl[i][0];
        world.y = y*16.0f + tmpl[i][1];
        world.z = z*16.0f + tmpl[i][2];
        world.u = kQuadUvs[i][0]; world.v = kQuadUvs[i][1];
        cam[i] = worldToCam(world);
    }
    texture = (Texture)texId;
    settings.cullBackface = (texSettings & TS_CULLBACK) != 0;
    settings.transparent  = (texSettings & TS_TRANSPARENT) != 0;
    settings.inverted     = (texSettings & TS_INVERTED) != 0;
    settings.overlay      = (texSettings & TS_OVERLAY) != 0;
    drawQuadCam(cam);
}

void Renderer::drawBlockQuad(int x,int y,int z,int quadId,uint8_t texId,int texSettings) {
    const int (*tmpl)[3] = quadTemplate(quadId);
    const float bx = (float)(x << 4), by = (float)(y << 4), bz = (float)(z << 4);
    Vertex cam[4];
    for (int i=0;i<4;i++) {
        Vertex world;
        world.x = bx + tmpl[i][0];
        world.y = by + tmpl[i][1];
        world.z = bz + tmpl[i][2];
        world.u = kQuadUvs[i][0]; world.v = kQuadUvs[i][1];
        cam[i] = worldToCam(world);
    }
    texture = (Texture)texId;
    settings.cullBackface = (texSettings & TS_CULLBACK) != 0;
    settings.transparent  = (texSettings & TS_TRANSPARENT) != 0;
    settings.inverted     = (texSettings & TS_INVERTED) != 0;
    settings.overlay      = (texSettings & TS_OVERLAY) != 0;
    drawQuadCam(cam);
}

void Renderer::renderFace(int x,int y,int z,uint8_t texId,int direction,bool small_) {
    int quadId = direction + (small_ ? 8 : 0);
    renderQuad(x, y, z, quadId, texId, TS_CULLBACK);
}

void Renderer::renderOverlay(const World& w,int x,int y,int z,int breakPhase) {
    int texId = TEX_BREAK0 + breakPhase;
    static const int Faces[6][3] = {{-1,0,0},{1,0,0},{0,0,-1},{0,0,1},{0,-1,0},{0,1,0}};
    static const int BlockQuads[6] = {QUAD_FULL_NEGX,QUAD_FULL_POSX,QUAD_FULL_NEGZ,QUAD_FULL_POSZ,QUAD_FULL_NEGY,QUAD_FULL_POSY};
    for (int i=0;i<6;i++) {
        uint8_t adj = w.getBlock(x+Faces[i][0], y+Faces[i][1], z+Faces[i][2]);
        if (blockIsTransparent(adj))
            renderQuad(x, y, z, BlockQuads[i], texId, TS_CULLBACK|TS_TRANSPARENT|TS_OVERLAY);
    }
}

void Renderer::renderItem(float x,float y,float z,uint8_t itemId,uint8_t inv) {
    const MeshEntry& it = meshItem(itemId);
    if (it.exists && itemIsBlockItem(itemId)) {
        static const int ItemQuads[6] = {QUAD_BLOCKITEM_NEGY,QUAD_BLOCKITEM_POSY,QUAD_BLOCKITEM_NEGX,
                                         QUAD_BLOCKITEM_POSX,QUAD_BLOCKITEM_NEGZ,QUAD_BLOCKITEM_POSZ};
        static const int TexIndices[6] = {1,0,2,2,3,2};
        for (int i=0;i<6;i++) {
            if (TexIndices[i] >= it.texCount) continue;
            const MeshTex& t = it.textures[TexIndices[i]];
            renderQuad(x, y, z, ItemQuads[i], t.id, t.settings ^ inv);
        }
    } else if (it.exists) {
        for (int qi=0; qi<it.quadCount; qi++) {
            const MeshQuadRef& q = it.quads[qi];
            if (q.texIndex >= it.texCount) continue;
            const MeshTex& t = it.textures[q.texIndex];
            renderQuad(x, y, z, q.quadId, t.id, t.settings ^ inv);
        }
    }
    renderQuad(x, y, z, QUAD_ITEMSHADOW, TEX_SHADOW, TS_CULLBACK|TS_TRANSPARENT|TS_INVERTED);
}

// tex[6] = negx,posx,negz,posz,negy,posy; headDir = world side the body's +Z
// points at, top/bottom faces are sampled in body space (v=0 at the head end)
// per face+vertex corner: bit0 pick x1, bit1 pick y1, bit2 pick z1
static const uint8_t kCorner[6][4] = {
    {4,6,2,0}, {1,3,7,5}, {0,2,3,1}, {5,7,6,4}, {4,0,1,5}, {2,6,7,3},
};

void Renderer::renderBox(float x0,float y0,float z0,float x1,float y1,float z1,
                         const uint8_t tex[6],int texSettings,uint8_t headDir) {
    const float px[2]={x0,x1}, py[2]={y0,y1}, pz[2]={z0,z1};
    settings.cullBackface = (texSettings & TS_CULLBACK) != 0;
    settings.transparent  = (texSettings & TS_TRANSPARENT) != 0;
    settings.inverted     = (texSettings & TS_INVERTED) != 0;
    settings.overlay      = (texSettings & TS_OVERLAY) != 0;
    for (int f=0;f<6;f++) {
        Vertex cam[4];
        for (int i=0;i<4;i++) {
            uint8_t c = kCorner[f][i];
            Vertex w;
            w.x = px[c&1]; w.y = py[(c>>1)&1]; w.z = pz[(c>>2)&1];
            if (f>=4) {
                const float cxb=(float)(c&1), czb=(float)((c>>2)&1);
                switch (headDir&3) {
                    case 0:  w.u=czb; w.v=cxb;      break;
                    case 1:  w.u=czb; w.v=1.0f-cxb; break;
                    case 2:  w.u=cxb; w.v=czb;      break;
                    default: w.u=cxb; w.v=1.0f-czb; break;
                }
            } else { w.u = kQuadUvs[i][0]; w.v = kQuadUvs[i][1]; }
            cam[i] = worldToCam(w);
        }
        texture = (Texture)tex[f];
        drawQuadCam(cam);
    }
}

// Lit dynamite entity: full block-size cube, (x,z) centre / y bottom in world
// sub-pixels; inv flashes the fuse
void Renderer::renderDynamite(float x,float y,float z,uint8_t inv) {
    const int bxc = ifloor(x*(1.0f/16.0f)), bzc = ifloor(z*(1.0f/16.0f));
    if (bxc < winX0 || bxc > winX1 || bzc < winZ0 || bzc > winZ1) return;
    static const uint8_t tex[6] = {TEX_DYNAMITE,TEX_DYNAMITE,TEX_DYNAMITE,TEX_DYNAMITE,
                                   TEX_DYNAMITETOP,TEX_DYNAMITETOP};
    renderBox(x-8.0f, y, z-8.0f, x+8.0f, y+16.0f, z+8.0f, tex, TS_CULLBACK ^ inv, 2);
}

// (x,y,z) feet centre in world sub-pixels; yaw = 16-step heading, camera
// convention fwd=(-sin,cos); inv = 0 or TS_INVERTED; sc16 = scale*16
void Renderer::renderMob(float x,float y,float z,uint8_t species,uint8_t yaw,uint8_t inv,uint8_t sc16) {
    const int bxc = ifloor(x*(1.0f/16.0f)), bzc = ifloor(z*(1.0f/16.0f));
    if (bxc < winX0 || bxc > winX1 || bzc < winZ0 || bzc > winZ1) return;
    const MobSpec& s = mobSpec(species);
    // local (lx,lz) -> world: {wx = c*lx - s*lz, wz = s*lx + c*lz}
    const float sn = kFsin[yaw & 0xF], cs = kFcos[yaw & 0xF];
    const float k = sc16*(1.0f/16.0f);
    settings.cullBackface = true;
    settings.transparent  = false;
    settings.inverted     = (inv & TS_INVERTED) != 0;
    settings.overlay      = false;

    int n;
    const MobBox* boxes = mobBoxes(species, n);
    for (int i=0;i<n;i++) {
        const MobBox& bx = boxes[i];
        const float lx[2]={bx.ox*k,(bx.ox+bx.sx)*k};
        const float ly[2]={y+bx.oy*k,y+(bx.oy+bx.sy)*k};
        const float lz[2]={bx.oz*k,(bx.oz+bx.sz)*k};
        for (int f=0;f<6;f++) {
            Vertex cam[4];
            for (int j=0;j<4;j++) {
                uint8_t c = kCorner[f][j];
                const float px=lx[c&1], pz=lz[(c>>2)&1];
                Vertex w;
                w.x = x + cs*px - sn*pz;
                w.y = ly[(c>>1)&1];
                w.z = z + sn*px + cs*pz;
                // top/bottom sampled in body space, v=0 at the head end
                if (f>=4) { w.u=(float)(c&1); w.v=1.0f-(float)((c>>2)&1); }
                else { w.u = kQuadUvs[j][0]; w.v = kQuadUvs[j][1]; }
                cam[j] = worldToCam(w);
            }
            texture = (Texture)((f==3 && (bx.flags&1)) ? s.texFront :
                                f>=4 ? s.texTop : s.texSide);
            drawQuadCam(cam);
        }
    }
    renderQuad((x-4.0f)/16.0f, y/16.0f, (z-4.0f)/16.0f, QUAD_ITEMSHADOW, TEX_SHADOW,
               TS_CULLBACK|TS_TRANSPARENT|TS_INVERTED);
}

// Rebuild the packed face list for the chunk resident in window slot (sx,sz).
// Two passes over the chunk's voxels up to its highest non-air layer: the
// first only counts, so the list is allocated once at exactly its final size
// (no push_back doubling, no high-water capacity kept between rebuilds). Runs
// only when the chunk changed, never per frame.
void Renderer::buildChunkMesh(const World& w, int sx, int sz) {
    ChunkMesh& cm = chunkMesh[sx][sz];
    const int cx = w.slotCX[sx][sz], cz = w.slotCZ[sx][sz];
    cm.cx = cx; cm.cz = cz; cm.gen = w.slotGen[sx][sz];
    // Free the old list before counting so the peak is one list, never two.
    cm.faces.clear();
    cm.faces.shrink_to_fit();

    const uint8_t (*B)[CHUNK_SIZE][CHUNK_SIZE] = w.slot[sx][sz];
    const int bx0 = cx << CHUNK_SHIFT, bz0 = cz << CHUNK_SHIFT;
    // An all-air chunk still owns its bedrock floor, so scan at least y == 0.
    const int yTop = w.slotMaxY[sx][sz] < 0 ? 0 : w.slotMaxY[sx][sz];

    uint32_t* out = nullptr;   // null on the counting pass
    int count = 0;
    auto emit = [&out, &count](int lx, int y, int lz, int quad, uint8_t tex, uint8_t set) {
        if (out)
            out[count] = (uint32_t)lx | ((uint32_t)lz << 3) | ((uint32_t)y << 6) |
                         ((uint32_t)quad << 10) | ((uint32_t)tex << 15) |
                         ((uint32_t)set << 23);
        count++;
    };
    // A face is visible when the neighbour is a different, see-through block.
    auto shows = [](uint8_t id, uint8_t n) { return n != id && blockIsTransparent(n); };

    auto scan = [&]() {
        for (int y = 0; y <= yTop; y++) {
            for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                const uint8_t* row = B[y][lz];
                for (int lx = 0; lx < CHUNK_SIZE; lx++) {
                    const uint8_t id = row[lx];
                    if (y == 0 && blockIsTransparent(id))
                        emit(lx, 0, lz, QUAD_BEDROCK, TEX_STONE, TS_CULLBACK|TS_INVERTED);
                    if (id == BLOCK_AIR) continue;

                    if (!blockIsFull(id)) {
                        const NonFullMesh& nf = gNonFull[id];
                        for (int i = 0; i < nf.count; i++)
                            emit(lx, y, lz, nf.q[i].quad, nf.q[i].tex, nf.q[i].set);
                        continue;
                    }

                    const FaceTex* ft = gFaceTex[id];
                    uint8_t n;
                    if (ft[2].valid) {  // side faces
                        n = lx > 0 ? row[lx-1] : w.getBlock(bx0-1, y, bz0+lz);
                        if (shows(id, n)) emit(lx, y, lz, QUAD_FULL_NEGX, ft[2].tex, ft[2].set);
                        n = lx < CHUNK_MASK ? row[lx+1] : w.getBlock(bx0+CHUNK_SIZE, y, bz0+lz);
                        if (shows(id, n)) emit(lx, y, lz, QUAD_FULL_POSX, ft[2].tex, ft[2].set);
                        n = lz > 0 ? B[y][lz-1][lx] : w.getBlock(bx0+lx, y, bz0-1);
                        if (shows(id, n)) emit(lx, y, lz, QUAD_FULL_NEGZ, ft[2].tex, ft[2].set);
                        n = lz < CHUNK_MASK ? B[y][lz+1][lx] : w.getBlock(bx0+lx, y, bz0+CHUNK_SIZE);
                        if (shows(id, n)) emit(lx, y, lz, QUAD_FULL_POSZ, ft[2].tex, ft[2].set);
                    }
                    // Down face: y == 0 can never be seen from below, skip it.
                    if (y > 0 && ft[1].valid && shows(id, B[y-1][lz][lx]))
                        emit(lx, y, lz, QUAD_FULL_NEGY, ft[1].tex, ft[1].set);
                    n = y < WORLD_SY - 1 ? B[y+1][lz][lx] : (uint8_t)BLOCK_AIR;
                    if (ft[0].valid && shows(id, n))
                        emit(lx, y, lz, QUAD_FULL_POSY, ft[0].tex, ft[0].set);
                }
            }
        }
    };

    scan();                     // pass 1: count faces
    cm.faces.resize(count);     // from zero capacity resize allocates exactly count
    out = cm.faces.data();
    count = 0;
    scan();                     // pass 2: fill; same input, so counts match
}

void Renderer::renderScene(const World& w) {
    const int camBX = ifloor(camPos[0] * (1.0f / (float)BLOCKSIZE));
    const int camBZ = ifloor(camPos[2] * (1.0f / (float)BLOCKSIZE));
    ActiveWindow win = activeWindowAround(camBX, camBZ, w.worldSX(), w.worldSZ());
    winX0 = win.x0; winX1 = win.x1; winZ0 = win.z0; winZ1 = win.z1;

    const float cpx = camPos[0], cpy = camPos[1], cpz = camPos[2];

    for (int sz = 0; sz < WINDOW_CHUNKS; sz++)
        for (int sx = 0; sx < WINDOW_CHUNKS; sx++) {
            const int cx = w.slotCX[sx][sz], cz = w.slotCZ[sx][sz];
            if (cx < 0) continue;
            const int bx0 = cx << CHUNK_SHIFT, bz0 = cz << CHUNK_SHIFT;
            if (bx0 > winX1 || bx0 + CHUNK_MASK < winX0 ||
                bz0 > winZ1 || bz0 + CHUNK_MASK < winZ0) continue;

            ChunkMesh& cm = chunkMesh[sx][sz];
            if (cm.cx != cx || cm.cz != cz || cm.gen != w.slotGen[sx][sz])
                buildChunkMesh(w, sx, sz);

            // Chunks fully inside the window skip the per-face window test.
            const bool clip = bx0 < winX0 || bx0 + CHUNK_MASK > winX1 ||
                              bz0 < winZ0 || bz0 + CHUNK_MASK > winZ1;

            for (uint32_t f : cm.faces) {
                const int gx = bx0 + (f & 7), gz = bz0 + ((f >> 3) & 7);
                if (clip && (gx < winX0 || gx > winX1 || gz < winZ0 || gz > winZ1))
                    continue;
                const int y = (f >> 6) & 15, quad = (f >> 10) & 31;
                const FaceCull& fc = kCull[quad];
                if (fc.axis >= 0) {
                    const float cam = fc.axis == 0 ? cpx : (fc.axis == 1 ? cpy : cpz);
                    const float plane =
                        (float)(((fc.axis == 0 ? gx : (fc.axis == 1 ? y : gz)) << 4) + fc.off);
                    if (fc.neg ? cam >= plane : cam <= plane) continue;
                }
                drawBlockQuad(gx, y, gz, quad, (uint8_t)((f >> 15) & 0xFF), (f >> 23) & 0xF);
            }
        }
}

}
