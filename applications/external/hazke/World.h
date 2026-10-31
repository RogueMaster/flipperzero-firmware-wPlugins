#pragma once
#include "Platform.h"
#include "Config.h"
#include "Render.h"
#include "Flight.h"
#include "Ship3D.h"

// Wireframe cockpit view: stars, dust, sun, planets, gate, ships,
// missiles, sparks and lasers, then the HUD strip. Everything is drawn as
// lit lines on black.

namespace World {

constexpr float Focal = 60.0f; // 128 px wide, about the Cardputer's field of view
constexpr float NearZ = 50.0f;
constexpr float VCX = (float)(Config::ViewX + Config::ViewW / 2);
constexpr float VCY = (float)(Config::ViewY + Config::ViewH / 2);

struct Cam {
    float px, py, pz;
    float rx, ry, rz;
    float ux, uy, uz;
    float fx, fy, fz;
};
inline Cam cam;

inline float animT = 0.0f; // seconds, for blinking and spinning

inline void syncCam() {
    const auto& s = SystemFlight::state;
    cam.px = s.px;
    cam.py = s.py;
    cam.pz = s.pz;
    cam.fx = s.fx;
    cam.fy = s.fy;
    cam.fz = s.fz;
    cam.ux = s.ux;
    cam.uy = s.uy;
    cam.uz = s.uz;
    cam.rx = s.uy * s.fz - s.uz * s.fy;
    cam.ry = s.uz * s.fx - s.ux * s.fz;
    cam.rz = s.ux * s.fy - s.uy * s.fx;
}

// World direction into camera axes (no translation).
inline void dirCam(float dx, float dy, float dz, float& cx, float& cy, float& cz) {
    cx = dx * cam.rx + dy * cam.ry + dz * cam.rz;
    cy = dx * cam.ux + dy * cam.uy + dz * cam.uz;
    cz = dx * cam.fx + dy * cam.fy + dz * cam.fz;
}

inline void toCam(float wx, float wy, float wz, float& cx, float& cy, float& cz) {
    dirCam(wx - cam.px, wy - cam.py, wz - cam.pz, cx, cy, cz);
}

inline void proj(float cx, float cy, float cz, float& sx, float& sy) {
    sx = VCX + cx * Focal / cz;
    sy = VCY - cy * Focal / cz;
}

inline bool projWorld(float wx, float wy, float wz, float& sx, float& sy, float& cz) {
    float cx, cy;
    toCam(wx, wy, wz, cx, cy, cz);
    if(cz < NearZ) return false;
    proj(cx, cy, cz, sx, sy);
    return true;
}

// Camera-space segment, clipped at the near plane.
inline void line3(float ax, float ay, float az, float bx, float by, float bz) {
    if(az < NearZ && bz < NearZ) return;
    if(az < NearZ) {
        float t = (NearZ - az) / (bz - az);
        ax += (bx - ax) * t;
        ay += (by - ay) * t;
        az = NearZ;
    } else if(bz < NearZ) {
        float t = (NearZ - bz) / (az - bz);
        bx += (ax - bx) * t;
        by += (ay - by) * t;
        bz = NearZ;
    }
    float sx0, sy0, sx1, sy1;
    proj(ax, ay, az, sx0, sy0);
    proj(bx, by, bz, sx1, sy1);
    Fb::line(sx0, sy0, sx1, sy1);
}

// ---------- Background: fixed stars and passing dust ----------

constexpr int NumStars = 48;
constexpr int NumDust = 14;
inline float stars[NumStars][3];
inline float dust[NumDust][3];
inline bool skyReady = false;
inline uint32_t skyRng = 0x5EEDu;

inline float skyRand() {
    skyRng = skyRng * 1664525u + 1013904223u;
    return (float)(skyRng >> 8) / 16777216.0f; // 0..1
}

inline void buildSky() {
    for(int i = 0; i < NumStars; i++) {
        float z = skyRand() * 2.0f - 1.0f;
        float a = skyRand() * 6.2831853f;
        float r = sqrtf(1.0f - z * z);
        stars[i][0] = r * cosf(a);
        stars[i][1] = z;
        stars[i][2] = r * sinf(a);
    }
    skyReady = true;
}

inline void respawnDust(int i, bool ahead) {
    float fwd = ahead ? 300.0f + skyRand() * 900.0f : (skyRand() * 2.0f - 1.0f) * 1200.0f;
    float lx = (skyRand() * 2.0f - 1.0f) * 700.0f;
    float ly = (skyRand() * 2.0f - 1.0f) * 450.0f;
    dust[i][0] = cam.px + cam.fx * fwd + cam.rx * lx + cam.ux * ly;
    dust[i][1] = cam.py + cam.fy * fwd + cam.ry * lx + cam.uy * ly;
    dust[i][2] = cam.pz + cam.fz * fwd + cam.rz * lx + cam.uz * ly;
}

inline void scatterDust() {
    syncCam();
    for(int i = 0; i < NumDust; i++)
        respawnDust(i, false);
}

inline void drawStars() {
    for(int i = 0; i < NumStars; i++) {
        float cx, cy, cz;
        dirCam(stars[i][0], stars[i][1], stars[i][2], cx, cy, cz);
        if(cz < 0.2f) continue;
        float sx, sy;
        proj(cx, cy, cz, sx, sy);
        Fb::px(Fb::iround(sx), Fb::iround(sy));
    }
}

inline void drawDust(float speedSysu) {
    float streak = speedSysu * 0.04f;
    if(streak > 260.0f) streak = 260.0f;
    for(int i = 0; i < NumDust; i++) {
        float cx, cy, cz;
        toCam(dust[i][0], dust[i][1], dust[i][2], cx, cy, cz);
        if(cz < 40.0f || cz > 1600.0f || fabsf(cx) > 1600.0f || fabsf(cy) > 1600.0f) {
            respawnDust(i, true);
            continue;
        }
        if(streak > 8.0f) {
            line3(cx, cy, cz, cx, cy, cz + streak);
        } else {
            float sx, sy;
            proj(cx, cy, cz, sx, sy);
            Fb::px(Fb::iround(sx), Fb::iround(sy));
        }
    }
}

// ---------- Bodies ----------

// Circle in a world plane through camera space: centre c (camera space),
// axes a and b (camera space, unit), radius r. `hide` drops segments.
template <typename Hide>
inline void ring3(
    const float c[3],
    const float a[3],
    const float b[3],
    float r,
    int segs,
    float phase,
    Hide hide) {
    float prev[3] = {0, 0, 0};
    for(int k = 0; k <= segs; k++) {
        float ang = phase + (float)k * 6.2831853f / (float)segs;
        float ca = cosf(ang) * r, sa = sinf(ang) * r;
        float p[3] = {
            c[0] + a[0] * ca + b[0] * sa,
            c[1] + a[1] * ca + b[1] * sa,
            c[2] + a[2] * ca + b[2] * sa};
        if(k > 0) {
            float m[3] = {
                (p[0] + prev[0]) * 0.5f, (p[1] + prev[1]) * 0.5f, (p[2] + prev[2]) * 0.5f};
            if(!hide(m)) line3(prev[0], prev[1], prev[2], p[0], p[1], p[2]);
        }
        prev[0] = p[0];
        prev[1] = p[1];
        prev[2] = p[2];
    }
}

inline float screenRadius(float r, float cz) {
    return r * Focal / cz;
}

inline void drawStar(const SolarSystem::POI& p) {
    float cx, cy, cz;
    toCam((float)p.x, (float)p.y, (float)p.z, cx, cy, cz);
    if(cz < NearZ) return;
    float sx, sy;
    proj(cx, cy, cz, sx, sy);
    float r = screenRadius((float)p.radius * SystemFlight::StarVisualScale, cz);
    if(r < 1.5f) {
        int x = Fb::iround(sx), y = Fb::iround(sy);
        Fb::px(x, y);
        Fb::px(x - 1, y);
        Fb::px(x + 1, y);
        Fb::px(x, y - 1);
        Fb::px(x, y + 1);
        return;
    }
    Fb::circle(sx, sy, r);
    if(r > 5.0f) Fb::circle(sx, sy, r * 0.72f);
    // Slowly turning corona rays.
    int rays = r > 10.0f ? 12 : 8;
    for(int k = 0; k < rays; k++) {
        float a = animT * 0.25f + (float)k * 6.2831853f / (float)rays;
        float len = (k & 1) ? 0.22f : 0.38f;
        float c = cosf(a), s = sinf(a);
        Fb::line(
            sx + c * r * 1.12f,
            sy + s * r * 1.12f,
            sx + c * r * (1.12f + len),
            sy + s * r * (1.12f + len));
    }
}

// Ring plane of a planet, as two world axes (from the Cardputer game).
inline void ringBasis(const SolarSystem::POI& p, float u[3], float v[3]) {
    float t = ((int)p.subIdx * 0.42f) + ((p.flags & 0x70) * 0.013f) - 0.6f;
    u[0] = 1.0f;
    u[1] = 0.0f;
    u[2] = 0.0f;
    v[0] = 0.0f;
    v[1] = -sinf(t);
    v[2] = cosf(t);
}

inline void drawPlanet(const SolarSystem::POI& p) {
    float c[3];
    toCam((float)p.x, (float)p.y, (float)p.z, c[0], c[1], c[2]);
    float R = (float)p.radius * SystemFlight::PlanetVisualScale;
    if(c[2] < -R * 2.2f) return; // well behind us, rings included
    bool front = c[2] > NearZ;
    float sx = 0, sy = 0, r = 0;
    if(front) {
        proj(c[0], c[1], c[2], sx, sy);
        // The outline of a near sphere is wider than R at its centre depth.
        float d2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
        float k = d2 - R * R;
        r = k > 1.0f ? Focal * R / sqrtf(k) : screenRadius(R, c[2]) * 4.0f;
    }
    if(front && r < 1.5f) {
        int x = Fb::iround(sx), y = Fb::iround(sy);
        Fb::px(x, y);
        Fb::px(x + 1, y);
        Fb::px(x, y + 1);
        Fb::px(x + 1, y + 1);
        return;
    }

    // World axes: u, v span the equator; n is the pole.
    float uw[3], vw[3];
    ringBasis(p, uw, vw);
    float nw[3] = {
        uw[1] * vw[2] - uw[2] * vw[1],
        uw[2] * vw[0] - uw[0] * vw[2],
        uw[0] * vw[1] - uw[1] * vw[0]};
    float u[3], v[3], n[3];
    dirCam(uw[0], uw[1], uw[2], u[0], u[1], u[2]);
    dirCam(vw[0], vw[1], vw[2], v[0], v[1], v[2]);
    dirCam(nw[0], nw[1], nw[2], n[0], n[1], n[2]);

    // A surface point is visible when its normal faces the camera.
    auto farSide = [&](const float m[3]) {
        return (m[0] - c[0]) * m[0] + (m[1] - c[1]) * m[1] + (m[2] - c[2]) * m[2] > 0.0f;
    };
    // A ring point is hidden when the planet sits between it and us.
    auto behindBody = [&](const float m[3]) {
        float ml = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        if(ml < 1.0f) return false;
        float t = (c[0] * m[0] + c[1] * m[1] + c[2] * m[2]) / ml;
        if(t <= 0.0f || t >= ml) return false;
        float d2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2] - t * t;
        return d2 < R * R;
    };

