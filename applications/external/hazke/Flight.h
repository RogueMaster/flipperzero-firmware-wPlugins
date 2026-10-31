#pragma once
#include "Platform.h"
#include "Config.h"
#include "Galaxy.h"
#include "SolarSystem.h"
#include "GameState.h"
#include "NPCShip.h"
#include "Combat.h"
#include "Missile.h"
#include "Faction.h"
#include "Rank.h"
#include "Particles.h"
#include "Audio.h"

// Free flight inside one star system: ship kinematics, targeting, warp,
// autolock, sun heat, collisions, landing and gate contact. The rules and
// numbers follow the Cardputer game; drawing lives in World.h.
//
// Orientation is a forward + up unit vector pair. right = up x forward.
// Pitch turns forward+up around right, roll turns up around forward, and
// yaw (autolock only) turns forward around up.

namespace SystemFlight {

constexpr float MaxSpeed = 600.0f; // sysu/s at full throttle
constexpr float WarpSpeed = 7000.0f; // sysu/s
constexpr float WarpDropRange = 3000.0f; // drop out of warp this close
constexpr float WarpMinRange = 1000.0f; // closer than this, steer instead

constexpr float PlanetVisualScale = 1.0f;
constexpr float StarVisualScale = 2.0f;
constexpr float LandingSkin = 30.0f;

constexpr float GateApproachRange = 1500.0f;
constexpr float GateContactRange = 350.0f;

constexpr float SunHeatBuffer = 6000.0f;
constexpr float SunCriticalBuffer = 600.0f;
constexpr float SunWarnFrac = 0.25f;
constexpr float SunDamageFrac = 0.55f;
constexpr float SunFatalFrac = 0.95f;
constexpr float HeatDamageMaxRate = 0.85f;

constexpr float LockMaxRange = 30000.0f;
constexpr float DeathAnimTime = 2.2f;
constexpr float ZoneHysteresis = 1000.0f;
constexpr float ZoneToastTime = 2.0f;

// A fitted ECM fires by itself when a hostile missile gets this close;
// there is no spare button for it.
constexpr float AutoEcmRange = 2000.0f;

// Throttle steps for the BACK tap.
constexpr int ThrottleSteps = 4; // 0, 1/3, 2/3, full

struct Kin {
    float px, py, pz;
    float fx, fy, fz;
    float ux, uy, uz;
    bool initialized;
    int loadedSys;
    int targetIdx; // POI marker, or -1
    bool warping;
    int lockedNPC; // ship marker, or -1 (exclusive with targetIdx)
    bool dying;
    float deathTimer;
    bool deathFinished;
    bool outOfZone;
    float zoneToast;
    bool autoLock;
};

inline Kin state = {
    0,  0,  0,     0.0f, 0.0f,  1.0f, 0.0f,  1.0f,  0.0f, false,
    -1, -1, false, -1,   false, 0.0f, false, false, 0.0f, false,
};

inline SolarSystem::Layout layout;
inline float starR = 0.0f;

// Autodock: autolock flies the ship into the marked planet (or gate) at
// full speed, and the landing (or the chart) takes over on contact.
inline bool autoDock = false;

// Where on the planet the ship came down, as a unit vector from the
// planet's centre. Launch goes back out along it.
inline float landNormal[3] = {0.0f, 0.0f, 0.0f};
inline bool landNormalValid = false;
inline bool patrolHostile[NPCShip::MaxNPCs] = {};

// One-line message over the 3D view.
inline char toastMsg[32] = "";
inline float toastT = 0.0f;
inline void toast(const char* msg, float secs = 1.6f) {
    strncpy(toastMsg, msg, sizeof(toastMsg) - 1);
    toastMsg[sizeof(toastMsg) - 1] = '\0';
    toastT = secs;
}

inline void renormalize() {
    float fl = sqrtf(state.fx * state.fx + state.fy * state.fy + state.fz * state.fz);
    if(fl > 1e-6f) {
        state.fx /= fl;
        state.fy /= fl;
        state.fz /= fl;
    }
    float d = state.ux * state.fx + state.uy * state.fy + state.uz * state.fz;
    state.ux -= state.fx * d;
    state.uy -= state.fy * d;
    state.uz -= state.fz * d;
    float ul = sqrtf(state.ux * state.ux + state.uy * state.uy + state.uz * state.uz);
    if(ul > 1e-6f) {
        state.ux /= ul;
        state.uy /= ul;
        state.uz /= ul;
    } else {
        // Forward is vertical: any horizontal up will do.
        state.ux = 1.0f;
        state.uy = 0.0f;
        state.uz = 0.0f;
    }
}

inline void loadLayout(int sysIdx) {
    SolarSystem::layoutFor(sysIdx, layout);
    state.loadedSys = sysIdx;
    starR = 0.0f;
    for(int i = 0; i < layout.numPOIs; i++) {
        if(layout.poi[i].type == SolarSystem::POIType::Star) starR = (float)layout.poi[i].radius;
    }
}

inline int firstPlanet() {
    for(int i = 0; i < layout.numPOIs; i++) {
        if(layout.poi[i].type == SolarSystem::POIType::Planet) return i;
    }
    return -1;
}

inline int gateIndex() {
    for(int i = 0; i < layout.numPOIs; i++) {
        if(layout.poi[i].type == SolarSystem::POIType::JumpGate) return i;
    }
    return -1;
}

// 0 outside the heat band, 1 at the surface.
inline float sunProximity() {
    if(starR <= 1.0f) return 0.0f;
    float d = sqrtf(state.px * state.px + state.py * state.py + state.pz * state.pz);
    float outer = starR + SunHeatBuffer;
    float inner = starR + SunCriticalBuffer;
    if(d >= outer) return 0.0f;
    if(d <= inner) return 1.0f;
    return (outer - d) / (outer - inner);
}

inline void resetTransient() {
    Combat::resetFlashes();
    Missile::resetAll();
    Particles::reset();
    state.lockedNPC = -1;
    state.dying = false;
    state.deathFinished = false;
    state.deathTimer = 0.0f;
    state.outOfZone = false;
    state.zoneToast = 0.0f;
    state.autoLock = false;
    state.warping = false;
    autoDock = false;
    toastT = 0.0f;
}

// Arrive at the jump gate, facing the star, with planet I marked.
inline void enter(int sysIdx) {
    loadLayout(sysIdx);
    NPCShip::spawnFor(sysIdx, layout);
    resetTransient();

    int gateI = gateIndex();
    if(gateI >= 0) {
        const auto& g = layout.poi[gateI];
        state.px = (float)g.x * 0.9f;
        state.py = (float)g.y;
        state.pz = (float)g.z * 0.9f;
        float len = sqrtf(state.px * state.px + state.py * state.py + state.pz * state.pz);
        state.fx = -state.px / len;
        state.fy = -state.py / len;
        state.fz = -state.pz / len;
    } else {
        state.px = 18000.0f;
        state.py = 0.0f;
        state.pz = 0.0f;
        state.fx = -1.0f;
        state.fy = 0.0f;
        state.fz = 0.0f;
    }
    state.ux = 0.0f;
    state.uy = 1.0f;
    state.uz = 0.0f;
    renormalize();
    state.targetIdx = firstPlanet();
    state.initialized = true;
}

// Launch from a planet: parked about three body radii out, nose away from
// the planet, the planet still marked. The ship goes back out over the spot
// where it landed when that's known, else on the side away from the star.
inline void enterNearPOI(int sysIdx, int poiIdx) {
    bool switched = (state.loadedSys != sysIdx);
    if(switched) {
        loadLayout(sysIdx);
        NPCShip::spawnFor(sysIdx, layout);
    }
    resetTransient();
    if(poiIdx < 0 || poiIdx >= layout.numPOIs) {
        enter(sysIdx);
        return;
    }
    const auto& p = layout.poi[poiIdx];
    float standoff = (float)p.radius * PlanetVisualScale * 3.0f + 600.0f;
    float dx, dy, dz;
    if(landNormalValid) {
        dx = landNormal[0];
        dy = landNormal[1];
        dz = landNormal[2];
    } else {
        float ox = (float)p.x, oz = (float)p.z;
        float len = sqrtf(ox * ox + oz * oz);
        dx = (len > 0.5f) ? ox / len : 1.0f;
        dy = 0.0f;
        dz = (len > 0.5f) ? oz / len : 0.0f;
    }
    state.px = (float)p.x + dx * standoff;
    state.py = (float)p.y + dy * standoff;
    state.pz = (float)p.z + dz * standoff;
    state.fx = dx;
    state.fy = dy;
    state.fz = dz;
    state.ux = 0.0f;
    state.uy = 1.0f;
    state.uz = 0.0f;
    renormalize();
    state.targetIdx = poiIdx;
    state.initialized = true;
}

// ---------- Targets ----------

inline float distTo(float wx, float wy, float wz) {
    float dx = wx - state.px, dy = wy - state.py, dz = wz - state.pz;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

inline float targetDistance() {
    if(state.targetIdx >= 0) {
        const auto& p = layout.poi[state.targetIdx];
        return distTo((float)p.x, (float)p.y, (float)p.z);
    }
    if(state.lockedNPC >= 0 && NPCShip::ships[state.lockedNPC].active) {
        const auto& s = NPCShip::ships[state.lockedNPC];
        return distTo(s.wx, s.wy, s.wz);
    }
    return -1.0f;
}

inline bool hasTarget() {
    return state.targetIdx >= 0 ||
           (state.lockedNPC >= 0 && NPCShip::ships[state.lockedNPC].active);
}

// Walk the marker through every planet, the gate and every live ship.
inline void cycleTarget(int dir) {
    int total = layout.numPOIs + NPCShip::MaxNPCs;
    int cur;
    if(state.lockedNPC >= 0)
        cur = layout.numPOIs + state.lockedNPC;
    else if(state.targetIdx >= 0)
        cur = state.targetIdx;
    else
        cur = (dir > 0) ? total - 1 : 0;
    state.autoLock = false;
    for(int step = 1; step <= total; step++) {
        int v = ((cur + dir * step) % total + total) % total;
        if(v < layout.numPOIs) {
            if(layout.poi[v].type == SolarSystem::POIType::Star) continue;
            state.targetIdx = v;
            state.lockedNPC = -1;
            return;
        }
        int s = v - layout.numPOIs;
        if(!NPCShip::ships[s].active) continue;
        state.lockedNPC = s;
        state.targetIdx = -1;
        return;
    }
}

inline void validateLock() {
    if(state.lockedNPC < 0) return;
    if(state.lockedNPC >= NPCShip::MaxNPCs || !NPCShip::ships[state.lockedNPC].active) {
        state.lockedNPC = -1;
        state.autoLock = false;
    }
}

inline const char* roleName(NPCShip::Role r) {
    switch(r) {
    case NPCShip::Role::Pirate:
        return "PIRATE";
    case NPCShip::Role::Patrol:
        return "PATROL";
    default:
        return "TRADER";
    }
}

inline bool shipHostile(int i) {
    const auto& s = NPCShip::ships[i];
    return s.role == NPCShip::Role::Pirate || patrolHostile[i] || s.provoked;
}

inline void targetName(char* out, size_t cap) {
    if(state.targetIdx >= 0) {
        SolarSystem::displayName(state.loadedSys, layout.poi[state.targetIdx], out, cap);
    } else if(state.lockedNPC >= 0 && NPCShip::ships[state.lockedNPC].active) {
        snprintf(out, cap, "%s", roleName(NPCShip::ships[state.lockedNPC].role));
    } else {
        snprintf(out, cap, "NONE");
    }
}

// ---------- Autolock ----------

constexpr float AutoGain = 6.0f;
constexpr float AutoPitchMax = 1.2f;
constexpr float AutoYawMax = 1.0f;
constexpr float AutoOverride = 0.1f;
constexpr float AutoAlignRad = 0.03f;
inline float autoAimErr = 3.1415927f;

inline bool autoTarget(float& wx, float& wy, float& wz) {
    if(state.lockedNPC >= 0 && state.lockedNPC < NPCShip::MaxNPCs) {
        const auto& sh = NPCShip::ships[state.lockedNPC];
        if(!sh.active) return false;
        wx = sh.wx;
        wy = sh.wy;
        wz = sh.wz;
        return true;
    }
    if(state.targetIdx >= 0 && state.targetIdx < layout.numPOIs) {
        const auto& p = layout.poi[state.targetIdx];
        if(p.type == SolarSystem::POIType::Star) return false;
        wx = (float)p.x;
        wy = (float)p.y;
        wz = (float)p.z;
        return true;
    }
    return false;
}

// Bend the aim point around the star's heat band when the straight line
// to the target would cut through it.
inline void autoAvoidSun(float& tx, float& ty, float& tz) {
    float sr = starR;
    if(sr <= 1.0f) return;
    const float safeR = sr + SunHeatBuffer - SunWarnFrac * (SunHeatBuffer - SunCriticalBuffer);
    const float safe2 = safeR * safeR;
    if(tx * tx + ty * ty + tz * tz < safe2) return;
    float px = state.px, py = state.py, pz = state.pz;
    float sx = tx - px, sy = ty - py, sz = tz - pz;
    float seg2 = sx * sx + sy * sy + sz * sz;
    if(seg2 < 1.0f) return;
    float t = -(px * sx + py * sy + pz * sz) / seg2;
    if(t >= 1.0f) return;
    if(t < 0.0f) t = 0.0f;
    float qx = px + sx * t, qy = py + sy * t, qz = pz + sz * t;
    if(qx * qx + qy * qy + qz * qz >= safe2) return;

    float r = sqrtf(px * px + py * py + pz * pz);
    float phx, phy, phz;
    if(r < 1.0f) {
        phx = state.fx;
        phy = state.fy;
        phz = state.fz;
        r = 1.0f;
    } else {
        phx = px / r;
        phy = py / r;
        phz = pz / r;
    }
    float k = tx * phx + ty * phy + tz * phz;
    float wx = tx - phx * k, wy = ty - phy * k, wz = tz - phz * k;
    float wl = sqrtf(wx * wx + wy * wy + wz * wz);
    if(wl < 1.0f) {
        k = state.ux * phx + state.uy * phy + state.uz * phz;
        wx = state.ux - phx * k;
        wy = state.uy - phy * k;
        wz = state.uz - phz * k;
        wl = sqrtf(wx * wx + wy * wy + wz * wz);
        if(wl < 1e-4f) {
            wx = -phz;
            wy = 0.0f;
            wz = phx;
            wl = sqrtf(wx * wx + wz * wz) + 1e-6f;
        }
    }
    wx /= wl;
    wy /= wl;
    wz /= wl;

    const float ringR = safeR * 1.15f;
    float dx, dy, dz;
    if(r > ringR) {
        float ca = ringR / r, sa = sqrtf(1.0f - ca * ca);
        dx = ringR * (ca * phx + sa * wx) - px;
        dy = ringR * (ca * phy + sa * wy) - py;
        dz = ringR * (ca * phz + sa * wz) - pz;
    } else {
        dx = wx + 0.35f * phx;
        dy = wy + 0.35f * phy;
        dz = wz + 0.35f * phz;
    }
    float dl = sqrtf(dx * dx + dy * dy + dz * dz);
    if(dl < 1e-4f) return;
    const float Lookahead = 4000.0f;
    tx = px + dx / dl * Lookahead;
    ty = py + dy / dl * Lookahead;
    tz = pz + dz / dl * Lookahead;
}

inline void steerAutolock(GameState& g) {
    if(!state.autoLock) return;
    float tx, ty, tz;
    if(state.outOfZone || !autoTarget(tx, ty, tz)) {
        state.autoLock = false;
        autoDock = false;
        Audio::lockFail();
        return;
    }
    autoAvoidSun(tx, ty, tz);
    if(autoDock) g.speed = 1.0f;

    float dx = tx - state.px, dy = ty - state.py, dz = tz - state.pz;
    float rx = state.uy * state.fz - state.uz * state.fy;
    float ry = state.uz * state.fx - state.ux * state.fz;
    float rz = state.ux * state.fy - state.uy * state.fx;
    float cx = dx * rx + dy * ry + dz * rz;
    float cy = dx * state.ux + dy * state.uy + dz * state.uz;
    float cz = dx * state.fx + dy * state.fy + dz * state.fz;

    float lat = sqrtf(cx * cx + cy * cy);
    float err = atan2f(lat, cz);
    autoAimErr = err;
    // Pressing pitch takes the stick back for as long as it's held.
    if(fabsf(g.pitchInput) > AutoOverride) return;
    float ep, ey;
    if(lat < 1e-3f) {
        ep = err;
        ey = 0.0f;
    } else {
        ep = err * cy / lat;
        ey = err * cx / lat;
    }
    float pr = AutoGain * ep, yr = AutoGain * ey;
    float k = 1.0f;
    if(fabsf(pr) * k > AutoPitchMax) k = AutoPitchMax / fabsf(pr);
    if(fabsf(yr) * k > AutoYawMax) k = AutoYawMax / fabsf(yr);
    g.pitchRate = pr * k;
    g.yawRate = yr * k;
}

inline bool autoAligned() {
    return state.autoLock && autoAimErr < AutoAlignRad;
}

// ---------- Engage: warp to a far marker, otherwise steer onto it ----------

// A marked planet or gate is docked with: warp in when it's far, then
// fly the last stretch on autolock. A marked ship is locked on to.
enum class Engage : uint8_t {
    None,
    Warp,
    Dock,
    Autolock
};

inline Engage engageKind() {
    if(!hasTarget() || state.outOfZone) return Engage::None;
    if(state.targetIdx >= 0) return targetDistance() > WarpDropRange ? Engage::Warp : Engage::Dock;
    return Engage::Autolock;
}

inline bool engaged() {
    return state.warping || state.autoLock;
}

inline void disengage() {
    state.warping = false;
    state.autoLock = false;
    autoDock = false;
}

inline bool targetIsGate() {
    return state.targetIdx >= 0 &&
           layout.poi[state.targetIdx].type == SolarSystem::POIType::JumpGate;
}

inline bool engage() {
    switch(engageKind()) {
    case Engage::Warp:
        state.warping = true;
        state.autoLock = false;
        autoDock = true;
        Audio::warpWhoosh();
        return true;
    case Engage::Dock:
        state.autoLock = true;
        autoDock = true;
        autoAimErr = 3.1415927f;
        Audio::lockOn();
        return true;
    case Engage::Autolock:
        state.autoLock = true;
        autoDock = false;
        autoAimErr = 3.1415927f;
        Audio::lockOn();
        return true;
    default:
        Audio::lockFail();
        return false;
    }
}

// ---------- Simulation ----------

inline void triggerDeath() {
    if(state.dying) return;
    state.dying = true;
    state.autoLock = false;
    state.warping = false;
    state.deathTimer = DeathAnimTime;
    Audio::playerDeath();
}

inline void update(GameState& g, float dt) {
    if(toastT > 0.0f) toastT -= dt;

    if(state.dying) {
        state.deathTimer -= dt;
        float ca = cosf(1.8f * dt), sa = sinf(1.8f * dt);
        float rx = state.uy * state.fz - state.uz * state.fy;
        float ry = state.uz * state.fx - state.ux * state.fz;
        float rz = state.ux * state.fy - state.uy * state.fx;
        state.ux = state.ux * ca + rx * sa;
        state.uy = state.uy * ca + ry * sa;
        state.uz = state.uz * ca + rz * sa;
        renormalize();
        static float sparkCD = 0.0f;
        sparkCD -= dt;
        if(sparkCD <= 0.0f) {
            Particles::spawnBurst(
                state.px + state.fx * 400.0f,
                state.py + state.fy * 400.0f,
                state.pz + state.fz * 400.0f,
                0,
                10);
            sparkCD = 0.18f;
        }
        if(state.deathTimer <= 0.0f) state.deathFinished = true;
        return;
    }

    if(state.warping) {
        if(state.targetIdx < 0) {
            state.warping = false;
            return;
        }
        const auto& p = layout.poi[state.targetIdx];
        // Steer round the star's heat band rather than through it.
        float tx = (float)p.x, ty = (float)p.y, tz = (float)p.z;
        autoAvoidSun(tx, ty, tz);
        float dx = tx - state.px;
        float dy = ty - state.py;
        float dz = tz - state.pz;
        float len = sqrtf(dx * dx + dy * dy + dz * dz);
        if(len > 1.0f) {
            state.fx = dx / len;
            state.fy = dy / len;
            state.fz = dz / len;
            state.ux = 0.0f;
            state.uy = 1.0f;
            state.uz = 0.0f;
            renormalize();
        }
        state.px += state.fx * WarpSpeed * dt;
        state.py += state.fy * WarpSpeed * dt;
        state.pz += state.fz * WarpSpeed * dt;
        if(targetDistance() < WarpDropRange) {
            // Drop out with the nose held on the marker, and dock.
            state.warping = false;
            state.autoLock = true;
            autoDock = true;
            autoAimErr = 3.1415927f;
            g.speed = 1.0f;
        }
        return;
    }

    steerAutolock(g);

    {
        float ap = g.pitchRate * dt * 0.9f;
        float cp = cosf(ap), sp = sinf(ap);
        float nfx = state.fx * cp + state.ux * sp;
        float nfy = state.fy * cp + state.uy * sp;
        float nfz = state.fz * cp + state.uz * sp;
        float nux = -state.fx * sp + state.ux * cp;
        float nuy = -state.fy * sp + state.uy * cp;
        float nuz = -state.fz * sp + state.uz * cp;
        state.fx = nfx;
        state.fy = nfy;
        state.fz = nfz;
        state.ux = nux;
        state.uy = nuy;
        state.uz = nuz;
    }
    {
        float ay = g.yawRate * dt * 0.9f;
        float cy = cosf(ay), sy = sinf(ay);
        float rx = state.uy * state.fz - state.uz * state.fy;
        float ry = state.uz * state.fx - state.ux * state.fz;
        float rz = state.ux * state.fy - state.uy * state.fx;
        state.fx = state.fx * cy + rx * sy;
        state.fy = state.fy * cy + ry * sy;
        state.fz = state.fz * cy + rz * sy;
    }
    {
        float ar = g.rollRate * dt * 1.4f;
        float cr = cosf(ar), sr = sinf(ar);
        float rx = state.uy * state.fz - state.uz * state.fy;
        float ry = state.uz * state.fx - state.ux * state.fz;
        float rz = state.ux * state.fy - state.uy * state.fx;
        state.ux = state.ux * cr + rx * sr;
        state.uy = state.uy * cr + ry * sr;
        state.uz = state.uz * cr + rz * sr;
    }
    renormalize();

    float speedMul = (g.hull < 0.5f) ? 0.5f : 1.0f;
    float v = g.speed * MaxSpeed * speedMul;
    state.px += state.fx * v * dt;
    state.py += state.fy * v * dt;
    state.pz += state.fz * v * dt;

    // Open space: past ZoneRadius the system drops behind and markers hide.
    {
        float r2 = state.px * state.px + state.py * state.py + state.pz * state.pz;
        const float outR = SolarSystem::ZoneRadius;
        const float inR = SolarSystem::ZoneRadius - ZoneHysteresis;
        if(!state.outOfZone && r2 > outR * outR) {
            state.outOfZone = true;
            state.zoneToast = ZoneToastTime;
            state.autoLock = false;
            autoDock = false;
            Audio::zoneOut();
        } else if(state.outOfZone && r2 < inR * inR) {
            state.outOfZone = false;
            state.zoneToast = ZoneToastTime;
            Audio::zoneIn();
        }
        if(state.zoneToast > 0.0f) state.zoneToast -= dt;
    }

    // Planets are hard spheres; touching one is the landing trigger.
    for(int i = 0; i < layout.numPOIs; i++) {
        const auto& pp = layout.poi[i];
        if(pp.type != SolarSystem::POIType::Planet) continue;
        float dx = state.px - (float)pp.x;
        float dy = state.py - (float)pp.y;
        float dz = state.pz - (float)pp.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        float surface = (float)pp.radius * PlanetVisualScale + LandingSkin;
        if(d2 < surface * surface) {
            float d = sqrtf(d2);
            if(d > 1.0f) {
                float k = surface / d;
                state.px = (float)pp.x + dx * k;
                state.py = (float)pp.y + dy * k;
                state.pz = (float)pp.z + dz * k;
            } else {
                state.px = (float)pp.x + surface;
            }
            break;
        }
    }

    // Sun heat.
    {
        float prox = sunProximity();
        g.hullHeat = prox;
        static float alarmCooldown = 0.0f;
        alarmCooldown -= dt;
        if(prox >= SunWarnFrac && alarmCooldown <= 0.0f) {
            Audio::alarm();
            alarmCooldown = 1.0f;
        }
        if(prox >= SunFatalFrac) {
            g.hull = 0.0f;
        } else if(prox >= SunDamageFrac) {
            float t = (prox - SunDamageFrac) / (SunFatalFrac - SunDamageFrac);
            if(t < 0.0f)
                t = 0.0f;
            else if(t > 1.0f)
                t = 1.0f;
            g.hull -= HeatDamageMaxRate * t * dt;
            if(g.hull < 0.0f) g.hull = 0.0f;
        }
    }

    // Patrols turn on a player whose standing with them has soured. System
    // 0 is the friendly starter zone.
    const bool friendlySystem = (state.loadedSys == 0);
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        const auto& sh = NPCShip::ships[i];
        patrolHostile[i] = sh.active && sh.role == NPCShip::Role::Patrol && !friendlySystem &&
                           Faction::patrolWouldAttack(g, (Faction::Id)sh.homeFaction);
    }

    NPCShip::update(layout, dt, state.px, state.py, state.pz, patrolHostile);

    // Ship-on-ship collisions.
    static float collisionCD = 0.0f;
    if(collisionCD > 0.0f) collisionCD -= dt;
    constexpr float CollisionRadius = 280.0f;
    constexpr float CollisionDamage = 0.18f;
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        auto& sh = NPCShip::ships[i];
        if(!sh.active) continue;
        float dx = sh.wx - state.px;
        float dy = sh.wy - state.py;
        float dz = sh.wz - state.pz;
        float d2 = dx * dx + dy * dy + dz * dz;
        if(d2 > CollisionRadius * CollisionRadius) continue;
        if(collisionCD <= 0.0f) {
            Combat::damagePlayerHull(g, CollisionDamage, sh.wx, sh.wy, sh.wz);
            Audio::collisionThump();
            collisionCD = 0.45f;
        }
        sh.hull -= CollisionDamage;
        sh.provoked = true;
        if(sh.hull <= 0.0f) Combat::registerPlayerKill(g, sh);
        float dist = sqrtf(d2);
        if(dist > 1.0f) {
            float push = (CollisionRadius - dist) + 30.0f;
            sh.wx += dx / dist * push;
            sh.wy += dy / dist * push;
            sh.wz += dz / dist * push;
        }
    }