    if(front) Fb::circle(sx, sy, r);
    // Close enough that the planet covers the whole view: a dotted surface,
    // drifting with the view so the approach still reads.
    {
        float d2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
        bool inside = d2 < R * R * 1.0001f;
        if(!inside && front) {
            const float corners[4][2] = {
                {0, 0},
                {(float)Config::ViewW, 0},
                {0, (float)Config::ViewH},
                {(float)Config::ViewW, (float)Config::ViewH}};
            inside = true;
            for(auto& k : corners) {
                float dx = k[0] - sx, dy = k[1] - sy;
                if(dx * dx + dy * dy > r * r) {
                    inside = false;
                    break;
                }
            }
        }
        if(inside) {
            int ox = (int)(fmodf(sx, 8.0f) + 8.0f), oy = (int)(fmodf(sy, 8.0f) + 8.0f);
            for(int y = oy % 8; y < Config::ViewH; y += 8) {
                for(int x = (ox + ((y / 8) & 1) * 4) % 8; x < Config::ViewW; x += 8)
                    Fb::px(x, y);
            }
        }
    }
    if(!front || r > 7.0f) {
        int segs = (!front || r > 40.0f) ? 28 : 18;
        float spin = animT * 0.08f + (float)p.subIdx;
        float cs = cosf(spin), sn = sinf(spin);
        float a[3] = {u[0] * cs + v[0] * sn, u[1] * cs + v[1] * sn, u[2] * cs + v[2] * sn};
        float b[3] = {v[0] * cs - u[0] * sn, v[1] * cs - u[1] * sn, v[2] * cs - u[2] * sn};
        ring3(c, a, b, R, segs, 0.0f, farSide); // equator
        ring3(c, a, n, R, segs, 0.0f, farSide); // meridians
        ring3(c, b, n, R, segs, 0.0f, farSide);
        // Two latitudes.
        for(int s = -1; s <= 1; s += 2) {
            float h = 0.6f * R * (float)s;
            float lc[3] = {c[0] + n[0] * h, c[1] + n[1] * h, c[2] + n[2] * h};
            ring3(lc, a, b, 0.8f * R, segs, 0.0f, farSide);
        }
    }
    if(p.flags & SolarSystem::PoiFlagRing) {
        int segs = (front && r < 8.0f) ? 16 : 32;
        ring3(c, u, v, R * 1.55f, segs, 0.0f, behindBody);
        ring3(c, u, v, R * 1.9f, segs, 0.0f, behindBody);
    }
}

inline void drawGate(const SolarSystem::POI& p) {
    float gx = (float)p.x, gy = (float)p.y, gz = (float)p.z;
    float c[3];
    toCam(gx, gy, gz, c[0], c[1], c[2]);
    if(c[2] < -600.0f) return;
    if(c[2] > NearZ) {
        float sx, sy;
        proj(c[0], c[1], c[2], sx, sy);
        if(screenRadius((float)p.radius, c[2]) < 1.5f) {
            int x = Fb::iround(sx), y = Fb::iround(sy);
            Fb::rect(x - 1, y - 1, 3, 3);
            return;
        }
    }
    // The ring faces the star.
    float len = sqrtf(gx * gx + gy * gy + gz * gz);
    float nw[3] = {-gx / len, -gy / len, -gz / len};
    float ew[3] = {nw[2], 0.0f, -nw[0]}; // n x up
    float el = sqrtf(ew[0] * ew[0] + ew[2] * ew[2]);
    if(el < 1e-4f) {
        ew[0] = 1.0f;
        ew[2] = 0.0f;
        el = 1.0f;
    }
    ew[0] /= el;
    ew[2] /= el;
    float fw[3] = {
        nw[1] * ew[2] - nw[2] * ew[1],
        nw[2] * ew[0] - nw[0] * ew[2],
        nw[0] * ew[1] - nw[1] * ew[0]};
    float a[3], b[3];
    dirCam(ew[0], ew[1], ew[2], a[0], a[1], a[2]);
    dirCam(fw[0], fw[1], fw[2], b[0], b[1], b[2]);
    auto none = [](const float*) { return false; };
    float R = (float)p.radius;
    ring3(c, a, b, R, 12, 0.0f, none);
    ring3(c, a, b, R * 0.72f, 12, 0.2618f, none);
    // Struts between the rings.
    for(int k = 0; k < 4; k++) {
        float ang = animT * 0.5f + (float)k * 1.5707963f;
        float ca = cosf(ang), sa = sinf(ang);
        float o[3], i[3];
        for(int j = 0; j < 3; j++) {
            o[j] = c[j] + (a[j] * ca + b[j] * sa) * R;
            i[j] = c[j] + (a[j] * ca + b[j] * sa) * R * 0.72f;
        }
        line3(o[0], o[1], o[2], i[0], i[1], i[2]);
    }
}