    // Warnings: missile beeper speeds up as it closes; klaxon on a wrecked hull.
    {
        static float beepCD = 0.0f, klaxonCD = 0.0f;
        beepCD -= dt;
        klaxonCD -= dt;
        float md = Missile::nearestIncomingDist(state.px, state.py, state.pz);
        if(md >= 0.0f && beepCD <= 0.0f) {
            Audio::incomingBeep();
            float iv = md / 2500.0f;
            beepCD = (iv < 0.12f) ? 0.12f : (iv > 0.9f ? 0.9f : iv);
        }
        if(g.hull > 0.0f && g.hull < 0.25f && klaxonCD <= 0.0f) {
            Audio::hullCritical();
            klaxonCD = 1.6f;
        }
    }

    if(g.hull <= 0.0f) triggerDeath();
}

// ---------- Gate and planets ----------

inline bool gateContact() {
    int i = gateIndex();
    if(i < 0) return false;
    const auto& p = layout.poi[i];
    return distTo((float)p.x, (float)p.y, (float)p.z) < GateContactRange;
}

// Leaving the chart without jumping: park clear of the ring, facing in.
inline void bumpOffGate() {
    int gateI = gateIndex();
    if(gateI < 0) return;
    state.autoLock = false;
    state.warping = false;
    const auto& g = layout.poi[gateI];
    float gx = (float)g.x, gy = (float)g.y, gz = (float)g.z;
    float glen = sqrtf(gx * gx + gz * gz);
    float ix = (glen > 1.0f) ? -gx / glen : -1.0f;
    float iz = (glen > 1.0f) ? -gz / glen : 0.0f;
    const float standoff = 1800.0f;
    state.px = gx + ix * standoff;
    state.py = gy;
    state.pz = gz + iz * standoff;
    state.fx = ix;
    state.fy = 0.0f;
    state.fz = iz;
    state.ux = 0.0f;
    state.uy = 1.0f;
    state.uz = 0.0f;
    renormalize();
}