// ---------- Ships ----------

inline bool hiddenByBody(float dpx, float dpy, float dpz, float dist, int skip = -1) {
    const auto& L = SystemFlight::layout;
    for(int b = 0; b < L.numPOIs; b++) {
        if(b == skip) continue;
        const auto& p = L.poi[b];
        float scale = (p.type == SolarSystem::POIType::Star)   ? SystemFlight::StarVisualScale :
                      (p.type == SolarSystem::POIType::Planet) ? SystemFlight::PlanetVisualScale :
                                                                 0.0f;
        if(scale <= 0.0f) continue;
        float ox = (float)p.x - cam.px, oy = (float)p.y - cam.py, oz = (float)p.z - cam.pz;
        float t = (ox * dpx + oy * dpy + oz * dpz) / dist;
        float R = (float)p.radius * scale;
        if(t < 0.0f || t > dist - R) continue;
        float closest2 = ox * ox + oy * oy + oz * oz - t * t;
        if(closest2 < R * R) return true;
    }
    return false;
}

// Model at camera-space centre c with camera-space forward f and up u.
inline void drawModel(
    const Ship3D::Model& m,
    const float c[3],
    const float f[3],
    const float u[3],
    float scale) {
    float r[3] = {u[1] * f[2] - u[2] * f[1], u[2] * f[0] - u[0] * f[2], u[0] * f[1] - u[1] * f[0]};
    float pts[16][3];
    int n = m.nverts < 16 ? m.nverts : 16;
    for(int i = 0; i < n; i++) {
        float vx = m.verts[i].x * scale, vy = m.verts[i].y * scale, vz = m.verts[i].z * scale;
        for(int j = 0; j < 3; j++)
            pts[i][j] = c[j] + vx * r[j] + vy * u[j] + vz * f[j];
    }
    for(int i = 0; i < m.nedges; i++) {
        const auto& e = m.edges[i];
        if(e.a >= n || e.b >= n) continue;
        line3(pts[e.a][0], pts[e.a][1], pts[e.a][2], pts[e.b][0], pts[e.b][1], pts[e.b][2]);
    }
}

// Forward and up of an NPC from its yaw and pitch, in camera space.
inline void npcBasis(const NPCShip::Ship& sh, float f[3], float u[3]) {
    float Cp = cosf(sh.pitch), Sp = sinf(sh.pitch);
    float Sy = sinf(sh.yaw), Cy = cosf(sh.yaw);
    float wf[3] = {Sy * Cp, Sp, Cy * Cp};
    float wu[3] = {-Sy * Sp, Cp, -Cy * Sp};
    dirCam(wf[0], wf[1], wf[2], f[0], f[1], f[2]);
    dirCam(wu[0], wu[1], wu[2], u[0], u[1], u[2]);
}

// Ships are never drawn smaller than this radius, in pixels.
constexpr float MinShipPx = 4.0f;

// Screen position of each ship this frame, for brackets and lasers.
inline bool shipOnScreen[NPCShip::MaxNPCs];
inline float shipSX[NPCShip::MaxNPCs], shipSY[NPCShip::MaxNPCs], shipSR[NPCShip::MaxNPCs];

inline void drawShips() {
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        shipOnScreen[i] = false;
        const auto& sh = NPCShip::ships[i];
        if(!sh.active) continue;
        float dpx = sh.wx - cam.px, dpy = sh.wy - cam.py, dpz = sh.wz - cam.pz;
        float c[3];
        dirCam(dpx, dpy, dpz, c[0], c[1], c[2]);
        if(c[2] < 60.0f) continue;
        float dist = sqrtf(dpx * dpx + dpy * dpy + dpz * dpz);
        if(dist > 1.0f && hiddenByBody(dpx, dpy, dpz, dist)) continue;
        float scale = Ship3D::scaleFor(sh.modelId);
        float sx, sy;
        proj(c[0], c[1], c[2], sx, sy);
        float sr = screenRadius(scale, c[2]);
        // Far ships keep a readable wireframe instead of shrinking to a dot.
        bool far = sr < MinShipPx;
        if(far) {
            scale = MinShipPx * c[2] / Focal;
            sr = MinShipPx;
        }
        shipOnScreen[i] = true;
        shipSX[i] = sx;
        shipSY[i] = sy;
        shipSR[i] = sr;
        // A blinking tick over distant hostiles.
        if(far && SystemFlight::shipHostile(i) && ((int)(animT * 4.0f) & 1)) {
            int x = Fb::iround(sx), y = Fb::iround(sy - sr) - 3;
            Fb::vline(x, y - 1, y);
        }
        float f[3], u[3];
        npcBasis(sh, f, u);
        drawModel(Ship3D::byId(sh.modelId), c, f, u, scale);
    }
}

inline void drawMissiles() {
    for(int i = 0; i < Missile::MaxMissiles; i++) {
        const auto& m = Missile::pool[i];
        if(!m.active) continue;
        float sx, sy, cz;
        if(!projWorld(m.wx, m.wy, m.wz, sx, sy, cz)) continue;
        int x = Fb::iround(sx), y = Fb::iround(sy);
        Fb::px(x, y);
        Fb::px(x - 1, y - 1);
        Fb::px(x + 1, y + 1);
        Fb::px(x - 1, y + 1);
        Fb::px(x + 1, y - 1);
    }
}

inline void drawParticles() {
    for(int i = 0; i < Particles::MaxParticles; i++) {
        const auto& p = Particles::pool[i];
        if(!p.alive) continue;
        float sx, sy, cz;
        if(!projWorld(p.wx, p.wy, p.wz, sx, sy, cz)) continue;
        int x = Fb::iround(sx), y = Fb::iround(sy);
        Fb::px(x, y);
        if(p.life > p.life0 * 0.6f) Fb::px(x + 1, y);
    }
}

inline void drawLasers() {
    const float bottom = (float)(Config::ViewY + Config::ViewH - 1);
    if(Combat::playerFlash.t > 0.0f) {
        float tx = VCX, ty = VCY;
        int t = Combat::playerFlash.shipIdx;
        if(t >= 0 && t < NPCShip::MaxNPCs && shipOnScreen[t]) {
            tx = shipSX[t];
            ty = shipSY[t];
        }
        Fb::line(10.0f, bottom, tx, ty);
        Fb::line(117.0f, bottom, tx, ty);
    }
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        if(Combat::npcFlash[i].t <= 0.0f || !shipOnScreen[i]) continue;
        float ex = VCX + ((i & 1) ? 9.0f : -9.0f);
        Fb::line(shipSX[i], shipSY[i], ex, VCY + 6.0f);
    }
}

// ---------- Markers ----------

inline void bracket(float sx, float sy, float r, bool tight) {
    int R = (int)(r + 3.0f);
    if(R < 4) R = 4;
    if(R > 24) R = 24;
    int x = Fb::iround(sx), y = Fb::iround(sy);
    int k = tight ? R : 3;
    if(k > R) k = R;
    Fb::hline(x - R, x - R + k - 1, y - R);
    Fb::vline(x - R, y - R, y - R + k - 1);
    Fb::hline(x + R - k + 1, x + R, y - R);
    Fb::vline(x + R, y - R, y - R + k - 1);
    Fb::hline(x - R, x - R + k - 1, y + R);
    Fb::vline(x - R, y + R - k + 1, y + R);
    Fb::hline(x + R - k + 1, x + R, y + R);
    Fb::vline(x + R, y + R - k + 1, y + R);
}

inline void formatDist(char* out, size_t cap, float d) {
    int di = (int)d;
    if(di < 1000)
        snprintf(out, cap, "%d", di);
    else if(di < 100000)
        snprintf(out, cap, "%d.%dK", di / 1000, (di % 1000) / 100);
    else
        snprintf(out, cap, "%dK", di / 1000);
}

// Bracket and name on the marked POI or ship, when it's in view.
inline void drawTargetMarker() {
    const auto& st = SystemFlight::state;
    if(st.outOfZone) return;
    float sx, sy, sr;
    if(st.lockedNPC >= 0) {
        if(!shipOnScreen[st.lockedNPC]) return;
        sx = shipSX[st.lockedNPC];
        sy = shipSY[st.lockedNPC];
        sr = shipSR[st.lockedNPC];
    } else if(st.targetIdx >= 0) {
        const auto& p = SystemFlight::layout.poi[st.targetIdx];
        float cz;
        if(!projWorld((float)p.x, (float)p.y, (float)p.z, sx, sy, cz)) return;
        sr = screenRadius((float)p.radius, cz);
        if(sr > 20.0f) return; // the body itself is the marker now
    } else {
        return;
    }
    if(sx < -10.0f || sx > 138.0f || sy < -10.0f || sy > 64.0f) return;
    bracket(sx, sy, sr, SystemFlight::autoAligned());
    char name[20];
    SystemFlight::targetName(name, sizeof(name));
    int ly = Fb::iround(sy) + (int)(sr + 3.0f) + 3;
    if(ly > Config::ViewH - 6) ly = Fb::iround(sy) - (int)(sr + 3.0f) - 8;
    int lx = Fb::iround(sx) - Fb::textWidth(name) / 2;
    if(lx < 1) lx = 1;
    if(lx + Fb::textWidth(name) > 126) lx = 126 - Fb::textWidth(name);
    Fb::label(lx, ly, name);
}

// Arrow on the view edge toward something off screen. (cx, cy) is its
// camera-space direction; `hollow` draws an open chevron.
inline void edgeArrow(float cx, float cy, bool hollow) {
    float l = sqrtf(cx * cx + cy * cy);
    float dx = 0.0f, dy = 1.0f;
    if(l > 1e-3f) {
        dx = cx / l;
        dy = -cy / l;
    }
    const float hw = (float)Config::ViewW * 0.5f - 3.0f;
    const float hh = (float)Config::ViewH * 0.5f - 3.0f;
    float t = 1e9f;
    if(fabsf(dx) > 1e-4f) t = hw / fabsf(dx);
    if(fabsf(dy) > 1e-4f && hh / fabsf(dy) < t) t = hh / fabsf(dy);
    float tx = VCX + dx * t, ty = VCY + dy * t;
    float bx = tx - dx * 5.0f, by = ty - dy * 5.0f;
    float px = -dy * 3.0f, py = dx * 3.0f;
    Fb::line(tx, ty, bx + px, by + py);
    Fb::line(tx, ty, bx - px, by - py);
    if(!hollow) {
        Fb::line(bx + px, by + py, bx - px, by - py);
        Fb::line(tx, ty, bx, by);
    }
}