inline int planetInLandingRange() {
    int best = -1;
    float bestD2 = 1e18f;
    for(int i = 0; i < layout.numPOIs; i++) {
        const auto& p = layout.poi[i];
        if(p.type != SolarSystem::POIType::Planet) continue;
        float dx = (float)p.x - state.px;
        float dy = (float)p.y - state.py;
        float dz = (float)p.z - state.pz;
        float d2 = dx * dx + dy * dy + dz * dz;
        float touch = (float)p.radius * PlanetVisualScale + LandingSkin;
        if(d2 < touch * touch && d2 < bestD2) {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

// ---------- One frame of flight ----------

enum class Event : uint8_t {
    None,
    Landed,
    Gate,
    Dead
};

inline Event step(GameState& g, const InputState& in, float dt) {
    // The stick always wins: any direction drops the autopilot.
    if(!state.dying) {
        bool stick = in.pitchUp || in.pitchDown || in.rollLeft || in.rollRight;
        if(stick && engaged()) {
            disengage();
            toast("MANUAL CONTROL", 1.0f);
            Audio::uiBack();
        }
    }
    if(state.warping || state.dying) {
        // Hands off the stick while the autopilot or the wreck has it.
        InputState none = {};
        g.update(none, dt);
    } else {
        g.update(in, dt);
    }
    update(g, dt);
    if(state.deathFinished) return Event::Dead;

    Combat::tickPlayerCooldown(dt);
    Combat::tick(dt);
    Particles::tick(dt);
    Rank::tickToast(dt);
    if(g.ecmCooldown > 0.0f) {
        g.ecmCooldown -= dt;
        if(g.ecmCooldown < 0.0f) g.ecmCooldown = 0.0f;
    }
    // The shield recharges while it holds; once down it stays down until
    // repaired on a planet.
    if(g.shield > 0.0f && g.shield < 1.0f) {
        g.shield += 0.0267f * dt;
        if(g.shield > 1.0f) g.shield = 1.0f;
    }
    if(state.dying) return Event::None;

    if(!state.warping) {
        Combat::updateNPCs(g, dt);
        if(in.fire) {
            Combat::tryPlayerFire(g, state.px, state.py, state.pz, state.fx, state.fy, state.fz);
        }
        Missile::tickPirateLaunches(g, dt);
        Missile::update(g, dt, state.px, state.py, state.pz);
        validateLock();

        if(g.ecm && g.ecmCooldown <= 0.0f) {
            float md = Missile::nearestIncomingDist(state.px, state.py, state.pz);
            if(md >= 0.0f && md < AutoEcmRange) {
                Missile::triggerECM(state.px, state.py, state.pz);
                g.ecmCooldown = Missile::ECMCooldown;
                toast("ECM");
            }
        }

        if(planetInLandingRange() >= 0) return Event::Landed;
        if(gateContact()) return Event::Gate;
    }
    return Event::None;
}

// Nearest hostile ship in any direction, or -1.
inline int nearestHostile() {
    int best = -1;
    float bestD = LockMaxRange;
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        if(!NPCShip::ships[i].active || !shipHostile(i)) continue;
        const auto& sh = NPCShip::ships[i];
        float d = distTo(sh.wx, sh.wy, sh.wz);
        if(d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

// Mark the nearest hostile and swing the nose onto it.
inline bool attackNearest() {
    int i = nearestHostile();
    if(i < 0) return false;
    state.warping = false;
    state.lockedNPC = i;
    state.targetIdx = -1;
    state.autoLock = true;
    autoDock = false;
    autoAimErr = 3.1415927f;
    Audio::lockOn();
    return true;
}

// Mark the ship closest to the nose, inside the lock cone. False if none.
constexpr float LockConeRad = 0.45f; // about 26 degrees either side
inline bool lockInSight() {
    float bestDot = cosf(LockConeRad);
    int best = -1;
    for(int i = 0; i < NPCShip::MaxNPCs; i++) {
        const auto& sh = NPCShip::ships[i];
        if(!sh.active) continue;
        float dx = sh.wx - state.px, dy = sh.wy - state.py, dz = sh.wz - state.pz;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if(dist < 1.0f || dist > LockMaxRange) continue;
        float dot = (dx * state.fx + dy * state.fy + dz * state.fz) / dist;
        if(dot > bestDot) {
            bestDot = dot;
            best = i;
        }
    }
    if(best < 0) return false;
    state.lockedNPC = best;
    state.targetIdx = -1;
    return true;
}

// Fire a missile at the marked ship, or at the ship nearest the nose when
// no ship is marked.
inline bool fireMissile(GameState& g) {
    if(g.hull < 0.25f) {
        toast("HULL TOO WEAK");
        Audio::deny();
        return false;
    }
    if(g.missiles == 0) {
        toast("NO MISSILES");
        Audio::deny();
        return false;
    }
    if((state.lockedNPC < 0 || !NPCShip::ships[state.lockedNPC].active) && !lockInSight()) {
        toast("NO SHIP IN SIGHT");
        Audio::lockFail();
        return false;
    }
    if(!Missile::spawnPlayer(
           state.px, state.py, state.pz, state.fx, state.fy, state.fz, state.lockedNPC)) {
        Audio::deny();
        return false;
    }
    g.missiles--;
    return true;
}

} // namespace SystemFlight