// Hostile ships and incoming missiles that aren't in view.
inline void drawEdgeArrows() {
    bool blink = ((int)(animT * 4.0f) & 1) != 0;
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        const auto& sh = NPCShip::ships[i];
        if(!sh.active || !SystemFlight::shipHostile(i)) continue;
        float cx, cy, cz;
        toCam(sh.wx, sh.wy, sh.wz, cx, cy, cz);
        if(cx * cx + cy * cy + cz * cz > NPCShip::DetectRange * NPCShip::DetectRange * 2.25f)
            continue;
        bool inView = shipOnScreen[i] && shipSX[i] >= 0.0f && shipSX[i] < (float)Config::ViewW &&
                      shipSY[i] >= 0.0f && shipSY[i] < (float)Config::ViewH;
        if(inView) continue;
        if(sh.attacking && !blink) continue;
        edgeArrow(cx, cy, false);
    }
    if(!blink) return;
    for(int i = 0; i < Missile::MaxMissiles; i++) {
        const auto& m = Missile::pool[i];
        if(!m.active || m.fromPlayer) continue;
        float cx, cy, cz, sx, sy;
        toCam(m.wx, m.wy, m.wz, cx, cy, cz);
        if(cz > NearZ) {
            proj(cx, cy, cz, sx, sy);
            if(sx >= 0.0f && sx < (float)Config::ViewW && sy >= 0.0f && sy < (float)Config::ViewH)
                continue;
        }
        edgeArrow(cx, cy, true);
    }
}

inline void crosshair() {
    int x = (int)VCX, y = (int)VCY;
    Fb::hline(x - 4, x - 2, y);
    Fb::hline(x + 2, x + 4, y);
    Fb::vline(x, y - 3, y - 2);
    Fb::vline(x, y + 2, y + 3);
}

// ---------- HUD strip ----------

inline void bar(int x, int y, int w, float v) {
    Fb::rect(x, y, w, 5);
    if(v < 0.0f) v = 0.0f;
    if(v > 1.0f) v = 1.0f;
    int fw = (int)((float)(w - 2) * v + 0.5f);
    if(fw > 0) Fb::fill(x + 1, y + 1, fw, 3);
}

// Small compass: where the marker is (or the sun, out in deep space). A
// solid dot is ahead of the nose, a hollow one behind.
inline void compass(int cx, int cy) {
    Fb::circle((float)cx, (float)cy, 4.0f);
    const auto& st = SystemFlight::state;
    float wx = 0.0f, wy = 0.0f, wz = 0.0f;
    bool have = false;
    if(st.outOfZone) {
        have = true; // star at the origin
    } else if(st.lockedNPC >= 0 && NPCShip::ships[st.lockedNPC].active) {
        const auto& s = NPCShip::ships[st.lockedNPC];
        wx = s.wx;
        wy = s.wy;
        wz = s.wz;
        have = true;
    } else if(st.targetIdx >= 0) {
        const auto& p = SystemFlight::layout.poi[st.targetIdx];
        wx = (float)p.x;
        wy = (float)p.y;
        wz = (float)p.z;
        have = true;
    }
    if(!have) {
        Fb::px(cx, cy);
        return;
    }
    float x, y, z;
    toCam(wx, wy, wz, x, y, z);
    float l = sqrtf(x * x + y * y + z * z);
    if(l < 1.0f) return;
    int dx = Fb::iround(x / l * 3.0f), dy = Fb::iround(-y / l * 3.0f);
    if(z >= 0.0f) {
        Fb::fill(cx + dx - 1, cy + dy - 1, 2, 2);
    } else if((int)(animT * 3.0f) & 1) {
        Fb::px(cx + dx, cy + dy);
    }
}

inline void drawHud(const GameState& g) {
    Fb::fullClip();
    const int y = Config::HudY + 2;
    Fb::hline(0, Fb::W - 1, Config::HudY - 1);
    Fb::text(0, y, "SH");
    bar(8, y, 14, g.shield);
    Fb::text(25, y, "HL");
    bar(33, y, 14, g.hull);
    Fb::text(50, y, "SP");
    int notches = (int)(g.speed * (float)(SystemFlight::ThrottleSteps - 1) + 0.5f);
    for(int i = 0; i < SystemFlight::ThrottleSteps - 1; i++) {
        if(i < notches)
            Fb::fill(58 + i * 4, y, 3, 5);
        else
            Fb::rect(58 + i * 4, y, 3, 5);
    }
    compass(76, Config::HudY + 4);
    char buf[16];
    snprintf(buf, sizeof(buf), "M%d", g.missiles);
    Fb::text(84, y, buf);
    float d = SystemFlight::targetDistance();
    // To a planet, show height above the surface.
    int ti = SystemFlight::state.targetIdx;
    if(d >= 0.0f && ti >= 0 && SystemFlight::layout.poi[ti].type == SolarSystem::POIType::Planet) {
        d -= (float)SystemFlight::layout.poi[ti].radius;
        if(d < 0.0f) d = 0.0f;
    }
    if(SystemFlight::state.outOfZone) {
        float r = sqrtf(cam.px * cam.px + cam.py * cam.py + cam.pz * cam.pz);
        formatDist(buf, sizeof(buf), r);
    } else if(d >= 0.0f) {
        formatDist(buf, sizeof(buf), d);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    Fb::textRight(Fb::W - 1, y, buf);
}

// Flags along the top of the view and the centre message line.
inline void drawOverlay(const GameState& g) {
    const auto& st = SystemFlight::state;
    bool blink = ((int)(animT * 4.0f) & 1) != 0;
    int x = 1;
    if(st.warping && blink)
        x = Fb::text(x, 1, "WARP") + 3;
    else if(st.warping)
        x += 19;
    if(st.autoLock) x = Fb::text(x, 1, SystemFlight::autoDock ? "DOCK" : "AUTO") + 3;
    (void)x;
    int rx = Fb::W - 2;
    if(g.hullHeat >= SystemFlight::SunWarnFrac && blink) {
        Fb::textRight(rx, 1, "HEAT");
        rx -= 20;
    }
    if(Missile::incomingToPlayer() && blink) {
        Fb::textRight(rx, 1, "MISSILE");
        rx -= 32;
    }
    if(g.hull > 0.0f && g.hull < 0.25f && !blink) {
        Fb::textRight(rx, 1, "HULL");
    }

    const char* msg = nullptr;
    char buf[28];
    if(st.dying) {
        msg = "SHIP DESTROYED";
    } else if(SystemFlight::toastT > 0.0f) {
        msg = SystemFlight::toastMsg;
    } else if(Combat::killToast > 0.0f) {
        if(Combat::killRole == NPCShip::Role::Pirate) {
            snprintf(buf, sizeof(buf), "PIRATE DOWN +%d CR", Combat::BountyTenthsCR / 10);
        } else {
            snprintf(buf, sizeof(buf), "%s DOWN", SystemFlight::roleName(Combat::killRole));
        }
        msg = buf;
    } else if(Rank::toast > 0.0f) {
        snprintf(buf, sizeof(buf), "RANK: %s", Rank::nameFor((Rank::Id)Rank::toastRank));
        msg = buf;
    } else if(st.zoneToast > 0.0f) {
        snprintf(
            buf,
            sizeof(buf),
            "%s %s",
            st.outOfZone ? "LEAVING" : "ENTERING",
            Galaxy::systems[st.loadedSys].name);
        msg = buf;
    }
    if(msg) Fb::labelCenter((int)VCX, 9, msg);

    // Hit flash: the view border.
    if(Combat::playerHitFlash > 0.0f) {
        Fb::rect(0, 0, Config::ViewW, Config::ViewH);
        Fb::rect(1, 1, Config::ViewW - 2, Config::ViewH - 2);
    }
}

// ---------- Whole frame ----------

inline void drawFlight(const GameState& g) {
    if(!skyReady) buildSky();
    syncCam();
    Fb::clear();
    Fb::setClip(
        Config::ViewX,
        Config::ViewY,
        Config::ViewX + Config::ViewW - 1,
        Config::ViewY + Config::ViewH - 1);
    drawStars();
    float v = SystemFlight::state.warping ? SystemFlight::WarpSpeed :
                                            g.speed * SystemFlight::MaxSpeed;
    drawDust(v);
    const auto& L = SystemFlight::layout;
    for(int i = 0; i < L.numPOIs; i++) {
        const auto& p = L.poi[i];
        if(p.type != SolarSystem::POIType::Star) {
            // Planets and the gate vanish behind the sun or a nearer planet.
            float dx = (float)p.x - cam.px, dy = (float)p.y - cam.py, dz = (float)p.z - cam.pz;
            float d = sqrtf(dx * dx + dy * dy + dz * dz);
            if(d > 1.0f && hiddenByBody(dx, dy, dz, d, i)) continue;
        }
        switch(p.type) {
        case SolarSystem::POIType::Star:
            drawStar(p);
            break;
        case SolarSystem::POIType::Planet:
            drawPlanet(p);
            break;
        case SolarSystem::POIType::JumpGate:
            drawGate(p);
            break;
        default:
            break;
        }
    }
    drawShips();
    drawMissiles();
    drawParticles();
    drawLasers();
    drawTargetMarker();
    if(!SystemFlight::state.dying) {
        drawEdgeArrows();
        crosshair();
    }
    drawOverlay(g);
    drawHud(g);
}

// ---------- Title ship and hyperspace ----------

inline void drawTitleShip(float t) {
    Fb::clear();
    if(!skyReady) buildSky();
    cam = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    Fb::fullClip();
    // Stars drift slowly behind the ship.
    float a = t * 0.05f;
    float ca = cosf(a), sa = sinf(a);
    for(int i = 0; i < NumStars; i++) {
        float x = stars[i][0] * ca + stars[i][2] * sa;
        float z = -stars[i][0] * sa + stars[i][2] * ca;
        if(z < 0.2f) continue;
        float sx, sy;
        proj(x, stars[i][1], z, sx, sy);
        Fb::px(Fb::iround(sx), Fb::iround(sy));
    }
    float yaw = t * 0.7f;
    float c[3] = {330.0f, -240.0f, 1050.0f}; // below eye level: seen from above
    float f[3] = {sinf(yaw), 0.15f * sinf(t * 0.9f), cosf(yaw)};
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    f[0] /= fl;
    f[1] /= fl;
    f[2] /= fl;
    float u[3] = {0.0f, 1.0f, 0.0f};
    float d = u[0] * f[0] + u[1] * f[1] + u[2] * f[2];
    for(int j = 0; j < 3; j++)
        u[j] -= f[j] * d;
    float ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for(int j = 0; j < 3; j++)
        u[j] /= ul;
    drawModel(Ship3D::byId(Ship3D::Wedge), c, f, u, 300.0f);
}

// A tunnel of rings rushing outward, with streaks, speeding up.
inline void drawHyperspace(float t) {
    Fb::clear();
    Fb::fullClip();
    if(!skyReady) buildSky();
    const float cx = 64.0f, cy = 30.0f;
    float phase = t * (1.0f + 2.5f * t);
    for(int k = 0; k < 5; k++) {
        float s = fmodf(phase + (float)k * 0.2f, 1.0f);
        s *= s;
        float rx = 4.0f + 80.0f * s, ry = 2.0f + 44.0f * s;
        float px = cx + rx, py = cy;
        for(int j = 1; j <= 20; j++) {
            float a = (float)j * 6.2831853f / 20.0f;
            float nx = cx + cosf(a) * rx, ny = cy + sinf(a) * ry;
            Fb::line(px, py, nx, ny);
            px = nx;
            py = ny;
        }
    }
    float len = 4.0f + 14.0f * t;
    for(int i = 0; i < 20; i++) {
        float ang = stars[i][0] * 3.14159f + stars[i][2] * 2.0f;
        float r0 = fmodf(0.5f + 0.5f * stars[i][1] + phase * 1.3f, 1.0f);
        float k0 = 6.0f + r0 * r0 * 70.0f;
        float c = cosf(ang), s = sinf(ang);
        Fb::line(
            cx + c * k0, cy + s * k0 * 0.55f, cx + c * (k0 + len), cy + s * (k0 + len) * 0.55f);
    }
}

// ---------- System map ----------

// Top-down view of the system: x across, z down. `sel` is a POI index or
// MaxPOIs + ship slot.
inline void drawSystemMap(int sel) {
    Fb::clear();
    Fb::fullClip();
    syncCam();
    const auto& L = SystemFlight::layout;
    const float cx = 64.0f, cy = 26.0f, k = 25.0f / 22000.0f;
    auto mx = [&](float x) { return cx + x * k; };
    auto my = [&](float z) { return cy + z * k; };
    // Zone edge, dotted.
    for(int a = 0; a < 64; a++) {
        float ang = (float)a * 6.2831853f / 64.0f;
        Fb::px(
            Fb::iround(cx + cosf(ang) * SolarSystem::ZoneRadius * k),
            Fb::iround(cy + sinf(ang) * SolarSystem::ZoneRadius * k));
    }
    for(int i = 0; i < L.numPOIs; i++) {
        const auto& p = L.poi[i];
        float x = mx((float)p.x), y = my((float)p.z);
        switch(p.type) {
        case SolarSystem::POIType::Star:
            Fb::circle(x, y, 3.0f);
            Fb::px(Fb::iround(x), Fb::iround(y));
            break;
        case SolarSystem::POIType::Planet: {
            // Orbit, dotted.
            float orb = sqrtf((float)p.x * (float)p.x + (float)p.z * (float)p.z) * k;
            for(int a = 0; a < 48; a += 2) {
                float ang = (float)a * 6.2831853f / 48.0f;
                Fb::px(Fb::iround(cx + cosf(ang) * orb), Fb::iround(cy + sinf(ang) * orb));
            }
            Fb::circle(x, y, 2.0f);
            break;
        }
        case SolarSystem::POIType::JumpGate:
            Fb::rect(Fb::iround(x) - 2, Fb::iround(y) - 2, 5, 5);
            break;
        default:
            break;
        }
        if(i == sel) bracket(x, y, 3.0f, false);
    }
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        const auto& s = NPCShip::ships[i];
        if(!s.active) continue;
        int x = Fb::iround(mx(s.wx)), y = Fb::iround(my(s.wz));
        bool hostile = SystemFlight::shipHostile(i);
        if(!hostile || ((int)(animT * 4.0f) & 1)) {
            Fb::px(x, y);
            if(hostile) {
                Fb::px(x - 1, y);
                Fb::px(x + 1, y);
                Fb::px(x, y - 1);
                Fb::px(x, y + 1);
            }
        }
        if(L.numPOIs + i == sel) bracket((float)x, (float)y, 2.0f, false);
    }
    // Player: a small arrow along the heading.
    float px = mx(cam.px), py = my(cam.pz);
    float hx = cam.fx, hz = cam.fz;
    float hl = sqrtf(hx * hx + hz * hz);
    if(hl < 1e-3f) {
        hx = 0.0f;
        hz = -1.0f;
        hl = 1.0f;
    }
    hx /= hl;
    hz /= hl;
    Fb::line(px, py, px + hx * 5.0f, py + hz * 5.0f);
    Fb::line(px + hx * 5.0f, py + hz * 5.0f, px - hz * 2.0f, py + hx * 2.0f);
    Fb::line(px + hx * 5.0f, py + hz * 5.0f, px + hz * 2.0f, py - hx * 2.0f);
}

} // namespace World
